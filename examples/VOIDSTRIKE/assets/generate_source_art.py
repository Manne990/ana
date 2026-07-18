#!/usr/bin/env python3
"""Generate the original indexed PPM source art for VOIDSTRIKE.

The source is deliberately tiny, deterministic pixel art rather than derived
from reference imagery.  Run this script after changing a shape; the committed
PPM files are the asset-pipeline inputs consumed by assets.ana.
"""

from pathlib import Path

OUT = Path(__file__).parent
P = [
    (0, 0, 0), (17, 17, 34), (34, 34, 51), (51, 68, 85),
    (85, 102, 119), (119, 136, 153), (170, 187, 204), (221, 238, 255),
    (0, 51, 102), (0, 85, 170), (0, 170, 221), (17, 102, 51),
    (68, 221, 119), (255, 170, 34), (255, 221, 68), (221, 51, 68),
]
K = 0


def image(w, h, fill=K):
    return [[fill for _ in range(w)] for _ in range(h)]


def rect(a, x, y, w, h, c):
    for yy in range(max(0, y), min(len(a), y + h)):
        for xx in range(max(0, x), min(len(a[0]), x + w)):
            a[yy][xx] = c


def px(a, x, y, c):
    if 0 <= y < len(a) and 0 <= x < len(a[0]):
        a[y][x] = c


def write(name, a):
    h, w = len(a), len(a[0])
    with (OUT / name).open("w", encoding="ascii") as f:
        f.write(f"P3\n{w} {h}\n255\n")
        for row in a:
            f.write(" ".join("%d %d %d" % P[v] for v in row) + "\n")


def palette():
    write("palette.ppm", [list(range(16))])


def player(name, modules=()):
    a = image(36, 28)
    # Narrow asymmetric spine, cockpit, stabilisers, and rear drive.
    for y, left, width, c in [(2, 16, 4, 6), (3, 15, 6, 7), (4, 14, 8, 6),
                              (5, 14, 8, 9), (6, 13, 10, 5), (7, 13, 10, 5),
                              (8, 12, 12, 3), (9, 12, 12, 6), (10, 12, 12, 4),
                              (11, 11, 14, 4), (12, 11, 14, 3), (13, 10, 16, 3),
                              (14, 10, 16, 4), (15, 9, 18, 3), (16, 9, 18, 4),
                              (17, 10, 16, 4), (18, 11, 14, 3), (19, 12, 12, 2),
                              (20, 13, 10, 9), (21, 14, 8, 10), (22, 15, 6, 8)]:
        rect(a, left, y, width, 1, c)
    rect(a, 16, 7, 4, 5, 7)
    rect(a, 17, 8, 2, 3, 10)
    rect(a, 13, 16, 2, 5, 6)
    rect(a, 21, 16, 2, 5, 6)
    if "speed" in modules:
        rect(a, 15, 23, 2, 3, 10); rect(a, 19, 23, 2, 3, 10); rect(a, 16, 26, 4, 1, 12)
    if "twin" in modules:
        rect(a, 6, 10, 6, 4, 5); rect(a, 24, 10, 6, 4, 5)
        rect(a, 7, 8, 2, 3, 10); rect(a, 27, 8, 2, 3, 10)
    if "wide" in modules:
        rect(a, 3, 14, 8, 4, 5); rect(a, 25, 14, 8, 4, 5)
        rect(a, 2, 12, 3, 3, 10); rect(a, 31, 12, 3, 3, 10)
    if "laser" in modules:
        rect(a, 16, 0, 4, 5, 10); rect(a, 17, 0, 2, 2, 7)
    write(name, a)


def enemies():
    a = image(24, 20); rect(a, 3, 8, 18, 9, 3); rect(a, 5, 5, 14, 12, 4)
    rect(a, 7, 3, 10, 4, 5); rect(a, 10, 7, 4, 4, 1); rect(a, 11, 8, 2, 2, 13)
    rect(a, 2, 16, 20, 2, 2); write("defense_node.ppm", a)
    a = image(28, 16); rect(a, 2, 7, 24, 6, 3); rect(a, 5, 4, 18, 8, 4)
    rect(a, 8, 5, 5, 3, 6); rect(a, 16, 5, 5, 3, 13)
    for x in (5, 10, 17, 22): rect(a, x, 13, 3, 2, 2)
    write("maintenance_crawler.ppm", a)
    a = image(60, 20)
    for ox, bank in ((0, -1), (20, 0), (40, 1)):
        rect(a, ox + 8 + bank, 5, 4, 10, 3); rect(a, ox + 5 + bank, 8, 10, 5, 4)
        rect(a, ox + 3 + bank, 10, 3, 2, 2); rect(a, ox + 14 + bank, 10, 3, 2, 2)
        rect(a, ox + 9 + bank, 9, 2, 2, 15)
    write("security_drone.ppm", a)


