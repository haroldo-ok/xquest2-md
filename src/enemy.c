/*
 * enemy.c - XQuest 2 for Sega Genesis (SGDK 1.70)
 * All 15 enemy types from the original XQuest 2
 *
 * Enemy roster (from docs):
 *   0  GRUNGER    - slow, stupid, hired muscle
 *   1  ZIPPO      - same as grunger but fast
 *   2  ZINGER     - fires bullets everywhere
 *   3  VINCE      - almost invulnerable
 *   4  MINER      - lays mines
 *   5  MEEBY      - big, annoying, tough
 *   6  RETALIATOR - shoots back when hit
 *   7  TERRIER    - homes in after sensing player
 *   8  DOINGER    - more dangerous the longer it's alive
 *   9  SNIPE      - waits for clear shot
 *  10  TRIBBLER   - splits when shot
 *  11  BUCKSHOT   - fires many bullets
 *  12  CLUSTER    - harmless until shot (then splits dangerously)
 *  13  STICKTIGHT - extremely hard to shake off
 *  14  REPULSOR   - pushes player away forcefully
 */

#include <genesis.h>
#include "xquest.h"
#include "tilemap.h"
#include "screens.h"
#include "sfx.h"
#include "enemy_variants.h"
#include "resources.h"

/* Enemy HP table */
static const s16 ENEMY_HP[ENEMY_TYPE_COUNT] = {
    1,   /* GRUNGER */
    1,   /* ZIPPO */
    2,   /* ZINGER */
   10,   /* VINCE (nearly invulnerable) */
    1,   /* MINER */
    5,   /* MEEBY */
    2,   /* RETALIATOR */
    1,   /* TERRIER */
    2,   /* DOINGER */
    2,   /* SNIPE */
    1,   /* TRIBBLER */
    2,   /* BUCKSHOT */
    3,   /* CLUSTER */
    3,   /* STICKTIGHT */
    3,   /* REPULSOR */
};

/* Enemy base speed (fix16) */
static const fix16 ENEMY_SPEED[ENEMY_TYPE_COUNT] = {
    FIX16(0.8),   /* GRUNGER */
    FIX16(2.0),   /* ZIPPO */
    FIX16(1.2),   /* ZINGER */
    FIX16(1.0),   /* VINCE */
    FIX16(0.6),   /* MINER */
    FIX16(0.7),   /* MEEBY */
    FIX16(1.0),   /* RETALIATOR */
    FIX16(1.8),   /* TERRIER */
    FIX16(0.9),   /* DOINGER */
    FIX16(0.4),   /* SNIPE */
    FIX16(1.0),   /* TRIBBLER */
    FIX16(1.5),   /* BUCKSHOT */
    FIX16(0.5),   /* CLUSTER */
    FIX16(1.6),   /* STICKTIGHT */
    FIX16(1.2),   /* REPULSOR */
};

/* Score value per kill */
static const u16 ENEMY_SCORE[ENEMY_TYPE_COUNT] = {
    100,  /* GRUNGER */
    150,  /* ZIPPO */
    200,  /* ZINGER */
    500,  /* VINCE */
    120,  /* MINER */
    400,  /* MEEBY */
    250,  /* RETALIATOR */
    200,  /* TERRIER */
    180,  /* DOINGER */
    300,  /* SNIPE */
    150,  /* TRIBBLER */
    350,  /* BUCKSHOT */
    200,  /* CLUSTER */
    300,  /* STICKTIGHT */
    400,  /* REPULSOR */
};

/* Sprite definition pointer table */
static const SpriteDefinition *ENEMY_SPR[ENEMY_TYPE_COUNT] = {
    &spr_grunger,    /* GRUNGER */
    &spr_zippo,      /* ZIPPO */
    &spr_grunger,    /* ZINGER (shares grunger sprite; differentiate by color in future) */
    &spr_vince,      /* VINCE */
    &spr_miner,      /* MINER */
    &spr_meeby,      /* MEEBY */
    &spr_retaliator, /* RETALIATOR */
    &spr_terrier,    /* TERRIER */
    &spr_doinger,    /* DOINGER */
    &spr_terrier,    /* SNIPE (shares terrier) */
    &spr_tribbler,   /* TRIBBLER */
    &spr_zippo,      /* BUCKSHOT (shares zippo) */
    &spr_tribbler,   /* CLUSTER (shares tribbler) */
    &spr_sticktight, /* STICKTIGHT */
    &spr_meeby,      /* REPULSOR (large, forceful) */
};

