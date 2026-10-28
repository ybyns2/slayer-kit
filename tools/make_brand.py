from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

RES = Path(__file__).resolve().parent.parent / "res"
S = 1024
ROUGE_A = (255, 46, 77)
ROUGE_B = (255, 138, 61)

def degrade(taille, a, b):
    g = Image.new("RGBA", (taille, taille))
    px = g.load()
    for y in range(taille):
        for x in range(taille):
            t = (x + y) / (2 * (taille - 1))
            px[x, y] = tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3)) + (255,)
    return g

def lame():
    m = Image.new("L", (S, S), 0)
    d = ImageDraw.Draw(m)
    g, dr, h, b, t, c = 352, 672, 232, 792, 112, 56
    mi = (h + b) // 2
    d.polygon([(g + c, h), (dr, h), (dr, h + t), (g + t, h + t), (g + t, mi - t // 2),
               (g, mi - t // 2), (g, h + c)], fill=255)
    d.rectangle([g, mi - t // 2, dr, mi + t // 2], fill=255)
    d.polygon([(dr - t, mi + t // 2), (dr, mi + t // 2), (dr, b - c), (dr - c, b), (g, b),
               (g, b - t), (dr - t, b - t)], fill=255)
    k = 0.21
    m = m.transform((S, S), Image.AFFINE, (1, k, -k * S / 2, 0, 1, 0), resample=Image.BICUBIC)
    ImageDraw.Draw(m).polygon([(700, 120), (736, 120), (340, 904), (304, 904)], fill=0)
    return m

fond = Image.new("RGBA", (S, S), (0, 0, 0, 0))
ImageDraw.Draw(fond).rounded_rectangle([40, 40, S - 40, S - 40], radius=220, fill=(16, 12, 22, 255))
masque = lame()
halo = Image.new("RGBA", (S, S), ROUGE_A + (0,))
halo.putalpha(masque.filter(ImageFilter.GaussianBlur(40)).point(lambda v: int(v * 0.55)))
fond.alpha_composite(halo)
grad = degrade(S, ROUGE_A, ROUGE_B)
grad.putalpha(masque)
fond.alpha_composite(grad)

fond.save(RES / "logo.png")
fond.resize((256, 256), Image.LANCZOS).save(
    RES / "app.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
print("ok", RES)
