#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["playwright"]
# ///
"""Render the README images from design/preview.html.

The images show the design preview, not a photo of the device. The preview
uses the same geometry, colours, fonts and real SMHI sample data as the
firmware (see "Preview and panel" in README.md).

    ./tools/render_readme_images.py      -> docs/images/*.png

Uses the installed Google Chrome (Playwright channel "chrome"), so no
browser download is necessary.
"""
import pathlib

from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parent.parent
PREVIEW = (ROOT / "design" / "preview.html").as_uri()
OUT = ROOT / "docs" / "images"

# (file name, data set button, page button)
SHOTS = [
    ("meteogram-dry.png", "dry", "0"),
    ("meteogram-wet.png", "wet", "0"),
    ("hours-page1.png", "wet", "1"),
]


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    with sync_playwright() as p:
        browser = p.chromium.launch(channel="chrome")
        page = browser.new_page(viewport={"width": 1200, "height": 1000},
                                device_scale_factor=1)
        page.goto(PREVIEW)
        page.click("button[data-scale='2']")
        for name, data_set, page_no in SHOTS:
            page.click(f"button[data-set='{data_set}']")
            page.click(f"button[data-page='{page_no}']")
            page.wait_for_timeout(150)
            page.locator(".bezel").screenshot(path=OUT / name)
            print(OUT / name)
        browser.close()


if __name__ == "__main__":
    main()
