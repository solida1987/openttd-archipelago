#!/usr/bin/env python3
"""Generate isometric ruin sprites for OpenTTD Archipelago mod.

Each sprite is a 64x40 8-bit indexed PNG using the OpenTTD DOS palette.
Index 0 = transparent. We draw simple but recognisable ruin shapes.

Output: sprites.png  (sprite sheet with all ruins side-by-side)
"""

from PIL import Image, ImageDraw
import struct, math, os

# ---------- OpenTTD DOS palette (indices 0-255) ----------
# Extracted from src/table/palettes.h  — only the first ~215 matter.
_PAL_RAW = [
    (  0,  0,  0,  0),  # 0: transparent
    ( 16, 16, 16),( 32, 32, 32),( 48, 48, 48),( 65, 64, 65),( 82, 80, 82),( 98,101, 98),(115,117,115),  # 1-7 grey
    (131,133,131),(148,149,148),(168,168,168),(184,184,184),(200,200,200),(216,216,216),(232,232,232),(252,252,252),  # 8-15
    ( 52, 60, 72),( 68, 76, 92),( 88, 96,112),(108,116,132),(132,140,152),(156,160,172),(176,184,196),(204,208,220),  # 16-23 blue-grey
    ( 48, 44,  4),( 64, 60, 12),( 80, 76, 20),( 96, 92, 28),(120,120, 64),(148,148,100),(176,176,132),(204,204,168),  # 24-31 olive
    ( 72, 44,  4),( 88, 60, 20),(104, 80, 44),(124,104, 72),(152,132, 92),(184,160,120),(212,188,148),(244,220,176),  # 32-39 tan/brown
    ( 64,  0,  4),( 88,  4, 16),(112, 16, 32),(136, 32, 52),(160, 56, 76),(188, 84,108),(204,104,124),(220,132,144),  # 40-47 pink/red
    (236,156,164),(252,188,192),(252,212,  0),(252,232, 60),(252,248,128),( 76, 40,  0),( 96, 60,  8),(116, 88, 28),  # 48-55
    (136,116, 56),(156,136, 80),(176,156,108),(196,180,136),( 68, 24,  0),( 96, 44,  4),(128, 68,  8),(156, 96, 16),  # 56-63
    (184,120, 24),(212,156, 32),(232,184, 16),(252,212,  0),(252,248,128),(252,252,192),( 32,  4,  0),( 64, 20,  8),  # 64-71
    ( 84, 28, 16),(108, 44, 28),(128, 56, 40),(148, 72, 56),(168, 92, 76),(184,108, 88),(196,128,108),(212,148,128),  # 72-79
    (  8, 52,  0),( 16, 64,  0),( 32, 80,  4),( 48, 96,  4),( 64,112, 12),( 84,132, 20),(104,148, 28),(128,168, 44),  # 80-87 green
    ( 28, 52, 24),( 44, 68, 32),( 60, 88, 48),( 80,104, 60),(104,124, 76),(128,148, 92),(152,176,108),(180,204,124),  # 88-95
    ( 16, 52, 24),( 32, 72, 44),( 56, 96, 72),( 76,116, 88),( 96,136,108),(120,164,136),(152,192,168),(184,220,200),  # 96-103
    ( 32, 24,  0),( 56, 28,  0),( 72, 40,  4),( 88, 52, 12),(104, 64, 24),(124, 84, 44),(140,108, 64),(160,128, 88),  # 104-111 dark brown
    ( 76, 40, 16),( 96, 52, 24),(116, 68, 40),(136, 84, 56),(164, 96, 64),(184,112, 80),(204,128, 96),(212,148,112),  # 112-119 brown
    (224,168,128),(236,188,148),( 80, 28,  4),(100, 40, 20),(120, 56, 40),(140, 76, 64),(160,100, 96),(184,136,136),  # 120-127
    ( 36, 40, 68),( 48, 52, 84),( 64, 64,100),( 80, 80,116),(100,100,136),(132,132,164),(172,172,192),(212,212,224),  # 128-135 steel blue
    ( 40, 20,112),( 64, 44,144),( 88, 64,172),(104, 76,196),(120, 88,224),(140,104,252),(160,136,252),(188,168,252),  # 136-143 purple
    (  0, 24,108),(  0, 36,132),(  0, 52,160),(  0, 72,184),(  0, 96,212),( 24,120,220),( 56,144,232),( 88,168,240),  # 144-151 blue
    (128,196,252),(188,224,252),( 16, 64, 96),( 24, 80,108),( 40, 96,120),( 52,112,132),( 80,140,160),(116,172,192),  # 152-159
    (156,204,220),(204,240,252),(172, 52, 52),(212, 52, 52),(252, 52, 52),(252,100, 88),(252,144,124),(252,184,160),  # 160-167 red
    (252,216,200),(252,244,236),( 72, 20,112),( 92, 44,140),(112, 68,168),(140,100,196),(168,136,224),(204,180,252),  # 168-175
    (204,180,252),(232,208,252),( 60,  0,  0),( 92,  0,  0),(128,  0,  0),(160,  0,  0),(196,  0,  0),(224,  0,  0),  # 176-183 dark red
    (252,  0,  0),(252, 80,  0),(252,108,  0),(252,136,  0),(252,164,  0),(252,192,  0),(252,220,  0),(252,252,  0),  # 184-191 orange/yellow
    (204,136,  8),(228,144,  4),(252,156,  0),(252,176, 48),(252,196,100),(252,216,152),(  8, 24, 88),( 12, 36,104),  # 192-199
    ( 20, 52,124),( 28, 68,140),( 40, 92,164),( 56,120,188),( 72,152,216),(100,172,224),( 92,156, 52),(108,176, 64),  # 200-207
    (124,200, 76),(144,224, 92),(224,244,252),(204,240,252),(180,220,236),(132,188,216),( 88,152,172),  # 208-214
]

