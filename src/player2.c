/*
 * player2.c - XQuest 2 for Sega Genesis (SGDK 1.70)
 * Two-player support
 *
 * XQuest 2 originally supported two simultaneous players. On Genesis we
 * use JOY_1 for player 1 and JOY_2 for player 2, with independent ship
 * sprites, scores, and lives. Both players share the same screen.
 *
 * Two-player mode differences from single:
 *  - Each player has independent lives, score, smartbombs
 *  - Bullets from either player kill enemies (not each other)
 *  - Game ends when BOTH players have 0 lives
 *  - Final score = sum of both scores for HOF entry
 *  - Player 2 ship uses PAL1 with an alternate sprite frame (hflipped)
 */

#include <genesis.h>
#include "xquest.h"
#include "tilemap.h"
#include "player2.h"
#include "sfx.h"

/* Global two-player flag — set by the options screen */
extern u8 g_two_player;  /* defined in main.c */

/* Player 2 data lives alongside Player 1 in GameData.
 * We extend GameData.player2 instead of the scaffold's
 * single-player design. Since adding a field to GameData
 * now would break existing code, we keep P2 state here. */
static Player p2_state;
static u8     p2_active = FALSE;

/* Direction → velocity tables (identical to player.c / shared via xquest.h) */
static const fix16 P2_DVX[8] = {
     FIX16(1),  FIX16(1),  FIX16(0), FIX16(-1),
    FIX16(-1), FIX16(-1),  FIX16(0),  FIX16(1)
};
static const fix16 P2_DVY[8] = {
     FIX16(0), FIX16(-1), FIX16(-1), FIX16(-1),
     FIX16(0),  FIX16(1),  FIX16(1),  FIX16(1)
};

/* ────────────────────────────────────────────────
 * Wall collision helper (mirrors player.c logic)
 * ──────────────────────────────────────────────── */
#define P2_HALF_HIT 5

static u8 p2_pixel_solid(const GameData *gd, s16 px, s16 py)
{
    if (px < 0 || px >= WORLD_W  || py < HUD_HEIGHT || py >= WORLD_H)
        return TRUE;
    u8 tx, ty;
    tilemap_cell_at_pixel(px, py, &tx, &ty);
    return tilemap_is_solid(gd, tx, ty);
}

static void p2_wall_collide(Player *p, GameData *gd)
{
    s16 px = fix16ToInt(p->x);
    s16 py = fix16ToInt(p->y);

    u8 hit_l = p2_pixel_solid(gd, px - P2_HALF_HIT, py) ||
               p2_pixel_solid(gd, px - P2_HALF_HIT, py - P2_HALF_HIT) ||
               p2_pixel_solid(gd, px - P2_HALF_HIT, py + P2_HALF_HIT);
    u8 hit_r = p2_pixel_solid(gd, px + P2_HALF_HIT, py) ||
               p2_pixel_solid(gd, px + P2_HALF_HIT, py - P2_HALF_HIT) ||
               p2_pixel_solid(gd, px + P2_HALF_HIT, py + P2_HALF_HIT);
    u8 hit_t = p2_pixel_solid(gd, px, py - P2_HALF_HIT) ||
               p2_pixel_solid(gd, px - P2_HALF_HIT, py - P2_HALF_HIT) ||
               p2_pixel_solid(gd, px + P2_HALF_HIT, py - P2_HALF_HIT);
    u8 hit_b = p2_pixel_solid(gd, px, py + P2_HALF_HIT) ||
               p2_pixel_solid(gd, px - P2_HALF_HIT, py + P2_HALF_HIT) ||
               p2_pixel_solid(gd, px + P2_HALF_HIT, py + P2_HALF_HIT);

    if (!hit_l && !hit_r && !hit_t && !hit_b) return;

    if (gd->difficulty == 0)   /* Easy: bounce */
    {
        if (hit_l || hit_r)
        {
            p->vx = hit_l ? fix16Abs(p->vx) : -fix16Abs(p->vx);
            p->x  = hit_l ? fix16Add(p->x, FIX16(2)) : fix16Sub(p->x, FIX16(2));
        }
        if (hit_t || hit_b)
        {
            p->vy = hit_t ? fix16Abs(p->vy) : -fix16Abs(p->vy);
            p->y  = hit_t ? fix16Add(p->y, FIX16(2)) : fix16Sub(p->y, FIX16(2));
        }
    }
    else
    {
        player2_die(gd);
    }
}

/* ────────────────────────────────────────────────
 * Public API
 * ──────────────────────────────────────────────── */

