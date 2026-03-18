#!/usr/bin/env python3
"""Generate isometric ruin sprites for OpenTTD Archipelago mod.

Each sprite is a 64x80 8-bit indexed PNG using the OpenTTD DOS palette.
Index 0 = transparent.  Only VERTICAL structures are drawn — the ground
is handled by GROUNDSPRITE_NORMAL in the spritelayout.

Output: sprites.png  (sprite sheet with all ruins side-by-side)
"""

from PIL import Image, ImageDraw
import math, os, random

# ---------- OpenTTD DOS palette (extracted from nml) ----------
def _get_dos_palette():
    """Get the exact DOS palette from the nml package."""
    from nml.palette import raw_palette_data
    return list(raw_palette_data[0])  # index 0 = DOS palette

# --- Useful palette indices (DOS palette) ---
TRANSPARENT = 0
DARK_GREY   = 1
MED_GREY    = 4
LIGHT_GREY  = 6
LIGHTER_GREY= 9
WHITE_GREY  = 11
WHITE       = 15
DARK_BROWN  = 104
MED_BROWN   = 106
BROWN       = 107
LIGHT_BROWN = 108
TAN         = 110
LIGHT_TAN   = 111
SAND        = 35
DARK_RED    = 178
RED         = 162
LIGHT_RED   = 163
ORANGE      = 185
YELLOW      = 191
OLIVE_DARK  = 24
OLIVE       = 25
BARE_BROWN  = 32
BARE_BROWN2 = 33
BARE_TAN    = 34
GREEN_DARK  = 80
GREEN       = 82
GREEN_MED   = 84
GREEN_LIGHT = 86
STEEL_DARK  = 128
STEEL       = 130
STEEL_LIGHT = 132

# Sprite dimensions
W, H = 64, 80
NUM_SPRITES = 7  # 6 main ruins + 1 shared debris for non-center tiles

# Ground plane reference: where the isometric tile surface sits
# The north corner of the tile diamond is at approximately (31, 49)
# The tile center (visually) is at approximately (31, 64)
# Building sprites are anchored so yoffs=-49 puts the tile surface at y=49 in sprite
TILE_SURFACE_Y = 49  # where the top of the ground tile is in our sprite


def build_palette():
    return _get_dos_palette()


def set_px(pixels, ox, x, y, color):
    """Set a pixel, bounds-checked."""
    ax = ox + x
    if 0 <= x < W and 0 <= y < H:
        pixels[ax, y] = color


def draw_rect(pixels, ox, x, y, w, h, color):
    """Draw a filled rectangle. y is the BOTTOM, drawing upward."""
    for dy in range(h):
        for dx in range(w):
            set_px(pixels, ox, x + dx, y - dy, color)


def draw_rect_shaded(pixels, ox, x, y, w, h, face_col, left_col, right_col, top_col):
    """Draw a shaded rectangular block (isometric-ish). y = bottom, draws upward."""
    for dy in range(h):
        for dx in range(w):
            if dy == h - 1:
                col = top_col
            elif dx == 0:
                col = left_col
            elif dx == w - 1:
                col = right_col
            else:
                col = face_col
            set_px(pixels, ox, x + dx, y - dy, col)


def draw_pillar(pixels, ox, cx, base_y, width, height, face, edge, top):
    """Draw a vertical pillar centered at cx."""
    x = cx - width // 2
    draw_rect_shaded(pixels, ox, x, base_y, width, height, face, edge, edge, top)


def draw_wall(pixels, ox, x, base_y, w, h, face, edge, top, jagged_seed=None):
    """Draw a wall with optional jagged top."""
    draw_rect_shaded(pixels, ox, x, base_y, w, h, face, edge, edge, top)
    if jagged_seed is not None:
        rng = random.Random(jagged_seed)
        for dx in range(w):
            cut = rng.randint(1, 5)
            for c in range(cut):
                set_px(pixels, ox, x + dx, base_y - h + 1 + c, TRANSPARENT)