/* Return speed adjusted for difficulty and game speed ramp */
static fix16 scaled_speed(const Enemy *e, fix16 base_speed)
{
    /* difficulty scale: enemy speed_scale is a percentage (100 = normal) */
    fix16 s = fix16Div(fix16Mul(base_speed, FIX16(e->speed_scale)), FIX16(100));
    /* game speed ramp (g_game_speed = FIX16(1) normally, >1 when over par) */
    return fix16Mul(s, g_game_speed);
}

/* ============================================================
 * UTILITY: Move toward target with speed
 * ============================================================ */
static void move_toward(Enemy *e, fix16 tx, fix16 ty, fix16 speed)
{
    fix16 dx = fix16Sub(tx, e->x);
    fix16 dy = fix16Sub(ty, e->y);
    fix16 dist = fix16Add(fix16Abs(dx), fix16Abs(dy));   /* Manhattan approx */
    if (dist < FIX16(1)) return;
    speed = scaled_speed(e, speed);
    e->vx = fix16Div(fix16Mul(dx, speed), dist);
    e->vy = fix16Div(fix16Mul(dy, speed), dist);
}

/* ============================================================
 * UTILITY: Move away from target
 * ============================================================ */
static void move_away(Enemy *e, fix16 tx, fix16 ty, fix16 speed) __attribute__((unused));
static void move_away(Enemy *e, fix16 tx, fix16 ty, fix16 speed)
{
    fix16 dx = fix16Sub(e->x, tx);
    fix16 dy = fix16Sub(e->y, ty);
    fix16 dist = fix16Add(fix16Abs(dx), fix16Abs(dy));
    if (dist < FIX16(1)) return;
    e->vx = fix16Div(fix16Mul(dx, speed), dist);
    e->vy = fix16Div(fix16Mul(dy, speed), dist);
}

/* ============================================================
 * ENEMY SPAWN
 * ============================================================ */
void enemy_spawn(Enemy *e, EnemyType type, fix16 x, fix16 y)
{
    memset(e, 0, sizeof(Enemy));
    e->x        = x;
    e->y        = y;
    e->type     = type;
    e->hp       = ENEMY_HP[type];
    e->active   = TRUE;
    e->ai_state = 0;
    e->ai_timer = 0;

    /* Apply difficulty speed scale (screens.h declares screen_get_difficulty) */
    {
        const DifficultySettings *diff = screen_get_difficulty();
        e->speed_scale = diff ? (u8)diff->enemy_spd_scale : 100;
    }

    /* Initialise curve components.
     * Meeby and Sticktight move in arcs (original: 'curves' flag true).
     * Others use identity rotation (no curve). */
    if (type == ENEMY_MEEBY || type == ENEMY_STICKTIGHT)
    {
        /* Small random rotation: ±(0..410) Q15 ≈ ±0.7° per frame */
        s16 cs = (s16)((s32)(random() % 820) - 410);
        s32 cs32 = (s32)cs * cs;
        s32 cc32 = 32767L - cs32 / 32767L;
        e->curvesin = cs;
        e->curvecos = (s16)(cc32 < 1000L ? 1000L : cc32);
    }
    else
    {
        e->curvesin = 0;
        e->curvecos = 32767;   /* identity */
    }

    e->spr = SPR_addSprite(ENEMY_SPR[type],
                           fix16ToInt(x) - 8,
                           fix16ToInt(y) - 8,
                           TILE_ATTR(PAL_ENEMY, TRUE, FALSE, FALSE));
    if (!e->spr) { e->active = FALSE; return; }
    SPR_setAnim(e->spr, 0);

    /* Apply variant palette/flip for shared-sprite enemy types */
    if (ev_is_variant(type))
    {
        ev_apply_to_sprite(type, e->spr);
        u8 off = ev_get_anim_offset(type);
        if (off > 0) SPR_setFrame(e->spr, off);
    }
}

