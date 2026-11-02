#!/usr/bin/env python3
"""Assemble separate GPL FFB plugin and MIT Model2VR files in one zip tree.

Invoked only in a GitHub Windows packaging job. Does not modify game files.
"""
from __future__ import annotations
import argparse
import re
import shutil
import sys
import zipfile
from pathlib import Path

SECTION_DEFAULTS = {
    "Model2Modern": {
        "Enabled": 1, "ConstantGain": 100, "SpringGain": 45,
        "FrictionGain": 35, "PeriodicGain": 30, "RumbleGain": 20,
        "Gamma": 100, "BuildRate": 18, "ReversalReleaseRate": 40,
        "SoftClipKnee": 75, "SoftClipLimit": 135, "InvertConstant": 0,
        "InvertSpring": 0, "PeriodicMode": 0, "ConditionMode": 0,
        "FocusSafety": 1, "CommandLeaseMs": 180, "Telemetry": 0,
    },
    **{f"Model2Modern.{name}": {"ConstantGain": 100}
       for name in ("SegaRally", "Daytona", "Indy500", "STCC", "OverRev", "SuperGT")},
}
REQUIRED = ("dinput8.dll", "SDL2.dll", "FFBPlugin.ini",
            "FFBPluginGUI.exe", "MetroFramework.dll")


def contains_game_id_25(content: str) -> bool:
    # The upstream Model 2 directory must have its own Settings/GameId.
    settings = re.search(r"(?ims)^\[Settings\]\s*$([\s\S]*?)(?=^\[[^\r\n]+\]|\Z)", content)
    return bool(settings and re.search(r"(?im)^\s*GameId\s*=\s*25\s*$",
                                       settings.group(1)))


def set_key(text: str, section: str, key: str, value: int) -> str:
    marker = re.compile(rf"(?im)^\[{re.escape(section)}\]\s*$")
    match = marker.search(text)
    if not match:
        return text.rstrip() + f"\n[{section}]\n{key}={value}\n"
    next_section = re.search(r"(?m)^\[[^\r\n]+\]\s*$", text[match.end():])
    end = match.end() + next_section.start() if next_section else len(text)
    part = text[match.end():end]
    existing = re.compile(rf"(?im)^\s*{re.escape(key)}\s*=\s*[^\r\n]*$")
    part, count = existing.subn(f"{key}={value}", part)
    if not count:
        part = f"\n{key}={value}" + part
    return text[:match.end()] + part + text[end:]


def assemble(archive: Path, source: Path, destination: Path) -> None:
    if destination.exists():
        raise ValueError("Destination must be new, refusing to overwrite FFB files")
    compiled = source / "Release.Win32" / "M2Emulator" / "dinput8.dll"
    if not compiled.is_file():
        raise FileNotFoundError(f"Missing compiled Generic DD v2 plugin: {compiled}")
    destination.mkdir(parents=True)
    root = destination.parent / ".ffb-official-unpacked"
    root.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as zf:
        for info in zf.infolist():
            rel = Path(info.filename.replace("\\", "/"))
            if rel.is_absolute() or ".." in rel.parts:
                raise ValueError("Unsafe upstream ZIP entry")
            zf.extract(info, root)
    candidates = sorted(root.rglob("FFBPlugin.ini"))
    baseline = next((p for p in candidates
                     if contains_game_id_25(p.read_text(encoding="utf-8-sig", errors="replace"))),
                    None)
    if not baseline:
        raise RuntimeError("Upstream Model 2 distribution (Settings/GameId=25) missing")
    for item in baseline.parent.iterdir():
        target = destination / item.name
        if item.is_dir():
            shutil.copytree(item, target, dirs_exist_ok=True)
        else:
            shutil.copy2(item, target)
    for name in ("FFBPluginGUI.exe", "MetroFramework.dll"):
        if not (destination / name).is_file():
            found = next((p for p in root.rglob(name) if p.is_file()), None)
            if found:
                shutil.copy2(found, destination / name)

    shutil.copy2(compiled, destination / "dinput8.dll")
    sdl = next((p for p in (source / "packages").rglob("SDL2.dll")
                if p.is_file() and any(x in str(p).lower()
                for x in ("x86", "win32"))), None)
    if not sdl:
        raise RuntimeError("SDL2 x86 runtime missing from restored FFB packages")
    shutil.copy2(sdl, destination / "SDL2.dll")
    ini_path = destination / "FFBPlugin.ini"
    ini = ini_path.read_text(encoding="utf-8-sig", errors="replace")
    if not contains_game_id_25(ini):
        raise RuntimeError("Wrong game configuration: GameId must be 25")
    ini = set_key(ini, "Settings", "Model2ModernFFB", 1)
    ini = set_key(ini, "Settings", "InputDeviceWheelEnable", 1)
    for section, values in SECTION_DEFAULTS.items():
        for key, value in values.items():
            # Keep user or upstream values in a section when present.
            if not re.search(rf"(?im)^\[{re.escape(section)}\][ \t]*$", ini):
                ini = ini.rstrip() + f"\n[{section}]\n"
            section_start = re.search(rf"(?im)^\[{re.escape(section)}\]\s*$", ini)
            next_start = re.search(r"(?m)^\[[^\r\n]+\]\s*$",
                                   ini[section_start.end():]) if section_start else None
            area = ini[section_start.end():section_start.end()+next_start.start()] \
                if section_start and next_start else ini[section_start.end():] \
                if section_start else ""
            if not re.search(rf"(?im)^\s*{re.escape(key)}\s*=", area):
                ini = set_key(ini, section, key, value)
    ini_path.write_text(ini, encoding="utf-8")
    shutil.copy2(source / "LICENSE", destination / "LICENSE-FFBArcadePlugin-GPLv3.txt")
    shutil.copy2(source / "MODEL2_MODERN_FFB.md", destination / "MODEL2_MODERN_FFB.md")
    missing = [name for name in REQUIRED if not (destination / name).is_file()]
    if missing:
        raise RuntimeError(f"Missing required Generic DD files: {missing}")
    if not contains_game_id_25(ini_path.read_text(encoding="utf-8")):
        raise RuntimeError("Model 2 GameId changed unexpectedly")
    shutil.rmtree(root)
    print(f"FFB Generic DD v2 bundle complete: {destination}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--official-zip", type=Path, required=True)
    parser.add_argument("--ffb-source", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    args = parser.parse_args()
    assemble(args.official_zip, args.ffb_source, args.destination)
    return 0


if __name__ == "__main__":
    sys.exit(main())