# ================================================================
# RUIN 1: Rubble Pile — heap of stone blocks
# ================================================================
def draw_rubble(pixels, ox):
    base = TILE_SURFACE_Y + 4  # slightly below tile surface for grounding

    # Large stacked stone blocks
    draw_rect_shaded(pixels, ox, 20, base, 12, 20, LIGHTER_GREY, MED_GREY, STEEL_DARK, WHITE_GREY)
    draw_rect_shaded(pixels, ox, 30, base, 14, 28, LIGHT_GREY, STEEL_DARK, MED_GREY, WHITE_GREY)
    draw_rect_shaded(pixels, ox, 24, base - 5, 8, 12, WHITE_GREY, LIGHTER_GREY, MED_GREY, WHITE)
    draw_rect_shaded(pixels, ox, 38, base - 2, 8, 16, LIGHTER_GREY, MED_GREY, STEEL_DARK, WHITE_GREY)
    draw_rect_shaded(pixels, ox, 33, base - 14, 10, 10, WHITE_GREY, LIGHTER_GREY, MED_GREY, WHITE)
    draw_rect_shaded(pixels, ox, 15, base + 2, 6, 10, MED_GREY, STEEL_DARK, DARK_GREY, LIGHTER_GREY)
    draw_rect_shaded(pixels, ox, 44, base, 5, 8, LIGHTER_GREY, MED_GREY, STEEL_DARK, WHITE_GREY)


# ================================================================
# RUIN 2: Broken Wall — tall wall with gap
# ================================================================
def draw_broken_wall(pixels, ox):
    base = TILE_SURFACE_Y + 6

    # Left wall — tall
    draw_wall(pixels, ox, 16, base, 7, 42, LIGHTER_GREY, STEEL_DARK, WHITE_GREY, jagged_seed=101)
    # Right wall — shorter
    draw_wall(pixels, ox, 38, base, 6, 30, LIGHTER_GREY, STEEL_DARK, WHITE_GREY, jagged_seed=102)
    # Low connecting wall on left side
    draw_rect_shaded(pixels, ox, 23, base, 8, 12, LIGHT_GREY, STEEL_DARK, MED_GREY, LIGHTER_GREY)
    # Rubble in the gap
    draw_rect_shaded(pixels, ox, 31, base + 2, 7, 8, MED_GREY, DARK_GREY, STEEL_DARK, LIGHTER_GREY)


# ================================================================
# RUIN 3: Crumbling Tower — cylindrical tower remnant
# ================================================================
def draw_tower(pixels, ox):
    base = TILE_SURFACE_Y + 5
    tower_cx = 31
    tower_height = 48
    base_radius = 10
    top_radius = 7

    for dy in range(tower_height):
        y = base - dy
        if y < 0:
            break
        t = dy / tower_height
        r = base_radius + (top_radius - base_radius) * t
        for dx in range(-int(r) - 1, int(r) + 2):
            x = tower_cx + dx
            dist = abs(dx) / r if r > 0 else 1
            if dist <= 1.0:
                if dist > 0.82:
                    col = STEEL_DARK
                elif dist > 0.6:
                    col = MED_GREY
                elif dx < 0:
                    col = LIGHTER_GREY
                else:
                    col = LIGHT_GREY
                set_px(pixels, ox, x, y, col)

    # Jagged broken top
    rng = random.Random(55)
    for dx in range(-int(top_radius), int(top_radius) + 1):
        cut = rng.randint(1, 8)
        x = tower_cx + dx
        for c in range(cut):
            set_px(pixels, ox, x, base - tower_height + 1 + c, TRANSPARENT)

    # Top highlight ring
    for dx in range(-int(top_radius) + 1, int(top_radius)):
        set_px(pixels, ox, tower_cx + dx, base - tower_height + 9, WHITE_GREY)

    # Small rubble at base
    draw_rect_shaded(pixels, ox, 19, base + 3, 5, 5, MED_GREY, DARK_GREY, STEEL_DARK, LIGHTER_GREY)
    draw_rect_shaded(pixels, ox, 40, base + 2, 4, 4, MED_GREY, DARK_GREY, STEEL_DARK, LIGHTER_GREY)


