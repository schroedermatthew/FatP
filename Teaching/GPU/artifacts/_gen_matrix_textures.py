"""Float matrix -> IEEE754 bytes -> RGBA texture bitmaps + HTML grid."""
import struct
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

TILE = 80
GRID = 5
MARGIN = 3
BG = (7, 7, 10)
PILL = (12, 12, 18, 220)
FLOAT_FG = (60, 232, 178)
HEX_FG = (180, 185, 200)

A = [
    [1.0, 2.0, 3.0, 4.0, 5.0],
    [6.0, 7.0, 8.0, 9.0, 10.0],
    [11.0, 12.0, 13.0, 14.0, 15.0],
    [16.0, 17.0, 18.0, 19.0, 20.0],
    [21.0, 22.0, 23.0, 24.0, 25.0],
]
B = [
    [0.0, 1.0, 0.0, 1.0, 0.0],
    [1.0, 0.0, 1.0, 0.0, 1.0],
    [2.0, 2.0, 2.0, 2.0, 2.0],
    [-1.0, 0.0, -1.0, 0.0, -1.0],
    [5.0, 5.0, 5.0, 5.0, 5.0],
]
C = [
    [1.0, 3.0, 3.0, 5.0, 5.0],
    [7.0, 7.0, 9.0, 9.0, 11.0],
    [13.0, 14.0, 15.0, 16.0, 17.0],
    [15.0, 17.0, 17.0, 19.0, 19.0],
    [26.0, 27.0, 28.0, 29.0, 30.0],
]


def load_fonts():
    candidates = [
        "C:/Windows/Fonts/consolab.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/CascadiaMono.ttf",
    ]
    for path in candidates:
        p = Path(path)
        if p.exists():
            return ImageFont.truetype(str(p), 18), ImageFont.truetype(str(p), 10)
    return ImageFont.load_default(), ImageFont.load_default()


def float_to_rgba_bytes(v: float) -> tuple[int, int, int, int]:
    b = struct.pack("<f", v)
    return b[0], b[1], b[2], b[3]


def float_to_u32_hex(v: float) -> str:
    u = struct.unpack("<I", struct.pack("<f", v))[0]
    return f"0x{u:08X}"


def fmt_float(v: float) -> str:
    return str(int(v)) + ".0" if v == int(v) else str(v)


def text_center(draw, xy, text, font, fill):
    x0, y0, x1, y1 = xy
    bbox = draw.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    x = x0 + (x1 - x0 - tw) // 2
    y = y0 + (y1 - y0 - th) // 2
    draw.text((x, y), text, fill=fill, font=font)


def render_bitmap(mat, path: Path, font, font_sm) -> None:
    size = GRID * TILE + (GRID + 1) * MARGIN
    img = Image.new("RGBA", (size, size), BG + (255,))
    draw = ImageDraw.Draw(img)
    for i in range(GRID):
        for j in range(GRID):
            v = mat[i][j]
            r, g, b, a = float_to_rgba_bytes(v)
            x0 = MARGIN + j * (TILE + MARGIN)
            y0 = MARGIN + i * (TILE + MARGIN)
            x1 = x0 + TILE - 1
            y1 = y0 + TILE - 1
            # byte pattern = texture tint
            draw.rectangle([x0, y0, x1, y1], fill=(r, g, b, 255), outline=(48, 48, 56))
            # float label — always readable, always centered
            pad = 6
            pill_y0 = y0 + 10
            pill_y1 = y0 + TILE - 26
            draw.rounded_rectangle(
                [x0 + pad, pill_y0, x1 - pad, pill_y1],
                radius=6,
                fill=PILL,
            )
            text_center(draw, (x0, pill_y0, x1, pill_y1), fmt_float(v), font, FLOAT_FG)
            # hex tucked under float
            hx = float_to_u32_hex(v)
            text_center(draw, (x0, y1 - 20, x1, y1 - 2), hx, font_sm, HEX_FG)
    out = img.resize((size * 2, size * 2), Image.NEAREST)
    out.convert("RGB").save(path)


def tex_grid_html(name: str, label: str, mat) -> str:
    cells = ""
    for i in range(GRID):
        for j in range(GRID):
            v = mat[i][j]
            r, g, b, a = float_to_rgba_bytes(v)
            hx = float_to_u32_hex(v)
            cells += (
                '<div style="display:flex;flex-direction:column;align-items:center;gap:3px;padding:4px 2px;'
                'border:1px solid var(--b1);background:var(--s1);min-width:88px">'
                f'<div style="font:600 11px var(--mono);color:var(--mint)">{fmt_float(v)}</div>'
                f'<div style="width:40px;height:28px;background:rgb({r},{g},{b});border:1px solid var(--b1);border-radius:2px"></div>'
                f'<span style="font:9px var(--mono);color:var(--fg3)">{hx}</span>'
                f'<span style="font:9px var(--mono);color:var(--fg3);white-space:nowrap">[{r}, {g}, {b}, {a}]</span></div>'
            )
    return (
        f'<div style="margin:14px 0">'
        f'<div style="color:var(--fg3);font:12px var(--mono);margin-bottom:8px">'
        f'{name}[5][5][4] &ndash; {label}</div>'
        f'<div style="display:inline-grid;grid-template-columns:repeat(5,max-content);gap:4px;'
        f'padding:10px 12px;background:var(--s2);border-left:3px solid var(--amber);border-radius:4px;overflow-x:auto">'
        f'{cells}</div></div>'
    )


if __name__ == "__main__":
    out = Path(__file__).resolve().parent
    font, font_sm = load_fonts()
    render_bitmap(A, out / "matrix_tex_A.png", font, font_sm)
    render_bitmap(B, out / "matrix_tex_B.png", font, font_sm)
    render_bitmap(C, out / "matrix_tex_C.png", font, font_sm)
    grids = (
        tex_grid_html("tex_A", "matrix A packed as RGBA bytes", A)
        + tex_grid_html("tex_B", "matrix B packed as RGBA bytes", B)
        + tex_grid_html("tex_C", "output C = A + B packed as RGBA bytes", C)
    )
    (out / "_tex_grids_snip.txt").write_text(grids, encoding="utf-8")
    print("done")