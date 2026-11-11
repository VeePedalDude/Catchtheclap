# Catch the CLAP: building effects for RIG/01

Everything an AI (or a person) needs to write effects and modulation sources for
[RIG/01](https://github.com/VeePedalDude/DIY-multi-effect-), build them into `.rigfx` plugins, and
prove they follow the rig's rules before they reach the rig.

A `.rigfx` holds the same effect three times: a CLAP plugin for Windows, one for Linux, and a WebCLAP
(WebAssembly) build for the browser. The rig loads it from its library (`Documents\RIG01\Effects`, or
**Load effect file** in the Add an effect panel) without being rebuilt.

## Handing it to an AI

- **Claude Code:** open this repository and ask, e.g. *"make a fuzz with a gate and a 3-band tone
  stack"*, *"port this Daisy Seed sketch"* (attach it), or *"add a MIX knob to trem"*. It reads
  `CLAUDE.md` and `AGENTS.md`, and follows the skill that fits: `rig-effect`, `rig-source`,
  `rig-port` or `rig-revise` (in `.claude/skills/`).
- **Other assistants** (Codex, Cursor, a chat with file access): point them at `AGENTS.md`. It is
  the whole contract: the API, the rules, the tools and what to hand back.

What comes back is a folder `plugins/<id>/` and the bundle `dist/plugins/<id>.rigfx`, with a report:
the knobs, the measured numbers (CPU, loudest output, each claim the effect makes), and what was
verified versus only inferred. Load the `.rigfx` into the rig, or first into `preview/rig-test.html`
(the rig as one page: open it in Chrome, press Start, then Add an effect → Load effect file).

## What's here

| | |
|---|---|
| `AGENTS.md` | the brief for AI authors: API, rules, helpers, limits, checks, hand-over |
| `.claude/skills/` | step-by-step procedures: new effect, new source, porting code, revising a released plugin |
| `tools/check.py` | **the one command**: build, then rigcheck, then the plugin's own tests; ends in PASSED or FAILED |
| `tools/rigcheck/` | the conformance checker: loads the built `.clap` with the rig's own loader and plays it hard (NaN and runaway output, clicks, heap allocation, shared state, sample rates, block sizes, CPU) |
| `tools/rigtest/` | helpers for a plugin's own `test.cpp`: the rig's test guitar riff, signals, measurements |
| `tools/new_plugin.py` | starts a plugin folder from `templates/` (it passes every check as it is) |
| `tools/wavdiff.py` | compares two renders: proves a revision kept the old sound |
| `plugins/trem`, `plugins/rand` | the rig's example effect and source, with tests |
| `tests/` | `selftest.py`, which checks the checker: each plugin in `tests/bad/` has one fault rigcheck must catch |
| `sdk/`, `engine/core/`, `tools/build_plugin.py`, `third_party/`, `rig.json`, `preview/` | the rig's own files, copied unchanged |

## Using it yourself

```sh
pip install ziglang==0.17.0          # the compiler RIG/01 builds with (see the note below)
python3 tools/check.py --all         # build and check every plugin
python3 tools/check.py plugins/trem --fast --render --set rate=6 --out trem.wav
python3 tools/check.py ~/Downloads/someones.rigfx    # check a bundle you were sent
```

You also need python3 and g++ (`--fast` builds and the checker use only these). zig 0.17.0 is marked
"yanked" on PyPI because its upload was partial: an exact `==0.17.0` still installs it on Linux
x86_64, but its wheel may be missing on other platforms. There, the nearest zig that pip offers
usually builds the same plugins, but the rig itself is tested with 0.17.0.

Every push runs the same checks on GitHub (`.github/workflows/plugins.yml`) and offers the built
bundles as the run's `rigfx` download. Pushing a tag like `v1.0.0` attaches them to a release.

## Keeping in step with the rig

The rig's files here are copies, recorded in `RIG_VERSION`. When RIG/01's plugin API changes, refresh
them and check everything again:

```sh
python3 tools/sync_from_rig.py ../DIY-multi-effect-
python3 tests/selftest.py && python3 tools/check.py --all
```

The copies come from RIG/01's `plugins` branch (pull request #20), where the plugin system lives
until it's merged.

## Licence

GPLv3 (`LICENSE`), like RIG/01, whose headers a plugin compiles against: a plugin built with this kit
is GPLv3 too. The CLAP headers (`third_party/clap`) and nlohmann/json are MIT.
