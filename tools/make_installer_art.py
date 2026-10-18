"""Generates installer artwork (icon + wizard images) from Assets/fox.png.
Run:  python tools/make_installer_art.py   (needs Pillow). Output is committed, CI does not need to run it."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

root = Path(__file__).resolve().parent.parent
fox = Image.open(root / "Assets" / "fox.png").convert("RGBA")
out = root / "installer" / "windows"
out.mkdir(parents=True, exist_ok=True)

TOP, BOT = (10, 1, 24), (43, 10, 71)
PINK, PURPLE = (255, 63, 164), (154, 92, 255)

def gradient(w, h):
    img = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / (h - 1)
        d.line([(0, y), (w, y)], fill=tuple(int(TOP[i] + (BOT[i] - TOP[i]) * t) for i in range(3)))
    return img

def paste_fox(img, size, center):
    f = fox.copy(); f.thumbnail((size, size), Image.LANCZOS)
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([center[0]-size*0.7, center[1]-size*0.7, center[0]+size*0.7, center[1]+size*0.7], fill=PINK + (70,))
    img.paste(Image.alpha_composite(img.convert("RGBA"), glow.filter(ImageFilter.GaussianBlur(size*0.25))).convert("RGB"))
    img.paste(f, (int(center[0]-f.width/2), int(center[1]-f.height/2)), f)

# large wizard image (welcome / finish pages): 164x314, perspective grid at the bottom
W, H = 164, 314
big = gradient(W, H)
d = ImageDraw.Draw(big)
vy = 215
for i in range(-9, 10):
    d.line([(W/2, vy), (W/2 + i*34, H)], fill=(90, 45, 150))
for i in range(1, 8):
    y = vy + (H - vy) * (i/7)**2
    d.line([(0, y), (W, y)], fill=(90, 45, 150))
paste_fox(big, 110, (W/2, 105))
big.save(out / "wizard-large.bmp")

# small wizard image: 55x55
sm = gradient(55, 55)
paste_fox(sm, 42, (27.5, 27.5))
sm.save(out / "wizard-small.bmp")

# app icon
ico = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
bg = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
m = Image.new("L", (256, 256), 0); ImageDraw.Draw(m).rounded_rectangle([0, 0, 255, 255], 52, fill=255)
g = gradient(256, 256).convert("RGBA"); bg.paste(g, (0, 0), m)
f = fox.copy(); f.thumbnail((190, 190), Image.LANCZOS)
bg.paste(f, ((256-f.width)//2, (256-f.height)//2), f)
bg.save(out / "KitsuneEQ.ico", sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
print("done")