/* ============================================================
 * PER-TYPE AI UPDATE
 * ============================================================ */
static void ai_grunger(Enemy *e, GameData *gd)
{
    /* Slow random walk toward player; changes direction every ~30 frames */
    e->ai_timer++;
    if (e->ai_timer >= 30)
    {
        e->ai_timer = 0;
        move_toward(e, gd->player.x, gd->player.y, ENEMY_SPEED[ENEMY_GRUNGER]);
        /* Small random perturbation so they don't all stack perfectly */
        e->vx = fix16Add(e->vx, FIX16_FROM_FRAC((s16)(random() % 40) - 20, 100));
        e->vy = fix16Add(e->vy, FIX16_FROM_FRAC((s16)(random() % 40) - 20, 100));
    }
}

static void ai_zippo(Enemy *e, GameData *gd)
{
    /* Like grunger but faster and more frequent direction changes */
    e->ai_timer++;
    if (e->ai_timer >= 12)
    {
        e->ai_timer = 0;
        move_toward(e, gd->player.x, gd->player.y, ENEMY_SPEED[ENEMY_ZIPPO]);
    }
}

static void ai_zinger(Enemy *e, GameData *gd)
{
    /* Moves erratically and fires bullets in multiple directions */
    e->ai_timer++;
    if (e->ai_timer >= 20)
    {
        e->ai_timer = 0;
        /* Fire in 4 directions */
        fix16 spd = FIX16(2.5);
        bullet_fire(gd, e->x, e->y,  spd,      FIX16(0), FALSE);
        bullet_fire(gd, e->x, e->y, FIX16(0),  spd,      FALSE);
        bullet_fire(gd, e->x, e->y, -spd,      FIX16(0), FALSE);
        bullet_fire(gd, e->x, e->y, FIX16(0), -spd,      FALSE);
        sfx_play(SFX_ENEMY_SHOOT);
    }
    /* Random movement */
    if ((g_frame_count & 0x1F) == 0)
    {
        e->vx = FIX16_FROM_FRAC((random() % 200 - 100) * 15, 1000);
        e->vy = FIX16_FROM_FRAC((random() % 200 - 100) * 15, 1000);
    }
}

static void ai_vince(Enemy *e, GameData *gd)
{
    /* Moves purposefully; takes many hits */
    move_toward(e, gd->player.x, gd->player.y, ENEMY_SPEED[ENEMY_VINCE]);
}

static void ai_miner(Enemy *e, GameData *gd)
{
    /* Moves around and periodically drops mines */
    e->ai_timer++;
    move_toward(e, gd->player.x, gd->player.y, ENEMY_SPEED[ENEMY_MINER]);
    if (e->ai_timer >= 90)
    {
        e->ai_timer = 0;
        mine_place(gd, e->x, e->y);
    }
}

static void ai_meeby(Enemy *e, GameData *gd)
{
    /* Large, slow, tough.
     * Original: moves in arcs (curves=TRUE) with periodic charge.
     * curvesin/curvecos rotate the velocity vector each frame, producing
     * the distinctive "moth" arc path.  Every ~90 frames the curve
     * direction flips randomly (changecurve) and a charge occurs. */
    e->ai_timer++;

    /* Rotate velocity by the curve matrix every frame */
    if (e->curvesin != 0)
    {
        /* new_vx = vx*cos - vy*sin  (all ×32767) */
        s32 nvx = ((s32)fix16ToRaw(e->vx) * e->curvecos -
                   (s32)fix16ToRaw(e->vy) * e->curvesin) >> 15;
        s32 nvy = ((s32)fix16ToRaw(e->vy) * e->curvecos +
                   (s32)fix16ToRaw(e->vx) * e->curvesin) >> 15;
        e->vx = fix16FromRaw((s32)nvx);
        e->vy = fix16FromRaw((s32)nvy);
    }

    /* Periodically re-aim and randomise curve */
    if (e->ai_timer >= 90)
    {
        e->ai_timer = 0;

        if (e->ai_state == 0)
        {
            /* Cruise phase: slow approach with curve */
            move_toward(e, gd->player.x, gd->player.y, FIX16(0.6));
            /* Pick a small random arc angle: ±3° (sin ≈ ±1638/32767) */
            e->curvesin = (s16)(((s16)(random() % 3276) - 1638));
            /* cos = sqrt(1 - sin²) approx via lookup – use 32700 for small angles */
            e->curvecos = 32700;
            e->ai_state = 1;
        }
        else
        {
            /* Charge phase: fast direct lunge */
            move_toward(e, gd->player.x, gd->player.y,
                        scaled_speed(e, FIX16(1.8)));
            e->curvesin = 0;
            e->curvecos = 32767;
            e->ai_state = 0;
        }
    }
}

