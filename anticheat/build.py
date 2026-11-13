#!/usr/bin/env python3
"""build.py - drives the toolchain described by build.yml.

build.yml is the single source of truth: this script only translates it into
commands. Steps: generate -> compile -> sign -> package.

    python build.py                 # everything
    python build.py --only compile
    python build.py --dry-run       # print the commands, run nothing
"""
import argparse
import os
import shutil
import subprocess
import sys

try:
    import yaml
except ImportError:  # pragma: no cover
    sys.stderr.write("PyYAML is required for the build step: pip install pyyaml\n")
    raise

ROOT = os.path.dirname(os.path.abspath(__file__))
SPEC = os.path.join(ROOT, "build.yml")


def load_spec(path=SPEC):
    with open(path, "r", encoding="utf8") as f:
        return yaml.safe_load(f)


def log(msg):
    print("[build] %s" % msg, flush=True)


def run(cmd, cwd, dry_run):
    printable = " ".join(cmd)
    log("$ %s" % printable)
    if dry_run:
        return 0
    proc = subprocess.run(cmd, cwd=cwd)
    if proc.returncode != 0:
        log("command failed: %s (exit %d)" % (printable, proc.returncode))
    return proc.returncode


def step_generate(spec, dry_run):
    data = spec.get("data", {})
    generator = os.path.join(ROOT, data.get("generator", "tools/gen_hooks.py"))
    inputs = data.get("generator_inputs", {})
    metadata = inputs.get("metadata")
    if metadata:
        metadata_path = os.path.normpath(os.path.join(ROOT, metadata))
        if not os.path.exists(metadata_path):
            hooks_json = os.path.join(ROOT, data.get("hooks_json", ""))
            if os.path.exists(hooks_json):
                log("metadata not present, keeping the committed hook table")
                return 0
            log("metadata not found: %s" % metadata_path)
            return 2
    cmd = [sys.executable, generator]
    if metadata:
        cmd += ["--metadata", os.path.normpath(os.path.join(ROOT, metadata))]
    rc = run(cmd, ROOT, dry_run)
    if rc != 0:
        return rc
    for key in ("hooks_json", "hooks_embedded_header"):
        if key in data:
            log("generated %s" % data[key])
    return 0


def step_materialise(spec, dry_run):
    """Writes the inline sources from build.yml into build/src."""
    inline = spec.get("sources_inline") or {}
    if not inline:
        log("no sources_inline section - nothing to materialise")
        return 0
    out_dir = os.path.join(ROOT, "build", "src")
    for rel, body in inline.items():
        dest = os.path.join(out_dir, rel)
        if dry_run:
            log("would write %s (%d bytes)" % (dest, len(body)))
            continue
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "w", encoding="utf8", newline="\n") as f:
            f.write(body)
        log("materialised %s" % rel)
    return 0


def step_compile(spec, dry_run, toolchain_override=None):
    tool = dict(spec.get("toolchain", {}))
    if toolchain_override:
        tool["kind"] = toolchain_override
    kind = tool.get("kind", "theos")
    # compile from the materialised copy so the spec really is the source
    inline = spec.get("sources_inline") or {}
    src_root = os.path.join(ROOT, "build", "src") if inline else ROOT
    if inline and not os.path.isdir(src_root):
        log("run the materialise step first")
        return 2
    if kind == "theos":
        theos = os.environ.get(tool.get("theos_var", "THEOS"), "")
        if not theos or not os.path.isdir(theos):
            log("THEOS is not set / not found - theos build needs a macOS host")
            log("use toolchain.kind: xcrun for a plain clang build")
            return 2
        cmd = ["make", "-f", tool.get("makefile", "Makefile")]
        return run(cmd, ROOT, dry_run)
    if kind == "xcrun":
        cxxflags = tool.get("cxxflags", [])
        ldflags = tool.get("ldflags", [])
        sources = [os.path.join(src_root, s) for s in spec.get("sources", [])]
        out = os.path.join(ROOT, "build", spec.get("target", {}).get("output", "out.dylib"))
        cmd = ["xcrun", "--sdk", "iphoneos", "clang++", "-arch", "arm64", "-miphoneos-version-min=13.0",
               "-dynamiclib", "-fobjc-arc", "-fblocks"]
        cmd += cxxflags + ldflags + sources + ["-framework", "Foundation", "-o", out]
        return run(cmd, ROOT, dry_run)
    log("unknown toolchain kind: %s" % kind)
    return 2


def step_sign(spec, dry_run):
    sign = spec.get("sign", {})
    target = spec.get("target", {})
    out = os.path.join(ROOT, "build", target.get("output", "TIZAntiCheat.dylib"))
    if not os.path.exists(out):
        log("nothing to sign: %s missing" % out)
        return 2
    ent = sign.get("entitlements")
    if ent:
        ent_path = os.path.join(ROOT, ent)
        if not os.path.exists(ent_path):
            log("entitlements missing: %s" % ent_path)
            return 2
    if not shutil.which("ldid"):
        log("ldid not found - the dylib stays unsigned (install will fail)")
        return 0
    # ldid takes the entitlements as a positional argument AFTER the binary:
    #   ldid -S <binary> <entitlements>
    # (-e means "extract entitlements", it is not the entitlements input)
    cmd = ["ldid", "-S", out]
    if ent:
        cmd += [os.path.join(ROOT, ent)]
    return run(cmd, ROOT, dry_run)


def step_package(spec, dry_run):
    target = spec.get("target", {})
    install = spec.get("install", {})
    src = os.path.join(ROOT, "build", target.get("output", "TIZAntiCheat.dylib"))
    if not os.path.exists(src):
        log("nothing to package: %s missing" % src)
        return 2
    stage = os.path.join(ROOT, "packages", install.get("dir", "/").strip("/").replace("/", "_") or "root")
    if dry_run:
        log("would stage %s -> %s" % (src, stage))
        return 0
    os.makedirs(stage, exist_ok=True)
    dst = os.path.join(stage, install.get("out", os.path.basename(src)))
    shutil.copy2(src, dst)
    log("staged %s" % dst)
    return 0


STEPS = {
    "generate": step_generate,
    "materialise": step_materialise,
    "compile": step_compile,
    "sign": step_sign,
    "package": step_package,
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--spec", default=SPEC)
    ap.add_argument("--only", choices=sorted(STEPS), default=None)
    ap.add_argument("--toolchain", choices=["theos", "xcrun"], default=None,
                    help="override toolchain.kind from build.yml")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    spec = load_spec(args.spec)
    log("%s %s (%s / %s)" % (spec["project"]["name"], spec["project"]["version"],
                             spec["target"]["platform"],
                             ",".join(spec["target"]["arch"])))

    order = [s["name"] for s in spec.get("steps", [])] or ["generate", "compile", "sign", "package"]
    if args.only:
        order = [args.only]

    for name in order:
        fn = STEPS.get(name)
        if fn is None:
            log("unknown step %s - skipped" % name)
            continue
        log("--- step %s ---" % name)
        if name == "compile":
            rc = fn(spec, args.dry_run, args.toolchain)
        else:
            rc = fn(spec, args.dry_run)
        if rc != 0:
            log("step %s failed with %d" % (name, rc))
            return rc
    log("done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
