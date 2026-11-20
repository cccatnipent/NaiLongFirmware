"""Prepare the user's RGB565 images and an Opus laugh without changing the originals."""

import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path


IMAGE_SOURCES = {
    "nl_idle.raw": "9.raw",
    "nl_laugh.raw": "2.raw",
    **{f"nl_reply{i}.raw": f"{source}.raw" for i, source in enumerate((1, 3, 4, 5, 6, 7, 8), 1)},
}


def prepare(materials: Path, output: Path, ffmpeg: str, laugh_seconds: float) -> None:
    inputs = list(materials.glob("*.ogg"))
    if len(inputs) != 1:
        raise ValueError("Materials must contain exactly one source OGG laugh")
    images = {name: (materials / source).read_bytes() for name, source in IMAGE_SOURCES.items()}
    for name, data in images.items():
        if len(data) != 240 * 240 * 2:
            raise ValueError(f"{name}: expected a 240x240 RGB565 image")
    output.mkdir(parents=True, exist_ok=True)
    for name, data in images.items():
        (output / name).write_bytes(data)
    command = [ffmpeg, "-hide_banner", "-y", "-i", str(inputs[0]), "-map_metadata", "-1"]
    if laugh_seconds > 0:
        command.extend(("-t", str(laugh_seconds)))
    command.extend(("-c:a", "libopus", "-ac", "1", "-ar", "16000", "-b:a", "24k",
                    "-frame_duration", "60", str(output / "nl_laugh.ogg")))
    subprocess.run(command, check=True)
    audio = (output / "nl_laugh.ogg").read_bytes()
    if b"OpusHead" not in audio:
        raise ValueError("The converted audio is not Ogg Opus")
    manifest = {
        "image_format": "240x240 RGB565 little endian",
        "images": {name: {"source": source, "sha256": hashlib.sha256(images[name]).hexdigest()}
                   for name, source in IMAGE_SOURCES.items()},
        "audio_source": inputs[0].name,
        "laugh_seconds": laugh_seconds if laugh_seconds > 0 else "full",
        "audio_format": "Ogg Opus, mono, 16000 Hz, 60 ms packets",
        "audio_sha256": hashlib.sha256(audio).hexdigest(),
    }
    (output / "nl_materials.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--materials", type=Path, default=root.parent / "Materials")
    parser.add_argument("--output", type=Path, default=root / "main/boards/otto-robot/nailong_assets")
    parser.add_argument("--ffmpeg", default=shutil.which("ffmpeg"))
    parser.add_argument("--laugh-seconds", type=float, default=5.0, help="0 keeps the entire clip")
    args = parser.parse_args()
    if not args.ffmpeg:
        parser.error("Specify an ffmpeg executable with --ffmpeg")
    prepare(args.materials, args.output, args.ffmpeg, args.laugh_seconds)


if __name__ == "__main__":
    main()
