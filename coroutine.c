#include <assert.h>
#include <stdlib.h>
#include <stdint.h>     // uintptr_t
#include <sys/mman.h>   // mmap munmap
#include <unistd.h>     // sysconf(_SC_PAGESIZE)

#include "coroutine.h"

static size_t page_size() {
    long psize = sysconf(_SC_PAGESIZE);
    return psize;
}

static size_t page_count(size_t size) {
    size_t psize = page_size();
    return (size + psize - 1) / psize;
}

// doubly linked list
void remove(coroutine_t **head, coroutine_t *co) {
    if (co->prev == NULL) {
        if (co->next != NULL) {
            co->next->prev = NULL;
        }
        *head = co->next;
    } else {
        co->prev->next = co->next;
        if (co->next != NULL) {
            co->next->prev = co->prev;
        }
    }
    co->prev = co->next = NULL;
}

void insert(coroutine_t **head, coroutine_t *co) {
    if (*head == NULL) {
        co->prev = NULL;
        co->next = NULL;
        *head = co;
    } else {
        co->prev = NULL;
        co->next = *head;
        (*head)->prev = co;
        *head = co;
    }
}

static coroutine_t *co_new(coroutine_mgr_t *mgr) {
    coroutine_t *co = (coroutine_t *)malloc(sizeof(coroutine_t));
    if (co == NULL) {
        // malloc failed
        return NULL;
    }
    co->mgr = mgr;

    // two guard page on each side
    //
    //  ----------------------------------
    // | guard | real stack space | guard |
    //  ----------------------------------
    //
    size_t size = (page_count(DEFAULT_STACK_SIZE) + 2) * page_size();
    char *addr = (char *)mmap(0, size, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == (char *)MAP_FAILED) {
        // mmap failed
        free(co);
        return NULL;
    }
    mprotect(addr, page_size(), PROT_NONE);
    mprotect(addr + size - page_size(), page_size(), PROT_NONE);
    co->stack = addr + page_size();
    co->stack_size = size - 2 * page_size();

    return co;
}

static void co_free(coroutine_t *co) {
    assert(co->stack != NULL);
    munmap(co->stack - page_size(), co->stack_size + 2 * page_size());
    co->stack = NULL;
    free(co);
}

static void co_main_func(uint32_t low32, uint32_t hi32) {
    coroutine_mgr_t *mgr = (coroutine_mgr_t *)((uintptr_t)low32 | ((uintptr_t)hi32 << 32));
    coroutine_t *co = mgr->current;
    assert(co != NULL);
    for (;;) {
        assert(co->func != NULL);
        co->func(co->args);
        co->func = NULL;
        co->args = NULL;
        co->state = COROUTINE_STATE_IDLE;
        co_yield(co);
    }
    // never go here
    co->state = COROUTINE_STATE_DEAD;
    mgr->current = NULL;
}

coroutine_mgr_t *co_mgr_open() {
    coroutine_mgr_t *mgr = (coroutine_mgr_t *)malloc(sizeof(coroutine_mgr_t));
    if (mgr == NULL) {
        // malloc failed!
        return NULL;
    }
    mgr->current = NULL;
    mgr->running = NULL;
    mgr->idle = NULL;
    mgr->idle_num = 0;
    return mgr;
}

void co_mgr_close(coroutine_mgr_t *mgr) {
    assert(mgr->current == NULL);
    assert(mgr->running == NULL);
    while (mgr->idle) {
        coroutine_t *del = mgr->idle;
        mgr->idle = del->next;
        co_free(del);
    }
    free(mgr);
}

coroutine_t *co_get(coroutine_mgr_t *mgr, co_func func, void *args) {
    coroutine_t *co = NULL;
    if (mgr->idle_num > 0) {
        co = mgr->idle;
        remove(&(mgr->idle), co);
        mgr->idle_num--;
        assert(co->state == COROUTINE_STATE_IDLE);
        // free excess coroutines
        while (mgr->idle_num > DEFAULT_POOL_IDLE_NUM) {
            coroutine_t *del = mgr->idle;
            remove(&(mgr->idle), del);
            mgr->idle_num--;
            co_free(del);
        }
    } else {
        co = co_new(mgr);
        if (co == NULL)
            return NULL;
        co->state = COROUTINE_STATE_READY;
    }
    // we always put new coroutine in used list,
    // although it does not call co_resume() yet.
    // because it is easy to find "coroutine leaks".
    insert(&(mgr->running), co);
    co->func = func;
    co->args = args;
    return co;
}

void co_yield(coroutine_t *co) {
    coroutine_mgr_t *mgr = co->mgr;
    assert(co == mgr->current);
    switch (co->state) {
        case COROUTINE_STATE_RUNNING:
            co->state = COROUTINE_STATE_SUSPEND;
            break;
        case COROUTINE_STATE_IDLE:
            // task is done.
            // remove coroutine from running list
            // and return it to idle list
            remove(&(mgr->running), co);
            insert(&(mgr->idle), co);
            mgr->idle_num++;
            break;
        default:
            assert(0);
    }
    mgr->current = NULL;
    swapcontext(&(co->uc), &(mgr->main_uc));
}

void co_resume(coroutine_t *co) {
    // not allow to resume another coroutine when has been in a coroutine.
    coroutine_mgr_t *mgr = co->mgr;
    assert(mgr->current == NULL);
    mgr->current = co;
    switch (co->state) {
        case COROUTINE_STATE_READY:
            getcontext(&co->uc);
            co->uc.uc_stack.ss_sp = co->stack;
            co->uc.uc_stack.ss_size = co->stack_size;
            co->uc.uc_link = &(mgr->main_uc);
            uintptr_t ptr = (uintptr_t)mgr;
            makecontext(&(co->uc), (void (*)(void))co_main_func, 2, (uint32_t)ptr, (uint32_t)(ptr >> 32));
        case COROUTINE_STATE_IDLE:
        case COROUTINE_STATE_SUSPEND:
            co->state = COROUTINE_STATE_RUNNING;
            swapcontext(&(mgr->main_uc), &(co->uc));
            break;
        default:
            assert(0);
    }
}

coroutine_t *co_current(coroutine_mgr_t *mgr) {
    return mgr->current;
}

enum CoroutineState co_state(coroutine_t *co) {
    return co->state;
}
