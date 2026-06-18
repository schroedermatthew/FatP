#!/usr/bin/env python3
import re
import shutil
from pathlib import Path

GPU = Path(r"C:\Users\mtthw\Desktop\AI Projects\FatP\Teaching\GPU")
ARTIFACTS = GPU / "artifacts"
NO_DIR = ARTIFACTS / "Art_History" / "NO"
IMG_EXT = {".jpg", ".jpeg", ".png", ".gif", ".webp"}


def main() -> None:
    NO_DIR.mkdir(parents=True, exist_ok=True)

    html_text = ""
    for html in GPU.rglob("*.html"):
        html_text += html.read_text(encoding="utf-8", errors="replace") + "\n"

    refs: set[str] = set()
    for match in re.finditer(r"artifacts/([^\"')>\s]+)", html_text):
        ref = match.group(1)
        ref = ref.replace("%20", " ").replace("%5B", "[").replace("%5D", "]")
        refs.add(ref.replace("\\", "/"))
        refs.add(Path(ref).name)

    used: list[str] = []
    unused: list[Path] = []

    for path in sorted(ARTIFACTS.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in IMG_EXT:
            continue
        if NO_DIR in path.parents:
            continue
        rel = path.relative_to(ARTIFACTS).as_posix()
        name = path.name
        is_used = (
            rel in refs
            or name in refs
            or any(r.endswith("/" + name) or r == name for r in refs)
        )
        if is_used:
            used.append(rel)
        else:
            unused.append(path)

    print(f"USED ({len(used)}):")
    for item in used:
        print(f"  {item}")

    print(f"\nUNUSED ({len(unused)}):")
    moved: list[tuple[str, str]] = []
    for src in unused:
        print(f"  {src.relative_to(ARTIFACTS).as_posix()}")
        dest = NO_DIR / src.name
        if dest.exists():
            stem, suffix = src.stem, src.suffix
            n = 2
            while dest.exists():
                dest = NO_DIR / f"{stem}_{n}{suffix}"
                n += 1
        shutil.move(str(src), str(dest))
        moved.append((src.relative_to(ARTIFACTS).as_posix(), dest.name))

    print(f"\nMOVED ({len(moved)}):")
    for rel, name in moved:
        print(f"  {rel} -> NO/{name}")


if __name__ == "__main__":
    main()