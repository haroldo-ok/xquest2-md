/*
 * player.c - XQuest 2 for Sega Genesis (SGDK 1.70)
 * Player ship: 8-direction movement with inertia, firing, smartbomb
 * Preserves original XQuest 2 physics feel
 */

#include <genesis.h>
#include "xquest.h"
#include "tilemap.h"
#include "sfx.h"
#include "resources.h"

/* ============================================================
 * WALL COLLISION HELPERS
 *
 * We test the four corners of the ship's 10x10 hitbox
 * (centred on p->x, p->y) against the tile map.
 * On contact: bounce velocity component and push out of wall.
 * On Easy difficulty (DiffLevel 0): rebound instead of die.
 * All other difficulties: die on wall contact.
 * ============================================================ */
#define HALF_HIT  5   /* half of SHIP_HIT_W / SHIP_HIT_H */

/* Return TRUE if pixel (px,py) is inside a solid tile */
static u8 pixel_solid(const GameData *gd, s16 px, s16 py)
{
    if (px < 0 || px >= SCREEN_W || py < HUD_HEIGHT || py >= SCREEN_H)
        return TRUE;   /* treat off-screen as solid */
    u8 tx, ty;
    tilemap_cell_at_pixel(px, py, &tx, &ty);
    return tilemap_is_solid(gd, tx, ty);
}

static void player_wall_collide(Player *p, GameData *gd)
{
    s16 px = fix16ToInt(p->x);
    s16 py = fix16ToInt(p->y);

    /* Test left / right edges */
    u8 hit_l = pixel_solid(gd, px - HALF_HIT, py) ||
               pixel_solid(gd, px - HALF_HIT, py - HALF_HIT) ||
               pixel_solid(gd, px - HALF_HIT, py + HALF_HIT);
    u8 hit_r = pixel_solid(gd, px + HALF_HIT, py) ||
               pixel_solid(gd, px + HALF_HIT, py - HALF_HIT) ||
               pixel_solid(gd, px + HALF_HIT, py + HALF_HIT);

    /* Test top / bottom edges */
    u8 hit_t = pixel_solid(gd, px, py - HALF_HIT) ||
               pixel_solid(gd, px - HALF_HIT, py - HALF_HIT) ||
               pixel_solid(gd, px + HALF_HIT, py - HALF_HIT);
    u8 hit_b = pixel_solid(gd, px, py + HALF_HIT) ||
               pixel_solid(gd, px - HALF_HIT, py + HALF_HIT) ||
               pixel_solid(gd, px + HALF_HIT, py + HALF_HIT);

    if (!hit_l && !hit_r && !hit_t && !hit_b) return;

    /* Easy difficulty: rebound (same as original Wimp/Timid with Shield) */
    u8 diff = gd->difficulty;
    if (diff == 0)
    {
        if (hit_l || hit_r)
        {
            p->vx = hit_l ? fix16Abs(p->vx) : -fix16Abs(p->vx);
            /* Push out */
            if (hit_l) p->x = fix16Add(p->x, FIX16(2));
            else       p->x = fix16Sub(p->x, FIX16(2));
        }
        if (hit_t || hit_b)
        {
            p->vy = hit_t ? fix16Abs(p->vy) : -fix16Abs(p->vy);
            if (hit_t) p->y = fix16Add(p->y, FIX16(2));
            else       p->y = fix16Sub(p->y, FIX16(2));
        }
    }
    else
    {
        /* Normal+ difficulty: die on wall contact */
        player_die(p, gd);
    }
}

/* Ship hitbox (centred on sprite) */
#define SHIP_HIT_W    10
#define SHIP_HIT_H    10

/* Direction -> delta velocity table (8-way)
 * Shared with enemy.c via extern in xquest.h */
const fix16 DIR_DVX[8] = {
     FIX16(1),  FIX16(1),  FIX16(0), FIX16(-1),
    FIX16(-1), FIX16(-1),  FIX16(0),  FIX16(1)
};
const fix16 DIR_DVY[8] = {
     FIX16(0), FIX16(-1), FIX16(-1), FIX16(-1),
     FIX16(0),  FIX16(1),  FIX16(1),  FIX16(1)
};