static void ai_retaliator(Enemy *e, GameData *gd)
{
    /* Moves slowly; fires back when hit (handled in damage code) */
    e->ai_timer++;
    if (e->ai_timer >= 60)
    {
        e->ai_timer = 0;
        move_toward(e, gd->player.x, gd->player.y, ENEMY_SPEED[ENEMY_RETALIATOR]);
    }
}

static void ai_terrier(Enemy *e, GameData *gd)
{
    /* Ignores player until within sensing range, then relentlessly chases */
    fix16 dx = fix16Abs(fix16Sub(gd->player.x, e->x));
    fix16 dy = fix16Abs(fix16Sub(gd->player.y, e->y));
    if (e->ai_state == 0)
    {
        /* Idle: random wander */
        if ((g_frame_count & 0x3F) == 0)
        {
            e->vx = FIX16_FROM_FRAC((random() % 100 - 50) * 2, 100);
            e->vy = FIX16_FROM_FRAC((random() % 100 - 50) * 2, 100);
        }
        /* Detect player */
        if (dx < FIX16(80) && dy < FIX16(80))
            e->ai_state = 1;
    }
    else
    {
        /* Hunting: fast chase */
        move_toward(e, gd->player.x, gd->player.y, ENEMY_SPEED[ENEMY_TERRIER]);
    }
}

static void ai_doinger(Enemy *e, GameData *gd)
{
    /* Gets faster and fires more as ai_timer increases.
     * Cap ai_timer at 600 so speed and fire rate don't overflow. */
    if (e->ai_timer < 600) e->ai_timer++;

    fix16 base_speed = fix16Add(ENEMY_SPEED[ENEMY_DOINGER],
                                FIX16_FROM_FRAC(e->ai_timer, 300));
    base_speed = MIN(base_speed, FIX16(3.5));
    move_toward(e, gd->player.x, gd->player.y, base_speed);

    /* Fire interval shrinks from 60 frames down to 10 as timer grows */
    u16 fire_interval = (u16)MAX(60 - (s16)(e->ai_timer / 10), 10);
    if ((g_frame_count % fire_interval) == 0)
    {
        fix16 spd = FIX16(2.5);
        fix16 dx = fix16Sub(gd->player.x, e->x);
        fix16 dy = fix16Sub(gd->player.y, e->y);
        fix16 dist = fix16Add(fix16Abs(dx), fix16Abs(dy));
        if (dist > FIX16(1))
            bullet_fire(gd, e->x, e->y,
                        fix16Div(fix16Mul(dx, spd), dist),
                        fix16Div(fix16Mul(dy, spd), dist), FALSE);
    }
}

static void ai_snipe(Enemy *e, GameData *gd)
{
    /* Stays still, waits for clear line of sight, then fires accurate shot */
    e->ai_timer++;
    if (e->ai_timer >= 120)
    {
        e->ai_timer = 0;
        /* Fire toward player */
        fix16 dx = fix16Sub(gd->player.x, e->x);
        fix16 dy = fix16Sub(gd->player.y, e->y);
        fix16 dist = fix16Add(fix16Abs(dx), fix16Abs(dy));
        if (dist > FIX16(1))
        {
            fix16 spd = FIX16(4.0);
            bullet_fire(gd, e->x, e->y,
                        fix16Div(fix16Mul(dx, spd), dist),
                        fix16Div(fix16Mul(dy, spd), dist), FALSE);
            sfx_play(SFX_ENEMY_SHOOT);
        }
    }
    /* Slight drift */
    if ((g_frame_count & 0x7F) == 0)
    {
        e->vx = FIX16_FROM_FRAC((random() % 60 - 30), 100);
        e->vy = FIX16_FROM_FRAC((random() % 60 - 30), 100);
    }
}

