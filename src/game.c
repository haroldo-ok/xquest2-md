/*
 * collision.c - pixel-accurate hitbox collision (preserved from original)
 * level.c     - procedural level generation
 * gem.c       - gem collection
 * bullet.c    - projectile system
 * mine.c      - mine placement and triggering
 * smartbomb.c - screen-clear weapon
 * hud.c       - score/lives/bombs display
 * title.c     - title screen
 * sfx.c       - sound effects
 *
 * All in one file for scaffold clarity; split as needed during full port.
 */

#include <genesis.h>
#include "xquest.h"
#include "tilemap.h"
#include "screens.h"
#include "player2.h"
#include "sfx.h"
#include "enemy_variants.h"
#include "resources.h"

/* ============================================================
 * collision.c
 * ============================================================ */
u8 rects_overlap(fix16 ax, fix16 ay, u8 aw, u8 ah,
                 fix16 bx, fix16 by, u8 bw, u8 bh)
{
    s16 ax1 = fix16ToInt(ax) - aw/2,  ax2 = ax1 + aw;
    s16 ay1 = fix16ToInt(ay) - ah/2,  ay2 = ay1 + ah;
    s16 bx1 = fix16ToInt(bx) - bw/2,  bx2 = bx1 + bw;
    s16 by1 = fix16ToInt(by) - bh/2,  by2 = by1 + bh;
    return (ax1 < bx2 && ax2 > bx1 && ay1 < by2 && ay2 > by1);
}

void collision_check_all(GameData *gd)
{
    Player *p = &gd->player;
    if (!p->active || p->invincible > 0) return;

    /* Player vs Enemies */
    for (u8 i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *e = &gd->enemies[i];
        if (!e->active) continue;
        if (rects_overlap(p->x, p->y, 10, 10, e->x, e->y, 12, 12))
            player_die(p, gd);
    }

    /* Player vs Mines */
    for (u8 i = 0; i < MAX_MINES; i++)
    {
        Mine *m = &gd->mines[i];
        if (!m->active) continue;
        if (rects_overlap(p->x, p->y, 10, 10, m->x, m->y, 10, 10))
        {
            m->active = FALSE;
            SPR_releaseSprite(m->spr);
            m->spr = NULL;
            player_die(p, gd);
        }
    }

    /* Player vs Gems */
    for (u8 i = 0; i < MAX_GEMS; i++)
    {
        Gem *g = &gd->gems[i];
        if (!g->active || g->collected) continue;
        if (rects_overlap(p->x, p->y, 10, 10, g->x, g->y, 10, 10))
        {
            g->collected = TRUE;
            g->active    = FALSE;
            SPR_releaseSprite(g->spr);
            g->spr = NULL;
            p->score += 50;
            gd->gems_remaining--;
            sfx_play(SFX_GEM_COLLECT);
        }
    }

    /* Player vs PowerCharges */
    for (u8 i = 0; i < MAX_POWERCHARGES; i++)
    {
        PowerCharge *pc = &gd->powercharges[i];
        if (!pc->active) continue;
        if (rects_overlap(p->x, p->y, 10, 10, pc->x, pc->y, 12, 12))
        {
            /* Random power-up effect */
            switch (random() % 5)
            {
                case 0: p->smartbombs = MIN(p->smartbombs+1, MAX_SMARTBOMBS); break;
                case 1: p->score += 1000;   break;
                case 2: p->lives  = MIN(p->lives+1, 9); break;
                case 3: p->invincible = 255; break;  /* temporary shield (~4 sec) */
                case 4: p->shoot_cooldown = 0; break; /* rapid fire brief burst */
            }
            pc->active = FALSE;
            SPR_releaseSprite(pc->spr);
            pc->spr = NULL;
            sfx_play(SFX_POWERUP);
        }
    }

    /* Player bullets vs Enemies */
    for (u8 bi = 0; bi < MAX_BULLETS; bi++)
    {
        Bullet *b = &gd->bullets[bi];
        if (!b->active || !b->is_player) continue;
        for (u8 ei = 0; ei < MAX_ENEMIES; ei++)
        {
            Enemy *e = &gd->enemies[ei];
            if (!e->active) continue;
            if (rects_overlap(b->x, b->y, 6, 6, e->x, e->y, 12, 12))
            {
                b->active = FALSE;
                SPR_releaseSprite(b->spr);
                b->spr = NULL;
                e->hp--;
                if (e->hp <= 0)
                    enemy_die(e, gd);
                /* Retaliator fires back immediately when hit */
                else if (e->type == ENEMY_RETALIATOR)
                {
                    fix16 dx = fix16Sub(p->x, e->x);
                    fix16 dy = fix16Sub(p->y, e->y);
                    fix16 dist = fix16Add(fix16Abs(dx), fix16Abs(dy));
                    if (dist > FIX16(1))
                    {
                        fix16 spd = FIX16(3.5);
                        bullet_fire(gd, e->x, e->y,
                                    fix16Div(fix16Mul(dx, spd), dist),
                                    fix16Div(fix16Mul(dy, spd), dist), FALSE);
                    }
                }
                break;
            }
        }
    }

    /* Enemy bullets vs Player */
    for (u8 bi = 0; bi < MAX_BULLETS; bi++)
    {
        Bullet *b = &gd->bullets[bi];
        if (!b->active || b->is_player) continue;
        if (rects_overlap(b->x, b->y, 6, 6, p->x, p->y, 10, 10))
        {
            b->active = FALSE;
            SPR_releaseSprite(b->spr);
            b->spr = NULL;
            player_die(p, gd);
        }
    }
}

