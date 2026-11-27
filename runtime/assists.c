// Options > Accessibility (settings.h): the assists, each off by default. Their patches in the game code
// (tools/patches.py) ask here while it runs; a game version whose code has no place for one (tools/game_sites.py
// OPTIONAL_GROUPS) goes without it.
#include <stdint.h>
#include <string.h>

#include "hle.h"
#include "platform.h"
#include "settings.h"
#include "rt.h"

static uint64_t link_damage_ns;
static int32_t link_hp_cur, link_hp_max;
static int32_t enemy_hp_cur, enemy_hp_max;
static uint64_t enemy_hit_ns, regen_last_ns;
RT_ON_RESET(assists_reset) {
    __atomic_store_n(&link_damage_ns, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&link_hp_cur, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&link_hp_max, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&enemy_hp_cur, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&enemy_hp_max, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&enemy_hit_ns, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&regen_last_ns, 0, __ATOMIC_RELAXED);
}
uint64_t rt_link_damage_ns(void) { return __atomic_load_n(&link_damage_ns, __ATOMIC_RELAXED); }

// The damage multipliers: d percent of a loss, rounded, and at least 1 while there is a loss
static int32_t scaled(int32_t d, int pct) {
    if (d <= 0 || pct == 100) return d;
    int32_t s = (int32_t)(((int64_t)d * pct + 50) / 100);
    return s < 1 ? 1 : s;
}

// The cost a hit takes off a life (tools/patches.py _no_wear_hit: every life goes through that function), by whose life
// it is: a weapon's, bow's or shield's durability (LIFE_WEAPON: Unbreakable weapons, from #296, else Weapon wear's share
// of the game's cost, rounded down: a light hit at a low setting can cost nothing), Link's hearts
// (LIFE_LINK: left to rt_link_life, which sees every loss of his), or anything else's (One-hit kill, the damage dealt).
enum { LIFE_OTHER, LIFE_WEAPON, LIFE_LINK };
uint32_t rt_life_cost(uint32_t cost, int32_t life, int whose) {
    if (whose == LIFE_WEAPON) {
        if (g_settings.no_weapon_wear || g_settings.weapon_wear <= 0) return 0;
        int pct = g_settings.weapon_wear;
        return pct == 100 || (int32_t)cost <= 0 ? cost : (uint32_t)((uint64_t)cost * (uint32_t)pct / 100);
    }
    if (whose == LIFE_LINK || (int32_t)cost <= 0) return cost;
    uint32_t final_cost = g_settings.one_hit_kill ? (life > (int32_t)cost ? (uint32_t)life : cost)
                                                  : (uint32_t)scaled((int32_t)cost, g_settings.damage_dealt);
    if (whose == LIFE_OTHER && life > 0) {
        int32_t after = life > (int32_t)final_cost ? life - (int32_t)final_cost : 0;
        uint64_t now = plat_now_ns();
        uint64_t prev_ns = __atomic_load_n(&enemy_hit_ns, __ATOMIC_RELAXED);
        int32_t prev_max = __atomic_load_n(&enemy_hp_max, __ATOMIC_RELAXED);
        int32_t prev_cur = __atomic_load_n(&enemy_hp_cur, __ATOMIC_RELAXED);
        if (!prev_ns || now < prev_ns || now - prev_ns > 8000000000ull || life > prev_max || prev_cur <= 0)
            __atomic_store_n(&enemy_hp_max, life, __ATOMIC_RELAXED);
        __atomic_store_n(&enemy_hp_cur, after, __ATOMIC_RELAXED);
        __atomic_store_n(&enemy_hit_ns, now, __ATOMIC_RELAXED);
    }
    return final_cost;
}

// Unbreakable weapons (from #296), the pouch's copy: its setter for an equipped item's value doesn't store a lower one
// (tools/patches.py _no_wear). Arrows (type 2) are counts, not wear, and are left alone.
int rt_keep_wear(uint32_t type, int32_t newv, int32_t oldv) {
    return (g_settings.no_weapon_wear || g_settings.weapon_wear <= 0) && type != 2 && newv < oldv;
}

// God mode, the damage taken: Link's damage handling stores his new life, in quarter hearts, through here (tools/patches.py
// _link_life): God mode keeps the old one, the multiplier scales the loss.
int32_t rt_link_life(int32_t oldv, int32_t newv) {
    if (newv >= oldv) {
        __atomic_store_n(&link_hp_cur, newv, __ATOMIC_RELAXED);
        return newv;
    }
    __atomic_store_n(&link_damage_ns, plat_now_ns(), __ATOMIC_RELAXED);
    __atomic_store_n(&regen_last_ns, 0, __ATOMIC_RELAXED);
    int32_t out = g_settings.god_mode ? oldv : oldv - scaled(oldv - newv, g_settings.damage_taken);
    if (out < 0) out = 0;
    __atomic_store_n(&link_hp_cur, out, __ATOMIC_RELAXED);
    return out;
}

// Infinite stamina and stamina drain multiplier: the stamina setter keeps the old value or scales the drain (#968).
uint32_t rt_stamina(uint32_t oldv, uint32_t newv) {
    float o, n;
    memcpy(&o, &oldv, 4), memcpy(&n, &newv, 4);
    if (n >= o) return newv;
    if (g_settings.infinite_stamina) return oldv;
    int pct = g_settings.stamina_drain;
    if (pct <= 0) return oldv;
    if (pct == 100) return newv;
    float s = o - (o - n) * ((float)pct / 100.0f);
    if (s < 0.0f) s = 0.0f;
    uint32_t out;
    memcpy(&out, &s, 4);
    return out;
}