static void ai_tribbler(Enemy *e, GameData *gd)
{
    /* Very friendly - moves toward player.
     * When hit (hp reaches 0), splits into 2 smaller ones.
     * Split handled in enemy_die. */
    move_toward(e, gd->player.x, gd->player.y, ENEMY_SPEED[ENEMY_TRIBBLER]);
}

static void ai_buckshot(Enemy *e, GameData *gd)
{
    /* Fires many bullets at once (more than Zinger) */
    e->ai_timer++;
    if (e->ai_timer >= 25)
    {
        e->ai_timer = 0;
        /* Fire in 8 directions */
        for (u8 d = 0; d < 8; d++)
        {
            fix16 spd = FIX16(2.0);
            bullet_fire(gd, e->x, e->y,
                        fix16Mul(DIR_DVX[d], spd),
                        fix16Mul(DIR_DVY[d], spd), FALSE);
        }
        sfx_play(SFX_ENEMY_SHOOT);
    }
    move_toward(e, gd->player.x, gd->player.y, ENEMY_SPEED[ENEMY_BUCKSHOT]);
}

static void ai_cluster(Enemy *e, GameData *gd)
{
    /* Slow and harmless - just drifts. On death splits into dangerous pieces.
     * Split handled in enemy_die. */
    e->ai_timer++;
    if (e->ai_timer >= 60)
    {
        e->ai_timer = 0;
        /* Gentle random drift */
        e->vx = FIX16_FROM_FRAC((random() % 100 - 50) * 8, 1000);
        e->vy = FIX16_FROM_FRAC((random() % 100 - 50) * 8, 1000);
    }
}

static void ai_sticktight(Enemy *e, GameData *gd)
{
    /* Moves fast toward player and is very hard to shake off (homing) */
    move_toward(e, gd->player.x, gd->player.y, ENEMY_SPEED[ENEMY_STICKTIGHT]);
    /* If close, maintain that distance and circle */
    fix16 dx = fix16Abs(fix16Sub(gd->player.x, e->x));
    fix16 dy = fix16Abs(fix16Sub(gd->player.y, e->y));
    if (dx < FIX16(20) && dy < FIX16(20))
    {
        /* Orbit: add perpendicular component */
        fix16 perp_x = fix16Sub(FIX16(0), e->vy);
        fix16 perp_y = e->vx;
        e->vx = fix16Add(e->vx, fix16Mul(perp_x, FIX16(0.3)));
        e->vy = fix16Add(e->vy, fix16Mul(perp_y, FIX16(0.3)));
    }
}

static void ai_repulsor(Enemy *e, GameData *gd)
{
    /* Pushes player away with a powerful force field */
    fix16 dx = fix16Sub(gd->player.x, e->x);
    fix16 dy = fix16Sub(gd->player.y, e->y);
    fix16 dist = fix16Add(fix16Abs(dx), fix16Abs(dy));

    if (dist < FIX16(96) && dist > FIX16(1))
    {
        /* Push player away - inversely proportional to distance */
        fix16 force = fix16Div(FIX16(12), dist);
        gd->player.vx = fix16Add(gd->player.vx, fix16Div(fix16Mul(dx, force), dist));
        gd->player.vy = fix16Add(gd->player.vy, fix16Div(fix16Mul(dy, force), dist));
    }

    /* Repulsor itself drifts slowly */
    if ((g_frame_count & 0x1F) == 0)
        move_toward(e, gd->player.x, gd->player.y, FIX16(0.3));
}