/* ============================================================
 * gem.c
 * ============================================================ */
void gems_init(GameData *gd, u8 count)
{
    gd->gems_remaining = 0;
    for (u8 i = 0; i < count && i < MAX_GEMS; i++)
    {
        Gem *g = &gd->gems[i];
        /* Random position on open floor tiles */
        do {
            g->x = FIX16(16 + random() % (SCREEN_W - 32));
            g->y = FIX16(HUD_HEIGHT + 16 + random() % (SCREEN_H - HUD_HEIGHT - 32));
        } while (tilemap_is_solid(gd, fix16ToInt(g->x)/TILE_W, (fix16ToInt(g->y)-HUD_HEIGHT)/TILE_H));

        g->collected = FALSE;
        g->active    = TRUE;
        g->spr = SPR_addSprite(&spr_gem,
                               fix16ToInt(g->x) - 8,
                               fix16ToInt(g->y) - 8,
                               TILE_ATTR(PAL_COLLECT, TRUE, FALSE, FALSE));
        gd->gems_remaining++;
    }
}

void gems_update(GameData *gd)
{
    for (u8 i = 0; i < MAX_GEMS; i++)
    {
        Gem *g = &gd->gems[i];
        if (!g->active) continue;
        SPR_setPosition(g->spr, fix16ToInt(g->x)-8, fix16ToInt(g->y)-8);
    }
}

void gems_draw(GameData *gd) { (void)gd; }

/* ============================================================
 * bullet.c
 * ============================================================ */
void bullet_fire(GameData *gd, fix16 x, fix16 y, fix16 vx, fix16 vy, u8 is_player)
{
    for (u8 i = 0; i < MAX_BULLETS; i++)
    {
        Bullet *b = &gd->bullets[i];
        if (b->active) continue;
        b->x = x;  b->y = y;
        b->vx = vx; b->vy = vy;
        b->is_player = is_player;
        b->active = TRUE;
        const SpriteDefinition *def = is_player ? &spr_bullet_player : &spr_bullet_green;
        u8 pal = is_player ? PAL_ACTIVE : PAL_ENEMY;
        b->spr = SPR_addSprite(def, fix16ToInt(x)-4, fix16ToInt(y)-4,
                               TILE_ATTR(pal, TRUE, FALSE, FALSE));
        return;
    }
}

void bullets_update(GameData *gd)
{
    for (u8 i = 0; i < MAX_BULLETS; i++)
    {
        Bullet *b = &gd->bullets[i];
        if (!b->active) continue;
        b->x = fix16Add(b->x, b->vx);
        b->y = fix16Add(b->y, b->vy);
        /* Destroy if off screen */
        s16 bx = fix16ToInt(b->x), by = fix16ToInt(b->y);
        if (bx < 0 || bx >= SCREEN_W || by < HUD_HEIGHT || by >= SCREEN_H)
        {
            b->active = FALSE;
            SPR_releaseSprite(b->spr);
            b->spr = NULL;
            continue;
        }
        /* Destroy if hits wall tile */
        {
            u8 btx, bty;
            tilemap_cell_at_pixel(bx, by, &btx, &bty);
            if (tilemap_is_solid(gd, btx, bty))
            {
                b->active = FALSE;
                SPR_releaseSprite(b->spr);
                b->spr = NULL;
                continue;
            }
        }
        SPR_setPosition(b->spr, bx-4, by-4);
    }
}

