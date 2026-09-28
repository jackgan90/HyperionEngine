"""Generate the deterministic Sky Light editor icon PNG."""

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

SIZE = 1254
SUPERSAMPLE = 2
BIG = SIZE * SUPERSAMPLE
OUTPUT = Path(__file__).resolve().parent.parent / "Content/Editor/Icons/SkyLight.png"

ROWS, COLUMNS = np.mgrid[0:BIG, 0:BIG].astype(np.float32) / SUPERSAMPLE


def scaled(value):
    return int(round(value * SUPERSAMPLE))


def draw_dome(draw, grow):
    center_x, center_y, radius = 627, 690, 440
    draw.pieslice(
        [scaled(center_x - radius - grow), scaled(center_y - radius - grow),
         scaled(center_x + radius + grow), scaled(center_y + radius + grow)],
        180, 360, fill=255)
    draw.rounded_rectangle(
        [scaled(150 - grow), scaled(center_y - 1 - grow), scaled(1104 + grow), scaled(790 + grow)],
        radius=scaled(48 + grow), fill=255)


def draw_cloud(draw, grow):
    for center_x, center_y, radius in ((730, 800, 150), (900, 860, 115), (575, 880, 110)):
        draw.ellipse(
            [scaled(center_x - radius - grow), scaled(center_y - radius - grow),
             scaled(center_x + radius + grow), scaled(center_y + radius + grow)],
            fill=255)
    draw.rounded_rectangle(
        [scaled(465 - grow), scaled(860 - grow), scaled(1015 + grow), scaled(995 + grow)],
        radius=scaled(68 + grow), fill=255)


def shape_mask(draw_shape, grow):
    image = Image.new("L", (BIG, BIG), 0)
    draw_shape(ImageDraw.Draw(image), grow)
    return np.asarray(image, dtype=np.float32)[..., None] / 255.0


def radial(center_x, center_y, radius, inner, outer):
    blend = np.clip(np.hypot(COLUMNS - center_x, ROWS - center_y) / radius, 0, 1)[..., None]
    return np.array(inner, np.float32) * (1 - blend) + np.array(outer, np.float32) * blend


def vertical(top_row, bottom_row, top, bottom):
    blend = np.clip((ROWS - top_row) / (bottom_row - top_row), 0, 1)[..., None]
    return np.array(top, np.float32) * (1 - blend) + np.array(bottom, np.float32) * blend


def main():
    color = np.zeros((BIG, BIG, 3), np.float32)
    alpha = np.zeros((BIG, BIG, 1), np.float32)

    def composite(mask, layer_color):
        nonlocal color, alpha
        color = color * (1 - mask) + layer_color * mask
        alpha = np.maximum(alpha, mask)

    def outlined(draw_shape, fill):
        composite(shape_mask(draw_shape, 34), np.array([52, 30, 12], np.float32))
        composite(shape_mask(draw_shape, 24), vertical(200, 1000, (236, 184, 104), (122, 74, 30)))
        composite(shape_mask(draw_shape, 13), vertical(200, 1000, (150, 96, 40), (84, 50, 20)))
        composite(shape_mask(draw_shape, 0), fill)

    outlined(draw_dome, radial(560, 470, 560, (255, 253, 240), (248, 222, 140)))
    outlined(draw_cloud, radial(700, 800, 380, (255, 255, 250), (236, 226, 196)))
    pixels = np.concatenate([color, alpha * 255], axis=2).clip(0, 255).astype(np.uint8)
    Image.fromarray(pixels, "RGBA").resize((SIZE, SIZE), Image.LANCZOS).save(OUTPUT, optimize=True)


if __name__ == "__main__":
    main()
