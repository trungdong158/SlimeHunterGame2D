#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_levels.py - Tạo tài nguyên cho 5 LEVEL từ các ảnh gốc (được gọi tự động từ make_assets.py).

Nguồn:
  * tools/source/forest_source.jpg     - bản đồ rừng 1800x1200 (Level 1)
  * tools/source/backgrounds_sheet.png - bảng 15 phong cảnh (chỉ dùng ô "Underground Crystalline Caves")

Cách tạo 5 level từ 2 ảnh gốc (ít tài nguyên nên tái sử dụng bằng biến thể):
  Level 1  Rừng xanh           : bản đồ rừng gốc
  Level 2  Rừng ma thuật       : rừng LẬT NGANG + đổi màu sang tím/hồng, nước thành xanh ngọc phát sáng
  Level 3  Hang động pha lê    : ô hang động, phóng 4x kiểu pixel (nearest), đặt trên nền đá đen lớn hơn
  Level 4  Vùng đất cháy       : rừng XOAY 180 độ + đổi màu cam-nâu, nước thành dung nham
  Level 5  Hang độc            : hang động LẬT NGANG + xoay màu (hồ nước thành xanh độc)

Mỗi level có: ảnh nền + mặt nạ va chạm (trắng = cấm, 1 pixel = 3px bản đồ). Điểm xuất hiện (spawn) và lối ra
(exit) được tính tự động bằng BFS trên vùng đi được, rồi IN RA để chép vào src/Levels.cpp.
"""
from collections import deque
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as ndi

import make_assets as ma

IMG = ma.IMG
SRC = ma.SRC
BG = IMG / "backgrounds"
CELL = 3                      # 1 ô mặt nạ = 3px bản đồ
WALK_RADIUS = ma.WALK_RADIUS  # 6 ô = 18px: nhân vật/quái bán kính <= 18px đi lọt


# ----------------------------------------------------------------------------
# Đổi màu (HSV): PIL dùng H,S,V trong 0..255
# ----------------------------------------------------------------------------
def _to_hsv(img):
    return np.array(img.convert("RGB").convert("HSV")).astype(float)


def _from_hsv(hsv):
    return Image.fromarray(np.clip(hsv, 0, 255).astype(np.uint8), "HSV").convert("RGB")


def recolor_magic(img):
    """Rừng ma thuật: xanh lá -> tím/hồng; nước -> xanh ngọc phát sáng; đất nâu -> tím xám."""
    hsv = _to_hsv(img)
    deg = hsv[..., 0] * 360 / 255
    sat, val = hsv[..., 1], hsv[..., 2]
    green = (deg >= 50) & (deg < 170)
    water = (deg >= 170) & (deg < 260)
    warm = (deg < 50) | (deg >= 330)
    new = deg.copy()
    new[green] = 255 + (deg[green] - 50) / 120 * 60           # 255..315: tím -> hồng tím
    new[water] = 178 + (deg[water] - 170) / 90 * 18           # xanh ngọc
    new[warm] = 290 + np.where(deg[warm] >= 330, deg[warm] - 360, deg[warm]) * 0.5
    sat = np.where(warm, sat * 0.55, sat)
    sat = np.where(water, np.minimum(sat * 1.15, 255), sat)
    val = np.where(water, np.minimum(val * 1.18, 255), val * 0.92)
    return _from_hsv(np.dstack([(new % 360) / 360 * 255, sat, val]))


def recolor_ember(img):
    """Vùng đất cháy: cây cỏ -> cam/nâu mùa thu; nước -> dung nham; hơi ám đỏ."""
    hsv = _to_hsv(img)
    deg = hsv[..., 0] * 360 / 255
    sat, val = hsv[..., 1], hsv[..., 2]
    green = (deg >= 50) & (deg < 170)
    water = (deg >= 170) & (deg < 260)
    new = deg.copy()
    new[green] = 22 + (deg[green] - 50) / 120 * 22            # 22..44: cam -> vàng đất
    new[water] = 8 + (deg[water] - 170) / 90 * 8              # đỏ cam
    sat = np.where(water, np.minimum(sat * 1.5, 255), sat * 0.92)
    val = np.where(water, np.minimum(val * 1.45 + 30, 255), val * 0.88)
    out = _from_hsv(np.dstack([(new % 360) / 360 * 255, sat, val]))
    arr = np.array(out).astype(float)
    arr[..., 0] = np.minimum(arr[..., 0] * 1.06, 255)         # ám đỏ nhẹ
    arr[..., 2] *= 0.92
    return Image.fromarray(arr.astype(np.uint8))


def hue_shift(img, degrees, sat_mul=1.0, val_mul=1.0):
    hsv = _to_hsv(img)
    hsv[..., 0] = (hsv[..., 0] + degrees / 360 * 255) % 255
    hsv[..., 1] = np.clip(hsv[..., 1] * sat_mul, 0, 255)
    hsv[..., 2] = np.clip(hsv[..., 2] * val_mul, 0, 255)
    return _from_hsv(hsv)


# ----------------------------------------------------------------------------
# Hang động: dựng nền + mặt nạ từ ô "Underground Crystalline Caves"
# ----------------------------------------------------------------------------
CAVE_SCALE = 4
CAVE_WORLD = (1440, 1080)          # bội số của 3 để mặt nạ là 480x360
CAVE_MARGIN = ((CAVE_WORLD[0] - 288 * CAVE_SCALE) // 2, (CAVE_WORLD[1] - 216 * CAVE_SCALE) // 2)

# Toạ độ đọc trên ảnh ô hang động phóng 3x (864x642). Đổi sang pixel bản đồ qua cave_px().
CAVE_FLOOR = [(60, 330), (240, 300), (520, 300), (790, 320), (835, 390), (825, 520), (790, 575), (700, 600),
              (560, 640), (430, 640), (330, 645), (230, 650), (150, 615), (70, 545), (45, 450)]
CAVE_POOL = [(265, 390), (400, 372), (520, 372), (610, 435), (632, 490), (600, 508), (520, 515), (470, 545),
             (400, 585), (330, 600), (255, 585), (195, 525), (185, 490), (205, 445)]
CAVE_RUIN = [(498, 330), (555, 232), (745, 290), (765, 350), (740, 385), (540, 375)]
# Tinh thể: chỉ chặn phần CHÂN (tán cao phía trên chỉ là hình trang trí) nên khoảng trống quanh hồ rộng hơn.
CAVE_CRYSTALS = [  # (cx, cy, rx, ry) đo trên toàn bộ tinh thể
    (255, 262, 36, 50), (338, 292, 38, 48), (460, 238, 48, 68), (800, 405, 34, 56), (775, 470, 44, 50),
    (715, 548, 44, 38), (540, 596, 50, 56), (212, 592, 32, 44), (276, 625, 28, 34), (100, 505, 38, 46),
    (78, 338, 38, 54),
]
CAVE_ROCKS = [(185, 322, 50, 50), (585, 545, 26, 18)]  # đá: chặn toàn bộ


def cave_px(p):
    """Toạ độ ảnh 3x -> toạ độ ô mặt nạ (1 ô = 3px bản đồ)."""
    x, y = p
    wx = CAVE_MARGIN[0] + x / 3 * CAVE_SCALE
    wy = CAVE_MARGIN[1] + y / 3 * CAVE_SCALE
    return wx / CELL, wy / CELL


def build_cave():
    sheet = Image.open(SRC / "backgrounds_sheet.png").convert("RGB")
    tw = sheet.width / 5
    x0, x1 = int(round(3 * tw)) + 2, int(round(4 * tw)) - 1
    tile = sheet.crop((x0, 2 * 240 + 1, x1, 3 * 240 - 1))
    art = tile.crop((0, 0, 288, 216))                          # bỏ dải chú thích tiếng Anh ở đáy ô
    border = np.concatenate([np.array(art)[0], np.array(art)[:, 0], np.array(art)[:, -1]]).reshape(-1, 3)
    bg_color = tuple(int(v) for v in np.median(border, axis=0))
    world = Image.new("RGB", CAVE_WORLD, bg_color)
    up = np.array(art.resize((288 * CAVE_SCALE, 216 * CAVE_SCALE), Image.NEAREST)).astype(float)
    h, w = up.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    edge = np.minimum(np.minimum(xx, w - 1 - xx), np.minimum(yy, h - 1 - yy)).astype(float)
    alpha = np.clip(edge / 44.0, 0, 1)                       # mờ dần trong 44px sát mép ảnh gốc
    alpha = (alpha * alpha * (3 - 2 * alpha))[..., None]     # smoothstep
    up = up * alpha + np.array(bg_color, float) * (1 - alpha)  # hòa vào màu nền => không còn đường viền cứng giữa ảnh và vùng đá đen
    world.paste(Image.fromarray(up.astype(np.uint8)), CAVE_MARGIN)

    mw, mh = CAVE_WORLD[0] // CELL, CAVE_WORLD[1] // CELL
    shapes = Image.new("L", (mw, mh), 255)                     # mặc định: CẤM (đá đen bao quanh)
    d = ImageDraw.Draw(shapes)
    d.polygon([cave_px(p) for p in CAVE_FLOOR], fill=0)        # sàn hang: đi được
    d.polygon([cave_px(p) for p in CAVE_POOL], fill=255)       # hồ nước
    d.polygon([cave_px(p) for p in CAVE_RUIN], fill=255)       # tàn tích
    k = CAVE_SCALE / 3 / CELL
    for cx, cy, rx, ry in CAVE_CRYSTALS:
        c = cave_px((cx, cy + ry * 0.55))                      # dịch xuống chân tinh thể
        rxx, ryy = rx * k * 0.62, ry * k * 0.3
        d.ellipse([c[0] - rxx, c[1] - ryy, c[0] + rxx, c[1] + ryy], fill=255)
    for cx, cy, rx, ry in CAVE_ROCKS:
        c = cave_px((cx, cy))
        d.ellipse([c[0] - rx * k * 0.92, c[1] - ry * k * 0.92, c[0] + rx * k * 0.92, c[1] + ry * k * 0.92], fill=255)
    return world, np.array(shapes) > 0


# ----------------------------------------------------------------------------
# Mặt nạ: giữ lại vùng đi được liên thông với điểm xuất phát (giống ma.build_map)
# ----------------------------------------------------------------------------
def bake_mask(blocked, hint_cell, search_radius=140):
    h, w = blocked.shape
    walk = ~blocked
    walk[:2, :] = walk[-2:, :] = False
    walk[:, :2] = walk[:, -2:] = False
    dist = ndi.distance_transform_edt(walk)
    core = dist >= WALK_RADIUS
    lab, _ = ndi.label(core)
    yy, xx = np.mgrid[0:h, 0:w]
    near = (np.hypot(yy - hint_cell[1], xx - hint_cell[0]) < search_radius) & core
    if not near.any():
        raise RuntimeError("Không có vùng đi được quanh điểm gợi ý %s" % (hint_cell,))
    start = np.argmax(np.where(near, dist, -1))
    main = lab == lab.flat[start]
    reach = ndi.distance_transform_edt(~main) <= WALK_RADIUS + 0.5
    return ~(reach & walk)


def geodesic(walk_core, start):
    """BFS 8 hướng trên các ô 'lõi' (cách vật cản >= WALK_RADIUS). Trả về mảng khoảng cách (-1 = không tới)."""
    h, w = walk_core.shape
    dist = np.full((h, w), -1, np.int32)
    sy, sx = start
    dist[sy, sx] = 0
    q = deque([(sy, sx)])
    while q:
        y, x = q.popleft()
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                ny, nx = y + dy, x + dx
                if (dy or dx) and 0 <= ny < h and 0 <= nx < w and walk_core[ny, nx] and dist[ny, nx] < 0:
                    dist[ny, nx] = dist[y, x] + 1
                    q.append((ny, nx))
    return dist


def pick_spawn_exit(blocked, spawn_hint):
    """Spawn = ô thoáng gần 'spawn_hint' nhất; Exit = ô thoáng XA NHẤT theo đường đi từ spawn."""
    walk = ~blocked
    clear = ndi.distance_transform_edt(walk)
    core = clear >= WALK_RADIUS + 2
    cand = np.argwhere(core)
    d2 = (cand[:, 1] - spawn_hint[0]) ** 2 + (cand[:, 0] - spawn_hint[1]) ** 2
    sy, sx = cand[np.argmin(d2)]
    dist = geodesic(core, (sy, sx))
    for need in (16, 13, 10, 8):                               # lối ra cần chỗ rộng: cổng bán kính ~40px = 13 ô
        ok = (dist >= 0) & (clear >= need)
        if ok.any():
            break
    if not ok.any():
        ok = dist >= 0
    ey, ex = np.unravel_index(np.argmax(np.where(ok, dist, -1)), dist.shape)
    return (sx, sy), (ex, ey), int(dist[ey, ex])


def world(cell):
    return (cell[0] * CELL + CELL / 2, cell[1] * CELL + CELL / 2)


def save_mask(blocked, name):
    Image.fromarray((blocked * 255).astype(np.uint8), "L").save(BG / name)


# ----------------------------------------------------------------------------
def build_all():
    report = []

    # ---- Level 1: rừng gốc (đã được make_assets.build_map tạo: forest.jpg + forest_collision.png) ----
    forest = Image.open(SRC / "forest_source.jpg").convert("RGB")   # đọc từ ảnh gốc, tránh nén JPEG hai lần
    m1 = np.array(Image.open(BG / "forest_collision.png")) > 128
    s1, e1, d1 = pick_spawn_exit(m1, (300, 200))
    report.append(("Level 1 - Rừng xanh", forest.size, world(s1), world(e1), d1))

    # ---- Level 2: rừng ma thuật = lật ngang + đổi màu ----
    f2 = recolor_magic(forest.transpose(Image.FLIP_LEFT_RIGHT))
    f2.save(BG / "forest_magic.jpg", quality=94)
    m2 = np.fliplr(m1)
    save_mask(m2, "forest_magic_collision.png")
    s2, e2, d2 = pick_spawn_exit(m2, (m1.shape[1] - 1 - e1[0], e1[1]))
    report.append(("Level 2 - Rừng ma thuật", f2.size, world(s2), world(e2), d2))

    # ---- Level 3: hang pha lê (ô trong bảng phong cảnh) ----
    cave, mc = build_cave()
    cave.save(BG / "cave_crystal.jpg", quality=94)
    mc = bake_mask(mc, cave_px((130, 430)))
    save_mask(mc, "cave_crystal_collision.png")
    s3, e3, d3 = pick_spawn_exit(mc, cave_px((130, 430)))
    report.append(("Level 3 - Hang pha lê", cave.size, world(s3), world(e3), d3))

    # ---- Level 4: vùng đất cháy = rừng xoay 180 độ + đổi màu ----
    f4 = recolor_ember(forest.transpose(Image.ROTATE_180))
    f4.save(BG / "forest_ember.jpg", quality=94)
    m4 = m1[::-1, ::-1]
    save_mask(m4, "forest_ember_collision.png")
    s4, e4, d4 = pick_spawn_exit(m4, (m1.shape[1] - 1 - e1[0], m1.shape[0] - 1 - e1[1]))
    report.append(("Level 4 - Vùng đất cháy", f4.size, world(s4), world(e4), d4))

    # ---- Level 5: hang độc = hang lật ngang + xoay màu (hồ nước -> xanh độc) ----
    c5 = hue_shift(cave.transpose(Image.FLIP_LEFT_RIGHT), -105, sat_mul=1.1, val_mul=1.05)
    c5.save(BG / "cave_toxic.jpg", quality=94)
    m5 = np.fliplr(mc)
    save_mask(m5, "cave_toxic_collision.png")
    s5, e5, d5 = pick_spawn_exit(m5, (mc.shape[1] - 1 - e3[0], e3[1]))
    report.append(("Level 5 - Hang độc", c5.size, world(s5), world(e5), d5))

    write_preview(report)
    print("\n[levels] CHÉP các số này vào src/Levels.cpp (toạ độ thế giới, pixel):")
    for name, size, s, e, d in report:
        print("  %-26s map %dx%d  spawn=(%.0f, %.0f)  exit=(%.0f, %.0f)  đường đi %d ô" % (name, size[0], size[1], s[0], s[1], e[0], e[1], d))

    # ---- Slime xanh dương (tank) từ Slime xanh ----
    full = ma.cut_slime()
    pal = ma.build_palette(np.array(full)[..., :3][np.array(full)[..., 3] > 0], 10)
    big = ma.pixelate(full, ma.SLIME_SRC_SCALE, palette=pal)
    small = ma.pixelate(big, big.width / ma.SLIME_SMALL_W, palette=pal)
    blue = ma.recolor(small, 216, mouth_hue_deg=350, sat_mul=0.95, val_mul=0.82)
    ma.make_sheet(blue, ma.SLIME_CELL, ma.SLIME_ANCHOR, IMG / "monsters" / "slime_blue.png")
    return report


# ----------------------------------------------------------------------------
# Ảnh xem trước: nền + vùng cấm (đỏ) + spawn (xanh lá) + exit (xanh dương), lưu vào tools/preview/
# ----------------------------------------------------------------------------
LEVEL_FILES = [("forest.jpg", "forest_collision.png"), ("forest_magic.jpg", "forest_magic_collision.png"),
               ("cave_crystal.jpg", "cave_crystal_collision.png"), ("forest_ember.jpg", "forest_ember_collision.png"),
               ("cave_toxic.jpg", "cave_toxic_collision.png")]


def write_preview(report):
    out_dir = ma.ROOT / "tools" / "preview"
    out_dir.mkdir(parents=True, exist_ok=True)
    tiles = []
    for (bgf, mf), (name, size, sp, ex, _d) in zip(LEVEL_FILES, report):
        bg = Image.open(BG / bgf).convert("RGB")
        mask = np.array(Image.open(BG / mf).resize(bg.size, Image.NEAREST)) > 128
        arr = np.array(bg).astype(float)
        arr[mask] = arr[mask] * 0.5 + np.array([255, 40, 40]) * 0.5
        im = Image.fromarray(arr.astype(np.uint8))
        d = ImageDraw.Draw(im)
        for (x, y), col in ((sp, (60, 255, 60)), (ex, (80, 160, 255))):
            d.ellipse([x - 40, y - 40, x + 40, y + 40], outline=col, width=8)
        tiles.append(im.resize((560, int(560 * im.height / im.width))))
    sheet = Image.new("RGB", (560 * 3 + 20, 380 * 2 + 10), (30, 30, 30))
    for i, t in enumerate(tiles):
        sheet.paste(t, ((i % 3) * 570, (i // 3) * 390))
    sheet.save(out_dir / "levels_overview.png")