def combat():
    a = image(8, 12); rect(a, 3, 1, 2, 9, 7); rect(a, 2, 3, 4, 5, 10); write("player_shot.ppm", a)
    a = image(8, 8); rect(a, 2, 1, 4, 6, 15); rect(a, 1, 2, 6, 4, 13); rect(a, 3, 3, 2, 2, 7); write("hostile_shot.ppm", a)
    a = image(16, 16)
    for x, y, c in [(7,2,14),(5,4,12),(9,4,12),(3,6,10),(11,6,10),(5,8,14),(9,8,14),(7,10,12),(7,6,7)]:
        rect(a, x, y, 2, 2, c)
    write("energy_core.ppm", a)
    a = image(96, 16)
    for frame in range(6):
        ox = frame * 16; radius = frame + 2
        for y in range(16):
            for x in range(16):
                d = abs(x - 7) + abs(y - 7)
                if d < radius: px(a, ox + x, y, 7 if d < 2 else (14 if d < radius - 1 else 13))
    write("explosion.ppm", a)


def boss():
    a = image(112, 56)
    rect(a, 8, 12, 96, 36, 2); rect(a, 14, 8, 30, 44, 3); rect(a, 70, 6, 28, 46, 3)
    rect(a, 20, 14, 18, 30, 4); rect(a, 76, 12, 16, 32, 4)
    rect(a, 46, 4, 20, 48, 5); rect(a, 50, 8, 12, 40, 6); rect(a, 53, 16, 6, 18, 10)
    rect(a, 54, 20, 4, 10, 7); rect(a, 6, 20, 10, 6, 13); rect(a, 96, 24, 10, 6, 13)
    rect(a, 28, 18, 4, 4, 15); rect(a, 82, 20, 4, 4, 15)
    write("reactor_guardian.ppm", a)


def terrain():
    a = image(192, 16, 1)
    for tile in range(12):
        ox = tile * 16; rect(a, ox, 0, 16, 16, 2)
        rect(a, ox + 1, 1, 14, 14, 3)
        if tile in (0, 7): rect(a, ox + 3, 3, 10, 10, 4)
        if tile in (1, 4, 8): rect(a, ox, 6, 16, 4, 8); rect(a, ox, 7, 16, 2, 10)
        if tile in (2, 5, 9): rect(a, ox + 6, 0, 4, 16, 8); rect(a, ox + 7, 0, 2, 16, 10)
        if tile == 3: rect(a, ox, 6, 16, 4, 8); rect(a, ox+6, 0, 4, 16, 8); rect(a, ox+7, 7, 2, 2, 10)
        if tile == 6:
            for x in (3, 7, 11): rect(a, ox+x, 3, 2, 10, 5)
        if tile == 10: rect(a, ox, 12, 16, 4, 13); rect(a, ox, 11, 16, 1, 14)
        if tile == 11: rect(a, ox+3, 3, 10, 10, 1); rect(a, ox+4, 4, 8, 8, 2); rect(a, ox+6, 6, 4, 4, 13)
    write("null_foundry_tiles.ppm", a)


def hud_and_title():
    a = image(128, 16, 1)
    for i in range(4):
        x = i * 32; rect(a, x+1, 1, 30, 14, 3); rect(a, x+2, 2, 28, 12, 2)
        if i == 0: rect(a, x+14, 4, 4, 8, 10); rect(a, x+12, 10, 8, 2, 10)
        if i == 1: rect(a, x+8, 4, 4, 7, 10); rect(a, x+20, 4, 4, 7, 10)
        if i == 2: rect(a, x+6, 5, 5, 6, 10); rect(a, x+21, 5, 5, 6, 10)
        if i == 3: rect(a, x+14, 3, 4, 10, 10); rect(a, x+12, 3, 8, 2, 7)
    write("module_dock.ppm", a)
    a = image(192, 40, 1)
    # Geometric original wordmark: VOID / STRIKE with a cyan reactor slash.
    glyphs = {"V":["10001","10001","01010","01010","00100"], "O":["01110","10001","10001","10001","01110"], "I":["11111","00100","00100","00100","11111"], "D":["11110","10001","10001","10001","11110"], "S":["01111","10000","01110","00001","11110"], "T":["11111","00100","00100","00100","00100"], "R":["11110","10001","11110","10100","10010"], "K":["10001","10010","11100","10010","10001"], "E":["11111","10000","11110","10000","11111"]}
    x = 12
    for ch in "VOID":
        for yy, row in enumerate(glyphs[ch]):
            for xx, bit in enumerate(row):
                if bit == "1": rect(a, x + xx*2, 7 + yy*4, 2, 4, 7)
        x += 14
    rect(a, 72, 3, 4, 34, 10)
    x = 88
    for ch in "STRIKE":
        for yy, row in enumerate(glyphs[ch]):
            for xx, bit in enumerate(row):
                if bit == "1": rect(a, x + xx*2, 7 + yy*4, 2, 4, 6)
        x += 14
    write("title_wordmark.ppm", a)