void bullets_draw(GameData *gd) { (void)gd; }

/* ============================================================
 * mine.c
 * ============================================================ */
void mine_place(GameData *gd, fix16 x, fix16 y)
{
    for (u8 i = 0; i < MAX_MINES; i++)
    {
        Mine *m = &gd->mines[i];
        if (m->active) continue;
        m->x = x;  m->y = y;
        m->active    = TRUE;
        m->triggered = FALSE;
        m->spr = SPR_addSprite(&spr_mine,
                               fix16ToInt(x)-8, fix16ToInt(y)-8,
                               TILE_ATTR(PAL_ACTIVE, TRUE, FALSE, FALSE));
        return;
    }
}

void mines_update(GameData *gd)
{
    for (u8 i = 0; i < MAX_MINES; i++)
    {
        Mine *m = &gd->mines[i];
        if (!m->active) continue;
        SPR_setPosition(m->spr, fix16ToInt(m->x)-8, fix16ToInt(m->y)-8);
    }
}

void mines_draw(GameData *gd) { (void)gd; }

/* ============================================================
 * smartbomb.c
 * ============================================================ */
void smartbomb_activate(GameData *gd)
{
    /* Destroy all enemies on screen */
    for (u8 i = 0; i < MAX_ENEMIES; i++)
        if (gd->enemies[i].active)
            enemy_die(&gd->enemies[i], gd);

    /* Destroy all enemy bullets */
    for (u8 i = 0; i < MAX_BULLETS; i++)
    {
        Bullet *b = &gd->bullets[i];
        if (b->active && !b->is_player)
        {
            b->active = FALSE;
            SPR_releaseSprite(b->spr);
            b->spr = NULL;
        }
    }

    /* Flash screen white */
    sfx_play(SFX_SMARTBOMB);
    PAL_setColor(0, 0x0EEE);
    SYS_doVBlankProcess();
    PAL_setColor(0, 0x0000);
}

/* ============================================================
 * level.c
 * ============================================================ */