// Infinite arrows: the bow's shot takes its arrows off the pouch through one function, which returns before taking
// any (tools/patches.py _remove_arrows). One arrow is still needed to shoot.
int rt_infinite_arrows(void) { return g_settings.infinite_arrows; }

// Silent Link: the game's noise setter, called with (object, channel, value) for one of an actor's four noise channels
// (tools/patches.py _silent_link). Channel 1 is the one whistling and the sounds like it go through; the HUD's sound
// wave and the enemies' hearing both read it, so keeping it at zero is what makes Link quiet. The other channels and
// the other actors are left alone.
int rt_silent_link(uint64_t object, uint32_t channel) {
    if (!g_settings.silent_link || channel != 1) return 0;
    if (object < GUEST_LO || object >= GUEST_HI) return 0;
    *(volatile float *)(uintptr_t)(object + 0x1c) = 0.0f;  // (+0x18: the four channels, one float each)
    return 1;
}

// Champion abilities (tools/patches.py _hero_soul_*): the game's own "can this be used" checks return 0 in shrines, in
// the Trial of the Sword and while the ability recharges. With the option on the check passes, so the ability works
// from the start of the game and in those places; off, the check runs as the game wrote it.
int rt_hero_soul(int which) {
    switch (which) {
    case 0: return g_settings.hero_soul_wind;
    case 1: return g_settings.hero_soul_electric;
    case 2: return g_settings.hero_soul_fire;
    case 3: return g_settings.hero_soul_water;
    }
    return 0;
}

bool assists_no_fall_damage_enabled(void) { return g_settings.no_fall_damage; }

void assists_link_hp_update(int32_t cur_qh, int32_t max_qh) {
    __atomic_store_n(&link_hp_cur, cur_qh, __ATOMIC_RELAXED);
    __atomic_store_n(&link_hp_max, max_qh, __ATOMIC_RELAXED);
}

int32_t assists_health_regen_step(int32_t cur_qh, int32_t max_qh, uint64_t now_ns) {
    int mode = g_settings.health_regen;
    if (mode <= HEALTH_REGEN_OFF || mode >= HEALTH_REGEN_COUNT || cur_qh <= 0 || max_qh < 12 || cur_qh >= max_qh) {
        __atomic_store_n(&regen_last_ns, 0, __ATOMIC_RELAXED);
        return cur_qh;
    }
    uint64_t dmg = rt_link_damage_ns();
    if (dmg && now_ns >= dmg && now_ns - dmg < 5000000000ull) {
        __atomic_store_n(&regen_last_ns, 0, __ATOMIC_RELAXED);
        return cur_qh;
    }
    uint64_t period = mode == HEALTH_REGEN_FAST ? 1000000000ull : mode == HEALTH_REGEN_NORMAL ? 2000000000ull : 4000000000ull;
    uint64_t last = __atomic_load_n(&regen_last_ns, __ATOMIC_RELAXED);
    if (!last || now_ns < last) {
        __atomic_store_n(&regen_last_ns, now_ns, __ATOMIC_RELAXED);
        return cur_qh;
    }
    if (now_ns - last >= period) {
        __atomic_store_n(&regen_last_ns, now_ns, __ATOMIC_RELAXED);
        return cur_qh + 1 <= max_qh ? cur_qh + 1 : max_qh;
    }
    return cur_qh;
}

float assists_speed_scale(float ratio, bool active) {
    int pct = g_settings.game_speed;
    if (!active || pct == 0 || pct == 100) return ratio;
    if (pct < 50) pct = 50;
    if (pct > 200) pct = 200;
    float r = ratio * ((float)pct / 100.0f);
    if (r < 0.05f) r = 0.05f;
    if (r > 1.5f) r = 1.5f;
    return r;
}

bool assists_health_hud_snapshot(int32_t *link_cur, int32_t *link_max, int32_t *enemy_cur, int32_t *enemy_max,
                                 float *enemy_fade, uint64_t now_ns) {
    if (!g_settings.show_health_numbers) return false;
    int32_t lc = __atomic_load_n(&link_hp_cur, __ATOMIC_RELAXED);
    int32_t lm = __atomic_load_n(&link_hp_max, __ATOMIC_RELAXED);
    int32_t ec = 0, em = 0;
    float fade = 0.0f;
    uint64_t e_ns = __atomic_load_n(&enemy_hit_ns, __ATOMIC_RELAXED);
    int32_t e_max = __atomic_load_n(&enemy_hp_max, __ATOMIC_RELAXED);
    if (e_ns && e_max > 0 && now_ns >= e_ns && now_ns - e_ns < 8000000000ull) {
        float age_s = (float)((double)(now_ns - e_ns) / 1e9);
        fade = age_s <= 6.0f ? 1.0f : (8.0f - age_s) / 2.0f;
        if (fade < 0.0f) fade = 0.0f;
        if (fade > 1.0f) fade = 1.0f;
        ec = __atomic_load_n(&enemy_hp_cur, __ATOMIC_RELAXED);
        em = e_max;
    }
    if (link_cur) *link_cur = lc;
    if (link_max) *link_max = lm;
    if (enemy_cur) *enemy_cur = ec;
    if (enemy_max) *enemy_max = em;
    if (enemy_fade) *enemy_fade = fade;
    return lm > 0 || fade > 0.0f;
}
// The map's completion rate (tools/patches.py _map_clear_rate): the map screen asks the game whether the save has the
// GameClear flag and hides its rate block when the answer is no. With the option on the answer is 1, so the rate shows
// before the game is cleared.
int rt_map_clear_rate(void) { return g_settings.map_clear_rate; }
