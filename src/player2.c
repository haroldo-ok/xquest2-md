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
#include "player2.h"

/* Global two-player flag — set by the options screen */
u8 g_two_player = FALSE;

/* Player 2 data lives alongside Player 1 in GameData.
 * We extend GameData.player2 instead of the scaffold's
 * single-player design. Since adding a field to GameData
 * now would break existing code, we keep P2 state here. */
static Player p2_state;
static u8     p2_active = FALSE;

/* ────────────────────────────────────────────────
 * Public API
 * ──────────────────────────────────────────────── */

void player2_init(fix16 x, fix16 y)
{
    if (!g_two_player) return;

    /* Re-use the player_init logic but hook into p2_state */
    p2_state.x             = x;
    p2_state.y             = y;
    p2_state.vx            = FIX16(0);
    p2_state.vy            = FIX16(0);
    p2_state.dir           = DIR_LEFT;   /* start facing opposite to P1 */
    p2_state.lives         = INITIAL_LIVES;
    p2_state.smartbombs    = 3;
    p2_state.score         = 0;
    p2_state.shoot_cooldown = 0;
    p2_state.invincible    = 0;
    p2_state.active        = TRUE;

    /* Player 2 uses the thrust sprite (visually distinct) hflipped */
    p2_state.spr = SPR_addSprite(
        &spr_ship_thrust,
        fix16ToInt(x) - 8,
        fix16ToInt(y) - 8,
        TILE_ATTR(PAL_ACTIVE, TRUE, FALSE, FALSE));
    SPR_setHFlip(p2_state.spr, TRUE);   /* mirror to distinguish P2 */

    p2_active = TRUE;
}

