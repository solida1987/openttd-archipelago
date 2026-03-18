#!/usr/bin/env python3
"""Generate isometric star sprites for OpenTTD Archipelago mod.

Each sprite is a 64x31 8-bit indexed PNG using the OpenTTD DOS palette.
Index 0 = transparent.  Stars are drawn as BLACK OUTLINE ONLY (pen-drawn look).
The ground is handled by GROUNDSPRITE_NORMAL in the spritelayout.

Output: sprites.png  (sprite sheet with 3 star variants side-by-side)
"""

from PIL import Image, ImageDraw
import math, os

# ---------- OpenTTD DOS palette ----------
def _get_dos_palette():
    from nml.palette import raw_palette_data
    return list(raw_palette_data[0])

# Palette indices
TRANSPARENT = 0
NEAR_BLACK  = 1   # Very dark grey, looks like pen ink
DARK_GREY   = 2

# Sprite dimensions (1x1 tile building sprite)
W, H = 64, 31
NUM_SPRITES = 3

# Center of the sprite (where the star sits on the tile)
CX, CY = 31, 18


def build_palette():
    return _get_dos_palette()


def set_px(pixels, ox, x, y, color):
    ax = ox + x
    if 0 <= x < W and 0 <= y < H:
        pixels[ax, y] = color


def draw_line(pixels, ox, x0, y0, x1, y1, color):
    """Bresenham line drawing."""
    dx = abs(x1 - x0)
    dy = abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx - dy
    while True:
        set_px(pixels, ox, x0, y0, color)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 > -dy:
            err -= dy
            x0 += sx
        if e2 < dx:
            err += dx
            y0 += sy


def star_points(cx, cy, outer_r, inner_r, num_points=5, rotation=0.0):
    """Generate vertices of a star polygon."""
    points = []
    for i in range(num_points * 2):
        angle = rotation + i * math.pi / num_points - math.pi / 2
        r = outer_r if i % 2 == 0 else inner_r
        x = cx + r * math.cos(angle)
        y = cy + r * math.sin(angle)
        points.append((int(round(x)), int(round(y))))
    return points


def draw_star_outline(pixels, ox, cx, cy, outer_r, inner_r,
                      color=NEAR_BLACK, rotation=0.0, num_points=5):
    """Draw a star as outline only (pen-drawn look)."""
    pts = star_points(cx, cy, outer_r, inner_r, num_points, rotation)
    for i in range(len(pts)):
        x0, y0 = pts[i]
        x1, y1 = pts[(i + 1) % len(pts)]
        draw_line(pixels, ox, x0, y0, x1, y1, color)


# ================================================================
# STAR 1: Standard 5-pointed star outline
# ================================================================
def draw_star_1(pixels, ox):
    draw_star_outline(pixels, ox, CX, CY, outer_r=10, inner_r=4,
                      color=NEAR_BLACK, rotation=0.0)


# ================================================================
# STAR 2: Slightly rotated star (18 degrees)
# ================================================================
def draw_star_2(pixels, ox):
    draw_star_outline(pixels, ox, CX, CY, outer_r=9, inner_r=4,
                      color=NEAR_BLACK, rotation=math.radians(18))


# ================================================================
# STAR 3: Star with tiny sparkle dots around it
# ================================================================
def draw_star_3(pixels, ox):
    draw_star_outline(pixels, ox, CX, CY, outer_r=8, inner_r=3,
                      color=NEAR_BLACK, rotation=math.radians(-10))
    # Small sparkle dots around the star
    sparkle_offsets = [
        (-14, -4), (14, -2), (-12, 6), (13, 7),
        (0, -12), (-6, -10), (7, -9), (-3, 10), (5, 10),
    ]
    for sx, sy in sparkle_offsets:
        set_px(pixels, ox, CX + sx, CY + sy, NEAR_BLACK)


def main():
    out_dir = os.path.dirname(os.path.abspath(__file__))
    sheet_w = W * NUM_SPRITES
    sheet_h = H
    img = Image.new('P', (sheet_w, sheet_h), TRANSPARENT)
    img.putpalette(build_palette())
    pixels = img.load()

    draw_star_1(pixels, 0 * W)
    draw_star_2(pixels, 1 * W)
    draw_star_3(pixels, 2 * W)

    out_path = os.path.join(out_dir, "sprites.png")
    img.save(out_path)
    print(f"Saved sprite sheet: {out_path} ({sheet_w}x{sheet_h})")

    for i in range(NUM_SPRITES):
        individual = img.crop((i * W, 0, (i + 1) * W, H))
        individual.save(os.path.join(out_dir, f"star_{i+1}.png"))
        print(f"  Saved star_{i+1}.png")


if __name__ == "__main__":
    main()
