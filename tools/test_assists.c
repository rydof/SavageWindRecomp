// Synthetic unit tests for runtime/assists.c (including stamina_drain multiplier, #968).
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "settings.h"

Settings g_settings = {.weapon_wear = 100, .damage_taken = 100, .damage_dealt = 100, .stamina_drain = 100, .game_speed = 100};

static uint64_t mock_ns;
uint64_t plat_now_ns(void) { return mock_ns; }

uint32_t rt_stamina(uint32_t oldv, uint32_t newv);
int32_t rt_link_life(int32_t oldv, int32_t newv);
uint32_t rt_life_cost(uint32_t cost, int32_t life, int whose);
bool assists_no_fall_damage_enabled(void);
void assists_link_hp_update(int32_t cur_qh, int32_t max_qh);
int32_t assists_health_regen_step(int32_t cur_qh, int32_t max_qh, uint64_t now_ns);
float assists_speed_scale(float ratio, bool active);
bool assists_health_hud_snapshot(int32_t *link_cur, int32_t *link_max, int32_t *enemy_cur, int32_t *enemy_max,
                                 float *enemy_fade, uint64_t now_ns);

static uint32_t f2u(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}
static float u2f(uint32_t u) {
    float f;
    memcpy(&f, &u, 4);
    return f;
}

static int checks, fails;
static void check(bool ok, const char *name) {
    checks++;
    if (!ok) {
        fails++;
        fprintf(stderr, "FAIL: %s\n", name);
    }
}

