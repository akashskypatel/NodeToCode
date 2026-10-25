#!/usr/bin/env python3
"""Split a unified patch into deterministic connector-sized parts.

The script writes zero-padded part files plus a SHA-256 manifest for the
reconstructed patch. It does not parse or modify the patch.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("patch", type=Path, help="Unified patch to split")
    parser.add_argument("output_dir", type=Path, help="Destination directory")
    parser.add_argument(
        "--part-bytes",
        type=int,
        default=48_000,
        help="Maximum bytes per part (default: 48000)",
    )
    parser.add_argument(
        "--prefix",
        default="change.patch.part-",
        help="Part filename prefix",
    )
    parser.add_argument(
        "--digest-name",
        default="change.patch.sha256",
        help="Digest manifest filename",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.part_bytes <= 0:
        raise SystemExit("--part-bytes must be positive")
    if not args.patch.is_file():
        raise SystemExit(f"Patch not found: {args.patch}")

    data = args.patch.read_bytes()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    for old in args.output_dir.glob(f"{args.prefix}*"):
        old.unlink()

    part_count = max(1, (len(data) + args.part_bytes - 1) // args.part_bytes)
    width = max(4, len(str(part_count - 1)))

    for index in range(part_count):
        start = index * args.part_bytes
        end = start + args.part_bytes
        part = args.output_dir / f"{args.prefix}{index:0{width}d}"
        part.write_bytes(data[start:end])

    digest = hashlib.sha256(data).hexdigest()
    (args.output_dir / args.digest_name).write_text(
        f"{digest}  reconstructed.patch\n", encoding="utf-8"
    )

    print(f"patch_bytes={len(data)}")
    print(f"part_count={part_count}")
    print(f"sha256={digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
