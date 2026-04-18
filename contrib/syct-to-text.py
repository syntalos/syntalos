#!/usr/bin/env python3
#
# Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
#
# SPDX-License-Identifier: LGPL-3.0+

"""Render a .syct file as a deterministic plain-text snapshot.

Text files are emitted as UTF-8 text blocks, binary files are emitted as base64.
This is intended for easy diffs between saved project archives.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import posixpath
import sys
import tarfile
from pathlib import Path
from typing import TextIO


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Convert a .syct file into a deterministic text representation " "for diffing."
        )
    )
    parser.add_argument("syct", help="Input path (e.g. project.syct)")
    parser.add_argument(
        "-o",
        "--output",
        default="-",
        help="Output text file path, or '-' for stdout (default)",
    )
    return parser.parse_args(argv)


def _is_text_utf8(data: bytes) -> bool:
    if b"\x00" in data:
        return False
    try:
        data.decode("utf-8")
    except UnicodeDecodeError:
        return False
    return True


def _entry_kind(member: tarfile.TarInfo) -> str:
    if member.isdir():
        return "dir"
    if member.isfile():
        return "file"
    if member.issym():
        return "symlink"
    if member.islnk():
        return "hardlink"
    return "other"


def _emit_header(out: TextIO, archive: Path) -> None:
    out.write("SYCT-TEXT-V1\n")
    out.write(f"project: {archive.name}\n")
    out.write("\n")


def _emit_member(
    out: TextIO,
    tf: tarfile.TarFile,
    member: tarfile.TarInfo,
) -> None:
    kind = _entry_kind(member)
    out.write(f"{kind}: {member.name}\n")
    out.write("═" * (len(member.name) + len(kind) + 2) + "\n")
    out.write(f"mode: {member.mode & 0o7777:04o}\n")

    if member.isdir():
        out.write("\n")
        return

    if member.issym() or member.islnk():
        out.write(f"target: {member.linkname}\n")
        out.write("\n")
        return

    if not member.isfile():
        out.write("\n")
        return

    stream = tf.extractfile(member)
    if stream is None:
        raise RuntimeError(f"Unable to extract file content for '{member.name}'")
    data = stream.read()

    digest = hashlib.sha256(data).hexdigest()
    out.write(f"size: {len(data)}\n")
    out.write(f"sha256: {digest}\n")

    if _is_text_utf8(data):
        out.write("encoding: utf-8\n")
        out.write(f"◀◀◀ {digest}\n")
        out.write(data.decode("utf-8"))
        # Make delimiters stable even when content has no trailing newline.
        if len(data) > 0 and data[-1] != 0x0A:
            out.write("\n")
        out.write(f"▶▶▶ {digest}\n")
    else:
        b64 = base64.b64encode(data).decode("utf-8")
        out.write("encoding: base64\n")
        out.write(f"◀◀◀ {digest}\n")
        out.write(f"{b64}\n")
        out.write(f"▶▶▶ {digest}\n")

    out.write("\n")


def _member_sort_key(member: tarfile.TarInfo) -> tuple:
    """Sort deterministically with parent directories before contained paths."""

    norm_name = posixpath.normpath(member.name)
    parts = tuple(p for p in norm_name.split("/") if p)
    is_dir = member.isdir()
    is_root = len(parts) <= 1

    if norm_name == "main.toml":
        return (0,)

    if is_root and not is_dir:
        # Other root files after main.toml.
        return (1, parts, member.type, member.linkname, member.mode, member.size)

    # Root directories and their descendants are grouped by top-level prefix.
    root = parts[0] if parts else ""
    if is_root and is_dir:
        # Emit the directory entry before any path in that subtree.
        return (2, root, 0, (), member.type, member.linkname, member.mode, member.size)

    return (
        2,
        root,
        1,
        parts[1:],
        0 if is_dir else 1,
        member.type,
        member.linkname,
        member.mode,
        member.size,
    )


def dump_archive(archive_path: Path, out: TextIO) -> None:
    with tarfile.open(archive_path, mode="r:") as tf:
        _emit_header(out, archive_path)
        # Canonical ordering keeps diffs stable and prioritizes the key project files.
        members = sorted(tf.getmembers(), key=_member_sort_key)
        for member in members:
            _emit_member(out, tf, member)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    syct_path = Path(args.syct)

    if not syct_path.exists():
        print(f"Error: project not found: {syct_path}", file=sys.stderr)
        return 1

    try:
        if args.output == "-":
            dump_archive(syct_path, sys.stdout)
        else:
            output_path = Path(args.output)
            with output_path.open("w", encoding="utf-8", newline="\n") as out:
                dump_archive(syct_path, out)
    except (OSError, tarfile.TarError, RuntimeError) as err:
        print(f"Error: {err}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
