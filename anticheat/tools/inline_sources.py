#!/usr/bin/env python3
"""inline_sources.py - fold the source files into build.yml.

build.yml is the single source of truth: the sources live inside it as literal
blocks, and build.py materialises them before compiling. This script rewrites
the `sources_inline:` section of build.yml from the files in source/, so the
two never drift.

    python tools/inline_sources.py          # rewrite build.yml
    python tools/inline_sources.py --check  # report drift, write nothing
"""
import argparse
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPEC = os.path.join(ROOT, "build.yml")

MARKER = "sources_inline:"
SOURCES = [
    "source/main.mm",
    "source/Hooks.mm",
    "source/HookTable.cpp",
    "source/HookTable.h",
    "source/Il2CppResolver.cpp",
    "source/Il2CppResolver.h",
    "source/TIZAntiCheat.h",
]


def render(paths):
    out = [MARKER]
    for rel in paths:
        with open(os.path.join(ROOT, rel), "r", encoding="utf8") as f:
            body = f.read()
        out.append("  %s: |" % rel)
        for line in body.splitlines():
            out.append(("    " + line).rstrip() if line.strip() else "")
        out.append("")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    block = render(SOURCES)
    with open(SPEC, "r", encoding="utf8") as f:
        text = f.read()

    idx = text.find(MARKER)
    if idx < 0:
        print("build.yml has no sources_inline: section", file=sys.stderr)
        return 2

    current = text[idx:]
    if current.strip() == block.strip():
        print("build.yml already matches source/ - nothing to do")
        return 0
    if args.check:
        print("DRIFT: build.yml and source/ differ")
        return 1

    text = text[:idx] + block + "\n"
    with open(SPEC, "w", encoding="utf8") as f:
        f.write(text)
    print("build.yml updated with %d inline sources" % len(SOURCES))
    return 0


if __name__ == "__main__":
    sys.exit(main())
