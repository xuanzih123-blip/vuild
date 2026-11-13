#!/usr/bin/env python3
"""gen_hooks.py - build the anti-cheat hook table from the IL2CPP metadata.

Reads global-metadata.dat, resolves every entry of the curated list against
the real metadata (so tokens and argument counts come from the game, not from
this script), and writes:

    source/anticheat_hooks.json          the table
    source/anticheat_hooks_embedded.h     the same table as a C string literal

Usage:
    python tools/gen_hooks.py --metadata ../global-metadata.dat
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import il2cpp_meta  # noqa: E402

# type, member, action, note
WANTED = [
    ("COW.GameFacade", "IsEmulator", "return_false",
     "client side emulator verdict"),
    ("COW.GameFacade", "NeedffantihackEmulatorCheck", "return_false",
     "gate that runs the FF AntiHack SDK"),
    ("COW.GameConfig", "get_CheckHacker", "return_false",
     "master switch of the hack check"),
    ("COW.EventLogger", "LogReportCheat", "skip",
     "sends the cheat report to the server"),
    ("COW.EventLogger", "LogReportCheatInHistory", "skip",
     "records the cheat report in history"),
    ("COW.EventLogger", "SendEventTypeHackClientFix", "skip",
     "reports the client side hack fix"),
    ("COW.UIModelLogin", "GetffantihackEnabled", "return_false",
     "login gate: ff anti hack enabled"),
    ("COW.UIModelLogin", "GetEffectiveffantihackEnabled", "return_false",
     "login gate: effective ff anti hack flag"),
    ("COW.UIModelLogin", "ProcessffantihackGGP", "skip",
     "login gate: runs the anti hack process"),
    ("COW.UIModelAntiHackWindow", "ProcessAntiHackCenterDesc", "skip",
     "anti hack centre description"),
    ("COW.UINavigationUtil", "GotoV2AntiHackWindow", "skip",
     "opens the anti hack window"),
    ("COW.UIDebugConsoleController", "SetAntiHackButton", "skip",
     "debug console anti hack button"),
    ("COW.GameVarDef", "MonoMemCheckFrameInterval", "set_static_int",
     "interval of the periodic memory check"),
    ("COW.GameVarDef", "MemoryCheckString", "set_static_string",
     "memory check report string"),
    ("COW.GameFacade", "m_IsEmulator", "skip",
     "instance field, not a static: needs a per instance write"),
]

# values used by the set_static_* actions
VALUES = {
    ("COW.GameVarDef", "MonoMemCheckFrameInterval"): {
        "int_value": 0x7FFFFFFF,
    },
    ("COW.GameVarDef", "MemoryCheckString"): {
        "string_value": "",
    },
}

PROFILE = {
    "method_info": {
        "method_pointer_offset": 0,
        "name_offset": 0x10,
        "klass_offset": 0x18,
        "token_offset": 0x30,
    },
    "class_info": {
        "static_fields_offset": 0xB8,
        "name_offset": 0x10,
        "namespace_offset": 0x18,
    },
    "field_info": {
        "offset_offset": 0x0,
        "name_offset": 0x8,
    },
}


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    ap = argparse.ArgumentParser()
    ap.add_argument("--metadata", default=os.path.join(root, "..", "global-metadata.dat"))
    ap.add_argument("--json", default=os.path.join(root, "source", "anticheat_hooks.json"))
    ap.add_argument("--header", default=os.path.join(root, "source",
                                                    "anticheat_hooks_embedded.h"))
    args = ap.parse_args()

    meta = il2cpp_meta.Meta(args.metadata)
    types = meta.types()
    methods = meta.methods()
    fields = meta.fields()

    def tname(t):
        return (t["ns"] + "." + t["name"]) if t["ns"] else t["name"]

    by_type = {tname(t): t for t in types}
    methods_of = {}
    fields_of = {}
    for t in types:
        for k in range(t["method_start"], t["method_start"] + t["method_count"]):
            methods_of.setdefault(methods[k]["name"], []).append(methods[k])
        for k in range(t["field_start"], t["field_start"] + t["field_count"]):
            fields_of.setdefault(fields[k]["name"], []).append(fields[k])

    entries = []
    missing = []
    for type_name, member, action, note in WANTED:
        t = by_type.get(type_name)
        if t is None:
            missing.append("%s (class not found)" % type_name)
            continue
        entry = {
            "type": type_name,
            "method": member,
            "action": action,
            "note": note,
            "token": 0,
            "arg_count": 0,
            "field_offset": 0,
        }
        entry.update(VALUES.get((type_name, member), {}))
        found = False
        for mm in methods_of.get(member, []):
            if mm["declaring_type"] in (-1, t["idx"]) or True:
                entry["token"] = mm["token"]
                entry["arg_count"] = mm["param_count"]
                entry["kind"] = "method"
                found = True
                break
        if not found:
            for ff in fields_of.get(member, []):
                entry["token"] = ff["token"]
                entry["kind"] = "field"
                found = True
                break
        if not found:
            missing.append("%s.%s (member not found)" % (type_name, member))
            continue
        entries.append(entry)

    doc = {
        "generator": "tools/gen_hooks.py",
        "metadata_version": meta.version,
        "metadata_file": os.path.basename(args.metadata),
        "il2cpp": PROFILE,
        "hooks": entries,
    }

    text = json.dumps(doc, indent=1, ensure_ascii=False)
    with open(args.json, "w", encoding="utf8") as f:
        f.write(text + "\n")

    header = (
        "// anticheat_hooks_embedded.h - generated by tools/gen_hooks.py, do not edit.\n"
        "#ifndef TIZ_ANTICHEAT_HOOKS_EMBEDDED_H\n"
        "#define TIZ_ANTICHEAT_HOOKS_EMBEDDED_H\n\n"
        "static const char kAnticheatHooksJson[] = R\"JSON(\n"
        + text + "\n)JSON\";\n\n"
        "#endif  // TIZ_ANTICHEAT_HOOKS_EMBEDDED_H\n")
    with open(args.header, "w", encoding="utf8") as f:
        f.write(header)

    print("metadata v%d  hooks %d  missing %d" % (meta.version, len(entries),
                                                  len(missing)))
    for mm in missing:
        print("  missing:", mm)
    print("wrote", args.json)
    print("wrote", args.header)


if __name__ == "__main__":
    main()
