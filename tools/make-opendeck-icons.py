#!/usr/bin/env python3
# Draws the OpenDeck plugin's key and list icons into src/opendeck/icons as
# PNG at the Stream Deck's two sizes, 72 and 144 pixels. A glyph on Vox's dark
# tile, grey when off and Vox yellow when on. Needs rsvg-convert (librsvg).
# Usage: tools/make-opendeck-icons.py
import os
import subprocess
import tempfile

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "opendeck", "icons")
ON, OFF, TILE = "#FFD60A", "#8a8a93", "#17171a"

GLYPHS = {
    # The tray's microphone.
    "mic": """<rect x="57" y="30" width="30" height="50" rx="15" fill="{c}"/>
              <path d="M42 70a30 30 0 0 0 60 0" fill="none" stroke="{c}" stroke-width="8" stroke-linecap="round"/>
              <path d="M72 100v12M58 114h28" fill="none" stroke="{c}" stroke-width="8" stroke-linecap="round"/>""",
    "mic-off": """<rect x="57" y="30" width="30" height="50" rx="15" fill="{c}"/>
              <path d="M42 70a30 30 0 0 0 60 0" fill="none" stroke="{c}" stroke-width="8" stroke-linecap="round"/>
              <path d="M72 100v12M58 114h28" fill="none" stroke="{c}" stroke-width="8" stroke-linecap="round"/>
              <path d="M36 30L108 114" stroke="{tile}" stroke-width="16" stroke-linecap="round"/>
              <path d="M36 30L108 114" stroke="{c}" stroke-width="8" stroke-linecap="round"/>""",
    # Three faders: a voice profile's settings.
    "profile": """<path d="M44 36v72M72 36v72M100 36v72" stroke="{c}" stroke-width="7" stroke-linecap="round" opacity="0.45"/>
              <rect x="34" y="78" width="20" height="12" rx="4" fill="{c}"/>
              <rect x="62" y="48" width="20" height="12" rx="4" fill="{c}"/>
              <rect x="90" y="66" width="20" height="12" rx="4" fill="{c}"/>""",
    # A horn with sound coming out of it.
    "sound": """<path d="M34 62h14l30-22v64l-30-22H34z" fill="{c}" stroke="{c}" stroke-width="6" stroke-linejoin="round"/>
              <path d="M92 56a22 22 0 0 1 0 32M102 44a38 38 0 0 1 0 56" fill="none" stroke="{c}" stroke-width="7" stroke-linecap="round"/>""",
    # Sparkles: the auto-tune.
    "effects": """<path d="M64 30l8 24 24 8-24 8-8 24-8-24-24-8 24-8z" fill="{c}"/>
              <path d="M102 80l4 12 12 4-12 4-4 12-4-12-12-4 12-4z" fill="{c}"/>""",
    "stop": """<rect x="44" y="44" width="56" height="56" rx="8" fill="{c}"/>""",
}


def svg(glyph, color, background=True):
    body = GLYPHS[glyph].format(c=color, tile=TILE)
    tile = f'<rect width="144" height="144" rx="24" fill="{TILE}"/>' if background else ""
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 144 144">{tile}{body}</svg>'


def render(name, document):
    with tempfile.NamedTemporaryFile("w", suffix=".svg", delete=False) as f:
        f.write(document)
        source = f.name
    try:
        for size, suffix in ((72, ""), (144, "@2x")):
            target = os.path.join(OUT, f"{name}{suffix}.png")
            subprocess.run(["rsvg-convert", "-w", str(size), "-h", str(size), "-o", target, source], check=True)
    finally:
        os.remove(source)
    print(name)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    # Key images: on a tile, as the key shows them.
    render("key-profile", svg("profile", OFF))
    render("key-profile-active", svg("profile", ON))
    render("key-sound", svg("sound", ON))
    render("key-effects", svg("effects", OFF))
    render("key-effects-on", svg("effects", ON))
    render("key-power", svg("mic-off", OFF))
    render("key-power-on", svg("mic", ON))
    render("key-stop", svg("stop", ON))
    # Action list icons: the glyph alone, on OpenDeck's own background.
    for name in ("profile", "sound", "effects", "stop"):
        render(f"action-{name}", svg(name, "#e8e8ea", background=False))
    render("action-power", svg("mic", "#e8e8ea", background=False))
    render("plugin", svg("mic", ON))