/* AI dispatch table */
typedef void (*AIFunc)(Enemy *, GameData *);
static const AIFunc AI_TABLE[ENEMY_TYPE_COUNT] = {
    ai_grunger,    /* 0  GRUNGER */
    ai_zippo,      /* 1  ZIPPO */
    ai_zinger,     /* 2  ZINGER */
    ai_vince,      /* 3  VINCE */
    ai_miner,      /* 4  MINER */
    ai_meeby,      /* 5  MEEBY */
    ai_retaliator, /* 6  RETALIATOR */
    ai_terrier,    /* 7  TERRIER */
    ai_doinger,    /* 8  DOINGER */
    ai_snipe,      /* 9  SNIPE */
    ai_tribbler,   /* 10 TRIBBLER */
    ai_buckshot,   /* 11 BUCKSHOT */
    ai_cluster,    /* 12 CLUSTER */
    ai_sticktight, /* 13 STICKTIGHT */
    ai_repulsor,   /* 14 REPULSOR */
};

/* ============================================================
 * ENEMIES UPDATE (all active enemies)
 * ============================================================ */
void enemies_update(GameData *gd)
{
    for (u8 i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *e = &gd->enemies[i];
        if (!e->active) continue;

        /* Run AI */
        AI_TABLE[e->type](e, gd);

        /* Apply curved motion: rotate velocity by curvesin/curvecos if set.
         * This is done after AI so the AI sets the base direction, and the
         * curve bends it each frame (matching original curvesin/curvecos logic). */
        if (e->curvesin != 0)
        {
            s32 nvx = ((s32)fix16ToRaw(e->vx) * (s32)e->curvecos -
                       (s32)fix16ToRaw(e->vy) * (s32)e->curvesin) >> 15;
            s32 nvy = ((s32)fix16ToRaw(e->vy) * (s32)e->curvecos +
                       (s32)fix16ToRaw(e->vx) * (s32)e->curvesin) >> 15;
            e->vx = fix16FromRaw(nvx);
            e->vy = fix16FromRaw(nvy);
        }

        /* Integrate position */
        e->x = fix16Add(e->x, e->vx);
        e->y = fix16Add(e->y, e->vy);

        /* Boundary bounce (original: enemies bounce off EnemyXMin/Max/YMin/Max).
         * Also guard against the vertical enemy-inlet strip at screen midpoint
         * (x near 0 or SCREEN_W), matching original EnemyEntering guard. */
        s16 ex = fix16ToInt(e->x);
        s16 ey = fix16ToInt(e->y);

        /* Left boundary */
        if (ex < 10)
        {
            e->x  = FIX16(10);
            e->vx = fix16Abs(e->vx);
        }
        /* Right boundary */
        else if (ex > SCREEN_W - 10)
        {
            e->x  = FIX16(SCREEN_W - 10);
            e->vx = -fix16Abs(e->vx);
        }
        /* Top boundary (below HUD) */
        if (ey < HUD_HEIGHT + 10)
        {
            e->y  = FIX16(HUD_HEIGHT + 10);
            e->vy = fix16Abs(e->vy);
        }
        /* Bottom boundary */
        else if (ey > SCREEN_H - 10)
        {
            e->y  = FIX16(SCREEN_H - 10);
            e->vy = -fix16Abs(e->vy);
        }

        /* Extra guard: don't let enemies wander into the spawn-portal columns
         * (x < 20 or x > SCREEN_W-20) except at the vertical midpoint row,
         * matching original "collision with enemy inlets" check. */
        ex = fix16ToInt(e->x);
        ey = fix16ToInt(e->y);
        s16 mid_y = (HUD_HEIGHT + SCREEN_H) / 2;
        if (ey < mid_y - 10 || ey > mid_y + 10)   /* not at portal row */
        {
            if (ex < 20)
            {
                e->x  = FIX16(20);
                e->vx = fix16Abs(e->vx);
            }
            else if (ex > SCREEN_W - 20)
            {
                e->x  = FIX16(SCREEN_W - 20);
                e->vx = -fix16Abs(e->vx);
            }
        }

        /* Wall tile collision: bounce off solid tiles */
        {
            u8 tx, ty;
            tilemap_cell_at_pixel(fix16ToInt(e->x), fix16ToInt(e->y), &tx, &ty);
            if (tilemap_is_solid(gd, tx, ty))
            {
                /* Push back and reverse relevant velocity */
                e->x = fix16Sub(e->x, e->vx);
                e->y = fix16Sub(e->y, e->vy);
                if (e->vx != 0) e->vx = -e->vx;
                if (e->vy != 0) e->vy = -e->vy;
            }
        }

        /* Animation cycle */
        e->anim_timer++;
        if (e->anim_timer >= 8)
        {
            e->anim_timer = 0;
            /* Advance frame, wrapping at the animation's frame count.
             * We read numFrame from the SpriteDefinition directly since
             * SPR_getAnimationDef does not exist in SGDK 1.70. */
            u16 num_frames = ENEMY_SPR[e->type]->animations[0]->numFrame;
            e->anim_frame = (e->anim_frame + 1) % num_frames;
            if (e->spr) SPR_setFrame(e->spr, e->anim_frame);
        }

        /* Update sprite position */
        if (e->spr) SPR_setPosition(e->spr,
                        fix16ToInt(e->x) - 8,
                        fix16ToInt(e->y) - 8);
    }
}

