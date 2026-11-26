/* Synthetic check of the task-01 launch-guard decision logic. The real helper lives in runtime/hle/framepace.c and
 * drags in the whole runtime, so its predicate and its two stores are mirrored here; keep the two in sync with
 * rt_slide_launch_clear. Guest addresses are directly addressable there; here a small byte buffer stands in for the
 * actor. No game files, generated code or saves are needed. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { LAUNCH_TIMER = 0x1844, LAUNCH_HEIGHT = 0x20bc, LAUNCH_HEIGHT_2 = 0x20c0 };

static const uint32_t stale = UINT32_C(0x3f800000);  /* 1.0f: a launch height left by the guest */
static unsigned char actor[0x2100];
static int guard_option;
static float frame_ratio;

static float actor_float(size_t off) {
    float value;
    memcpy(&value, actor + off, sizeof value);
    return value;
}

static uint32_t field(size_t off) {
    uint32_t value;
    memcpy(&value, actor + off, sizeof value);
    return value;
}

/* Mirrors rt_slide_launch_clear (runtime/hle/framepace.c). */
static void slide_launch_clear(uint32_t value) {
    if (!guard_option || frame_ratio >= 1.0f || actor_float(LAUNCH_TIMER) <= 0.0f) {
        memcpy(actor + LAUNCH_HEIGHT, &value, sizeof value);
        memcpy(actor + LAUNCH_HEIGHT_2, &value, sizeof value);
    }
}

static void reset(uint32_t timer_bits, float ratio, int option) {
    memset(actor, 0, sizeof actor);
    memcpy(actor + LAUNCH_HEIGHT, &stale, sizeof stale);
    memcpy(actor + LAUNCH_HEIGHT_2, &stale, sizeof stale);
    memcpy(actor + LAUNCH_TIMER, &timer_bits, sizeof timer_bits);
    frame_ratio = ratio;
    guard_option = option;
}

static void expect_fields(uint32_t want) {
    assert(field(LAUNCH_HEIGHT) == want);
    assert(field(LAUNCH_HEIGHT_2) == want);
}

int main(void) {
    const uint32_t running = UINT32_C(0x40a00000);  /* 5.0f: the delta-scaled launch timer is still running */
    const uint32_t ended = UINT32_C(0xbf800000);    /* -1.0f: it has run out */
    const uint32_t slow_value = UINT32_C(0x3d4ccccd);  /* 0.05f: guest s0 on one reachable path */

    /* Option off: the guest stores happen at every frame rate. */
    reset(running, 1.0f, 0);
    slide_launch_clear(0);
    expect_fields(0);
    reset(running, 0.25f, 0);
    slide_launch_clear(0);
    expect_fields(0);

    /* Option on at 30 fps: the clear still happens. */
    reset(running, 1.0f, 1);
    slide_launch_clear(0);
    expect_fields(0);

    /* Option on above 30 fps with the timer running: both fields are left alone. */
    reset(running, 0.25f, 1);
    slide_launch_clear(0);
    expect_fields(stale);

    /* Above 30 fps once the timer ends (zero or negative): the clear resumes. */
    reset(0, 0.25f, 1);
    slide_launch_clear(0);
    expect_fields(0);
    reset(ended, 0.25f, 1);
    slide_launch_clear(0);
    expect_fields(0);

    /* The stores write the guest s0 value through, not a literal zero. */
    reset(running, 0.25f, 1);
    slide_launch_clear(slow_value);
    expect_fields(stale);
    reset(0, 0.25f, 1);
    slide_launch_clear(slow_value);
    expect_fields(slow_value);

    puts("Launch guard decision logic: PASS (in-game effect unverified)");
    return 0;
}
