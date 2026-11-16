#!/usr/bin/env python3
"""Verify the static MIFARE Classic research against the exact firmware image.

Read-only: this script never connects to a device, executes firmware, changes
NFC configuration or writes to the band. It reproduces the static findings in
docs/mifare-classic-research.md and writes JSON evidence under
docs/evidence/mifare-classic/.

Usage:
    python3 tools/inspect_classic_firmware.py
    python3 tools/inspect_classic_firmware.py --image references/vela_ap.bin
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_IMAGE = ROOT / "references/vela_ap.bin"
DEFAULT_OUT = ROOT / "docs/evidence/mifare-classic"
EXPECTED_SHA256 = "66701c11073aebbd1d925cb6e097aa56a3101fc747d9f0fc280cd250dce72a40"
MODEL = "miwear.watch.p67glt"
FIRMWARE = "3.201.026"
COMMIT = "b434549051f1d122a138bae7c434716d4864a5d2"
FLASH_BASE = 0x2C0C0000
CONFIG_OFFSET = 0x95C294
CONFIG_LENGTH_BYTE = 0x39

# File offset, role, first 8 bytes. Roles come from the static review in issue
# #1; the script verifies the exact image bytes, not a public ABI.
ADDRESSES = [
    (0x95C03C, "NFC-A config table region", "0000000003000000"),
    (0x5897E0, "NFC-A config search entry", "f1b190b500230f4c"),
    (0x587C14, "config loader", "2de9f0410024adf5"),
    (0x586D20, "loader call with table key 0x30", "302000f077ff0028"),
    (0x590D14, "NFA_GetConfig", "2de9f04105460e46"),
    (0x59B230, "NFC_SetConfig wrapper", "0b4680b5014600af"),
    (0x59B240, "NFC_GetConfig wrapper", "0b4680b5014600af"),
    (0x59A134, "CORE_SET_CONFIG builder", "2de9f041054600af"),
    (0x59A028, "CORE_GET_CONFIG builder", "2de9f041054600af"),
    (0x50D1F0, "nfc_nfcid_set_config_callback (stub)", "90b5064c83b00420"),
    (0x50D28C, "nfc_nfcid_command_batch", "90b5064c83b00420"),
    (0x5A5510, "m1_sector_read", "2de9f04716460c46"),
    (0x58C31C, "NXP block read via NFA_SendRawFrame", "2c4b1b78012b35d1"),
    (0x5A6B74, "SeApi_WiredTransceive_lower_layer", "2de9f04f8bb00023"),
    (0x5A9CD4, "SeApi_open_card_emulation_lower_layer", "80b5002000afe2f7"),
    (0x50C434, "ISSUE_CARD handler", "2de9f04387b002af"),
    (0x510AE0, "ADD_CARD handler", "2de9f04f04469db0"),
]

EXPECTED_PARAMS = {
    0x30: "08",  # LA_BIT_FRAME_SDD
    0x31: "03",  # LA_PLATFORM_CFG
    0x32: "20",  # LA_SEL_INFO
    0x33: "01020304",  # LA_NFCID1
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_config(image: bytes) -> dict:
    if CONFIG_OFFSET + 1 + CONFIG_LENGTH_BYTE > len(image):
        raise ValueError("Config offset is outside the image")
    length = image[CONFIG_OFFSET]
    if length != CONFIG_LENGTH_BYTE:
        raise ValueError(f"Unexpected config length byte: {length:#04x}")
    command = image[CONFIG_OFFSET + 1 : CONFIG_OFFSET + 1 + length]
    if len(command) != length:
        raise ValueError("Truncated config command")
    if command[:2] != b"\x20\x02":
        raise ValueError(f"Unexpected NCI header: {command[:2].hex()}")
    payload_length = command[2]
    if payload_length != length - 3:
        raise ValueError(f"Unexpected NCI payload length: {payload_length}")
    declared = command[3]
    if declared != 16:
        raise ValueError(f"Unexpected parameter count: {declared}")

    params = []
    at = 4
    truncated = None
    while len(params) < declared:
        if at + 2 > len(command):
            truncated = {
                "id": None,
                "declared_length": None,
                "available": 0,
                "value_hex": "",
                "reason": "payload ended before parameter header",
            }
            break
        parameter_id = command[at]
        parameter_length = command[at + 1]
        value_start = at + 2
        available = min(parameter_length, len(command) - value_start)
        value = command[value_start : value_start + available]
        entry = {
            "id": f"0x{parameter_id:02x}",
            "declared_length": parameter_length,
            "available": available,
            "value_hex": value.hex(),
        }
        if available < parameter_length:
            entry["truncated"] = True
            truncated = entry
            break
        params.append(entry)
        at = value_start + parameter_length

    by_id = {entry["id"]: entry["value_hex"] for entry in params}
    for parameter_id, expected in EXPECTED_PARAMS.items():
        key = f"0x{parameter_id:02x}"
        if by_id.get(key) != expected:
            raise ValueError(f"Unexpected value for {key}: {by_id.get(key)} != {expected}")

    return {
        "offset": f"0x{CONFIG_OFFSET:06x}",
        "xip": f"0x{CONFIG_OFFSET + FLASH_BASE:08x}",
        "length_byte": length,
        "command_hex": command.hex(),
        "nci_header": command[:3].hex(),
        "nci_payload_length": payload_length,
        "declared_parameters": declared,
        "complete_parameters": len(params),
        "params": params,
        "truncated_param": truncated,
        "nfcid1": by_id.get("0x33"),
        "listen_params": {
            "0x30_LA_BIT_FRAME_SDD": by_id.get("0x30"),
            "0x31_LA_PLATFORM_CFG": by_id.get("0x31"),
            "0x32_LA_SEL_INFO": by_id.get("0x32"),
            "0x33_LA_NFCID1": by_id.get("0x33"),
        },
    }


def check_addresses(image: bytes) -> list:
    checks = []
    for offset, role, expected in ADDRESSES:
        actual = image[offset : offset + len(expected) // 2].hex()
        checks.append(
            {
                "offset": f"0x{offset:06x}",
                "xip": f"0x{offset + FLASH_BASE:08x}",
                "role": role,
                "expected_prefix": expected,
                "actual_prefix": actual,
                "ok": actual == expected,
            }
        )
    return checks


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=DEFAULT_IMAGE)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()

    image_path = args.image.resolve()
    image = image_path.read_bytes()
    sha256 = digest(image)
    if sha256 != EXPECTED_SHA256:
        raise SystemExit(f"Image SHA-256 mismatch: {sha256}")

    config = parse_config(image)
    addresses = check_addresses(image)
    failed = [item for item in addresses if not item["ok"]]
    if failed:
        raise SystemExit("Address prefix mismatch: " + ", ".join(item["offset"] for item in failed))

    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    manifest = {
        "schema": 1,
        "scope": "Static firmware analysis only; no device access, no NFC command, no config or flash change.",
        "model": MODEL,
        "firmware": FIRMWARE,
        "commit": COMMIT,
        "image": str(image_path.relative_to(ROOT)) if image_path.is_relative_to(ROOT) else str(image_path),
        "image_sha256": sha256,
        "result": "PASS",
        "config": config,
        "address_checks": addresses,
        "hardware_tested": False,
        "limits": [
            "Roles and addresses are from the static review, not a public ABI.",
            "The listen configuration is a built-in template, not a measured band UID.",
            "The last declared parameter 0xA0 is truncated in this static blob; runtime construction is not proven.",
            "No CORE_GET_CONFIG was sent to the controller.",
            "No MIFARE Classic card was read, created or personalized on the eSE.",
            "No physical emulation or intercom test was performed.",
        ],
    }
    (out / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
    (out / "nfc-a-config.json").write_text(json.dumps(config, ensure_ascii=False, indent=2) + "\n")
    (out / "addresses.json").write_text(json.dumps(addresses, ensure_ascii=False, indent=2) + "\n")

    summary = {
        "result": "PASS",
        "image_sha256": sha256,
        "config_offset": config["offset"],
        "config_xip": config["xip"],
        "declared_parameters": config["declared_parameters"],
        "complete_parameters": config["complete_parameters"],
        "truncated_param": config["truncated_param"],
        "listen_params": config["listen_params"],
        "address_checks": len(addresses),
        "evidence": str(out),
        "hardware_tested": False,
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
