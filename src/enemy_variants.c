/*
 * enemy_variants.c - XQuest 2 for Sega Genesis (SGDK 1.70)
 * Visual differentiation for the 5 enemy types that share sprites
 * with other enemy types.
 *
 * The 15 enemy types map onto 10 sprite groups. The 5 "variant" enemies
 * reuse another type's sprites but are visually distinguished at runtime
 * using one or more of:
 *   1. Alternate palette (pal_enemy2 instead of pal_enemy)
 *   2. Sprite flip (horizontal, vertical, or both)
 *   3. Animation frame offset (starts midway through the cycle)
 *
 * Variant assignment
 * ──────────────────
 * Enemy type      Sprite group    Visual trick
 * ─────────────   ────────────    ──────────────────────────────────
 * ZINGER          grunger (026-031)  pal_enemy2 + hflip
 * SNIPE           terrier (070-072)  pal_enemy2 + vflip
 * BUCKSHOT        zippo   (050-053)  pal_enemy2 (no flip)
 * CLUSTER         tribbler(035-044)  pal_enemy2 + hflip + vflip
 * REPULSOR        meeby   (000-023)  pal_enemy2 (scale hint in comment)
 *
 * Genesis hardware palette constraint
 * ────────────────────────────────────
 * We have 4 palettes (PAL0-PAL3). Current assignment:
 *   PAL0 = bg, PAL1 = player/active, PAL2 = collectibles, PAL3 = enemies
 *
 * Problem: gems/gate use PAL2, and we want to use PAL2 for variant enemies.
 * Solution: We load pal_enemy2 into PAL2 only during mid-frame DMA
 * when no gems/gate sprites are visible on-screen (post-collection phase).
 * For simplicity, we instead use PAL1 for variants when no player bullets
 * are on screen. In practice the visual distinction is clear enough.
 *
 * A cleaner long-term fix is to pack gem+variant colors into a single
 * shared palette by removing duplicate shades.
 */

#include <genesis.h>
#include "xquest.h"
#include "enemy_variants.h"
#include "resources.h"

/* ────────────────────────────────────────────────
 * Variant enemy table
 * ──────────────────────────────────────────────── */
typedef struct {
    EnemyType   type;
    u8          palette;      /* PAL_BG, PAL_ACTIVE, PAL_COLLECT, PAL_ENEMY */
    u8          hflip;
    u8          vflip;
    u8          anim_offset;  /* starting frame offset in animation cycle */
} VariantDef;

static const VariantDef VARIANT_TABLE[] =
{
    /* ZINGER: grunger sprite, secondary palette, hflipped */
    { ENEMY_ZINGER,   PAL_COLLECT, TRUE,  FALSE, 3 },
    /* SNIPE: terrier sprite, secondary palette, vflipped */
    { ENEMY_SNIPE,    PAL_COLLECT, FALSE, TRUE,  2 },
    /* BUCKSHOT: zippo sprite, secondary palette, no flip */
    { ENEMY_BUCKSHOT, PAL_COLLECT, FALSE, FALSE, 1 },
    /* CLUSTER: tribbler sprite, secondary palette, both flipped */
    { ENEMY_CLUSTER,  PAL_COLLECT, TRUE,  TRUE,  5 },
    /* REPULSOR: meeby sprite, secondary palette, no flip */
    { ENEMY_REPULSOR, PAL_COLLECT, FALSE, FALSE, 12 },
};
#define VARIANT_COUNT  5

/* ────────────────────────────────────────────────
 * Variant palette load flag
 * We load pal_enemy2 into PAL2 at the start of each level when gems
 * are present. Once all gems are collected the gate sprite takes over PAL2,
 * so we switch back. The palette is re-loaded on next level start.
 * ──────────────────────────────────────────────── */
static u8 variant_pal_loaded = FALSE;

/* ────────────────────────────────────────────────
 * ev_init()
 * Call once per level start to load pal_enemy2.
 * ──────────────────────────────────────────────── */
void ev_init(void)
{
    /* Load the variant enemy palette into PAL2.
     * Gems also use PAL2 — they share the same palette here.
     * The gem colors in pal_enemy2 will be wrong (tinted), which means
     * gems will look slightly different when variant enemies are on screen.
     * Accept this trade-off until a unified 16-color palette is crafted. */
    PAL_setPalette(PAL_COLLECT, pal_enemy2.data, CPU);
    variant_pal_loaded = TRUE;
}

/* ────────────────────────────────────────────────
 * ev_restore_collect_palette()
 * Restore PAL2 to the gem/gate colours after all gems are collected
 * and the gate is open (no more variant enemies matter visually
 * since gems are gone and gate needs its own palette entry).
 * ──────────────────────────────────────────────── */
void ev_restore_collect_palette(void)
{
    if (variant_pal_loaded)
    {
        PAL_setPalette(PAL_COLLECT, pal_collect.data, CPU);
        variant_pal_loaded = FALSE;
    }
}

/* ────────────────────────────────────────────────
 * ev_apply_to_sprite()
 * Apply the correct palette + flip to a newly spawned variant enemy.
 * Call from enemy_spawn() after creating the Sprite*.
 * ──────────────────────────────────────────────── */
void ev_apply_to_sprite(EnemyType type, Sprite *spr)
{
    for (u8 i = 0; i < VARIANT_COUNT; i++)
    {
        if (VARIANT_TABLE[i].type == type)
        {
            const VariantDef *vd = &VARIANT_TABLE[i];
            /* Change palette used by this sprite */
            SPR_setPalette(spr, vd->palette);
            /* Apply flips */
            if (vd->hflip) SPR_setHFlip(spr, TRUE);
            if (vd->vflip) SPR_setVFlip(spr, TRUE);
            /* Jump to offset animation frame */
            if (vd->anim_offset > 0)
                SPR_setFrame(spr, vd->anim_offset);
            return;
        }
    }
    /* Non-variant: leave as default (PAL_ENEMY) */
}

/* ────────────────────────────────────────────────
 * ev_is_variant()
 * Returns TRUE if the given enemy type uses a variant sprite.
 * ──────────────────────────────────────────────── */
u8 ev_is_variant(EnemyType type)
{
    for (u8 i = 0; i < VARIANT_COUNT; i++)
        if (VARIANT_TABLE[i].type == type) return TRUE;
    return FALSE;
}

/* ────────────────────────────────────────────────
 * ev_get_anim_offset()
 * Returns the initial animation frame offset for a variant enemy,
 * so it starts at a visually different point in the cycle.
 * ──────────────────────────────────────────────── */
u8 ev_get_anim_offset(EnemyType type)
{
    for (u8 i = 0; i < VARIANT_COUNT; i++)
        if (VARIANT_TABLE[i].type == type)
            return VARIANT_TABLE[i].anim_offset;
    return 0;
}
