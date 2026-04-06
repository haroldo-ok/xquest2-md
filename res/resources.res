
# ============================================================
# XQuest 2 for Sega Genesis - SGDK 1.70 Resource Declarations
# ============================================================

# --- PALETTES ---
PALETTE pal_bg          "palettes/pal_bg.png"
PALETTE pal_active      "palettes/pal_active.png"
PALETTE pal_collect     "palettes/pal_collect.png"
PALETTE pal_enemy       "palettes/pal_enemy.png"

# --- TITLE SCREEN ---
IMAGE   img_title       "screens/title_screen.png"  BEST

# --- BACKGROUND TILES ---

# --- SPRITES ---
# Player ship  (16x16, PAL1, 8 direction frames)
SPRITE  spr_ship        "sprites/ship_player_00.png"          2 2 FAST  0
SPRITE  spr_ship_thrust "sprites/ship_player_thrust_00.png"   2 2 FAST  0
SPRITE  spr_ship_diag   "sprites/ship_player_diagonal_00.png" 2 2 FAST  0

# Gem (16x16, PAL2, 4 animation frames)
SPRITE  spr_gem         "sprites/gem_00.png"          2 2 SLOW  0

# Mine (16x16, PAL1, 5 animation frames)
SPRITE  spr_mine        "sprites/mine_f0_00.png"      2 2 SLOW  0

# Exit Gate (16x16, PAL2, 9 frame open animation)
SPRITE  spr_gate        "sprites/gate_exit_f0_00.png" 2 2 SLOW  0

# PowerCharge pickup (16x16, PAL2)
SPRITE  spr_powercharge "sprites/item_powercharge_00.png" 2 2 SLOW 0

# SmartBomb effect (16x16, PAL3, 2 frames)
SPRITE  spr_smartbomb   "sprites/fx_smartbomb_f0_00.png"  2 2 FAST 0

# Explosion (16x16, PAL3, 3 frames)
SPRITE  spr_explosion   "sprites/fx_explosion_f0_00.png"  2 2 FAST 0

# Bullets
SPRITE  spr_bullet_player "sprites/bullet_player_00.png"      2 2 FAST 0
SPRITE  spr_bullet_green  "sprites/bullet_enemy_green_00.png" 2 2 FAST 0
SPRITE  spr_bullet_yellow "sprites/bullet_enemy_yellow_00.png" 2 2 FAST 0

# --- ENEMIES ---
# Meeby (24 frames = 12 rotation positions x 2 anim frames)
SPRITE  spr_meeby       "sprites/enemy_meeby_00.png"      2 2 NORMAL 0
# Grunger (6 frames)
SPRITE  spr_grunger     "sprites/enemy_grunger_00.png"    2 2 NORMAL 0
# Zippo (4 frames)
SPRITE  spr_zippo       "sprites/enemy_zippo_00.png"      2 2 FAST   0
# Tribbler (10 frames - splits when shot)
SPRITE  spr_tribbler    "sprites/enemy_tribbler_00.png"   2 2 NORMAL 0
# Retaliator (1 frame, shoots back)
SPRITE  spr_retaliator  "sprites/enemy_retaliator_00.png" 2 2 NORMAL 0
# Terrier (3 frames)
SPRITE  spr_terrier     "sprites/enemy_terrier_00.png"    2 2 FAST   0
# Sticktight (3 frames)
SPRITE  spr_sticktight  "sprites/enemy_sticktight_00.png" 2 2 NORMAL 0
# Doinger (1 frame)
SPRITE  spr_doinger     "sprites/enemy_doinger_00.png"    2 2 NORMAL 0
# Vince (1 frame, nearly invulnerable)
SPRITE  spr_vince       "sprites/enemy_vince_00.png"      2 2 NORMAL 0
# Miner (1 frame, lays mines)
SPRITE  spr_miner       "sprites/enemy_miner_00.png"      2 2 NORMAL 0

# --- SOUND EFFECTS ---
# All at 8000 Hz, 8-bit unsigned PCM (negate: XOR 0x80 for SGDK signed)
SOUND   sfx_0   "sounds/snd_00_music_intro.wav"   DPCM
SOUND   sfx_1   "sounds/snd_01_music_loop_a.wav"  DPCM
SOUND   sfx_2   "sounds/snd_02_music_loop_b.wav"  DPCM
SOUND   sfx_3   "sounds/snd_03_music_loop_c.wav"  DPCM
SOUND   sfx_4   "sounds/snd_04_music_loop_d.wav"  DPCM
SOUND   sfx_5   "sounds/snd_05_sfx_burst_a.wav"   DPCM
SOUND   sfx_6   "sounds/snd_06_sfx_burst_b.wav"   DPCM
SOUND   sfx_7   "sounds/snd_07_sfx_sweep.wav"     DPCM
SOUND   sfx_8   "sounds/snd_08_sfx_short_a.wav"   DPCM

# --- BACKGROUND TILESET ---
# 136 subtiles of 8x8 = 34 logical 16x16 tiles (wall, wall-edge, floor, cyan-floor)
TILESET bg_tileset      "tiles/bg_tileset.png"   NONE

# --- CUSTOM FONT ---
# 96 chars (ASCII 32-127), 8x8 px each, light green on black
TILESET xquest_font     "font/xquest_font.png"    NONE

# --- ENEMY VARIANT PALETTE ---
# Used for Zinger, Snipe, Buckshot, Cluster, Repulsor
PALETTE pal_enemy2      "palettes/pal_enemy2.png"