void level_generate(GameData *gd, u16 level_num)
{
    /* Clear tilemap - all floor */
    memset(gd->tilemap, 0, sizeof(gd->tilemap));

    /* Generate wall border */
    for (u8 x = 0; x < MAP_TILES_W; x++)
    {
        gd->tilemap[0][x] = 1;
        gd->tilemap[MAP_TILES_H-1][x] = 1;
    }
    for (u8 y = 0; y < MAP_TILES_H; y++)
    {
        gd->tilemap[y][0] = 1;
        gd->tilemap[y][MAP_TILES_W-1] = 1;
    }

    /* Random interior walls (more at higher levels) */
    u8 wall_count = 5 + level_num / 3;
    for (u8 w = 0; w < wall_count; w++)
    {
        u8 wx = 1 + random() % (MAP_TILES_W - 2);
        u8 wy = 1 + random() % (MAP_TILES_H - 2);
        u8 wlen = 2 + random() % 4;
        u8 horiz = random() & 1;
        for (u8 i = 0; i < wlen; i++)
        {
            u8 tx = horiz ? MIN(wx+i, MAP_TILES_W-2) : wx;
            u8 ty = horiz ? wy : MIN(wy+i, MAP_TILES_H-2);
            gd->tilemap[ty][tx] = 1;
        }
    }

    /* Apply difficulty scaling */
    const DifficultySettings *diff = screen_get_difficulty();
    s16 gem_adj   = diff ? diff->gem_count_add   : 0;
    s16 enm_adj   = diff ? diff->enemy_count_add : 0;

    /* Gem count scales with level (+ difficulty adjustment) */
    s16 gem_count_raw = (s16)(8 + MIN(level_num * 2, 24)) + gem_adj;
    u8 gem_count = (u8)MAX(2, MIN(gem_count_raw, MAX_GEMS));
    gems_init(gd, gem_count);

    /* Spawn enemies - count and types scale with level (+ difficulty) */
    s16 num_enemies_raw = (s16)(4 + (s16)level_num) + enm_adj;
    u8 num_enemies = (u8)MAX(1, MIN(num_enemies_raw, MAX_ENEMIES));
    for (u8 i = 0; i < num_enemies; i++)
    {
        /* Select enemy type based on level */
        EnemyType type;
        if      (level_num < 3)  type = ENEMY_GRUNGER;
        else if (level_num < 6)  type = (random() & 1) ? ENEMY_GRUNGER : ENEMY_ZIPPO;
        else if (level_num < 10) type = random() % 4;
        else                     type = random() % ENEMY_TYPE_COUNT;

        fix16 ex, ey;
        /* Spawn away from player */
        do {
            ex = FIX16(16 + random() % (SCREEN_W-32));
            ey = FIX16(HUD_HEIGHT+16 + random() % (SCREEN_H-HUD_HEIGHT-32));
        } while (
            tilemap_is_solid(gd,
                fix16ToInt(ex) / TILE_W,
                (fix16ToInt(ey) - HUD_HEIGHT) / TILE_H) ||
            (fix16Abs(fix16Sub(ex, gd->player.x)) < FIX16(80) &&
             fix16Abs(fix16Sub(ey, gd->player.y)) < FIX16(80)));

        enemy_spawn(&gd->enemies[i], type, ex, ey);
    }

    /* Initialise variant enemy palette (pal_enemy2 → PAL2) */
    ev_init();

    /* Gate starts closed at top-centre */
    gd->gate_open   = FALSE;
    gd->level_timer = 0;

    /* Redraw the new tilemap to VDP plane B.
     * On first call this is a no-op since tilemap_init hasn't been called yet;
     * game_init() calls tilemap_init() + tilemap_draw() explicitly.
     * On subsequent level-ups, we call directly here. */
    /* tilemap_draw(gd); */  /* Enabled by level_next() instead */
}

void level_check_complete(GameData *gd)
{
    /* Level timer increments every frame */
    gd->level_timer++;

    /* Step 1: Open gate when all gems collected */
    if (gd->gems_remaining == 0 && !gd->gate_open)
    {
        gd->gate_open = TRUE;
        sfx_play(SFX_GATE_OPEN);
    }

    /* Step 2: If gate is open, check if player has reached it.
     * Gate is at top-centre of the screen (tile 10, row 0). */
    if (gd->gate_open && gd->player.active)
    {
        fix16 gate_x = FIX16(SCREEN_W / 2);
        fix16 gate_y = FIX16(HUD_HEIGHT + 8);   /* just below HUD */

        fix16 dx = fix16Abs(fix16Sub(gd->player.x, gate_x));
        fix16 dy = fix16Abs(fix16Sub(gd->player.y, gate_y));

        if (dx < FIX16(24) && dy < FIX16(24))
        {
            /* Time bonus: up to 5000 pts for fast completion */
            u32 time_bonus = 0;
            if (gd->level_timer < 1800u)   /* < 30 seconds at 60fps */
                time_bonus = 5000u - (gd->level_timer * 5u / 3u);
            gd->player.score += time_bonus;

            sfx_play(SFX_LEVEL_CLEAR);

            /* Advance to next level */
            level_next(gd);
        }
    }
}

/* ============================================================
 * sfx.c
 * ============================================================ */

/* SFX pointer table - maps sfx IDs to rescomp-generated arrays */
static const u8 *SFX_TABLE[] = {
    sfx_0, sfx_1, sfx_2, sfx_3, sfx_4,
    sfx_5, sfx_6, sfx_7, sfx_8
};

#define SFX_COUNT 9

/* sfx_N_size are extern variables, not compile-time constants, so we
 * cannot use them in a static initializer. Look them up inline instead. */
static u32 sfx_get_size(u8 id)
{
    switch (id)
    {
        case 0: return sfx_0_size;
        case 1: return sfx_1_size;
        case 2: return sfx_2_size;
        case 3: return sfx_3_size;
        case 4: return sfx_4_size;
        case 5: return sfx_5_size;
        case 6: return sfx_6_size;
        case 7: return sfx_7_size;
        case 8: return sfx_8_size;
        default: return 0;
    }
}