/* ============================================================
 * PLAYER INIT
 * ============================================================ */
void player_init(Player *p, fix16 x, fix16 y)
{
    p->x             = x;
    p->y             = y;
    p->vx            = FIX16(0);
    p->vy            = FIX16(0);
    p->dir           = DIR_RIGHT;
    p->lives         = INITIAL_LIVES;
    p->smartbombs    = 3;
    p->score         = 0;
    p->shoot_cooldown = 0;
    p->invincible    = 0;
    p->active        = TRUE;

    p->spr = SPR_addSprite(&spr_ship,
                           fix16ToInt(x) - 8,
                           fix16ToInt(y) - 8,
                           TILE_ATTR(PAL_ACTIVE, TRUE, FALSE, FALSE));
    if (!p->spr) { p->active = FALSE; return; }
    SPR_setAnim(p->spr, 0);
}

/* ============================================================
 * READ INPUT & DETERMINE DIRECTION
 * ============================================================ */
static Direction read_direction(u16 joy)
{
    u8 right = (joy & BUTTON_RIGHT) != 0;
    u8 left  = (joy & BUTTON_LEFT)  != 0;
    u8 up    = (joy & BUTTON_UP)    != 0;
    u8 down  = (joy & BUTTON_DOWN)  != 0;

    /* 8-way direction from DPAD */
    if (right && !up && !down) return DIR_RIGHT;
    if (right && up)           return DIR_UP_RIGHT;
    if (up    && !right && !left) return DIR_UP;
    if (left  && up)           return DIR_UP_LEFT;
    if (left  && !up && !down) return DIR_LEFT;
    if (left  && down)         return DIR_DOWN_LEFT;
    if (down  && !right && !left) return DIR_DOWN;
    if (right && down)         return DIR_DOWN_RIGHT;

    return DIR_NONE;
}

/* ============================================================
 * PLAYER UPDATE  (called once per frame)
 * ============================================================ */