# ================================================================
# RUIN 4: Foundation with Pillars — tall columns
# ================================================================
def draw_foundation(pixels, ox):
    base = TILE_SURFACE_Y + 5

    # Four tall pillars
    draw_pillar(pixels, ox, 20, base, 5, 38, LIGHTER_GREY, STEEL_DARK, WHITE_GREY)
    draw_pillar(pixels, ox, 43, base, 5, 32, LIGHTER_GREY, STEEL_DARK, WHITE_GREY)
    draw_pillar(pixels, ox, 20, base + 10, 5, 24, LIGHT_GREY, MED_GREY, WHITE_GREY)
    draw_pillar(pixels, ox, 43, base + 10, 5, 30, LIGHT_GREY, MED_GREY, WHITE_GREY)

    # Broken beam across top two pillars
    draw_rect_shaded(pixels, ox, 22, base - 32, 19, 3, BROWN, DARK_BROWN, MED_BROWN, LIGHT_BROWN)

    # Low wall between bottom pillars
    draw_rect_shaded(pixels, ox, 22, base + 8, 19, 8, MED_GREY, STEEL_DARK, DARK_GREY, LIGHTER_GREY)


# ================================================================
# RUIN 5: Scorched Earth — burnt timber + flames
# ================================================================
def draw_scorched(pixels, ox):
    base = TILE_SURFACE_Y + 5

    # Tall charred timber posts
    posts = [(18, 0, 4, 40), (28, -2, 3, 30), (36, 0, 4, 45), (46, 2, 3, 22)]
    for px, yoff, pw, ph in posts:
        y = base + yoff
        draw_rect_shaded(pixels, ox, px, y, pw, ph, DARK_GREY, 1, MED_GREY, MED_GREY)
        # Charred top
        for dx in range(pw):
            set_px(pixels, ox, px + dx, y - ph + 1, 1)
            set_px(pixels, ox, px + dx, y - ph + 2, DARK_GREY)

    # Orange/red ember glow at base
    embers = [(18, base + 2), (36, base + 2), (28, base)]
    for ex, ey in embers:
        for dx in range(-2, 5):
            for dy in range(-2, 2):
                nx, ny = ex + dx, ey + dy
                d = abs(dx - 1) + abs(dy)
                if d < 2:
                    set_px(pixels, ox, nx, ny, YELLOW)
                elif d < 3:
                    set_px(pixels, ox, nx, ny, ORANGE)
                elif d < 4:
                    set_px(pixels, ox, nx, ny, RED)

    # Smoke wisps above posts
    rng = random.Random(77)
    for px, yoff, pw, ph in posts:
        for i in range(5):
            sx = px + pw // 2 + rng.randint(-3, 3)
            sy = base + yoff - ph - 3 - i * 3
            set_px(pixels, ox, sx, sy, LIGHTER_GREY)
            if rng.random() > 0.5:
                set_px(pixels, ox, sx + 1, sy, LIGHT_GREY)


