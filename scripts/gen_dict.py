#!/usr/bin/env python3
"""
gen_dict.py — Convert a TSV word list to binary .glide dictionary format.

Usage:
    python3 gen_dict.py input.tsv output.glide --lang en-US

Input format (TSV):
    word<TAB>frequency
    hello\t100000
    world\t95000
    ...

One word per line. Frequency is a positive integer (higher = more common).
Lines starting with # are comments. Empty lines are skipped.
"""

import argparse
import struct
import sys
import os

# Constants matching SwipeTypeTypes.h
DICT_MAGIC = 0x474C4944  # "GLID"
DICT_VERSION = 1
DICT_HEADER_SIZE = 32
MAX_WORD_LENGTH = 64


def parse_args():
    parser = argparse.ArgumentParser(
        description="Convert TSV word list to binary .glide dictionary format."
    )
    parser.add_argument("input", help="Input TSV file (word<TAB>frequency)")
    parser.add_argument("output", help="Output .glide binary file")
    parser.add_argument(
        "--lang", default="en-US",
        help="BCP 47 language tag (default: en-US)"
    )
    parser.add_argument(
        "--sort", action="store_true", default=True,
        help="Sort entries alphabetically (default: true)"
    )
    parser.add_argument(
        "--no-sort", action="store_false", dest="sort",
        help="Don't sort entries"
    )
    parser.add_argument(
        "--proper-nouns", action="store_true", default=False,
        help="Mark capitalized words as proper nouns"
    )
    return parser.parse_args()


def read_tsv(filepath):
    """Read a TSV file and return list of (word, frequency, flags) tuples."""
    entries = []
    line_num = 0

    with open(filepath, "r", encoding="utf-8") as f:
        for line in f:
            line_num += 1
            line = line.strip()

            # Skip empty lines and comments
            if not line or line.startswith("#"):
                continue

            parts = line.split("\t")
            if len(parts) < 2:
                print(f"WARNING: Line {line_num}: expected 'word\\tfrequency', got '{line}'",
                      file=sys.stderr)
                continue

            word = parts[0].strip()
            try:
                frequency = int(parts[1].strip())
            except ValueError:
                print(f"WARNING: Line {line_num}: invalid frequency '{parts[1]}'",
                      file=sys.stderr)
                continue

            # Validate
            word_bytes = word.encode("utf-8")
            if len(word_bytes) == 0:
                print(f"WARNING: Line {line_num}: empty word, skipping",
                      file=sys.stderr)
                continue

            if len(word_bytes) > MAX_WORD_LENGTH:
                print(f"WARNING: Line {line_num}: word '{word}' exceeds {MAX_WORD_LENGTH} bytes, skipping",
                      file=sys.stderr)
                continue

            if frequency < 0:
                print(f"WARNING: Line {line_num}: negative frequency for '{word}', using 0",
                      file=sys.stderr)
                frequency = 0

            if frequency > 0xFFFFFFFF:
                print(f"WARNING: Line {line_num}: frequency too large for '{word}', clamping",
                      file=sys.stderr)
                frequency = 0xFFFFFFFF

            # Flags (optional 3rd column)
            flags = 0
            if len(parts) >= 3:
                flag_str = parts[2].strip().lower()
                if "proper" in flag_str:
                    flags |= 0x01
                if "profanity" in flag_str:
                    flags |= 0x02

            entries.append((word, frequency, flags))

    return entries


def write_glide(entries, output_path, language_tag, sorted_flag):
    """Write entries to a binary .glide file."""
    lang_bytes = language_tag.encode("utf-8")
    if len(lang_bytes) > 18:
        print(f"ERROR: Language tag '{language_tag}' exceeds 18 bytes",
              file=sys.stderr)
        sys.exit(1)

    # Compute header flags
    hdr_flags = 0
    if sorted_flag:
        hdr_flags |= 0x01  # bit 0: sorted alphabetically

    with open(output_path, "wb") as f:
        # === Write header (32 bytes) ===
        header = bytearray(DICT_HEADER_SIZE)

        # Magic (bytes 0-3)
        struct.pack_into("<I", header, 0, DICT_MAGIC)
        # Version (bytes 4-5)
        struct.pack_into("<H", header, 4, DICT_VERSION)
        # Flags (bytes 6-7)
        struct.pack_into("<H", header, 6, hdr_flags)
        # Entry count (bytes 8-11)
        struct.pack_into("<I", header, 8, len(entries))
        # Language tag length (bytes 12-13)
        struct.pack_into("<H", header, 12, len(lang_bytes))
        # Language tag (bytes 14+)
        header[14:14 + len(lang_bytes)] = lang_bytes
        # Remaining bytes stay zero (padding to 32-byte boundary)

        f.write(header)

        # === Write entries ===
        for word, frequency, entry_flags in entries:
            word_bytes = word.encode("utf-8")
            word_len = len(word_bytes)

            # Word length (1 byte)
            f.write(struct.pack("<B", word_len))
            # Word bytes (variable length)
            f.write(word_bytes)
            # Frequency (4 bytes, little-endian uint32)
            f.write(struct.pack("<I", frequency))
            # Flags (1 byte)
            f.write(struct.pack("<B", entry_flags))


def main():
    args = parse_args()

    # Read input
    print(f"Reading: {args.input}")
    entries = read_tsv(args.input)

    if not entries:
        print("ERROR: No valid entries found", file=sys.stderr)
        sys.exit(1)

    # Mark proper nouns if requested
    if args.proper_nouns:
        new_entries = []
        for word, freq, flags in entries:
            if word[0].isupper():
                flags |= 0x01
            new_entries.append((word.lower(), freq, flags))
        entries = new_entries
    else:
        # Lowercase all words
        entries = [(word.lower(), freq, flags) for word, freq, flags in entries]

    # Remove duplicates (keep highest frequency)
    seen = {}
    for word, freq, flags in entries:
        if word not in seen or freq > seen[word][1]:
            seen[word] = (word, freq, flags)
    entries = list(seen.values())

    # Sort if requested
    if args.sort:
        entries.sort(key=lambda e: e[0])

    # Write output
    print(f"Writing: {args.output}")
    write_glide(entries, args.output, args.lang, args.sort)

    # Summary
    file_size = os.path.getsize(args.output)
    max_freq = max(e[1] for e in entries) if entries else 0
    min_freq = min(e[1] for e in entries) if entries else 0

    print(f"\n=== Dictionary Summary ===")
    print(f"  Language:    {args.lang}")
    print(f"  Words:       {len(entries)}")
    print(f"  File size:   {file_size:,} bytes ({file_size / 1024:.1f} KB)")
    print(f"  Sorted:      {'yes' if args.sort else 'no'}")
    print(f"  Freq range:  {min_freq:,} — {max_freq:,}")
    print(f"  Format:      GLID v{DICT_VERSION}")


if __name__ == "__main__":
    main()