void player_update(Player *p, GameData *gd)
{
    if (!p->active) return;

    u16 joy = JOY_readJoypad(JOY_1);

    /* --- Movement --- */
    Direction dir = read_direction(joy);
    if (dir != DIR_NONE)
    {
        p->dir = dir;
        p->vx = fix16Add(p->vx, fix16Mul(DIR_DVX[dir], SHIP_ACCEL));
        p->vy = fix16Add(p->vy, fix16Mul(DIR_DVY[dir], SHIP_ACCEL));
    }

    /* Clamp speed */
    fix16 speed = fix16Mul(p->vx, p->vx);
    speed = fix16Add(speed, fix16Mul(p->vy, p->vy));
    if (speed > fix16Mul(SHIP_MAX_SPEED, SHIP_MAX_SPEED))
    {
        /* Normalize: approximate sqrt then scale */
        fix16 inv = fix16Div(SHIP_MAX_SPEED, fix16Sqrt(speed));
        p->vx = fix16Mul(p->vx, inv);
        p->vy = fix16Mul(p->vy, inv);
    }

    /* Friction */
    p->vx = fix16Mul(p->vx, SHIP_FRICTION);
    p->vy = fix16Mul(p->vy, SHIP_FRICTION);

    /* Integrate position */
    p->x = fix16Add(p->x, p->vx);
    p->y = fix16Add(p->y, p->vy);

    /* Screen wrap (original XQuest wraps at edges) */
    if (fix16ToInt(p->x) < 0)          p->x = FIX16(SCREEN_W - 1);
    if (fix16ToInt(p->x) >= SCREEN_W)  p->x = FIX16(0);
    if (fix16ToInt(p->y) < HUD_HEIGHT) p->y = FIX16(SCREEN_H - 1);
    if (fix16ToInt(p->y) >= SCREEN_H)  p->y = FIX16(HUD_HEIGHT);

    /* Wall / background collision */
    if (p->active && p->invincible == 0)
        player_wall_collide(p, gd);

    /* --- Firing (Button A or C) --- */
    if (p->shoot_cooldown > 0)
        p->shoot_cooldown--;

    if ((joy & (BUTTON_A | BUTTON_C)) && p->shoot_cooldown == 0 && p->dir != DIR_NONE)
    {
        /* Bullet velocity in current direction */
        fix16 bvx = fix16Mul(DIR_DVX[p->dir], BULLET_SPEED);
        fix16 bvy = fix16Mul(DIR_DVY[p->dir], BULLET_SPEED);
        bullet_fire(gd, p->x, p->y, bvx, bvy, TRUE);
        p->shoot_cooldown = SHIP_FIRE_COOLDOWN;
        sfx_play(SFX_SHOOT);   /* sfx_7 = short sweep / shoot sound */
    }

    /* --- SmartBomb (Button B) --- */
    static u8 bomb_pressed = FALSE;
    if ((joy & BUTTON_B) && p->smartbombs > 0)
    {
        if (!bomb_pressed)
        {
            smartbomb_activate(gd);
            p->smartbombs--;
            sfx_play(SFX_SMARTBOMB);
        }
        bomb_pressed = TRUE;
    }
    else
    {
        bomb_pressed = FALSE;
    }

    /* --- Invincibility countdown --- */
    if (p->invincible > 0)
    {
        p->invincible--;
        /* Flash sprite during invincibility */
        SPR_setVisibility(p->spr, (p->invincible & 4) ? HIDDEN : VISIBLE);
    }
    else
    {
        SPR_setVisibility(p->spr, VISIBLE);
    }

    /* --- Extra life check --- */
    static u32 next_life_score = SCORE_EXTRA_LIFE_BASE;
    if (p->score >= next_life_score)
    {
        p->lives = MIN(p->lives + 1, 9);
        next_life_score += SCORE_EXTRA_LIFE_BASE + (gd->level * 1000);
        sfx_play(SFX_EXTRA_LIFE);
    }

    /* --- Draw --- */
    player_draw(p);
}

/* ============================================================
 * PLAYER DRAW
 * ============================================================ */
void player_draw(Player *p)
{
    if (!p->active) return;
    SPR_setPosition(p->spr,
                    fix16ToInt(p->x) - 8,
                    fix16ToInt(p->y) - 8);
    /* spr_ship has 8 frames in Direction order (0=RIGHT .. 7=DOWN_RIGHT).
     * Simply set the frame to the current direction index. */
    if (p->dir != DIR_NONE)
        SPR_setFrame(p->spr, (u16)p->dir);
    SPR_setHFlip(p->spr, FALSE);   /* no flipping needed */
}

/* ============================================================
 * PLAYER DIE
 * ============================================================ */
void player_die(Player *p, GameData *gd)
{
    if (p->invincible > 0) return;   /* already dying */

    sfx_play(SFX_PLAYER_DIE);   /* sfx_5 = explosion burst */

    /* Spawn explosion at player position */
    for (u8 i = 0; i < MAX_EXPLOSIONS; i++)
    {
        if (!gd->explosions[i].active)
        {
            gd->explosions[i].x      = p->x;
            gd->explosions[i].y      = p->y;
            gd->explosions[i].active = TRUE;
            gd->explosions[i].frame  = 0;
            break;
        }
    }

    p->lives--;
    if (p->lives == 0)
    {
        p->active = FALSE;
        SPR_setVisibility(p->spr, HIDDEN);
        gd->state = STATE_GAME_OVER;
        return;
    }

    /* Respawn at centre with invincibility frames */
    p->x          = FIX16(SCREEN_W / 2);
    p->y          = FIX16(SCREEN_H / 2);
    p->vx         = FIX16(0);
    p->vy         = FIX16(0);
    p->invincible = 120;   /* 2 seconds @ 60fps */
}