void player2_update(GameData *gd)
{
    if (!g_two_player || !p2_active) return;
    Player *p = &p2_state;
    if (!p->active) return;

    u16 joy = JOY_readJoypad(JOY_2);

    /* --- Direction from JOY_2 D-pad --- */
    u8 right = (joy & BUTTON_RIGHT) != 0;
    u8 left  = (joy & BUTTON_LEFT)  != 0;
    u8 up    = (joy & BUTTON_UP)    != 0;
    u8 down  = (joy & BUTTON_DOWN)  != 0;

    Direction dir = DIR_NONE;
    if (right && !up && !down)  dir = DIR_RIGHT;
    else if (right && up)       dir = DIR_UP_RIGHT;
    else if (up && !right&& !left) dir = DIR_UP;
    else if (left && up)        dir = DIR_UP_LEFT;
    else if (left && !up&& !down)  dir = DIR_LEFT;
    else if (left && down)      dir = DIR_DOWN_LEFT;
    else if (down && !right&&!left) dir = DIR_DOWN;
    else if (right && down)     dir = DIR_DOWN_RIGHT;

    /* Movement (same constants as P1) */
    if (dir != DIR_NONE)
    {
        static const fix16 DVX[8] = {
             FIX16(1),  FIX16(1),  FIX16(0), FIX16(-1),
            FIX16(-1), FIX16(-1),  FIX16(0),  FIX16(1)
        };
        static const fix16 DVY[8] = {
             FIX16(0), FIX16(-1), FIX16(-1), FIX16(-1),
             FIX16(0),  FIX16(1),  FIX16(1),  FIX16(1)
        };
        p->dir = dir;
        p->vx = fix16Add(p->vx, fix16Mul(DVX[dir], SHIP_ACCEL));
        p->vy = fix16Add(p->vy, fix16Mul(DVY[dir], SHIP_ACCEL));
    }

    /* Clamp speed */
    fix16 speed_sq = fix16Add(fix16Mul(p->vx, p->vx), fix16Mul(p->vy, p->vy));
    fix16 max_sq   = fix16Mul(SHIP_MAX_SPEED, SHIP_MAX_SPEED);
    if (speed_sq > max_sq)
    {
        fix16 inv = fix16Div(SHIP_MAX_SPEED, fix16Sqrt(speed_sq));
        p->vx = fix16Mul(p->vx, inv);
        p->vy = fix16Mul(p->vy, inv);
    }

    p->vx = fix16Mul(p->vx, SHIP_FRICTION);
    p->vy = fix16Mul(p->vy, SHIP_FRICTION);
    p->x  = fix16Add(p->x, p->vx);
    p->y  = fix16Add(p->y, p->vy);

    /* Screen wrap */
    if (fix16ToInt(p->x) < 0)         p->x = FIX16(SCREEN_W - 1);
    if (fix16ToInt(p->x) >= SCREEN_W) p->x = FIX16(0);
    if (fix16ToInt(p->y) < HUD_HEIGHT) p->y = FIX16(SCREEN_H - 1);
    if (fix16ToInt(p->y) >= SCREEN_H)  p->y = FIX16(HUD_HEIGHT);

    /* Fire: JOY_2 buttons A or C */
    if (p->shoot_cooldown > 0) p->shoot_cooldown--;
    if ((joy & (BUTTON_A | BUTTON_C)) && p->shoot_cooldown == 0 && p->dir != DIR_NONE)
    {
        static const fix16 DVX2[8] = {
             FIX16(1),  FIX16(1),  FIX16(0), FIX16(-1),
            FIX16(-1), FIX16(-1),  FIX16(0),  FIX16(1)
        };
        static const fix16 DVY2[8] = {
             FIX16(0), FIX16(-1), FIX16(-1), FIX16(-1),
             FIX16(0),  FIX16(1),  FIX16(1),  FIX16(1)
        };
        fix16 bvx = fix16Mul(DVX2[p->dir], BULLET_SPEED);
        fix16 bvy = fix16Mul(DVY2[p->dir], BULLET_SPEED);
        bullet_fire(gd, p->x, p->y, bvx, bvy, TRUE);
        p->shoot_cooldown = SHIP_FIRE_COOLDOWN;
        sfx_play(7);
    }

    /* SmartBomb: JOY_2 button B */
    if ((joy & BUTTON_B) && p->smartbombs > 0)
    {
        static u8 bomb_held = FALSE;
        if (!bomb_held)
        {
            smartbomb_activate(gd);
            p->smartbombs--;
            sfx_play(6);
        }
        bomb_held = TRUE;
    }
    else
    {
        static u8 bomb_held = FALSE;
        bomb_held = FALSE;
    }

    /* Invincibility flash */
    if (p->invincible > 0)
    {
        p->invincible--;
        SPR_setVisibility(p->spr, (p->invincible & 4) ? HIDDEN : VISIBLE);
    }
    else
    {
        SPR_setVisibility(p->spr, VISIBLE);
    }

    /* Update sprite position */
    SPR_setPosition(p->spr, fix16ToInt(p->x) - 8, fix16ToInt(p->y) - 8);
    SPR_setHFlip(p->spr, dir == DIR_RIGHT || dir == DIR_UP_RIGHT || dir == DIR_DOWN_RIGHT
                         ? FALSE : TRUE);
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
            player2_die(gd);
    }
    /* vs mines */
    for (u8 i = 0; i < MAX_MINES; i++)
    {
        Mine *m = &gd->mines[i];
        if (!m->active) continue;
        if (rects_overlap(p->x, p->y, 10, 10, m->x, m->y, 10, 10))
        {
            m->active = FALSE;
            SPR_releaseSprite(m->spr);
            m->spr = NULL;
            player2_die(gd);
        }
    }
    /* vs gems (P2 also collects gems) */
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
            sfx_play(7);
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
            SPR_releaseSprite(b->spr);
            b->spr = NULL;
            player2_die(gd);
        }
    }
}

void player2_die(GameData *gd)
{
    Player *p = &p2_state;
    if (p->invincible > 0) return;

    sfx_play(5);
    p->lives--;

    if (p->lives == 0)
    {
        p->active = FALSE;
        SPR_setVisibility(p->spr, HIDDEN);
        p2_active = FALSE;
        /* Check if P1 is also dead → game over */
        if (!gd->player.active || gd->player.lives == 0)
            gd->state = STATE_GAME_OVER;
        return;
    }

    p->x = FIX16(SCREEN_W / 4);   /* respawn offset from P1 */
    p->y = FIX16(SCREEN_H / 2);
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
