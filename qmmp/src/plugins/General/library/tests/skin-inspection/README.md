# Winamp Skin Inspection Fixtures

Run `python3 inspect_skins.py` from this directory. Pillow is required. The script reports dimensions and writes PNG previews of `gen*.bmp`, `cbuttons.bmp`, `main.bmp`, and `titlebar.bmp` assets to `previews/`.

- `garfield6.wsz` is the supplied Garfield 6 example. It has no `gen.bmp` or `genex.bmp`, exercising the no-general-window-assets fallback.
- `yufo.wsz` includes `gen_gslyrics.bmp`, exercising discovery of a plugin-specific general-window bitmap.

Sources:

- https://skins.webamp.org/skin/9b6a0ded052592c8c0a5f111246eec66/garfield6.wsz
- https://r2.webampskins.org/skins/5ea2d974dad5507b83a6ddff7003a8ee.wsz