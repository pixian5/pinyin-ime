#!/usr/bin/env python3
"""gen_sogou_skin_assets.py — 生成「搜狗风格」皮肤所需的 9-slice PNG 资源。

fcitx5 classicui 的圆角只能靠带 alpha 的 PNG + Margin（九宫格）实现：
只在四角保留圆角像素，中间被拉伸，因此面板任意尺寸都不变形。

生成物（写入 config/themes/sogou-{light,dark}/）：
  panel.png     候选面板底：圆角矩形 + 1px 描边（默认不启用：见下方说明）
  highlight.png 首选高亮块：实心圆角矩形（默认启用，画在面板之上，圆角安全）
  menu_bg.png   右键菜单底（默认不启用，同 panel.png）
  menu_hl.png   菜单选中项
  prev.png / next.png  翻页箭头（‹ ›）
  radio.png     菜单勾选
  arrow.png     子菜单箭头

为什么 panel.png 默认不启用：
  带 alpha 的圆角面板在没有 X 合成器（compositing）的桌面上会露出黑色直角，
  比直角更难看。fcitx5 无法探测合成器状态，所以默认用「直角面板 + 圆角高亮胶囊」，
  两者叠加后的观感已接近搜狗；开了合成器的桌面可把 theme.conf 里
  [InputPanel/Background] 的 Image 改成 panel.png、BorderWidth=0、
  Margin 改成 12，即可获得真圆角面板。

用法：python3 scripts/gen_sogou_skin_assets.py
"""
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
THEMES = ROOT / "config" / "themes"

# 9-slice 边距：每边 12px 对应 theme.conf 里的 /Margin
PANEL = 64
MARGIN = 12
RADIUS = 10
SS = 4  # 超采样倍数，先画大图再缩小，边缘更平滑


def rounded_panel(fill, border=None, border_w=1, radius=RADIUS, size=PANEL):
    """圆角矩形（可带描边），外部全透明。"""
    big = size * SS
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    r = radius * SS
    # PIL 的 rounded_rectangle 描边是内描边，视觉上更接近原生控件
    d.rounded_rectangle(
        [0, 0, big - 1, big - 1],
        radius=r,
        fill=fill,
        outline=border,
        width=border_w * SS if border else 0,
    )
    return img.resize((size, size), Image.LANCZOS)


def solid_rounded(fill, radius=6, size=48):
    big = size * SS
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, big - 1, big - 1], radius=radius * SS, fill=fill)
    return img.resize((size, size), Image.LANCZOS)


def chevron(color, pointing="left", size=16, width=2):
    """翻页箭头：两条线拼成的 chevron，透明底。"""
    big = size * SS
    img = Image.new("RGBA", (big, size * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    w = width * SS
    pad = 5 * SS
    mid = big // 2
    if pointing == "left":
        pts = [(big - pad, pad), (pad, mid), (big - pad, big - pad)]
    else:
        pts = [(pad, pad), (big - pad, mid), (pad, big - pad)]
    d.line(pts, fill=color, width=w, joint="curve")
    return img.resize((size, size), Image.LANCZOS)


def checkmark(color, size=16, width=2):
    big = size * SS
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    w = width * SS
    d.line(
        [(3 * SS, 8 * SS), (6 * SS, 12 * SS), (13 * SS, 3 * SS)],
        fill=color,
        width=w,
        joint="curve",
    )
    return img.resize((size, size), Image.LANCZOS)


PALETTES = {
    "sogou-light": dict(
        panel=(255, 255, 255, 255),
        border=(217, 222, 229, 255),
        hl=(58, 142, 230, 255),
        icon=(138, 148, 166, 255),
    ),
    "sogou-dark": dict(
        panel=(43, 47, 54, 255),
        border=(61, 67, 77, 255),
        hl=(74, 158, 255, 255),
        icon=(154, 164, 178, 255),
    ),
}


def main():
    for name, p in PALETTES.items():
        d = THEMES / name
        d.mkdir(parents=True, exist_ok=True)
        rounded_panel(p["panel"], p["border"], 1).save(d / "panel.png")
        solid_rounded(p["hl"]).save(d / "highlight.png")
        rounded_panel(p["panel"], p["border"], 1).save(d / "menu_bg.png")
        solid_rounded(p["hl"], radius=6, size=48).save(d / "menu_hl.png")
        chevron(p["icon"], "left").save(d / "prev.png")
        chevron(p["icon"], "right").save(d / "next.png")
        checkmark(p["hl"]).save(d / "radio.png")
        chevron(p["icon"], "right", size=12).save(d / "arrow.png")
        print(f"{name}: 8 个资源已生成 -> {d}")


if __name__ == "__main__":
    main()
