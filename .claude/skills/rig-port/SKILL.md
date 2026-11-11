---
name: rig-port
description: Port existing DSP code (a Daisy Seed / Terrarium sketch, an Arduino or Teensy effect, DaisySP, JUCE, Faust output, Pure Data, a paper's algorithm) into a RIG/01 effect or source plugin. Use when given code or a sketch to "make work in the rig".
---

# Porting DSP code to a RIG/01 plugin

The goal is the same sound in the rig's shape: one header, the rig's rules (AGENTS.md), and proof that
it matches the original.

1. **Read the original. Then run it if you can.**
   - List its controls: pots, switches, footswitches, encoders and menu pages.
   - List its outputs, buffers and memory, and anything it allocates or does per sample versus per
     block.
   - Wherever the original builds on the host, stub the platform API and render the test riff
     through it (`tools/rigtest/signals.h` has `riff()` and `writeWav()`): that's your reference.
     Hardware calls like `hw.adc`, `hw.SetLed` and `System::GetNow` become fixed values and no-ops.
2. **Map the controls onto at most 8 knobs.**

   | Original | In the rig |
   |---|---|
   | a pot | a knob, with its real range and curve (log for frequencies and times) |
   | a toggle or rotary switch | a knob with named `stops` (rounded in `process()`) |
   | a menu page of settings | knobs 5-8, on the card's back |
   | a bypass footswitch | nothing: the rig bypasses every effect |
   | a momentary footswitch | a knob, which the user can patch a source to |
   | an LED | a meter (the face screen can draw it) |
   | a tap-tempo switch | `ctx.bpm` (effects) or `c.bpm` (sources), the rig's tap tempo |

   If there are more than 8 controls, keep the ones that matter most and say which you dropped.
3. **Translate the code:**
   - `AudioCallback(in, out, size)` becomes `process(inL, inR, oL, oR, n, e, ctx)`.
   - For a mono original, run the mono path on (L+R)/2 and write it to both sides, unless a
     stereo-ised version is asked for.
   - A hard-coded 48000 (or `hw.AudioSampleRate()`) becomes `sr`, set in `init()`.
   - `DSY_SDRAM_BSS` buffers become fixed-size members (`DelayLine<N>`, `float buf[N]`), sized for
     the longest time at 96 kHz.
   - Anything `new`ed, or static, becomes a member. `rand()` becomes a member LCG.
   - DaisySP classes: the rig's helpers often fit (`fonepole` is `OnePole`, `fclamp` is `clampf`,
     `Svf` is `eq.h`'s `Svf`). Otherwise copy the class into the header. DaisySP is MIT: keep its
     notice in a comment. Check it doesn't allocate.
   - Pot reads that jump become smoothed knobs (`OnePole`, 20-30 ms).
   - Fixed-point and integer DSP: keep it bit-exact if the sound depends on it (a bit crusher, a
     console chip). Otherwise use float.
4. **Prove it matches.**
   - Render the riff through the original (step 1) and through the port at the same settings, then
     run `python3 tools/wavdiff.py original.wav port.wav`.
   - Bit-identical is the best case. Otherwise report the difference in dB and explain it: float vs
     fixed point, the sample rate, a changed smoother.
   - Without a runnable original, test the claims the original's comments or docs make.
5. **Check and loop** with `python3 tools/check.py plugins/<id> --fast --quick`, as in the rig-effect skill
   (AGENTS.md rules 5-9).
   Ported code often fails three checks:
   - **clicks:** add smoothing;
   - **instances:** move statics into members;
   - **sample rates:** replace the hard-coded 48000s.
6. **Finish** with the full build and the AGENTS.md section 9 report. Add:
   - which control became which knob;
   - what was dropped or changed;
   - how closely the port matches the original, measured.
