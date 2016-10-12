#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "coroutine.h"

#define YIELD_TIMES 5
coroutine_mgr_t *mgr = NULL;
coroutine_t *co1 = NULL;
coroutine_t *co2 = NULL;
int g_counter = 0;

void print_odd(void *args) {
    int i;
    for (i = 0; i < YIELD_TIMES; i++) {
        g_counter++;
        printf("%d\n", g_counter);
        co_yield(co1);
    }
    char buff[60 * 1024];
    memset(buff, 0, sizeof(buff));
}

void print_even(void *args) {
    char buff[60 * 1024];
    memset(buff, 0, sizeof(buff));
    int i;
    for (i = 0; i < YIELD_TIMES; i++) {
        g_counter++;
        printf("%d\n", g_counter);
        co_yield(co2);
    }
}

int main() {
    mgr = co_mgr_open();
    assert(mgr != NULL);
    co1 = co_get(mgr, print_odd, NULL);
    assert(co1 != NULL);
    co2 = co_get(mgr, print_even, NULL);
    assert(co2 != NULL);
    int i;
    for (i = 0; i <= YIELD_TIMES; i++) {
        co_resume(co1);
        co_resume(co2);
    }
    assert(g_counter == YIELD_TIMES * 2);
    co_mgr_close(mgr);
    return 0;
}
