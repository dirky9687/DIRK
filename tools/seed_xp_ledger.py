#!/usr/bin/env python3
"""Seed DIRK v0.7.9 XP registry using existing Wardrive session histories.

Run with the SD root path after mounting it on Kali:
    python3 tools/seed_xp_ledger.py /run/media/USER/CARD --write
This script does not change career XP or the original Wardrive logs.
"""
import argparse
import csv
import json
from pathlib import Path
import re

MAC = re.compile(r"(?i)^[0-9a-f]{2}(:[0-9a-f]{2}){5}$")
DIR = Path("dirk/wardrive/config/xp-contacts")

def key(kind, value):
    value = (value or "").strip()
    if not MAC.fullmatch(value):
        return None
    mac = bytes.fromhex(value.replace(":", ""))
    if mac == bytes(6) or (kind == 1 and mac[0] & 1):
        return None
    return bytes([kind]) + mac

def bucket(record):
    h = 0
    for b in record:
        h = ((h * 33) ^ b) & 255
    return f"{h & 63:02X}.bin"

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("sd_root", type=Path)
    ap.add_argument("--write", action="store_true", help="Create/update registry files (otherwise preview)")
    args = ap.parse_args()
    root = args.sd_root
    sessions = root / "dirk/wardrive/sessions"
    if not sessions.is_dir():
        ap.error(f"No existing Wardrive session directory: {sessions}")
    index = root / DIR
    known = set()
    if index.is_dir():
        for f in index.glob("*.bin"):
            data = f.read_bytes()
            if len(data) % 7:
                ap.error(f"Corrupt 7-byte XP record file: {f}")
            known.update(data[i:i+7] for i in range(0, len(data), 7))
    initial = len(known)
    found_files = 0
    for f in sorted(sessions.rglob("dirk.jsonl")):
        found_files += 1
        with f.open(encoding="utf-8", errors="replace") as fin:
            for line in fin:
                try:
                    item = json.loads(line)
                except (ValueError, TypeError):
                    continue
                if item.get("type") == "wifi":
                    rec = key(1, item.get("bssid"))
                    if rec: known.add(rec)
    for f in sorted(sessions.rglob("ble.csv")):
        found_files += 1
        with f.open(encoding="utf-8", errors="replace", newline="") as fin:
            reader = csv.DictReader(fin)
            for item in reader:
                rec = key(2, item.get("address"))
                if rec: known.add(rec)
    print(f"History files: {found_files}; known: {initial}; distinct total: {len(known)}; to seed: {len(known)-initial}")
    if not args.write:
        print("Preview only. Add --write to seed history before flashing firmware.")
        return
    index.mkdir(parents=True, exist_ok=True)
    # Preserve existing file ordering: append only records not already recorded.
    current = set()
    for f in index.glob("*.bin"):
        data = f.read_bytes()
        current.update(data[i:i+7] for i in range(0, len(data), 7))
    by_bucket = {}
    for rec in known - current:
        by_bucket.setdefault(bucket(rec), []).append(rec)
    for name, records in by_bucket.items():
        with (index / name).open("ab") as fout:
            for rec in sorted(records): fout.write(rec)
    print(f"Seeded {sum(map(len,by_bucket.values()))} previously seen contacts. Career XP unchanged.")

if __name__ == "__main__":
    main()
