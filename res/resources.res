
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
SPRITE  spr_ship        "sprites/ship_player_00.png"          2 2 NONE
SPRITE  spr_ship_thrust "sprites/ship_player_thrust_00.png"   2 2 NONE
SPRITE  spr_ship_diag   "sprites/ship_player_diagonal_00.png" 2 2 NONE

# Gem (16x16, PAL2, 4 animation frames)
SPRITE  spr_gem         "sprites/gem_00.png"          2 2 NONE

# Mine (16x16, PAL1, 5 animation frames)
SPRITE  spr_mine        "sprites/mine_f0_00.png"      2 2 NONE

# Exit Gate (16x16, PAL2, 9 frame open animation)
SPRITE  spr_gate        "sprites/gate_exit_f0_00.png" 2 2 NONE

# Enemy spawn portal (16x24, PAL1, 5 frame open animation)
# Displayed on left and right walls at screen midpoint when enemies enter.
# Frame 0 = closed wall, frame 4 = fully open.
SPRITE  spr_portal      "sprites/portal_00.png"        2 3 NONE

# PowerCharge pickup (16x16, PAL2)
SPRITE  spr_powercharge "sprites/item_powercharge_00.png" 2 2 NONE

# SmartBomb effect (16x16, PAL3, 2 frames)
SPRITE  spr_smartbomb   "sprites/fx_smartbomb_f0_00.png"  2 2 NONE

# Explosion (16x16, PAL3, 3 frames)
SPRITE  spr_explosion   "sprites/fx_explosion_f0_00.png"  2 2 NONE

# Bullets
SPRITE  spr_bullet_player "sprites/bullet_player_00.png"      2 2 NONE
SPRITE  spr_bullet_green  "sprites/bullet_enemy_green_00.png" 2 2 NONE
SPRITE  spr_bullet_yellow "sprites/bullet_enemy_yellow_00.png" 2 2 NONE

# --- ENEMIES ---
# Meeby (24 frames = 12 rotation positions x 2 anim frames)
SPRITE  spr_meeby       "sprites/enemy_meeby_00.png"      2 2 NONE
# Grunger (6 frames)
SPRITE  spr_grunger     "sprites/enemy_grunger_00.png"    2 2 NONE
# Zippo (4 frames)
SPRITE  spr_zippo       "sprites/enemy_zippo_00.png"      2 2 NONE
# Tribbler (10 frames - splits when shot)
SPRITE  spr_tribbler    "sprites/enemy_tribbler_00.png"   2 2 NONE
# Retaliator (1 frame, shoots back)
SPRITE  spr_retaliator  "sprites/enemy_retaliator_00.png" 2 2 NONE
# Terrier (3 frames)
SPRITE  spr_terrier     "sprites/enemy_terrier_00.png"    2 2 NONE
# Sticktight (3 frames)
SPRITE  spr_sticktight  "sprites/enemy_sticktight_00.png" 2 2 NONE
# Doinger (1 frame)
SPRITE  spr_doinger     "sprites/enemy_doinger_00.png"    2 2 NONE
# Vince (1 frame, nearly invulnerable)
SPRITE  spr_vince       "sprites/enemy_vince_00.png"      2 2 NONE
# Miner (1 frame, lays mines)
SPRITE  spr_miner       "sprites/enemy_miner_00.png"      2 2 NONE

# --- SOUND EFFECTS ---
# All converted to 8-bit signed PCM at 8000 Hz via rescomp WAV driver 0 (PCM).
# Use SND_startPlay_PCM(sfx_N, sfx_N_size, SOUND_PCM_CH1, FALSE) to play.
WAV     sfx_0   "sounds/snd_00_music_intro.wav"   0 8000
WAV     sfx_1   "sounds/snd_01_music_loop_a.wav"  0 8000
WAV     sfx_2   "sounds/snd_02_music_loop_b.wav"  0 8000
WAV     sfx_3   "sounds/snd_03_music_loop_c.wav"  0 8000
WAV     sfx_4   "sounds/snd_04_music_loop_d.wav"  0 8000
WAV     sfx_5   "sounds/snd_05_sfx_burst_a.wav"   0 8000
WAV     sfx_6   "sounds/snd_06_sfx_burst_b.wav"   0 8000
WAV     sfx_7   "sounds/snd_07_sfx_sweep.wav"     0 8000
WAV     sfx_8   "sounds/snd_08_sfx_short_a.wav"   0 8000

# --- BACKGROUND TILESET ---
# 136 subtiles of 8x8 = 34 logical 16x16 tiles (wall, wall-edge, floor, cyan-floor)
TILESET bg_tileset      "tiles/bg_tileset.png"   NONE

# --- CUSTOM FONT ---
# 96 chars (ASCII 32-127), 8x8 px each, light green on black
TILESET xquest_font     "font/xquest_font.png"    NONE

# --- ENEMY VARIANT PALETTE ---
# Used for Zinger, Snipe, Buckshot, Cluster, Repulsor
PALETTE pal_enemy2      "palettes/pal_enemy2.png"