/* ============================================================
 * ENEMY DIE
 * ============================================================ */
void enemy_die(Enemy *e, GameData *gd)
{
    if (!e->active) return;

    /* Award score */
    gd->player.score += ENEMY_SCORE[e->type];

    /* Special death effects */
    if (e->type == ENEMY_TRIBBLER)
    {
        /* Original spawns 5 tribbles on death (ntyp+1 = next smaller type,
         * approximated here as ENEMY_GRUNGER for the fast small splits). */
        u8 spawned = 0;
        for (u8 i = 0; i < MAX_ENEMIES && spawned < 5; i++)
        {
            if (!gd->enemies[i].active)
            {
                enemy_spawn(&gd->enemies[i], ENEMY_GRUNGER,
                            fix16Add(e->x, FIX16((s16)(random() % 24) - 12)),
                            fix16Add(e->y, FIX16((s16)(random() % 24) - 12)));
                /* Give each child a random outward velocity */
                gd->enemies[i].vx = FIX16_FROM_FRAC((s16)(random() % 200) - 100, 50);
                gd->enemies[i].vy = FIX16_FROM_FRAC((s16)(random() % 200) - 100, 50);
                spawned++;
            }
        }
    }
    else if (e->type == ENEMY_CLUSTER)
    {
        /* Splits into dangerous fast pieces */
        for (u8 n = 0; n < 3; n++)
        {
            for (u8 i = 0; i < MAX_ENEMIES; i++)
            {
                if (!gd->enemies[i].active)
                {
                    enemy_spawn(&gd->enemies[i], ENEMY_ZIPPO,
                                e->x, e->y);
                    gd->enemies[i].vx = FIX16_FROM_FRAC((random() % 100 - 50) * 4, 100);
                    gd->enemies[i].vy = FIX16_FROM_FRAC((random() % 100 - 50) * 4, 100);
                    break;
                }
            }
        }
    }
    else if (e->type == ENEMY_RETALIATOR)
    {
        /* Fires in all 4 directions on death */
        fix16 spd = FIX16(3.0);
        bullet_fire(gd, e->x, e->y,  spd,      FIX16(0), FALSE);
        bullet_fire(gd, e->x, e->y, FIX16(0),  spd,      FALSE);
        bullet_fire(gd, e->x, e->y, -spd,      FIX16(0), FALSE);
        bullet_fire(gd, e->x, e->y, FIX16(0), -spd,      FALSE);
        sfx_play(SFX_ENEMY_SHOOT);
    }

    /* Spawn explosion */
    for (u8 i = 0; i < MAX_EXPLOSIONS; i++)
    {
        if (!gd->explosions[i].active)
        {
            gd->explosions[i].x      = e->x;
            gd->explosions[i].y      = e->y;
            gd->explosions[i].active = TRUE;
            gd->explosions[i].frame  = 0;
            break;
        }
    }

    sfx_play(SFX_EXPLOSION_SM);

    e->active = FALSE;
    if (e->spr) { SPR_releaseSprite(e->spr); e->spr = NULL; }
}

/* ============================================================
 * ENEMIES DRAW (position already set in update)
 * ============================================================ */
void enemies_draw(GameData *gd)
{
    /* Positions already updated in enemies_update via SPR_setPosition.
     * SGDK sprite engine handles the actual rendering via SPR_update()
     * called from main.c. Nothing extra needed here. */
    (void)gd;
}