def presentation_board():
    """A 320x256 native-resolution readability board, not a runtime asset."""
    a = image(320, 256, 1)
    # Top status band and lower module dock area.
    rect(a, 0, 0, 320, 18, 2); rect(a, 0, 220, 320, 36, 2)
    rect(a, 8, 6, 54, 4, 6); rect(a, 266, 6, 38, 4, 6)
    # Broad industrial plates and blue conduit regions: calm central lane.
    for y in range(24, 216, 32):
        rect(a, 0, y, 82, 28, 3); rect(a, 238, y + 8, 82, 24, 3)
        rect(a, 4, y + 4, 70, 20, 4); rect(a, 246, y + 12, 70, 16, 4)
        rect(a, 78, y + 10, 10, 4, 8); rect(a, 232, y + 18, 10, 4, 8)
        rect(a, 80, y + 11, 6, 2, 10); rect(a, 234, y + 19, 6, 2, 10)
    # Reactor guardian at the top, with a readable cyan core and amber ports.
    rect(a, 108, 26, 104, 30, 2); rect(a, 116, 22, 28, 40, 3)
    rect(a, 176, 20, 28, 42, 3); rect(a, 148, 18, 24, 48, 5)
    rect(a, 156, 28, 8, 20, 10); rect(a, 158, 32, 4, 12, 7)
    rect(a, 103, 38, 14, 6, 13); rect(a, 203, 40, 14, 6, 13)
    # A player craft whose modules visibly widen the silhouette.
    rect(a, 151, 152, 18, 26, 4); rect(a, 156, 144, 8, 18, 6)
    rect(a, 158, 148, 4, 8, 10); rect(a, 128, 162, 26, 10, 5)
    rect(a, 166, 162, 26, 10, 5); rect(a, 126, 158, 8, 5, 10)
    rect(a, 186, 158, 8, 5, 10); rect(a, 156, 178, 8, 10, 10)
    # Distinct hostile drone, red bolt, and green energy core.
    rect(a, 98, 104, 14, 14, 3); rect(a, 94, 108, 22, 6, 4); rect(a, 103, 109, 4, 4, 15)
    rect(a, 214, 116, 6, 10, 15); rect(a, 212, 118, 10, 6, 13)
    rect(a, 210, 174, 6, 6, 12); rect(a, 207, 177, 12, 6, 10); rect(a, 211, 181, 4, 5, 14)
    # Four original bottom module cells with cyan selected outline and green installed state.
    for i in range(4):
        x = 88 + i * 36; rect(a, x, 228, 30, 18, 4 if i != 2 else 10)
        rect(a, x + 2, 230, 26, 14, 2)
        if i in (0, 1, 3): rect(a, x + 11, 235, 8, 4, 12)
    # A compact title treatment placed in the status band with no reference typography.
    rect(a, 122, 5, 18, 4, 7); rect(a, 122, 9, 4, 5, 7); rect(a, 136, 9, 4, 5, 7)
    rect(a, 144, 5, 4, 10, 10); rect(a, 152, 5, 18, 4, 6); rect(a, 152, 11, 18, 4, 6)
    rect(a, 174, 5, 18, 4, 7); rect(a, 174, 11, 18, 4, 7); rect(a, 174, 5, 4, 10, 7)
    write("presentation_board.ppm", a)


def main():
    palette(); player("player_base.ppm"); player("player_speed.ppm", ("speed",))
    player("player_twin.ppm", ("twin",)); player("player_wide.ppm", ("wide",))
    player("player_laser.ppm", ("laser",)); enemies(); combat(); boss(); terrain(); hud_and_title(); presentation_board()


if __name__ == "__main__":
    main()
