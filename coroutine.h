#ifndef __COROUTINE_H__
#define __COROUTINE_H__

#if __APPLE__ && __MACH__
    #include <sys/ucontext.h>
#else 
    #include <ucontext.h>
#endif

#define DEFAULT_POOL_IDLE_NUM (32)                  // 16
#define DEFAULT_STACK_SIZE (64 * 1024)             

typedef void(*co_func)(void *);

enum CoroutineState {
    COROUTINE_STATE_DEAD    = 0,
    COROUTINE_STATE_READY   = 1,
    COROUTINE_STATE_RUNNING = 2,
    COROUTINE_STATE_SUSPEND = 3,
    COROUTINE_STATE_IDLE    = 4,
};

struct coroutine_mgr_t;

typedef struct coroutine_t {
    enum CoroutineState state;
    char *stack;                    // coroutine stack
    size_t stack_size;
    ucontext_t uc;
    struct coroutine_mgr_t *mgr;
    co_func func;
    void *args;
    // doubly linked list
    struct coroutine_t *prev;
    struct coroutine_t *next;
} coroutine_t;

typedef struct coroutine_mgr_t {
    ucontext_t main_uc;
    coroutine_t *current;           // current coroutine
    coroutine_t *running;           // all running coroutines(include suspends)
    coroutine_t *idle;              // all idle coroutines
    int idle_num;
} coroutine_mgr_t;

coroutine_mgr_t *co_mgr_open();
void co_mgr_close(coroutine_mgr_t *mgr);

coroutine_t *co_get(coroutine_mgr_t *mgr, co_func func, void *args);
void co_yield(coroutine_t *co);
void co_resume(coroutine_t *co);
enum CoroutineState co_state(coroutine_t *co);

#endif
