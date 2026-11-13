# TIZAntiCheat

Runtime hook layer that neutralises the **client-side** anti-cheat entry points of
the Free Fire build this project targets. The hook list is generated from the
game's own `global-metadata.dat`, so class/method names and tokens come from the
game rather than from guesswork.

```
anticheat/
  build.yml                      THE WHOLE MODULE: spec + the sources inline
  build.py                       reads build.yml and drives the toolchain
  Makefile                       Theos wiring
  source/                        the same sources, checked out for editing
    TIZAntiCheat.h               shared types (hook entry, actions, profile)
    HookTable.h / .cpp           tiny JSON reader + hook table loader
    Il2CppResolver.h / .cpp      managed name -> native pointer
    Hooks.mm                     the detours and the installer
    main.mm                      constructor, waits for the game image
    anticheat_hooks.json         generated hook table
    anticheat_hooks_embedded.h   generated, embedded into the binary
  tools/
    gen_hooks.py                 metadata -> hook table
    il2cpp_meta.py               IL2CPP metadata parser
    inline_sources.py            source/ -> build.yml (keep them in sync)
    syntax_check/                host side build + behaviour test (MSVC/cmake)
  resources/entitlements.plist   ldid entitlements
```

## One file

`build.yml` is the module: the target description, the toolchain flags, the
IL2CPP runtime profile, the policy, the signing and install layout, and every
source file as a literal block at the bottom. `build.py` materialises those
blocks into `build/src` and compiles from there, so a clean checkout only needs
`build.yml` (plus `build.py` and `tools/gen_hooks.py`).

The same sources are checked out under `source/` for editing; after changing
them run `python tools/inline_sources.py` to fold them back into the spec
(`--check` reports drift without writing).

## How the hooking works

Preferred mechanism: the managed method is located through the exported
`il2cpp_*` API, and `Il2CppMethodInfo::methodPointer` is rewritten to point at
our detour. That needs no Substrate, it cannot corrupt the target function, and
it survives game updates that only renumber tokens.

Fallback for pattern-scanned addresses only: `MSHookFunction` from Substrate or
ElleKit when the device has it.

## What it touches (all from the game's own metadata)

| class | member | token | action |
|---|---|---|---|
| `COW.GameFacade` | `IsEmulator` | `0x06008972` | return false |
| `COW.GameFacade` | `NeedffantihackEmulatorCheck` | `0x0600899c` | return false |
| `COW.GameConfig` | `get_CheckHacker` | `0x0602fb27` | return false |
| `COW.EventLogger` | `LogReportCheat` | `0x06008291` | skip |
| `COW.EventLogger` | `LogReportCheatInHistory` | `0x06008292` | skip |
| `COW.EventLogger` | `SendEventTypeHackClientFix` | `0x0600833f` | skip |
| `COW.UIModelLogin` | `GetffantihackEnabled` | `0x06029ab2` | return false |
| `COW.UIModelLogin` | `GetEffectiveffantihackEnabled` | `0x06029ab3` | return false |
| `COW.UIModelLogin` | `ProcessffantihackGGP` | `0x06029ab5` | skip |
| `COW.UIModelAntiHackWindow` | `ProcessAntiHackCenterDesc` | `0x06027827` | skip |
| `COW.UINavigationUtil` | `GotoV2AntiHackWindow` | `0x06008175` | skip |
| `COW.UIDebugConsoleController` | `SetAntiHackButton` | `0x0600fe9e` | skip |
| `COW.GameVarDef` | `MonoMemCheckFrameInterval` | `0x040125e9` | static int (needs offset) |
| `COW.GameVarDef` | `MemoryCheckString` | `0x04011cff` | static string (needs offset) |
| `COW.GameFacade` | `m_IsEmulator` | `0x0401714b` | instance field, skipped |

Static field writes need the runtime offset of the field; it is not in the
metadata, so the table carries `field_offset` and the entry is skipped (with a
log line) until that number is filled in.

## Build on GitHub Actions

`.github/workflows/build-dylib.yml` builds the dylib on a macOS runner and
uploads it as an artifact. Two jobs:

- **host-check** (ubuntu): makes sure `build.yml` still matches `source/`,
  then compiles the portable core and runs the behaviour test.
- **dylib** (macos-14): materialises the sources out of `build.yml`, compiles
  with `xcrun clang++ -dynamiclib`, signs with `ldid -S`, and uploads
  `TIZAntiCheat.dylib`.

```
Actions -> build-dylib -> Run workflow
```

Pick the path with the workflow input: `xcrun` (default, no Theos needed) or
`theos` (installs Theos on the runner and uses the Makefile). The artifact is
downloaded from the run summary and dropped into
`/Library/MobileSubstrate/DynamicLibraries/` on the device.

The 54 MB `global-metadata.dat` is **not** in the repo: the generated hook
table (`source/anticheat_hooks.json` plus the embedded header) is committed, so
the build works without it. The `generate` step notices the missing file and
keeps the committed table. To regenerate the table from a newer game build,
drop the metadata next to the module and run `python tools/gen_hooks.py`.

## Build

The iOS dylib needs an Apple toolchain (clang + iOS SDK), so the build runs on
macOS. On any host the portable core can be verified:

```
# regenerate the hook table from the metadata
python tools/gen_hooks.py --metadata ../global-metadata.dat

# host side compile + behaviour test (no iOS SDK needed)
cmake -S tools/syntax_check -B build/hostcheck
cmake --build build/hostcheck --config Release
./build/hostcheck/Release/tiz_anticheat_check source/anticheat_hooks.json
```

The same test runs against the copy that `build.yml` materialises:

```
python build.py --only materialise
cmake -S tools/syntax_check -B build/hostcheck_yaml \
      -DTIZ_SRC_DIR=$PWD/build/src/source
cmake --build build/hostcheck_yaml --config Release
./build/hostcheck_yaml/Release/tiz_anticheat_check source/anticheat_hooks.json
```

On macOS with Theos installed:

```
export THEOS=/opt/theos
python build.py            # generate -> materialise -> compile -> sign -> package
```

or with plain clang:

```
python build.py --only compile    # toolchain.kind: xcrun in build.yml
ldid -Sentitlements.plist build/TIZAntiCheat.dylib
```

Install into `/Library/MobileSubstrate/DynamicLibraries/` and reload the game.

## Behaviour at runtime

The hook table is embedded in the dylib. A table on disk wins over the embedded
one, so the behaviour can be changed without a rebuild:

```
/var/mobile/Library/TIZAntiCheat/hooks.json
```

The install thread logs every decision to the system log:

```
[TIZAntiCheat] il2cpp API bound from /var/containers/.../UnityFramework
[TIZAntiCheat] hooked COW.GameFacade.IsEmulator (method info 0x...)
[TIZAntiCheat] installed 13, skipped 2, failed 0
```

## Limits

These are **client-side** hooks. They cut what the client reports, not what the
server decides: the emulator tag, the emulator score, the room/group
`enable_emulator_check` flag and the ban list are server side, and the native
AntiHack SDK (`AnoSDKNamespace.*` is only the managed bridge) is out of reach of
a managed hook. The game's `IFix` hotfix system can also restore behaviour at
runtime.
