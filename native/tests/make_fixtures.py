#!/usr/bin/env python3
"""Generate small valid and malformed GGUF fixtures for the native tests."""
from __future__ import annotations

import hashlib
import struct
from pathlib import Path

OUT = Path(__file__).parent / "fixtures"
ALIGNMENT = 32
MAGIC = b"GGUF"


def s(value: str) -> bytes:
    b = value.encode()
    return struct.pack("<Q", len(b)) + b


def kv_string(key: str, value: str) -> bytes:
    return s(key) + struct.pack("<I", 8) + s(value)


def kv_u32(key: str, value: int) -> bytes:
    return s(key) + struct.pack("<I", 4) + struct.pack("<I", value)


def tensor(name: str, dims: list[int], ttype: int, offset: int) -> bytes:
    return (s(name) + struct.pack("<I", len(dims)) +
            struct.pack("<" + "Q" * len(dims), *dims) +
            struct.pack("<I", ttype) + struct.pack("<Q", offset))


def payload() -> bytes:
    a = bytes((i * 7) % 256 for i in range(64))       # F32, 16 elements
    b = bytes((0xA0 + i) % 256 for i in range(16))    # F16, 8 elements
    c = bytes((i * 13 + 5) % 256 for i in range(144)) # Q4_0, 256 elements
    pad = b"\0" * 16                                   # align c from 80 to 96
    return a + b + pad + c


def build(tensors: list[bytes], payload_bytes: bytes, version: int = 3,
          magic: bytes = MAGIC) -> bytes:
    kv = kv_u32("general.alignment", ALIGNMENT) + kv_string("general.architecture", "test")
    header = (magic + struct.pack("<I", version) +
              struct.pack("<Q", len(tensors)) + struct.pack("<Q", 2) +
              kv + b"".join(tensors))
    pad = (-len(header)) % ALIGNMENT
    return header + b"\0" * pad + payload_bytes


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    base_tensors = [
        tensor("a", [16], 0, 0),
        tensor("b", [8], 1, 64),
        tensor("c", [256], 2, 96),
    ]
    data = payload()
    valid = build(base_tensors, data)
    (OUT / "valid.gguf").write_bytes(valid)
    (OUT / "expected.sha").write_text(hashlib.sha256(valid).hexdigest() + "\n")

    (OUT / "truncated.gguf").write_bytes(valid[:-10])
    (OUT / "wrong_endian.gguf").write_bytes(build(base_tensors, data, magic=b"FUGG"))
    (OUT / "bad_version.gguf").write_bytes(build(base_tensors, data, version=99))

    zero = [base_tensors[0], tensor("b", [0], 1, 64), base_tensors[2]]
    (OUT / "zero_tensor.gguf").write_bytes(build(zero, data))

    overlap = [base_tensors[0], tensor("b", [8], 1, 0), base_tensors[2]]
    (OUT / "overlap.gguf").write_bytes(build(overlap, data))

    unaligned = [base_tensors[0], base_tensors[1], tensor("c", [256], 2, 100)]
    (OUT / "unaligned.gguf").write_bytes(build(unaligned, data))

    bad_block = [base_tensors[0], base_tensors[1], tensor("c", [17], 2, 96)]
    (OUT / "bad_block.gguf").write_bytes(build(bad_block, data))

    oob = [base_tensors[0], base_tensors[1], tensor("c", [256], 2, 4096)]
    (OUT / "out_of_bounds.gguf").write_bytes(build(oob, data))

    expected_payload_sha = hashlib.sha256(data[:64]).hexdigest()
    (OUT / "expected_tensor_a.sha").write_text(expected_payload_sha + "\n")
    print(f"wrote {len(list(OUT.glob('*.gguf')))} fixtures to {OUT}")


if __name__ == "__main__":
    main()
