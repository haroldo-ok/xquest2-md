/*
 * player.c - XQuest 2 for Sega Genesis (SGDK 1.70)
 * Player ship: 8-direction movement with inertia, firing, smartbomb
 * Preserves original XQuest 2 physics feel
 */

#include <genesis.h>
#include "xquest.h"
#include "sfx.h"
#include "resources.h"

/* Ship hitbox (centred on sprite) */
#define SHIP_HIT_W    10
#define SHIP_HIT_H    10

/* Direction -> delta velocity table (8-way) */
static const fix16 DIR_DVX[8] = {
     FIX16(1),  FIX16(1),  FIX16(0), FIX16(-1),
    FIX16(-1), FIX16(-1),  FIX16(0),  FIX16(1)
};
static const fix16 DIR_DVY[8] = {
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
    if ((joy & BUTTON_B) && p->smartbombs > 0)
    {
        /* Debounce: wait for release handled via cooldown */
        static u8 bomb_pressed = FALSE;
        if (!bomb_pressed)
        {
            smartbomb_activate(gd);
            p->smartbombs--;
            bomb_pressed = TRUE;
            sfx_play(SFX_SMARTBOMB);
        }
        bomb_pressed = TRUE;
    }
    else
    {
        static u8 bomb_pressed = FALSE;
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
    u32 threshold = SCORE_EXTRA_LIFE_BASE;
    /* threshold scales with level -- simplified linear */
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
    /* Flip sprite based on horizontal direction for free */
    u8 hflip = (p->dir == DIR_LEFT  ||
                p->dir == DIR_UP_LEFT ||
                p->dir == DIR_DOWN_LEFT);
    SPR_setHFlip(p->spr, hflip);
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
