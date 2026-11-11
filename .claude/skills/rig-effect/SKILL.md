---
name: rig-effect
description: Build a new RIG/01 effect plugin (.rigfx) from a description, such as "make a fuzz", "a dotted-eighth delay" or "a phaser with a tempo-synced sweep". Use when asked to write, design or make an effect for the rig.
---

# A new RIG/01 effect

AGENTS.md is the contract (API, rules, helpers, limits). Read it before writing code. Then:

1. **Pin down what to build.** Write down, in a few lines:
   - the effect in one sentence;
   - its knobs: symbol, label, range, unit, curve and default (8 at most, the most-used first);
   - its meters, if any;
   - the category;
   - the claims you will measure ("FEEDBACK 50% halves each repeat").

   If the request could mean two different effects, say which one you chose and why. Ask only if you
   can't build it correctly without an answer.
2. **Scaffold it:**
   `python3 tools/new_plugin.py <id> --name "NAME" --sub "what it is" --category <cat>`.
   - The id is a-z, 0-9 and _, 12 characters at most, and not in rig.json.
   - Run `python3 tools/check.py plugins/<id> --fast` once: it passes as it is. That's your green start.
3. **Write the tests first.** In `plugins/<id>/test.cpp`, give one `TEST` per claim from step 1,
   each `CHECK` printing the measured number (AGENTS.md section 6, "Writing test.cpp"). They fail for
   now.
4. **Write the effect** in `plugins/<id>/<id>.h`: the knob table, the `ModuleDesc`, one struct.
   - Smooth every knob.
   - Fixed-size buffers as members.
   - `undenorm()` in feedback paths.
   - Times and frequencies from `sr`.
   - Read the inputs before writing the outputs.
5. **Loop: keep or revert** (AGENTS.md rules 5-9 keep it short). After each single change, run
   `python3 tools/check.py plugins/<id> --fast --quick` (about 2.5 s), and `--fast` once before the final build.
   - Keep the change if everything passes and the numbers moved the way you meant.
   - Revert it otherwise.
   - When a check fails, find the cause before changing anything: rigcheck's message names the
     signal and the knob setting.
   - The fixes table is in AGENTS.md section 6.
6. **Listen**, if the user can: `python3 tools/check.py plugins/<id> --fast --render --set knob=value --out x.wav`.
   It plays the test riff. Use `--in` for a real guitar WAV.
7. **Finish:**
   - Remove debug code.
   - Run `python3 tools/check.py plugins/<id>` without `--fast`, for all three targets. It must end
     in PASSED.
   - Hand over `plugins/<id>/` and `dist/plugins/<id>.rigfx` with the report in AGENTS.md section 9:
     the knobs (`--info`), rigcheck's CPU and loudest output, each claim's measured number, any WARN
     left and why, and what is verified versus inferred.