# ================================================================
# RUIN 6: Overgrown Ruins — stone arch with vines
# ================================================================
def draw_overgrown(pixels, ox):
    base = TILE_SURFACE_Y + 5

    # Two tall stone pillars
    draw_pillar(pixels, ox, 19, base, 6, 42, LIGHTER_GREY, STEEL_DARK, WHITE_GREY)
    draw_pillar(pixels, ox, 39, base, 6, 40, LIGHTER_GREY, STEEL_DARK, WHITE_GREY)

    # Stone arch connecting at top
    arch_top = base - 38
    for x in range(22, 39):
        curve = int(3 * math.sin((x - 22) / 17.0 * math.pi))
        for t in range(4):
            y = arch_top + curve + t
            col = LIGHTER_GREY if t < 2 else MED_GREY if t < 3 else STEEL_DARK
            set_px(pixels, ox, x, y, col)

    # Hanging vines from arch
    rng = random.Random(99)
    vine_positions = [24, 27, 30, 33, 36]
    for vx in vine_positions:
        vine_len = rng.randint(8, 20)
        start_y = arch_top + 5
        for dy in range(vine_len):
            y = start_y + dy
            x = vx + (1 if dy % 4 == 0 else (-1 if dy % 4 == 2 else 0))
            col = GREEN if rng.random() > 0.3 else GREEN_MED
            set_px(pixels, ox, x, y, col)
            if rng.random() > 0.7:  # leaf cluster
                set_px(pixels, ox, x - 1, y, GREEN_DARK)
                set_px(pixels, ox, x + 1, y, GREEN_LIGHT)

    # Moss patches on pillars
    for pcx in [19, 39]:
        for _ in range(10):
            mx = pcx - 3 + rng.randint(0, 5)
            my = rng.randint(base - 35, base - 5)
            if 0 <= ox + mx < ox + W and 0 <= my < H:
                if pixels[ox + mx, my] != TRANSPARENT:
                    set_px(pixels, ox, mx, my, GREEN_DARK)

    # Low wall between pillars at base
    draw_rect_shaded(pixels, ox, 22, base + 2, 17, 6, MED_GREY, STEEL_DARK, DARK_GREY, LIGHTER_GREY)


# ================================================================
# SPRITE 7: Scattered debris — shared sprite for non-center tiles
# ================================================================
def draw_debris(pixels, ox):
    """Small scattered rubble pieces for the 8 surrounding tiles of a 3x3 ruin."""
    base = TILE_SURFACE_Y + 4
    rng = random.Random(333)

    # Several small stone blocks scattered around
    debris_blocks = [
        (12, base + 3, 5, 6, LIGHTER_GREY, MED_GREY),
        (24, base + 1, 6, 8, LIGHT_GREY, STEEL_DARK),
        (36, base + 2, 5, 5, MED_GREY, DARK_GREY),
        (44, base + 4, 4, 4, LIGHTER_GREY, STEEL_DARK),
        (18, base - 1, 4, 5, WHITE_GREY, MED_GREY),
        (40, base, 3, 6, LIGHT_GREY, DARK_GREY),
        (30, base + 3, 7, 4, MED_GREY, STEEL_DARK),
    ]
    for bx, by, bw, bh, face, edge in debris_blocks:
        draw_rect_shaded(pixels, ox, bx, by, bw, bh, face, edge, edge, WHITE_GREY)

    # Tiny scattered stone pixels
    for _ in range(20):
        sx = rng.randint(8, 54)
        sy = rng.randint(base - 2, base + 8)
        col = rng.choice([LIGHTER_GREY, MED_GREY, LIGHT_GREY, STEEL_DARK])
        set_px(pixels, ox, sx, sy, col)
        set_px(pixels, ox, sx + 1, sy, col)


def main():
    out_dir = os.path.dirname(os.path.abspath(__file__))
    sheet_w = W * NUM_SPRITES
    sheet_h = H
    img = Image.new('P', (sheet_w, sheet_h), TRANSPARENT)
    img.putpalette(build_palette())
    pixels = img.load()

    draw_rubble(pixels, 0 * W)
    draw_broken_wall(pixels, 1 * W)
    draw_tower(pixels, 2 * W)
    draw_foundation(pixels, 3 * W)
    draw_scorched(pixels, 4 * W)
    draw_overgrown(pixels, 5 * W)
    draw_debris(pixels, 6 * W)

    out_path = os.path.join(out_dir, "sprites.png")
    img.save(out_path)
    print(f"Saved sprite sheet: {out_path} ({sheet_w}x{sheet_h})")

    for i in range(NUM_SPRITES):
        individual = img.crop((i * W, 0, (i + 1) * W, H))
        individual.save(os.path.join(out_dir, f"ruin_{i+1}.png"))
        print(f"  Saved ruin_{i+1}.png")


if __name__ == "__main__":
    main()