void player2_init(fix16 x, fix16 y)
{
    if (!g_two_player) return;

    p2_state.x             = x;
    p2_state.y             = y;
    p2_state.vx            = FIX16(0);
    p2_state.vy            = FIX16(0);
    p2_state.dir           = DIR_LEFT;
    p2_state.lives         = INITIAL_LIVES;
    p2_state.smartbombs    = 3;
    p2_state.score         = 0;
    p2_state.shoot_cooldown = 0;
    p2_state.invincible    = 0;
    p2_state.active        = TRUE;

    /* Player 2 uses the thrust sprite hflipped to distinguish visually */
    p2_state.spr = SPR_addSprite(
        &spr_ship_thrust,
        fix16ToInt(x) - 8,
        fix16ToInt(y) - 8,
        TILE_ATTR(PAL_ACTIVE, TRUE, FALSE, FALSE));
    if (p2_state.spr)
        SPR_setHFlip(p2_state.spr, TRUE);

    p2_active = TRUE;
}

void player2_update(GameData *gd)
{
    if (!g_two_player || !p2_active) return;
    Player *p = &p2_state;
    if (!p->active) return;

    u16 joy = JOY_readJoypad(JOY_2);

    /* Direction from D-pad */
    u8 right = (joy & BUTTON_RIGHT) != 0;
    u8 left  = (joy & BUTTON_LEFT)  != 0;
    u8 up    = (joy & BUTTON_UP)    != 0;
    u8 down  = (joy & BUTTON_DOWN)  != 0;

    Direction dir = DIR_NONE;
    if      (right && !up && !down)      dir = DIR_RIGHT;
    else if (right && up)                dir = DIR_UP_RIGHT;
    else if (up    && !right && !left)   dir = DIR_UP;
    else if (left  && up)                dir = DIR_UP_LEFT;
    else if (left  && !up && !down)      dir = DIR_LEFT;
    else if (left  && down)              dir = DIR_DOWN_LEFT;
    else if (down  && !right && !left)   dir = DIR_DOWN;
    else if (right && down)              dir = DIR_DOWN_RIGHT;

    /* Movement */
    if (dir != DIR_NONE)
    {
        p->dir = dir;
        p->vx  = fix16Add(p->vx, fix16Mul(P2_DVX[dir], SHIP_ACCEL));
        p->vy  = fix16Add(p->vy, fix16Mul(P2_DVY[dir], SHIP_ACCEL));
    }

    /* Speed clamp — Manhattan-length approximation, no fix16Sqrt needed */
    {
        fix16 ax = fix16Abs(p->vx), ay = fix16Abs(p->vy);
        fix16 hi = (ax > ay) ? ax : ay;
        fix16 lo = (ax > ay) ? ay : ax;
        fix16 approx_len = fix16Add(hi, fix16Mul(lo, FIX16(0.5)));
        if (approx_len > SHIP_MAX_SPEED && approx_len > FIX16(0.01))
        {
            fix16 scale = fix16Div(SHIP_MAX_SPEED, approx_len);
            p->vx = fix16Mul(p->vx, scale);
            p->vy = fix16Mul(p->vy, scale);
        }
    }

    /* Friction */
    p->vx = fix16Mul(p->vx, SHIP_FRICTION);
    p->vy = fix16Mul(p->vy, SHIP_FRICTION);

    /* Integrate */
    p->x = fix16Add(p->x, p->vx);
    p->y = fix16Add(p->y, p->vy);

    /* Screen wrap */
    /* World wrap */
    if (fix16ToInt(p->x) < 0)         p->x = FIX16(WORLD_W - 1);
    if (fix16ToInt(p->x) >= WORLD_W)  p->x = FIX16(0);
    if (fix16ToInt(p->y) < HUD_HEIGHT) p->y = FIX16(WORLD_H - 1);
    if (fix16ToInt(p->y) >= WORLD_H)  p->y = FIX16(HUD_HEIGHT);

    /* Wall collision */
    if (p->active && p->invincible == 0)
        p2_wall_collide(p, gd);

    /* Fire: buttons A or C */
    if (p->shoot_cooldown > 0) p->shoot_cooldown--;
    if ((joy & (BUTTON_A | BUTTON_C)) && p->shoot_cooldown == 0 && p->dir != DIR_NONE)
    {
        fix16 bvx = fix16Mul(P2_DVX[p->dir], BULLET_SPEED);
        fix16 bvy = fix16Mul(P2_DVY[p->dir], BULLET_SPEED);
        bullet_fire(gd, p->x, p->y, bvx, bvy, TRUE);
        p->shoot_cooldown = SHIP_FIRE_COOLDOWN;
        sfx_play(SFX_SHOOT);
    }

    /* SmartBomb: button B */
    {
        static u8 bomb_held = FALSE;
        if ((joy & BUTTON_B) && p->smartbombs > 0)
        {
            if (!bomb_held)
            {
                smartbomb_activate(gd);
                p->smartbombs--;
                sfx_play(SFX_SMARTBOMB);
            }
            bomb_held = TRUE;
        }
        else
        {
            bomb_held = FALSE;
        }
    }

    /* Invincibility flash */
    if (p->invincible > 0)
    {
        p->invincible--;
        if (p->spr)
            SPR_setVisibility(p->spr, (p->invincible & 4) ? HIDDEN : VISIBLE);
    }
    else if (p->spr)
    {
        SPR_setVisibility(p->spr, VISIBLE);
    }

    /* Extra life check for P2 */
    static u32 p2_next_life = SCORE_EXTRA_LIFE_BASE;
    if (p->score >= p2_next_life)
    {
        p->lives = MIN(p->lives + 1, 9);
        p2_next_life += SCORE_EXTRA_LIFE_BASE + (gd->level * 1000u);
        sfx_play(SFX_EXTRA_LIFE);
    }

    /* Update sprite */
    if (p->spr)
    {
        SPR_setPosition(p->spr,
                        fix16ToInt(p->x) - 8 - gd->cam_x,
                        fix16ToInt(p->y) - 8 - gd->cam_y);
        if (p->dir != DIR_NONE)
            SPR_setFrame(p->spr, (u16)p->dir);
    }
}