# --- Useful palette indices ---
TRANSPARENT = 0
DARK_GREY   = 1   # (16,16,16)
MED_GREY    = 4   # (65,64,65)
LIGHT_GREY  = 6   # (98,101,98)
LIGHTER_GREY= 9   # (148,149,148)
WHITE_GREY  = 11  # (184,184,184)
DARK_BROWN  = 104 # (32,24,0)
MED_BROWN   = 106 # (72,40,4)
BROWN       = 107 # (88,52,12)
LIGHT_BROWN = 108 # (104,64,24)
TAN         = 110 # (140,108,64)
LIGHT_TAN   = 111 # (160,128,88)
SAND        = 35  # (152,132,92)
DARK_RED    = 178 # (60,0,0)
RED         = 162 # (172,52,52)
ORANGE      = 185 # (252,80,0)
OLIVE_DARK  = 24  # (48,44,4)
OLIVE       = 25  # (64,60,12)
BARE_BROWN  = 32  # (72,44,4)
BARE_BROWN2 = 33  # (88,60,20)
BARE_TAN    = 34  # (104,80,44)
GREEN_DARK  = 80  # (8,52,0)
GREEN       = 82  # (32,80,4)

# Sprite dimensions: 64 wide, 40 tall (standard OpenTTD isometric tile)
W, H = 64, 40
NUM_SPRITES = 6

def build_palette():
    """Build a proper 256-colour PIL palette from _PAL_RAW."""
    pal = []
    for i, c in enumerate(_PAL_RAW):
        if len(c) == 4:
            pal.extend(c[:3])  # skip alpha for palette definition
        else:
            pal.extend(c)
    # Pad remaining entries to 256 with magenta (unused pink)
    while len(pal) < 256 * 3:
        pal.extend([212, 0, 212])
    return pal

def in_diamond(x, y, cx=31, cy=15, hw=31, hh=15):
    """Check if (x,y) is inside an isometric diamond centred at (cx,cy)."""
    dx = abs(x - cx)
    dy = abs(y - cy)
    return (dx / hw + dy / hh) <= 1.0

def draw_ground(pixels, ox, ground_color=BARE_BROWN2):
    """Fill the isometric diamond with a ground colour."""
    for y in range(H):
        for x in range(W):
            if in_diamond(x, y):
                pixels[ox + x, y] = ground_color

def draw_rubble_stones(pixels, ox, seed=42):
    """Draw scattered stone/rubble blocks on the ground."""
    import random
    rng = random.Random(seed)

    # Dark ground base
    draw_ground(pixels, ox, BARE_BROWN)

    # Scatter some lighter ground patches
    for _ in range(40):
        sx = rng.randint(8, W - 9)
        sy = rng.randint(4, H - 5)
        if in_diamond(sx, sy, hw=28, hh=13):
            pixels[ox + sx, sy] = BARE_BROWN2

    # Draw rubble stones (small rectangles)
    stones = [
        (20, 12, 4, 3, MED_GREY),
        (35, 18, 5, 3, LIGHT_GREY),
        (15, 20, 3, 2, MED_GREY),
        (42, 14, 4, 3, LIGHTER_GREY),
        (28, 22, 5, 2, MED_GREY),
        (22, 8,  3, 2, LIGHT_GREY),
        (38, 25, 4, 2, MED_GREY),
        (12, 15, 3, 3, LIGHTER_GREY),
        (48, 20, 3, 2, MED_GREY),
        (30, 10, 4, 2, LIGHT_GREY),
        (25, 28, 3, 2, MED_GREY),
        (18, 25, 4, 2, LIGHTER_GREY),
    ]
    for sx, sy, sw, sh, col in stones:
        for dy in range(sh):
            for dx in range(sw):
                px, py = sx + dx, sy + dy
                if 0 <= px < W and 0 <= py < H and in_diamond(px, py, hw=29, hh=14):
                    pixels[ox + px, py] = col
                    # Add shadow below
                    if dy == sh - 1 and py + 1 < H and in_diamond(px, py + 1, hw=29, hh=14):
                        pixels[ox + px, py + 1] = DARK_GREY

