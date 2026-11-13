#!/usr/bin/env python3
"""Verify preserved releases, exact firmware bindings and recorded hardware proof.

Offline and read-only. This checks saved evidence, never connects to a device
and does not substitute for running new hardware tests after a code change.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

from package_face import build as pack_face

ROOT = Path(__file__).resolve().parents[1]
FLASH_BASE = 0x2C0C0000


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def fingerprints(image):
    entry = re.compile(r"\{\s*(0x[0-9a-fA-F]+)u\s*,\s*\{([^{}]+)\}\s*\}")
    checks = []
    expected_counts = {
        "band/module_fingerprints.h": 2,
        "band/transport_fingerprints.h": 8,
        "band/nfc_fingerprints.h": 17,
        "band/diagnostic.c": 3,
    }
    for name, count in expected_counts.items():
        found = entry.findall((ROOT / name).read_text())
        require(len(found) == count, f"Unexpected fingerprint count in {name}")
        checks.extend((name, int(address, 16), body) for address, body in found)
    clock = (ROOT / "band/native_clock.c").read_text()
    for variable, address in (("first", 0x2C1C8E70), ("second", 0x2C1C9024)):
        match = re.search(r"uint8_t\s+" + variable + r"\[24\]\s*=\s*\{([^{}]+)\}", clock)
        require(match is not None, f"Missing clock fingerprint {variable}")
        checks.append(("band/native_clock.c", address, match.group(1)))
    for name, address, body in checks:
        values = re.findall(r"0x([0-9a-fA-F]{2})\b", body)
        require(len(values) == 24, f"Invalid fingerprint length in {name}")
        expected = bytes(int(value, 16) for value in values)
        offset = address - FLASH_BASE
        require(0 <= offset <= len(image) - 24, f"Address outside image: {address:#x}")
        require(image[offset:offset + 24] == expected, f"Firmware mismatch: {name} {address:#x}")
    return len(checks)


def hardware_evidence():
    observation = json.loads((ROOT / "docs/evidence/pixel-gatt-v6-observation.json").read_text())
    log = (ROOT / "docs/evidence/pixel-v6-pass-log.txt").read_text()
    require(observation["result"] == "PASS" and observation["run"] == 1, "Unexpected recorded run")
    challenge_match = re.search(r"NFC TX challenge=([0-9A-F]+) run=1", log)
    require(challenge_match is not None, "Missing NFC challenge in recorded log")
    challenge = bytes.fromhex(challenge_match.group(1))
    require(len(challenge) == 16, "Invalid recorded challenge length")
    capdu = "8010000010" + challenge.hex().upper() + "00"
    rapdu = bytes(value ^ 0xA5 for value in reversed(challenge)).hex().upper() + "9000"
    select = "00A4040008F04D42415044550100"
    for expected in (
        f"NFC TX SELECT {select}",
        f"BT RX seq={observation['select_ble_seq']} CAPDU={select}",
        f"BT TX seq={observation['select_ble_seq']} ATT подтверждён RAPDU=9000",
        "NFC RX SELECT 9000",
        f"BT RX seq={observation['challenge_ble_seq']} CAPDU={capdu}",
        f"BT TX seq={observation['challenge_ble_seq']} ATT подтверждён RAPDU={rapdu}",
        f"NFC RX {rapdu}",
        "PASS · NFC → BT → Pixel → BT → NFC",
        f"STATUS seq={observation['cleanup_stop_seq']} STOPPED",
        f"STATUS seq={observation['post_cleanup_query_seq']} STOPPED",
    ):
        require(expected in log, f"Missing recorded proof: {expected}")
    return {"recorded_runs": 1, "result": "PASS", "duration_ms": observation["elapsed_ms_app"],
            "scope": "Saved synthetic run; no new hardware test performed"}


def verify():
    release_dir = ROOT / "releases"
    manifest = json.loads((release_dir / "manifest.json").read_text())
    sums = {}
    for line in (release_dir / "SHA256SUMS").read_text().splitlines():
        sha, name = line.split(maxsplit=1)
        sums[name.lstrip("*")] = sha
    require(set(sums) == set(manifest["files"]), "SHA256SUMS and manifest file sets differ")
    for name, info in manifest["files"].items():
        require(Path(name).name == name, "Release filename must be local to releases/")
        payload = (release_dir / name).read_bytes()
        require(len(payload) == info["bytes"], f"Release size mismatch: {name}")
        require(digest(payload) == info["sha256"] == sums[name], f"Release hash mismatch: {name}")
    provenance = json.loads((ROOT / "references/provenance.json").read_text())
    for name in ("recorder.face", "vela_ap.bin"):
        require(digest((ROOT / "references" / name).read_bytes()) == provenance[name]["sha256"],
                f"Reference hash mismatch: {name}")
    count = fingerprints((ROOT / "references/vela_ap.bin").read_bytes())
    elf_name = next(name for name in sums if name.endswith(".elf"))
    face_name = next(name for name in sums if name.endswith(".face"))
    elf = (release_dir / elf_name).read_bytes()
    packed, _ = pack_face((ROOT / "references/recorder.face").read_bytes(),
                          (ROOT / "watchface/main.lua").read_bytes(), elf)
    require(packed == (release_dir / face_name).read_bytes(), "Frozen FACE does not match template/Lua/ELF")
    require(manifest["elf_resource_bytes"] - len(elf) == manifest["elf_spare_bytes"], "Wrong ELF capacity")
    return {"result": "PASS", "releases_verified": len(sums), "firmware_fingerprints_verified": count,
            "face_repack_byte_identical": True, "elf_spare_bytes": manifest["elf_spare_bytes"],
            "hardware_evidence": hardware_evidence()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, help="Optional JSON report path")
    args = parser.parse_args()
    try:
        result = verify()
    except (ValueError, OSError, KeyError, StopIteration) as error:
        raise SystemExit(f"Project verification FAILED: {error}") from error
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