void player2_collision(GameData *gd)
{
    if (!g_two_player || !p2_active) return;
    Player *p = &p2_state;
    if (!p->active || p->invincible > 0) return;

    /* vs enemies */
    for (u8 i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *e = &gd->enemies[i];
        if (!e->active) continue;
        if (rects_overlap(p->x, p->y, 10, 10, e->x, e->y, 12, 12))
        { player2_die(gd); return; }
    }
    /* vs mines */
    for (u8 i = 0; i < MAX_MINES; i++)
    {
        Mine *m = &gd->mines[i];
        if (!m->active) continue;
        if (rects_overlap(p->x, p->y, 10, 10, m->x, m->y, 10, 10))
        {
            m->active = FALSE;
            if (m->spr) { SPR_releaseSprite(m->spr); m->spr = NULL; }
            player2_die(gd);
            return;
        }
    }
    /* vs gems — P2 collects gems too */
    for (u8 i = 0; i < MAX_GEMS; i++)
    {
        Gem *g = &gd->gems[i];
        if (!g->active || g->collected) continue;
        if (rects_overlap(p->x, p->y, 10, 10, g->x, g->y, 10, 10))
        {
            g->collected = TRUE;
            g->active    = FALSE;
            if (g->spr) { SPR_releaseSprite(g->spr); g->spr = NULL; }
            p->score += 200;   /* original: 200 pts per crystal */
            gd->gems_remaining--;
            sfx_play(SFX_GEM_COLLECT);
        }
    }
    /* vs enemy bullets */
    for (u8 i = 0; i < MAX_BULLETS; i++)
    {
        Bullet *b = &gd->bullets[i];
        if (!b->active || b->is_player) continue;
        if (rects_overlap(b->x, b->y, 6, 6, p->x, p->y, 10, 10))
        {
            b->active = FALSE;
            if (b->spr) { SPR_releaseSprite(b->spr); b->spr = NULL; }
            player2_die(gd);
            return;
        }
    }
}

void player2_die(GameData *gd)
{
    Player *p = &p2_state;
    if (!p->active || p->invincible > 0) return;

    sfx_play(SFX_PLAYER_DIE);
    p->lives--;

    if (p->lives == 0)
    {
        p->active = FALSE;
        if (p->spr) SPR_setVisibility(p->spr, HIDDEN);
        p2_active = FALSE;
        if (!gd->player.active || gd->player.lives == 0)
            gd->state = STATE_GAME_OVER;
        return;
    }

    p->x = FIX16(WORLD_W / 4);
    p->y = FIX16(WORLD_H / 2);
    p->vx = p->vy = FIX16(0);
    p->invincible = 120;
}

u8 player2_is_active(void)
{
    return p2_active && p2_state.active;
}

Player *player2_get(void)
{
    return &p2_state;
}

u32 player2_get_score(void)
{
    return p2_active ? p2_state.score : 0;
}
