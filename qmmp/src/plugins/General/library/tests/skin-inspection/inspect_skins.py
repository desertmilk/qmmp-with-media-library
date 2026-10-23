#!/usr/bin/env python3

"""Inspect general-purpose bitmap assets in the bundled Winamp skin fixtures."""

from io import BytesIO
from pathlib import Path
from zipfile import ZipFile

from PIL import Image


FIXTURE_DIR = Path(__file__).resolve().parent
PREVIEW_DIR = FIXTURE_DIR / "previews"


def inspect_skin(archive_path: Path) -> int:
    with ZipFile(archive_path) as archive:
        bitmaps = sorted(
            name
            for name in archive.namelist()
            if Path(name).suffix.lower() == ".bmp"
            and (
                Path(name).name.lower().startswith("gen")
                or Path(name).name.lower() in {"cbuttons.bmp", "main.bmp", "titlebar.bmp"}
            )
        )

        if not bitmaps:
            print(f"{archive_path.name}: no relevant bitmap assets")
            return 0

        PREVIEW_DIR.mkdir(exist_ok=True)
        general_bitmaps = 0
        for name in bitmaps:
            image = Image.open(BytesIO(archive.read(name)))
            image.load()
            preview_path = PREVIEW_DIR / f"{archive_path.stem}_{Path(name).stem}.png"
            image.save(preview_path)
            print(f"{archive_path.name}: {name}: {image.width}x{image.height} -> {preview_path.name}")
            general_bitmaps += Path(name).name.lower().startswith("gen")

        if not general_bitmaps:
            print(f"{archive_path.name}: no gen*.bmp assets")

    return general_bitmaps


def main() -> None:
    fixtures = sorted(FIXTURE_DIR.glob("*.wsz"))
    if not fixtures:
        raise SystemExit("No .wsz fixtures found")

    total = sum(inspect_skin(path) for path in fixtures)
    print(f"Found {total} gen*.bmp asset(s) across {len(fixtures)} fixture(s)")


if __name__ == "__main__":
    main()