def draw_broken_wall(pixels, ox, seed=123):
    """Draw a partial broken wall structure."""
    import random
    rng = random.Random(seed)

    draw_ground(pixels, ox, BARE_BROWN2)

    # Left wall section (going from bottom-left toward center)
    wall_color = LIGHTER_GREY
    wall_dark = MED_GREY
    wall_shadow = DARK_GREY

    # Draw an L-shaped broken wall remnant
    # Bottom wall segment
    for x in range(14, 36):
        for y_off in range(0, 4):
            y = 26 - y_off
            if in_diamond(x, y, hw=28, hh=13):
                pixels[ox + x, y] = wall_color if y_off < 3 else wall_dark
    # Top edge highlight
    for x in range(14, 36):
        y = 22
        if in_diamond(x, y, hw=28, hh=13):
            pixels[ox + x, y] = WHITE_GREY

    # Left wall going up (partial - broken)
    for y in range(10, 26):
        for x_off in range(0, 3):
            x = 14 + x_off
            if in_diamond(x, y, hw=28, hh=13):
                pixels[ox + x, y] = wall_color if x_off < 2 else wall_dark

    # Jagged top on left wall (broken edge)
    broken_heights = [10, 9, 11, 10, 12]
    for i, bh in enumerate(broken_heights):
        x = 14 + i % 3
        if bh < 13 and in_diamond(x, bh, hw=28, hh=13):
            pixels[ox + x, bh] = wall_dark

    # Rubble at base
    for _ in range(8):
        sx = rng.randint(16, 40)
        sy = rng.randint(27, 32)
        if in_diamond(sx, sy, hw=28, hh=13):
            pixels[ox + sx, sy] = MED_GREY

def draw_crater(pixels, ox):
    """Draw an impact crater."""
    draw_ground(pixels, ox, BARE_BROWN2)

    cx, cy = 31, 18  # center of crater
    outer_r = 10
    inner_r = 6

    for y in range(H):
        for x in range(W):
            if not in_diamond(x, y, hw=28, hh=13):
                continue
            # Distance from center (squished for isometric)
            dx = (x - cx)
            dy = (y - cy) * 2  # stretch Y for isometric
            dist = math.sqrt(dx*dx + dy*dy)

            if dist < inner_r:
                # Inner crater - dark
                pixels[ox + x, y] = DARK_BROWN
            elif dist < inner_r + 2:
                # Crater rim shadow
                pixels[ox + x, y] = MED_BROWN
            elif dist < outer_r:
                # Raised rim
                if y < cy:
                    pixels[ox + x, y] = LIGHT_TAN  # lit side
                else:
                    pixels[ox + x, y] = BROWN  # shadow side

def draw_foundation_ruins(pixels, ox):
    """Draw abandoned building foundations (concrete outline)."""
    draw_ground(pixels, ox, BARE_BROWN2)

    # Draw a rectangular foundation outline
    fx, fy = 12, 8   # top-left of foundation
    fw, fh = 38, 22  # width, height

    for x in range(fx, fx + fw):
        for y in range(fy, fy + fh):
            if not in_diamond(x, y, hw=28, hh=13):
                continue
            # Foundation border (2px wide)
            on_border = (x < fx + 2 or x >= fx + fw - 2 or y < fy + 2 or y >= fy + fh - 2)
            if on_border:
                pixels[ox + x, y] = LIGHTER_GREY
            elif (x == fx + 2 or y == fy + 2):
                pixels[ox + x, y] = MED_GREY  # inner edge shadow

    # Some broken sections (gaps in the wall)
    for x in range(24, 30):
        for y in range(fy, fy + 2):
            if in_diamond(x, y, hw=28, hh=13):
                pixels[ox + x, y] = BARE_BROWN2

    # Rubble inside
    rubble_spots = [(20, 16), (30, 20), (35, 14), (25, 24), (18, 22)]
    for rx, ry in rubble_spots:
        if in_diamond(rx, ry, hw=28, hh=13):
            pixels[ox + rx, ry] = MED_GREY
            if rx + 1 < W:
                pixels[ox + rx + 1, ry] = LIGHT_GREY

