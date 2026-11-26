#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "online.h"

uint64_t plat_now_ns(void) { return 1; }
struct { uint64_t dummy[64]; } g_site;

int main(void) {
    assert(online_state() == ONLINE_OFF);
    assert(ONLINE_MAX == 8);
    assert(ONLINE_PORT == 52520);
    printf("PASS: coop-addon default-off state and protocol constants\n");
    return 0;
}