int main(void) {
    // 100% passthrough
    g_settings.stamina_drain = 100;
    g_settings.infinite_stamina = false;
    check(rt_stamina(f2u(100.0f), f2u(90.0f)) == f2u(90.0f), "100% drain exact passthrough");

    // 50% drain: 100 -> 90 becomes 100 -> 95
    g_settings.stamina_drain = 50;
    check(fabsf(u2f(rt_stamina(f2u(100.0f), f2u(90.0f))) - 95.0f) < 1e-5f, "50% drain halves loss");

    // 25% drain: 100 -> 80 becomes 100 -> 95
    g_settings.stamina_drain = 25;
    check(fabsf(u2f(rt_stamina(f2u(100.0f), f2u(80.0f))) - 95.0f) < 1e-5f, "25% drain quarters loss");

    // 200% drain: 100 -> 90 becomes 100 -> 80, and clamps at 0.0f
    g_settings.stamina_drain = 200;
    check(fabsf(u2f(rt_stamina(f2u(100.0f), f2u(90.0f))) - 80.0f) < 1e-5f, "200% drain doubles loss");
    check(fabsf(u2f(rt_stamina(f2u(5.0f), f2u(1.0f))) - 0.0f) < 1e-5f, "200% drain clamps at 0");

    // Recovery (new > old) is never scaled
    check(rt_stamina(f2u(50.0f), f2u(80.0f)) == f2u(80.0f), "recovery passes through unchanged");

    // Infinite stamina overrides drain multiplier
    g_settings.infinite_stamina = true;
    check(rt_stamina(f2u(100.0f), f2u(90.0f)) == f2u(100.0f), "infinite_stamina blocks drain");

    // Damage multipliers sanity
    g_settings.damage_taken = 50;
    check(rt_link_life(20, 12) == 16, "50% damage_taken");
    g_settings.damage_dealt = 200;
    check(rt_life_cost(10, 100, 0) == 20, "200% damage_dealt");

    // No fall damage toggle
    g_settings.no_fall_damage = false;
    check(!assists_no_fall_damage_enabled(), "no_fall_damage off");
    g_settings.no_fall_damage = true;
    check(assists_no_fall_damage_enabled(), "no_fall_damage on");

    // Health regeneration: off, dead, full, yellow hearts, 5s combat delay, Slow/Normal/Fast intervals
    mock_ns = 10000000000ull;
    g_settings.health_regen = HEALTH_REGEN_OFF;
    check(assists_health_regen_step(8, 20, mock_ns) == 8, "regen off");
    g_settings.health_regen = HEALTH_REGEN_FAST;
    check(assists_health_regen_step(0, 20, mock_ns) == 0, "regen dead Link stays 0");
    check(assists_health_regen_step(20, 20, mock_ns) == 20, "regen full HP unchanged");
    check(assists_health_regen_step(24, 20, mock_ns) == 24, "regen bonus yellow hearts untouched");
    g_settings.god_mode = false;
    g_settings.damage_taken = 100;
    rt_link_life(16, 12);  // damage at t = 10s
    mock_ns = 14000000000ull;  // +4s (< 5s combat delay)
    check(assists_health_regen_step(12, 20, mock_ns) == 12, "regen blocked during 5s combat delay");
    mock_ns = 15500000000ull;  // +5.5s: arms timer
    check(assists_health_regen_step(12, 20, mock_ns) == 12, "regen arms after delay");
    mock_ns = 16500000000ull;  // +1.0s at Fast
    check(assists_health_regen_step(12, 20, mock_ns) == 13, "regen fast +1 quarter-heart after 1s");
    g_settings.health_regen = HEALTH_REGEN_NORMAL;
    mock_ns = 18000000000ull;  // +1.5s (< 2s)
    check(assists_health_regen_step(13, 20, mock_ns) == 13, "regen normal waits 2s");
    mock_ns = 18500000000ull;  // +2.0s
    check(assists_health_regen_step(13, 20, mock_ns) == 14, "regen normal +1 after 2s");
    g_settings.health_regen = HEALTH_REGEN_SLOW;
    mock_ns = 22500000000ull;  // +4.0s
    check(assists_health_regen_step(14, 20, mock_ns) == 15, "regen slow +1 after 4s");

    // Game speed multiplier
    g_settings.game_speed = 100;
    check(fabsf(assists_speed_scale(0.5f, true) - 0.5f) < 1e-5f, "speed 100% passthrough");
    g_settings.game_speed = 50;
    check(fabsf(assists_speed_scale(0.5f, false) - 0.5f) < 1e-5f, "speed inactive passthrough");
    check(fabsf(assists_speed_scale(0.5f, true) - 0.25f) < 1e-5f, "speed 50% halves ratio");
    g_settings.game_speed = 200;
    check(fabsf(assists_speed_scale(0.5f, true) - 1.0f) < 1e-5f, "speed 200% doubles ratio");
    check(fabsf(assists_speed_scale(1.0f, true) - 1.5f) < 1e-5f, "speed clamps at 1.5f ceiling");

    // On-screen health HUD snapshot + enemy fade
    int32_t lc = 0, lm = 0, ec = 0, em = 0;
    float ef = 0.0f;
    g_settings.show_health_numbers = false;
    assists_link_hp_update(16, 20);
    check(!assists_health_hud_snapshot(&lc, &lm, &ec, &em, &ef, mock_ns), "health HUD off returns false");
    g_settings.show_health_numbers = true;
    g_settings.one_hit_kill = false;
    g_settings.damage_dealt = 100;
    mock_ns = 30000000000ull;
    rt_life_cost(25, 100, 0);  // enemy 100 -> 75
    check(assists_health_hud_snapshot(&lc, &lm, &ec, &em, &ef, mock_ns), "health HUD active");
    check(lc == 16 && lm == 20 && ec == 75 && em == 100 && fabsf(ef - 1.0f) < 1e-5f, "health HUD values exact");
    check(assists_health_hud_snapshot(&lc, &lm, &ec, &em, &ef, mock_ns + 7000000000ull) && fabsf(ef - 0.5f) < 1e-5f,
          "enemy HUD fades at 7s");
    assists_health_hud_snapshot(&lc, &lm, &ec, &em, &ef, mock_ns + 8500000000ull);
    check(ef == 0.0f && em == 0, "enemy HUD disappears after 8s");

    printf("test_assists: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