def draw_scorched_earth(pixels, ox, seed=77):
    """Draw scorched/burnt ground with charred remains."""
    import random
    rng = random.Random(seed)

    # Very dark ground base
    draw_ground(pixels, ox, DARK_BROWN)

    # Scattered dark patches (char marks)
    for _ in range(50):
        sx = rng.randint(6, W - 7)
        sy = rng.randint(3, H - 4)
        if in_diamond(sx, sy, hw=27, hh=12):
            pixels[ox + sx, sy] = DARK_GREY

    # Some orange/red embers
    embers = [(22, 15), (35, 20), (28, 12), (40, 18), (18, 22), (32, 25)]
    for ex, ey in embers:
        if in_diamond(ex, ey, hw=27, hh=12):
            pixels[ox + ex, ey] = ORANGE
            # Glow around ember
            for dx, dy in [(-1,0),(1,0),(0,-1),(0,1)]:
                nx, ny = ex + dx, ey + dy
                if in_diamond(nx, ny, hw=27, hh=12):
                    pixels[ox + nx, ny] = RED if rng.random() > 0.5 else DARK_RED

    # Charred wooden beams
    for x in range(20, 42):
        y = 17
        if in_diamond(x, y, hw=27, hh=12):
            pixels[ox + x, y] = DARK_GREY
    for x in range(15, 28):
        y = 23
        if in_diamond(x, y, hw=27, hh=12):
            pixels[ox + x, y] = DARK_GREY

def draw_overgrown_ruins(pixels, ox, seed=99):
    """Draw ruins being reclaimed by nature — stone with green patches."""
    import random
    rng = random.Random(seed)

    # Mix of bare ground and grass
    for y in range(H):
        for x in range(W):
            if in_diamond(x, y):
                pixels[ox + x, y] = GREEN_DARK if rng.random() > 0.4 else BARE_BROWN2

    # Some green grass patches
    for _ in range(25):
        sx = rng.randint(6, W - 7)
        sy = rng.randint(3, H - 4)
        if in_diamond(sx, sy, hw=28, hh=13):
            pixels[ox + sx, sy] = GREEN

    # Stone wall remnants poking through
    walls = [
        (18, 10, 3, 8),   # left pillar
        (40, 12, 3, 7),   # right pillar
        (18, 10, 25, 2),  # connecting top wall
    ]
    for wx, wy, ww, wh in walls:
        for dy in range(wh):
            for dx in range(ww):
                px, py = wx + dx, wy + dy
                if 0 <= px < W and 0 <= py < H and in_diamond(px, py, hw=28, hh=13):
                    pixels[ox + px, py] = LIGHTER_GREY if dy == 0 else MED_GREY

    # Vines/moss on walls (green spots on grey)
    for _ in range(8):
        vx = rng.randint(18, 43)
        vy = rng.randint(10, 20)
        if 0 <= vx < W and 0 <= vy < H and pixels[ox + vx, vy] in (LIGHTER_GREY, MED_GREY):
            pixels[ox + vx, vy] = GREEN


def main():
    out_dir = os.path.dirname(os.path.abspath(__file__))

    # Create sprite sheet: NUM_SPRITES side by side, each 64x40
    sheet_w = W * NUM_SPRITES
    sheet_h = H
    img = Image.new('P', (sheet_w, sheet_h), TRANSPARENT)
    img.putpalette(build_palette())

    pixels = img.load()

    # Generate each sprite
    draw_rubble_stones(pixels, 0 * W)      # Ruin 1: Rubble pile
    draw_broken_wall(pixels, 1 * W)         # Ruin 2: Broken wall
    draw_crater(pixels, 2 * W)              # Ruin 3: Crater
    draw_foundation_ruins(pixels, 3 * W)    # Ruin 4: Foundation
    draw_scorched_earth(pixels, 4 * W)      # Ruin 5: Scorched earth
    draw_overgrown_ruins(pixels, 5 * W)     # Ruin 6: Overgrown ruins

    # Save sprite sheet
    out_path = os.path.join(out_dir, "sprites.png")
    img.save(out_path)
    print(f"Saved sprite sheet: {out_path} ({sheet_w}x{sheet_h})")

    # Also save individual sprites for preview
    for i in range(NUM_SPRITES):
        individual = img.crop((i * W, 0, (i + 1) * W, H))
        individual.save(os.path.join(out_dir, f"ruin_{i+1}.png"))
        print(f"  Saved ruin_{i+1}.png")


if __name__ == "__main__":
    main()