void sfx_init(void)
{
    /* Z80 PCM driver initialised by SGDK automatically */
}

void sfx_play(u8 sfx_id)
{
    /* SND_startPlay_PCM(sample, len, rate, pan, loop)
     * rate: output rate in Hz (8000 Hz matches our WAV declaration)
     * pan:  SOUND_PAN_CENTER (both speakers)
     * loop: FALSE (one-shot) */
    if (sfx_id >= SFX_COUNT) return;
    SND_startPlay_PCM(SFX_TABLE[sfx_id], sfx_get_size(sfx_id),
                      8000, SOUND_PAN_CENTER, FALSE);
}

void sfx_play_sustained(u8 sfx_id)
{
    if (sfx_id >= SFX_COUNT) return;
    SND_startPlay_PCM(SFX_TABLE[sfx_id], sfx_get_size(sfx_id),
                      8000, SOUND_PAN_CENTER, FALSE);
}

void sfx_stop(void)
{
    SND_stopPlay_PCM();
}

/* ============================================================
 * hud.c
 * ============================================================ */
void hud_draw(GameData *gd)
{
    char buf[40];

    if (g_two_player && player2_is_active())
    {
        /* Two-player HUD: P1 left, P2 right */
        Player *p2 = player2_get();

        /* P1 */
        sprintf(buf, "1:%08lu L%d B%d",
                gd->player.score,
                gd->player.lives,
                gd->player.smartbombs);
        VDP_clearText(0, 0, 20);
        VDP_drawText(buf, 0, 0);

        /* Level (centred) */
        sprintf(buf, "L%02d", gd->level);
        VDP_clearText(18, 0, 4);
        VDP_drawText(buf, 18, 0);

        /* P2 */
        sprintf(buf, "2:%08lu L%d B%d",
                p2->score,
                p2->lives,
                p2->smartbombs);
        VDP_clearText(22, 0, 18);
        VDP_drawText(buf, 22, 0);
    }
    else
    {
        /* Single-player HUD */
        sprintf(buf, "SCORE:%08lu  L%02d  SHIPS:%d  BOMBS:%d",
                gd->player.score,
                gd->level,
                gd->player.lives,
                gd->player.smartbombs);
        VDP_clearText(0, 0, 40);
        VDP_drawText(buf, 0, 0);
    }
}

void hud_update_score(u32 score)
{
    /* Called when score changes for efficiency */
    (void)score;
}

/* ============================================================
 * title.c
 * ============================================================ */
void title_show(void)
{
    /* Clear display */
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    /* Load title image to plane B */
    /* Genesis display is 320x224; title is 320x240 - crop 8px top/bottom */
    VDP_drawImageEx(BG_B, &img_title, TILE_ATTR_FULL(PAL_BG, FALSE, FALSE, FALSE, 0),
                    0, 0, FALSE, CPU);

    /* Draw title text */
    VDP_drawText("XQUEST  2", 11, 12);
    VDP_drawText("PRESS START", 10, 22);
    VDP_drawText("by Mark Mackey", 9, 24);
    VDP_drawText("SEGA GENESIS PORT", 7, 26);

    /* Wait for START */
    while (TRUE)
    {
        SYS_doVBlankProcess();
        u16 joy = JOY_readJoypad(JOY_1);
        if (joy & BUTTON_START) break;
    }

    VDP_clearPlane(BG_B, TRUE);
    VDP_clearPlane(BG_A, TRUE);
}

/* ============================================================
 * level_next()
 * Advance to the next level: generate map, redraw tilemap,
 * re-spawn enemies. Called from level_check_complete() when
 * the player steps through the open gate.
 * ============================================================ */
void level_next(GameData *gd)
{
    gd->level++;

    /* Clear all active entities */
    SPR_reset();

    /* Re-init player at centre (keeps score/lives) */
    player_init(&gd->player,
                FIX16(SCREEN_W / 2),
                FIX16(SCREEN_H / 2));

    /* Generate new level layout */
    level_generate(gd, gd->level);

    /* Redraw background */
    tilemap_draw(gd);

    /* Refresh HUD */
    /* hud_draw(gd); */   /* called from game_run each frame */
}
