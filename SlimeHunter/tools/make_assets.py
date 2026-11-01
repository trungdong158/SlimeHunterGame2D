#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_assets.py - Tạo toàn bộ tài nguyên cho SLIME HUNTER từ 3 ảnh gốc trong tools/source/.

Cách chạy (từ thư mục gốc của dự án):
    pip install pillow numpy scipy
    python tools/make_assets.py            # tạo assets/
    python tools/make_assets.py --preview  # tạo thêm ảnh xem trước trong tools/preview/

Script làm 5 việc:
  1. Tách nền caro giả của player_source.png và slime_source.png (flood-fill + khử viền sáng).
  2. Pixel-hóa Player (ảnh vector mượt) về cùng mật độ pixel với bản đồ (1 pixel art = 3px màn hình).
  3. Tạo sprite sheet có nhiều frame (đứng yên / đi / đánh / bị đánh / chết) cho Player và các Slime.
  4. Tạo mặt nạ va chạm (nước + cây lớn + đá + vách đá) cho bản đồ rừng.
  5. Tạo bóng đổ dưới chân.
  6. (make_levels.py) Tạo 5 level: nền, mặt nạ va chạm, điểm xuất hiện / lối ra, và Slime xanh dương.

Các hằng số kích thước ô (CELL) bên dưới PHẢI khớp với hằng số trong Player.cpp / Monster.cpp / Slime.cpp.
"""
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as ndi

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "tools" / "source"
IMG = ROOT / "assets" / "images"
PREVIEW = ROOT / "tools" / "preview"

ART_SCALE = 3  # 1 pixel art = 3 pixel màn hình (khớp với bản đồ 1800x1200 = 600x400 pixel art x3)


# ----------------------------------------------------------------------------
# Các hàm tiện ích xử lý ảnh
# ----------------------------------------------------------------------------
def border_connected(candidate):
    """Trả về mask các vùng 'candidate' có chạm viền ảnh (tức là nền, không phải vật thể)."""
    lab, _ = ndi.label(candidate)
    border = set(np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]]))) - {0}
    return np.isin(lab, list(border))


def kill_halo(bg, light, passes=2):
    """Xóa lớp viền sáng (halo) còn sót sát nền sau khi tách."""
    fg = ~bg
    for _ in range(passes):
        near_bg = ndi.binary_dilation(bg, structure=np.ones((3, 3)))
        bg = bg | (fg & near_bg & light)
        fg = ~bg
    return bg


def largest_component(mask):
    lab, n = ndi.label(mask)
    if n <= 1:
        return mask
    sizes = ndi.sum(mask, lab, range(1, n + 1))
    return lab == (1 + int(np.argmax(sizes)))


def build_palette(rgb_pixels, k):
    """Gom màu bằng median-cut, chỉ trên các pixel của vật thể (bỏ nền)."""
    px = rgb_pixels.reshape(-1, 1, 3).astype(np.uint8)
    q = Image.fromarray(px).quantize(colors=k, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = np.array(q.getpalette()[: k * 3]).reshape(-1, 3)
    return pal[np.unique(np.array(q))]


def nearest_index(rgb, palette):
    d = ((rgb[..., None, :].astype(int) - palette[None, None, :, :].astype(int)) ** 2).sum(-1)
    return d.argmin(-1)


def pixelate(img, scale, colors=20, palette=None, outline_share=0.30, alpha_share=0.5):
    """Thu nhỏ ảnh RGBA bằng cách chọn MÀU CHIẾM ĐA SỐ trong mỗi ô scale x scale.
    Khác với lấy trung bình, cách này giữ màu tươi và nét viền gọn, đúng kiểu pixel-art."""
    arr = np.array(img.convert("RGBA"))
    h, w = arr.shape[:2]
    opaque = arr[..., 3] >= 128
    pal = palette if palette is not None else build_palette(arr[..., :3][opaque], colors)
    idx = nearest_index(arr[..., :3], pal)
    dark_ids = np.where(pal.sum(1) < 150)[0]  # màu rất tối = nét viền / tóc: được ưu tiên giữ
    out_h, out_w = max(1, round(h / scale)), max(1, round(w / scale))
    out = np.zeros((out_h, out_w, 4), np.uint8)
    for oy in range(out_h):
        y0 = int(round(oy * scale)); y1 = min(h, max(y0 + 1, int(round((oy + 1) * scale))))
        for ox in range(out_w):
            x0 = int(round(ox * scale)); x1 = min(w, max(x0 + 1, int(round((ox + 1) * scale))))
            if x0 >= w or y0 >= h:
                continue
            op = opaque[y0:y1, x0:x1]
            if op.mean() < alpha_share:
                continue
            ids = idx[y0:y1, x0:x1][op]
            cnt = np.bincount(ids, minlength=len(pal))
            best = int(np.argmax(cnt))
            if len(dark_ids) and cnt[dark_ids].sum() / cnt.sum() >= outline_share and best not in dark_ids:
                best = int(dark_ids[np.argmax(cnt[dark_ids])])
            out[oy, ox, :3] = pal[best]
            out[oy, ox, 3] = 255
    return Image.fromarray(out)


def blit(dst, src, x, y):
    """Dán mảng RGBA src lên dst tại (x, y); chỉ chép pixel có alpha > 0."""
    h, w = src.shape[:2]
    for yy in range(h):
        ty = y + yy
        if 0 <= ty < dst.shape[0]:
            for xx in range(w):
                tx = x + xx
                if 0 <= tx < dst.shape[1] and src[yy, xx, 3] > 0:
                    dst[ty, tx] = src[yy, xx]


def trim(img):
    """Cắt bỏ phần trong suốt thừa."""
    box = img.getbbox()
    return img.crop(box) if box else img


# ----------------------------------------------------------------------------
# 1) PLAYER
# ----------------------------------------------------------------------------
PLAYER_CELL = (40, 40)     # kích thước 1 frame (pixel art)
PLAYER_ANCHOR = (20, 38)   # điểm "chân" trong ô; Player.cpp đặt origin của sprite tại đây
PLAYER_HEIGHT_ART = 34     # chiều cao thân nhân vật sau pixel-hóa
PLAYER_LEG_ROWS = 6        # số hàng pixel coi là "chân" (để làm animation đi bộ)
BAT_ZOOM = 1.5             # phóng gậy lên một chút để dễ nhìn thấy khi vung


def cut_player():
    im = np.array(Image.open(SRC / "player_source.png").convert("RGB")).astype(int)
    h, w, _ = im.shape
    mx, mn = im.max(2), im.min(2)
    sat = mx - mn
    # Bước 1: nền trắng/xám nhạt nối với viền ảnh (không đụng tới áo trắng vì áo có viền đen bao quanh)
    bg = border_connected((mn >= 225) & (sat <= 6))
    # Bước 2: bóng đổ xám + mảng caro bị bóng "nhốt" giữa hai chân, CHỈ xét vùng chân (y >= 215)
    yy = np.arange(h)[:, None] * np.ones((1, w))
    cand2 = ((mn >= 140) & (sat <= 10) & (yy >= 215)) | bg
    lab, _ = ndi.label(cand2)
    seeds = set(np.unique(lab[bg])) - {0}
    bg = bg | np.isin(lab, list(seeds))
    # Bước 3: khử viền sáng
    bg = kill_halo(bg, light=(mn >= 105) & (sat <= 40), passes=2)
    fg = largest_component(~bg)
    out = np.zeros((h, w, 4), np.uint8)
    out[..., :3] = im
    out[..., 3] = fg * 255
    return out


def build_player():
    rgba = cut_player()
    h, w = rgba.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    # Gậy nằm ở x >= 187 và 163 <= y <= 211 (đã đo trên ảnh gốc). Phần còn lại là thân nhân vật.
    bat_mask = (xx >= 187) & (yy >= 163) & (yy <= 211) & (rgba[..., 3] > 0)
    body = rgba.copy(); body[bat_mask, 3] = 0
    bat = rgba.copy(); bat[~bat_mask, 3] = 0

    ys, xs = np.where(body[..., 3] > 0)
    bx0, by0, bx1, by1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
    body_img = Image.fromarray(body).crop((bx0, by0, bx1, by1))
    s = body_img.height / PLAYER_HEIGHT_ART                      # số pixel gốc / 1 pixel art
    body_px = np.array(pixelate(body_img, s, colors=20))
    bh, bw = body_px.shape[:2]

    # --- gậy: xoay ngang (cán bên trái, đầu gậy bên phải) rồi pixel-hóa ---
    ys, xs = np.where(bat[..., 3] > 0)
    bat_img = Image.fromarray(bat).crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))
    a = np.array(bat_img)[..., 3] > 0
    cols = np.where(a.any(0))[0]
    left = np.array([np.where(a[:, c])[0].mean() for c in cols[:3]]).mean()
    right = np.array([np.where(a[:, c])[0].mean() for c in cols[-3:]]).mean()
    angle = math.degrees(math.atan2(right - left, cols[-1] - cols[0]))   # góc (hệ toạ độ ảnh, y xuống)
    bat_rot = bat_img.rotate(angle, resample=Image.BICUBIC, expand=True)
    pal_all = build_palette(np.array(Image.fromarray(rgba[..., :3]))[rgba[..., 3] > 0], 20)
    bat_px = trim(pixelate(bat_rot, s / BAT_ZOOM, palette=pal_all))
    bat_arr = np.array(bat_px)
    bat_pad = np.zeros((bat_arr.shape[0] + 2, bat_arr.shape[1] + 2, 4), np.uint8)
    bat_pad[1:-1, 1:-1] = bat_arr
    # điểm cầm gậy (grip): đầu trái của cán, giữa theo chiều dọc
    c0 = np.where(bat_pad[..., 3].any(0))[0][0]
    rows = np.where(bat_pad[:, c0 + 1, 3] > 0)[0]
    bat_pivot = (c0 + 0.5, float(rows.mean()) + 0.5)
    Image.fromarray(bat_pad).save(IMG / "player" / "player_bat.png")

    # --- vị trí bàn tay (nơi cầm gậy) trong tọa độ thân đã pixel-hóa ---
    hand_art = ((185 - bx0) / s, (207 - by0) / s)

    # --- điểm chân: trung bình x của các pixel ở 2 hàng cuối ---
    bottom = np.where(body_px[-2:, :, 3].any(0))[0]
    feet_cx = float(bottom.mean()) + 0.5
    ox = int(round(PLAYER_ANCHOR[0] - feet_cx))
    oy = PLAYER_ANCHOR[1] - bh
    hip = bh - PLAYER_LEG_ROWS
    # khe giữa hai chân: tìm đoạn cột trống dài nhất ở 3 hàng cuối
    occ = body_px[-3:, :, 3].any(0)
    first, last = np.where(occ)[0][0], np.where(occ)[0][-1]
    best_gap, split = (0, int(feet_cx))
    run_start = None
    for x in range(first, last + 2):
        empty = x <= last and not occ[x]
        if empty and run_start is None:
            run_start = x
        if (not empty) and run_start is not None:
            if x - run_start > best_gap:
                best_gap, split = x - run_start, (run_start + x) // 2
            run_start = None

    upper = body_px[:hip]
    legs_L = body_px[hip:].copy(); legs_L[:, split:, 3] = 0
    legs_R = body_px[hip:].copy(); legs_R[:, :split, 3] = 0

    def compose(upper_dy=0, upper_dx=0, L=(0, 0), R=(0, 0)):
        cell = np.zeros((PLAYER_CELL[1], PLAYER_CELL[0], 4), np.uint8)
        blit(cell, legs_L, ox + L[0], oy + hip + L[1])
        blit(cell, legs_R, ox + R[0], oy + hip + R[1])
        blit(cell, upper, ox + upper_dx, oy + upper_dy)
        for k in range(1, -upper_dy + 1):            # thân kéo lên -> lấp khe hở bằng hàng áp chót
            blit(cell, upper[-1:], ox + upper_dx, oy + hip - k)
        return cell

    idle = [compose(0), compose(0), compose(-1), compose(-1)]
    walk = [compose(0, 0, (-1, 0), (1, 0)), compose(0, 0, (-1, 0), (0, -1)), compose(-1, 0, (0, 0), (0, -1)),
            compose(0, 0, (1, 0), (-1, 0)), compose(0, 0, (0, -1), (-1, 0)), compose(-1, 0, (0, -1), (0, 0))]
    attack = [compose(1, 0, (-1, 0), (1, 0)), compose(-1, 0, (-1, 0), (1, 0)),
              compose(-1, 0, (-1, 0), (1, 0)), compose(0)]
    hurt = [compose(1, -1, (-1, 0), (1, 0)), compose(0, -1)]

    def rot(cell, deg, ty):
        im = Image.fromarray(cell).rotate(deg, resample=Image.NEAREST, center=(20, 22), translate=(0, ty))
        return np.array(im)

    crouch = compose(2, 0, (-1, 0), (1, 0))
    death = [crouch, rot(crouch, -35, 3), rot(crouch, -90, 7), rot(crouch, -90, 7)]

    rows_list = [idle, walk, attack, hurt, death]          # 0 idle, 1 walk, 2 attack, 3 hurt, 4 death
    sheet = np.zeros((len(rows_list) * PLAYER_CELL[1], 6 * PLAYER_CELL[0], 4), np.uint8)
    for r, frames in enumerate(rows_list):
        for c, f in enumerate(frames):
            blit(sheet, f, c * PLAYER_CELL[0], r * PLAYER_CELL[1])
    Image.fromarray(sheet).save(IMG / "player" / "player_sheet.png")

    hand_cell = (hand_art[0] + ox, hand_art[1] + oy)
    print("[player] thân %dx%d, hip=%d, split=%d, feet_cx=%.1f" % (bw, bh, hip, split, feet_cx))
    print("[player] sheet 6x5 ô %s; HAND_OFFSET (so với chân) = (%.1f, %.1f) pixel-art"
          % (PLAYER_CELL, hand_cell[0] - PLAYER_ANCHOR[0], hand_cell[1] - PLAYER_ANCHOR[1]))
    print("[player] gậy %dx%d, BAT_PIVOT = (%.1f, %.1f) pixel-art" % (bat_pad.shape[1], bat_pad.shape[0], *bat_pivot))
    return sheet


# ----------------------------------------------------------------------------
# 2) SLIME (xanh, tím, và Slime King làm boss)
# ----------------------------------------------------------------------------
SLIME_SRC_SCALE = 6.8      # đo được: 1 pixel gốc của ảnh slime = 6.8px
SLIME_SMALL_W = 28         # slime thường: thu nhỏ từ 44x42 xuống ~28x27
SLIME_CELL = (48, 40)
SLIME_ANCHOR = (24, 38)
KING_CELL = (80, 64)
KING_ANCHOR = (40, 61)

# (scale_x, scale_y) cho từng frame; bám đáy ô nên squash/stretch trông như nảy lên
SLIME_ANIMS = [
    [(1.00, 1.00), (1.05, 0.94), (1.10, 0.88), (0.96, 1.05)],                                   # 0 idle
    [(1.15, 0.82), (1.06, 0.93), (0.88, 1.15), (0.92, 1.10), (0.96, 1.04), (1.16, 0.80)],       # 1 hop
    [(1.20, 0.80), (0.82, 1.22), (1.05, 0.95), (1.00, 1.00)],                                   # 2 attack
    [(1.20, 0.78), (0.92, 1.10)],                                                               # 3 hurt
    [(1.10, 0.85), (1.30, 0.60), (1.45, 0.38), (1.55, 0.22)],                                   # 4 death
]


def cut_slime():
    im = np.array(Image.open(SRC / "slime_source.png").convert("RGB")).astype(int)
    mx, mn = im.max(2), im.min(2)
    sat = mx - mn
    bg = border_connected((mn >= 215) & (sat <= 10))
    ys, xs = np.where(~bg)
    box = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)   # lưới pixel-art bắt đầu từ góc này
    bg = kill_halo(bg, light=(mn >= 100) & (sat <= 70), passes=2)
    out = np.zeros(im.shape[:2] + (4,), np.uint8)
    out[..., :3] = im
    out[..., 3] = (~bg) * 255
    return Image.fromarray(out).crop(box)


def recolor(img, green_hue_deg, mouth_hue_deg=None, sat_mul=1.0, val_mul=1.0):
    """Đổi bảng màu: nhóm xanh lá -> màu mới; nhóm tím (miệng/mắt) -> màu khác nếu muốn."""
    arr = np.array(img)
    hsv = np.array(Image.fromarray(arr[..., :3]).convert("HSV")).astype(float)
    hue = hsv[..., 0]
    green = (hue >= 45) & (hue <= 110)
    purple = (hue >= 160) & (hue <= 215)
    new = hue.copy()
    new[green] = green_hue_deg / 360 * 255
    if mouth_hue_deg is not None:
        new[purple] = mouth_hue_deg / 360 * 255
    hsv[..., 0] = new
    hsv[..., 1] = np.clip(hsv[..., 1] * np.where(green, sat_mul, 1.0), 0, 255)
    hsv[..., 2] = np.clip(hsv[..., 2] * np.where(green, val_mul, 1.0), 0, 255)
    rgb = np.array(Image.fromarray(hsv.astype(np.uint8), "HSV").convert("RGB"))
    return Image.fromarray(np.dstack([rgb, arr[..., 3]]))


def squash_frame(base, sx, sy, cell, anchor):
    nw, nh = max(1, round(base.width * sx)), max(1, round(base.height * sy))
    im = base.resize((nw, nh), Image.NEAREST)
    out = Image.new("RGBA", cell, (0, 0, 0, 0))
    out.paste(im, (anchor[0] - nw // 2, anchor[1] - nh), im)
    return out


def make_sheet(base, cell, anchor, path):
    sheet = Image.new("RGBA", (6 * cell[0], len(SLIME_ANIMS) * cell[1]), (0, 0, 0, 0))
    for r, frames in enumerate(SLIME_ANIMS):
        for c, (sx, sy) in enumerate(frames):
            sheet.paste(squash_frame(base, sx, sy, cell, anchor), (c * cell[0], r * cell[1]))
    sheet.save(path)


def draw_crown(w=11, h=6):
    """Vương miện vàng cho Slime King, vẽ bằng ImageDraw không khử răng cưa nên giữ nguyên pixel."""
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    gold, dark = (255, 214, 64, 255), (120, 72, 10, 255)
    d.polygon([(0, h - 1), (0, 0), (2, 2), (w // 2, 0), (w - 3, 2), (w - 1, 0), (w - 1, h - 1)], fill=gold, outline=dark)
    d.point([(w // 2, h - 3)], fill=(230, 40, 60, 255))
    d.point([(2, h - 3)], fill=(60, 140, 255, 255))
    d.point([(w - 3, h - 3)], fill=(60, 140, 255, 255))
    return im


def build_slimes():
    full = cut_slime()
    pal = build_palette(np.array(full)[..., :3][np.array(full)[..., 3] > 0], 10)
    big = pixelate(full, SLIME_SRC_SCALE, palette=pal)           # ~44x42 pixel-art (độ phân giải gốc)
    small_scale = big.width / SLIME_SMALL_W
    small = pixelate(big, small_scale, palette=pal)              # ~28x27 cho slime thường
    print("[slime] gốc %dx%d, thường %dx%d" % (*big.size, *small.size))

    green = small
    purple = recolor(small, 278, mouth_hue_deg=330, sat_mul=0.85, val_mul=1.0)
    make_sheet(green, SLIME_CELL, SLIME_ANCHOR, IMG / "monsters" / "slime_green.png")
    make_sheet(purple, SLIME_CELL, SLIME_ANCHOR, IMG / "monsters" / "slime_purple.png")

    # Slime King: dùng ảnh 44x42 gốc, đổi sang đỏ-cam, đội vương miện
    king_body = recolor(big, 6, mouth_hue_deg=275, sat_mul=1.45, val_mul=0.88)
    crown = draw_crown()
    king = Image.new("RGBA", (king_body.width, king_body.height + crown.height - 1), (0, 0, 0, 0))
    king.paste(king_body, (0, crown.height - 1), king_body)
    peak_x = int(np.where(np.array(king_body)[0:3, :, 3].any(0))[0].mean())      # đỉnh đầu slime
    king.paste(crown, (peak_x - crown.width // 2, 0), crown)
    make_sheet(king, KING_CELL, KING_ANCHOR, IMG / "monsters" / "slime_king.png")
    return green, purple, king


# ----------------------------------------------------------------------------
# 3) BÓNG ĐỔ
# ----------------------------------------------------------------------------
def build_shadow():
    im = Image.new("RGBA", (26, 10), (0, 0, 0, 0))
    ImageDraw.Draw(im).ellipse([0, 0, 25, 9], fill=(20, 30, 20, 95))
    im.save(IMG / "effects" / "shadow.png")


# ----------------------------------------------------------------------------
# 4) BẢN ĐỒ + MẶT NẠ VA CHẠM
# ----------------------------------------------------------------------------
CELL = 3                 # 1 ô mặt nạ = 3px bản đồ = 1 pixel-art
MASK_W, MASK_H = 600, 400
WALK_RADIUS = 6          # ô; vùng đi được phải rộng đủ cho nhân vật bán kính <= 18px
SHRINK = 0.86            # va chạm nhỏ hơn tán lá một chút để nhân vật có thể lấn vào mép lá

# Vật cản lớn: (cx, cy, rx, ry) đọc từ ảnh lưới 1344x896 (1 ô pixel-art = 2.24px ảnh lưới).
OBSTACLES = [
    (395, 95, 48, 52), (520, 170, 58, 58), (665, 155, 78, 72), (770, 60, 58, 60), (828, 165, 48, 52),
    (645, 40, 50, 45), (240, 200, 48, 55), (290, 305, 75, 75), (880, 75, 52, 52), (1075, 45, 32, 35),
    (737, 405, 62, 58), (870, 400, 35, 55), (440, 485, 85, 85), (685, 625, 52, 50), (980, 575, 58, 68),
    (1078, 612, 42, 42), (1197, 495, 62, 60), (1273, 335, 65, 78), (703, 800, 65, 72), (1152, 815, 52, 55),
    (895, 695, 40, 38), (50, 595, 55, 55), (108, 662, 38, 36), (326, 693, 33, 45), (1040, 695, 35, 30),
    (240, 790, 55, 65), (342, 420, 28, 28), (40, 85, 50, 48), (1093, 355, 33, 32), (1305, 725, 48, 45),
    (195, 648, 45, 42), (180, 255, 38, 38),
]
CLIFF = [(578, 470), (700, 478), (800, 470), (960, 490), (965, 600), (930, 670), (800, 672),
         (740, 660), (700, 615), (640, 600), (585, 570)]      # vách đá dưới cao nguyên (toạ độ ảnh lưới)
GRID = 1 / 2.24


def build_map():
    src = Image.open(SRC / "forest_source.jpg").convert("RGB")
    src.save(IMG / "backgrounds" / "forest.jpg", quality=95)

    small = np.array(src.resize((MASK_W, MASK_H), Image.BOX)).astype(float) / 255
    r, g, b = small[..., 0], small[..., 1], small[..., 2]
    sat = (small.max(2) - small.min(2)) / (small.max(2) + 1e-6)
    water = (b > r + 0.12) & (b >= g - 0.02) & (sat > 0.25)               # nước: phát hiện theo màu
    water = ndi.binary_closing(water, iterations=2)
    lab, n = ndi.label(water)
    for i, sz in enumerate(ndi.sum(water, lab, range(1, n + 1)), start=1):
        if sz < 40:
            water[lab == i] = False

    shapes = Image.new("L", (MASK_W, MASK_H), 0)
    d = ImageDraw.Draw(shapes)
    for cx, cy, rx, ry in OBSTACLES:
        cx, cy, rx, ry = cx * GRID, cy * GRID, rx * GRID * SHRINK, ry * GRID * SHRINK
        d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=255)
    d.polygon([(x * GRID, y * GRID) for x, y in CLIFF], fill=255)
    blocked = water | (np.array(shapes) > 0)

    # Chỉ giữ vùng đi được liên thông với điểm xuất phát (tránh nhân vật/quái bị nhốt trong ô kín)
    walk = ~blocked
    walk[:2, :] = walk[-2:, :] = False
    walk[:, :2] = walk[:, -2:] = False
    dist = ndi.distance_transform_edt(walk)
    core = dist >= WALK_RADIUS
    lab, n = ndi.label(core)
    cy0, cx0 = MASK_H // 2, MASK_W // 2
    yy, xx = np.mgrid[0:MASK_H, 0:MASK_W]
    near = (np.hypot(yy - cy0, xx - cx0) < 90) & core
    start = np.argmax(np.where(near, dist, -1))
    main_label = lab.flat[start]
    main = lab == main_label
    reach = ndi.distance_transform_edt(~main) <= WALK_RADIUS + 0.5
    final_walk = reach & walk
    blocked_final = ~final_walk
    Image.fromarray((blocked_final * 255).astype(np.uint8), "L").save(IMG / "backgrounds" / "forest_collision.png")
    print("[map] vùng cấm %.1f%%, vùng đi được liên thông %.1f%% (%d cụm lõi, giữ cụm chính)"
          % (blocked_final.mean() * 100, final_walk.mean() * 100, n))
    return blocked_final


# ----------------------------------------------------------------------------
# Xem trước (tùy chọn)
# ----------------------------------------------------------------------------
def write_previews(player_sheet, slimes, blocked):
    PREVIEW.mkdir(parents=True, exist_ok=True)
    green_bg = (110, 170, 80, 255)

    def on_bg(im, scale):
        bg = Image.new("RGBA", im.size, green_bg)
        bg.alpha_composite(im.convert("RGBA"))
        return bg.resize((im.width * scale, im.height * scale), Image.NEAREST)

    on_bg(Image.fromarray(player_sheet), 4).save(PREVIEW / "player_sheet_x4.png")
    for name, s in zip(("slime_green", "slime_purple", "slime_king"), slimes):
        on_bg(Image.open(IMG / "monsters" / f"{name}.png"), 3 if name != "slime_king" else 2).save(PREVIEW / f"{name}_x3.png")
    over = np.array(Image.open(IMG / "backgrounds" / "forest.jpg").resize((900, 600), Image.LANCZOS)).copy()
    m = np.array(Image.fromarray((blocked * 255).astype(np.uint8)).resize((900, 600), Image.NEAREST)) > 0
    over[m] = (over[m] * 0.45 + np.array([255, 40, 40]) * 0.55).astype(np.uint8)
    Image.fromarray(over).save(PREVIEW / "collision_overlay.png")
    print("[preview] đã ghi vào", PREVIEW)


def main():
    for sub in ("player", "monsters", "backgrounds", "effects"):
        (IMG / sub).mkdir(parents=True, exist_ok=True)
    sheet = build_player()
    slimes = build_slimes()
    build_shadow()
    blocked = build_map()
    import make_levels  # 5 level: nền + mặt nạ + slime xanh dương (chạy sau build_map vì dùng forest.jpg)
    make_levels.build_all()
    if "--preview" in sys.argv:
        write_previews(sheet, slimes, blocked)
    print("Xong. Tài nguyên nằm trong", IMG)


if __name__ == "__main__":
    main()
