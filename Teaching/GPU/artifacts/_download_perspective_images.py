#!/usr/bin/env python3
"""Download perspective-painting and portrait images into Art_History/."""

import json
import re
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

OUT_DIR = Path(r"C:\Users\mtthw\Desktop\AI Projects\FatP\Teaching\GPU\artifacts\Art_History")
WORKTREE_DIR = Path(__file__).resolve().parent
USER_AGENT = "FatP-Teaching-Artifact/1.0 (educational; local)"
REQUEST_DELAY = 4.0
THUMB_WIDTH = 1200

WIKI_ITEMS = [
    ("Cimabue_Maesta", "Santa_Trinita_Maestà"),
    ("Masaccio_Holy_Trinity", "Holy_Trinity_(Masaccio)"),
    ("Piero_Flagellation_of_Christ", "Flagellation_of_Christ_(Piero_della_Francesca)"),
    ("Raphael_School_of_Athens", "The_School_of_Athens"),
    ("Titian_Pieta", "Pietà_(Titian)"),
    ("Titian_Death_of_Actaeon", "The_Death_of_Actaeon"),
    ("Titian_Flaying_of_Marsyas", "Flaying_of_Marsyas_(Titian)"),
    ("Cimabue_portrait", "Cimabue"),
    ("Masaccio_portrait", "Masaccio"),
    ("Piero_della_Francesca_portrait", "Piero_della_Francesca"),
    ("Leonardo_da_Vinci_portrait", "Leonardo_da_Vinci"),
    ("Raphael_portrait", "Raphael"),
    ("Titian_portrait", "Titian"),
    ("Filippo_Brunelleschi_portrait", "Filippo_Brunelleschi"),
    ("Leon_Battista_Alberti_portrait", "Leon_Battista_Alberti"),
    ("Donato_Bramante_portrait", "Donato_Bramante"),
    ("Girard_Desargues_portrait", "Girard_Desargues"),
    ("Jean_Victor_Poncelet_portrait", "Jean-Victor_Poncelet"),
    ("August_Ferdinand_Mobius_portrait", "August_Ferdinand_Möbius"),
    ("Julius_Plucker_portrait", "Julius_Plücker"),
]


def fetch(url: str, timeout: int = 60) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    for attempt in range(8):
        try:
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                return resp.read()
        except urllib.error.HTTPError as exc:
            if exc.code == 429 and attempt < 7:
                wait = REQUEST_DELAY * (2 ** attempt)
                print(f"  rate limited, waiting {wait:.0f}s...")
                time.sleep(wait)
                continue
            raise
    raise RuntimeError("unreachable")


def api_get(params: dict) -> dict:
    base = "https://en.wikipedia.org/w/api.php"
    params = {**params, "format": "json"}
    url = base + "?" + urllib.parse.urlencode(params)
    return json.loads(fetch(url).decode("utf-8"))


def wiki_thumb_image(title: str) -> str | None:
    data = api_get(
        {
            "action": "query",
            "titles": title,
            "prop": "pageimages|pageprops",
            "pithumbsize": THUMB_WIDTH,
            "redirects": 1,
        }
    )
    pages = data.get("query", {}).get("pages", {})
    for page in pages.values():
        if "missing" in page:
            return None
        thumb = page.get("thumbnail")
        if thumb and thumb.get("source"):
            return thumb["source"]
        orig = page.get("original")
        if orig and orig.get("source"):
            return orig["source"]
    return None


def wiki_first_file_image(title: str) -> str | None:
    data = api_get(
        {
            "action": "query",
            "titles": title,
            "prop": "images",
            "imlimit": 20,
            "redirects": 1,
        }
    )
    skip = re.compile(r"(icon|logo|symbol|edit|padlock|question|wikimedia|commons|ambox)", re.I)
    pages = data.get("query", {}).get("pages", {})
    for page in pages.values():
        for img in page.get("images", []):
            name = img.get("title", "")
            if not name.startswith("File:") or skip.search(name):
                continue
            info = api_get(
                {
                    "action": "query",
                    "titles": name,
                    "prop": "imageinfo",
                    "iiprop": "url",
                    "iiurlwidth": THUMB_WIDTH,
                }
            )
            time.sleep(REQUEST_DELAY)
            for fpage in info.get("query", {}).get("pages", {}).values():
                ii = fpage.get("imageinfo", [{}])[0]
                url = ii.get("thumburl") or ii.get("url")
                if url:
                    return url
    return None


def ext_from_url(url: str, fallback: str = ".jpg") -> str:
    path = urllib.parse.urlparse(url).path.lower()
    for ext in (".jpg", ".jpeg", ".png", ".webp", ".gif", ".svg"):
        if path.endswith(ext):
            return ext
    return fallback


def already_have(stem: str, directory: Path) -> Path | None:
    for ext in (".jpg", ".jpeg", ".png", ".webp", ".gif", ".svg"):
        p = directory / f"{stem}{ext}"
        if p.exists() and p.stat().st_size > 1000:
            return p
    return None


def copy_from_worktree(stem: str) -> Path | None:
    src = already_have(stem, WORKTREE_DIR)
    if not src:
        return None
    dest = OUT_DIR / src.name
    if dest.exists() and dest.stat().st_size == src.stat().st_size:
        return dest
    dest.write_bytes(src.read_bytes())
    return dest


def resolve_image(title: str) -> str | None:
    url = wiki_thumb_image(title)
    time.sleep(REQUEST_DELAY)
    if url:
        return url
    return wiki_first_file_image(title)


def download(url: str, dest: Path) -> None:
    dest.write_bytes(fetch(url, timeout=120))


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    results = []

    for stem, title in WIKI_ITEMS:
        existing = already_have(stem, OUT_DIR)
        if existing:
            results.append((stem, "SKIP", existing.name))
            print(f"SKIP  {existing.name}")
            continue

        copied = copy_from_worktree(stem)
        if copied:
            results.append((stem, "COPY", copied.name))
            print(f"COPY  {copied.name}")
            continue

        try:
            url = resolve_image(title)
            if not url:
                results.append((stem, "MISSING", title))
                print(f"FAIL  {stem}: no image for {title}")
                continue
            dest = OUT_DIR / f"{stem}{ext_from_url(url)}"
            download(url, dest)
            results.append((stem, "OK", dest.name))
            print(f"OK    {dest.name}")
            time.sleep(REQUEST_DELAY)
        except Exception as exc:
            results.append((stem, "ERROR", str(exc)))
            print(f"ERROR {stem}: {exc}")
            time.sleep(REQUEST_DELAY * 2)

    print(f"\n--- {OUT_DIR} ---")
    ok = sum(1 for _, status, _ in results if status in ("OK", "SKIP", "COPY"))
    print(f"Ready {ok}/{len(WIKI_ITEMS)}")
    for stem, status, detail in results:
        if status not in ("OK", "SKIP", "COPY"):
            print(f"  {status}: {stem} - {detail}")


if __name__ == "__main__":
    main()