---
name: rig-source
description: Build a new RIG/01 modulation source plugin (an LFO, envelope, sequencer, random or chaotic modulator) that moves other effects' knobs. Use when asked for a modulator, LFO or "something that moves the knobs" for the rig.
---

# A new RIG/01 modulation source

A source makes one value in -1..1 per 128-sample block. The rig patches it to any knob. Read
AGENTS.md sections 3 and 6 first. Then:

1. **Pin it down.** Write down:
   - what moves it: time and tempo (`time`), the player's playing (`playing`), a simulation
     (`chaos`), or the outside world (`outside`);
   - its knobs (8 at most);
   - its meters;
   - the claims you will measure ("RATE 2 per beat gives 4 new values a second at 120 bpm").
2. **Scaffold it:** `python3 tools/new_plugin.py <id> --source --name "NAME" --sub "..." --category <cat>`.
   It passes `check.py --fast` as it is.
3. **Tests first**, in `plugins/<id>/test.cpp`, using `Src<T>`:
   - `s.set(...)`;
   - `s.bpm`;
   - `s.run(signal)`, or `s.run({}, secs)` for silence: it gives one value per block.

   Count cycles by zero crossings and steps by value changes. Measure ranges with min and max.
4. **Write `update()`:**
   - Return a finite value in -1..1: `clampf` it.
   - Accumulate phases: `ph += c.dt * rate`. Never compute a phase from the rate, or moving RATE jumps
     the output.
   - Use `c.bpm` for tempo sync and `c.in` (this block's dry guitar) for playing-driven sources.
   - Use a member generator for randomness, never `rand()`.
   - Settings, tables, taps and `ext()` aren't available to plugins.
5. **Loop: keep or revert** with `python3 tools/check.py plugins/<id> --fast --quick` after each change
   (AGENTS.md rules 5-9), and `--fast` once before the final build. For a
   source, rigcheck's clicks check is the output's largest jump per block when a knob jumps.
6. **Finish** with `python3 tools/check.py plugins/<id>`, for all targets, ending in PASSED. Report as
   AGENTS.md section 9 says. `--render` writes the value as a control track you can plot.
