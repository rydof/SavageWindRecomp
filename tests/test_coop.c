#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "online.h"

uint64_t plat_now_ns(void) { return 1; }
struct { uint64_t dummy[64]; } g_site;

int main(void) {
    // 1. Default-off state
    assert(online_state() == ONLINE_OFF);
    assert(ONLINE_MAX == 8);
    assert(ONLINE_PORT == 52520);

    // 2. Character creator defaults & sanitization
    OnlineAppearance a;
    online_appearance_default(&a);
    assert(a.outfit_preset == ONLINE_OUTFIT_LIVE);
    assert(a.scale == 1.0f);
    assert(a.name_color[0] == 120 && a.name_color[1] == 225 && a.name_color[2] == 255);

    a.outfit_preset = 99;
    a.dye_color = 42;
    a.scale = 5.0f;
    online_appearance_sanitize(&a);
    assert(a.outfit_preset == ONLINE_OUTFIT_LIVE);
    assert(a.dye_color == 0);
    assert(a.scale == 1.15f);

    a.scale = NAN;
    online_appearance_sanitize(&a);
    assert(a.scale == 1.0f);

    // 3. Preset actors & round-trip
    for (int p = 1; p < ONLINE_OUTFIT_CUSTOM; p++) {
        const char *h = NULL, *u = NULL, *l = NULL;
        online_appearance_preset_actors(p, &h, &u, &l);
        assert(h && h[0] && u && u[0] && l && l[0]);
    }
    a.outfit_preset = ONLINE_OUTFIT_DARK_LINK;
    a.scale = 1.10f;
    online_set_appearance(&a);
    OnlineAppearance got;
    online_get_appearance(&got);
    assert(got.outfit_preset == ONLINE_OUTFIT_DARK_LINK);
    assert(fabsf(got.scale - 1.10f) < 1e-5f);

    // 4. online.ini persistence round-trip in /tmp
    a.name_color[0] = 10; a.name_color[1] = 200; a.name_color[2] = 90;
    a.outfit_preset = ONLINE_OUTFIT_CUSTOM;
    a.dye_color = 5;
    a.scale = 0.92f;
    snprintf(a.head_actor, sizeof a.head_actor, "Armor_001_Head");
    online_set_appearance(&a);
    online_settings_save();
    OnlineAppearance reset;
    online_appearance_default(&reset);
    online_set_appearance(&reset);
    online_settings_load();
    online_get_appearance(&got);
    assert(got.name_color[0] == 10 && got.name_color[1] == 200 && got.name_color[2] == 90);
    assert(got.outfit_preset == ONLINE_OUTFIT_CUSTOM);
    assert(got.dye_color == 5);
    assert(fabsf(got.scale - 0.92f) < 1e-3f);
    assert(strcmp(got.head_actor, "Armor_001_Head") == 0);
    remove("online.ini");

    printf("PASS: coop-addon default-off state, character creator sanitize, presets, and round-trip\n");
    return 0;
}
