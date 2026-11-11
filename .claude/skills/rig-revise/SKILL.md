---
name: rig-revise
description: Change a RIG/01 plugin that's already been released or used in presets, such as a bug fix, a new knob or a sound change, without breaking saved presets. Use when asked to fix, update, extend or tweak an existing plugin.
---

# Revising a released RIG/01 plugin

Players' presets store each knob's symbol and value. A revision must load every old preset and sound
the same unless the change is the point.

1. **Render the current version first:**
   `python3 tools/check.py plugins/<id> --fast --render --out before.wav`. Do it at the defaults, plus
   any setting the change touches (`--set knob=value`). Note rigcheck's numbers, too.
2. **Make the smallest change that does it** (AGENTS.md section 8):
   - **Append** new knobs at the end of the table. Never insert, reorder or rename them, and never
     rename the id.
   - Give a new knob a default that keeps the old sound. A "MIX" added at 100% wet, or a "MODE" whose
     default stop is the old behaviour, are examples.
   - Don't touch anything the request didn't ask for. Report other bugs you notice; don't fix them
     quietly.
3. **Prove the old sound is kept.** Render again with the same settings, and run
   `python3 tools/wavdiff.py before.wav after.wav`.
   - It must say bit-identical, unless the change is meant to alter the sound.
   - If it is meant to, report by how much (the dB it prints) and where (the first frame it differs).
4. **Test the change.** Add a `TEST` to `plugins/<id>/test.cpp` for the new behaviour, then run
   `python3 tools/check.py plugins/<id> --fast` until it passes. Keep the old tests passing.
5. **Bump `version`** in `plugin.json`: patch for a fix, minor for a new knob.
6. **Finish** with the full `python3 tools/check.py plugins/<id>` and report:
   - what changed;
   - the wavdiff result;
   - rigcheck's numbers before and after;
   - the new test's numbers.

   A rig that's running picks up the new `.rigfx` without a restart. It swaps the copies in the
   chain onto it and keeps their knobs.
