# Building effects for RIG/01

You are writing an **effect** or a **modulation source** for RIG/01, a guitar multi-effect that runs
in a browser page and in a native host (`rig-host`, Windows and Linux). It ships as a **plugin**: one
`.rigfx` file the player loads into the rig, which isn't rebuilt. This repository has everything to
write, build and check one. Read this file first; it is the whole contract.

```
plugins/<id>/
  <id>.h         the effect: a knob table, a description, one struct         (required)
  plugin.json    kind, category, tags, author, version                        (required)
  test.cpp       your tests: does it do what it says? (tools/rigtest/rigtest.h) (expected)
  screen.js      a face drawing for the 3D stage                              (optional)
```

## Quick start

```sh
pip install ziglang==0.17.0                                  # once: the compiler for all three targets
python3 tools/new_plugin.py fuzzbox --name "FUZZ BOX" --sub "gated germanium fuzz" --category drive
#   -> plugins/fuzzbox/{fuzzbox.h, plugin.json, test.cpp}: a working effect that passes every check
python3 tools/check.py plugins/fuzzbox --fast                # seconds: Linux build, rigcheck, test.cpp
#   ...edit fuzzbox.h and test.cpp, check again, until PASSED...
python3 tools/check.py plugins/fuzzbox                       # the final build: Windows, Linux, browser
#   -> dist/plugins/fuzzbox.rigfx                            # what you hand over
python3 tools/check.py plugins/fuzzbox --render --set drive=80% --out fuzz.wav  # hear it (the test riff)
```

`tools/check.py` is the one command. Its last line says `PASSED` or `FAILED`, and every line above it
gives a number. A `--fast` pass isn't a hand-over: finish with the full build.

## How to work

These are the owner's working rules (after
[andrej-karpathy-skills](https://github.com/forrestchang/andrej-karpathy-skills), MIT):

1. **Think before coding.** State your assumptions. If the request could mean two effects, say which
   one you built. Ask only when you can't build it correctly without an answer.
2. **Simplicity first.** Build exactly the knobs and behaviour asked for: nothing speculative, no
   options nobody requested. If 200 lines could be 50, rewrite it.
3. **Surgical changes.** When revising a plugin, change only what the revision needs. Never rename a
   knob `symbol` or the id: presets store them.
4. **Goal-driven.** Turn every claim into a number in `test.cpp` ("RATE 4 Hz swings at 3.99 Hz"), and
   loop until `check.py` passes.

**The loop: keep or revert.** Make one change at a time. Run `check.py --fast`, and compare its numbers
with the last good run. Keep the change if everything still passes and the numbers moved the way you
meant. Otherwise, revert it and try something else. Don't stack untested changes.

**Measure, don't read.** Build and run it; don't argue from the code. Feed it the hard cases (rigcheck
does), and report numbers with units. When something fails, find the cause (bisect: halve the change, or
the signal, until it shows) instead of guessing. Before handing over, strip debug code and run the full
build.

**Report verified and inferred separately.** "Measured: the echo is 375 ms ± 1 sample" is verified.
"Should sound warm" is inferred. Flag anything you couldn't fix.

### Working fast

Rules 1–4 keep the work right; these keep it short.

5. **Start from green, never from blank.**
   - `new_plugin.py` makes a plugin that already passes every check. Change it one step at a time and
     you're never more than one change from green.
   - Copy, don't recall. The knob table, smoothing and `test.cpp` patterns are in the template and in
     `plugins/trem` and `plugins/rand`. Copying them is faster than writing them from memory, and they
     are already right.
   - Use a helper before you write DSP. `OnePole`, `Svf`, `Biquad`, `DelayLine`, `Allpass` and
     `vaSat` are tested, allocation-free and correct at every sample rate.
   - Read this file once. Open a rig header only when you're about to use what's in it.

6. **Run the quickest check that answers your question.**

   | When | Command | Time |
   |---|---|---|
   | each change | `check.py plugins/<id> --fast --quick` | about 2.5 s |
   | before the final build | `check.py plugins/<id> --fast` (all 27 knob settings, and CPU) | about 3 s |
   | once, at the end | `check.py plugins/<id>` (all three targets) | about 15 s; minutes the first time |

   - To look at one failure, reproduce that case alone. The FAIL line names the input and the knob
     setting; `--render --set knob=value` plays it in a second. Don't rerun everything to see it again.
   - Don't run the full build to find a compile error: `--fast` shows the same error in seconds.

7. **Read the failure, then change one thing.**
   - Every FAIL line carries the evidence: the signal, the knob setting, and how far off it was. Fix
     the cause it points to, not the nearest constant.
   - Three failed fixes for the same check mean the approach is wrong, not the numbers. Stop tuning.
     Bisect (take out half of `process()` until the check passes), or change the structure.
   - Never loosen a test, or work around a rigcheck FAIL, to get to PASSED. A WARN you leave needs one
     sentence saying why it's acceptable.

8. **Tests prove claims, nothing more.**
   - Write one `TEST` per claim the player would care about, and set its target number before you write
     the DSP. Stop tuning when the number is met; "sounds better" isn't a target.
   - Don't re-test what rigcheck already checks: NaN, clicks, allocation, shared state, sample rates,
     block sizes and CPU.
   - Copy every number in your report from the tools' output. Never estimate or recompute one.

9. **Done is done.** Stop when:
   - the full `check.py` ends in PASSED;
   - `test.cpp` covers every claim;
   - the report is written.

   Write no README and no extra files. Add no knobs nobody asked for. Make no edits to the rig's
   files. Don't do a second pass "to polish".

**The test:** every command you ran answered a question you had, and every line you wrote traces to
the request.

## 1. An effect

Your header includes `"module.h"` (the build puts `engine/core/` on the include path). It defines one
knob table, one `ModuleDesc` and one `struct <Name> : Module`, all in `namespace rig`. This is
`plugins/trem/trem.h`, in full:

```cpp
// plugins/trem/trem.h - TREMOLO: volume swells from sine to square. DEPTH 0-100%,
// RATE 0.5-16 Hz, SHAPE sine (0) to square (1). Default 50% at 3 Hz, sine.
#pragma once
#include "module.h"

namespace rig {

//                 symbol   label    min    max    default unit  curve       decimals sign
inline constexpr ParamDesc TREM_PARAMS[] = {
  {"depth", "DEPTH",  0.f,  100.f, 0.5f,  "%",  Curve::Lin, 0, false},
  {"rate",  "RATE",   0.5f, 16.f,  0.4f,  "Hz", Curve::Log, 1, false},
  {"shape", "SHAPE",  0.f,  1.f,   0.f,   "",   Curve::Lin, 2, false},
};
//                                       id      face title  subtitle          knobs            meters
inline constexpr ModuleDesc TREM_DESC = {"trem", "TREMOLO", "sine to square", 3, TREM_PARAMS, 1, {"gain", nullptr}};

struct Trem : Module {
  // All state in members: the chain can hold several of these, and nothing in process() may allocate.
  float ph = 0, gain = 1; OnePole depthS, rateS;
  const ModuleDesc& desc() const override { return TREM_DESC; }
  void init(float s) override { sr = s; depthS.setMs(sr, 20); rateS.setMs(sr, 20); reset(); }
  void reset() override { ph = 0; gain = 1; depthS.y = 0.5f; rateS.y = 3.f; }
  // e[i] = knob i after modulation (0..1), for this block; map(i, e[i]) gives real units.
  void process(const float* inL, const float* inR, float* oL, float* oR, int n,
               const float* e, const Context&) override {
    const float depthT = map(0, e[0]) * 0.01f, rateT = map(1, e[1]), sh = e[2];
    for (int i = 0; i < n; i++) {
      const float depth = depthS.next(depthT), hz = rateS.next(rateT);    // smoothed: a jumping knob never clicks
      ph += hz / sr; if (ph >= 1) ph -= 1;
      const float s = sinf(2 * PI_F * ph), lfo = (1 - sh) * s + sh * (s > 0 ? 1.f : -1.f);
      gain = 1.f - depth * 0.5f * (1.f + lfo);
      oL[i] = inL[i] * gain; oR[i] = inR[i] * gain;
    }
  }
  void meters(float* out) const override { out[0] = gain; }        // up to 2 display values, named in the desc
};

} // namespace rig
```

and its `plugin.json`:

```json
{ "struct": "rig::Trem", "kind": "effect", "category": "modulation",
  "tags": ["tremolo", "volume", "lfo"], "author": "KC", "version": "1.0.0" }
```

### The API (from `engine/core/module.h` and `param.h`, which are in this repo)

```cpp
constexpr int MAXPAR = 8;          // knobs per effect or source
constexpr int MAXMODMETERS = 2;    // display values an effect can report
enum class Curve { Lin, Log };
struct ParamStop { float v; const char* name; };   // a named stop, in real units

struct ParamDesc {
  const char* symbol;              // preset/protocol name: [a-z][a-z0-9_]*, up to 16 chars, unique in the effect
  const char* label;               // the knob's label (10 characters or fewer reads best)
  float min, max;                  // real-world range (max > min; Log needs min > 0)
  float def;                       // default, NORMALISED 0..1
  const char* unit;                // "", "dB", "ms", "s", "Hz", "%", "x", "bpm", "beats", "per beat", "st", "ct", "deg"
  Curve curve;                     // Lin, or Log
  int decimals;                    // 0..4 digits shown
  bool sign;                       // show a leading + for positive values
  const ParamStop* stops = nullptr; // optional named stops, ascending, inside min..max
  int nstops = 0;
  float map(float v) const;        // normalised -> real: Log is min * (max/min)^v, Lin is min + (max-min) * v
};

struct ModuleDesc {
  const char* id;                  // short stable id: "trem" ([a-z][a-z0-9_]*, 12 chars at most)
  const char* name;                // face title: "TREMOLO"
  const char* kind;                // subtitle: "sine to square"
  int nparams;
  const ParamDesc* params;
  int nmeters;
  const char* meters[MAXMODMETERS];  // names of the values meters() reports
  // (an asset slot follows; plugins leave it empty: see Limits)
};

struct Context { float sr = 48000; float bpm = 120; };   // per block; bpm follows the rig's tap tempo

struct Module {
  float sr = 48000;
  virtual const ModuleDesc& desc() const = 0;
  virtual void init(float s) = 0;     // the sample rate: set up smoothers, clear state
  virtual void reset() = 0;           // back to silence (clear delay lines, filters, phases)
  // e[] = effective normalised knobs (0..1, already modulated) for this block. n <= 128.
  virtual void process(const float* inL, const float* inR, float* oL, float* oR, int n,
                       const float* e, const Context& ctx) = 0;
  virtual void meters(float* out) const {}          // optional: fills nmeters values
  float map(int i, float v) const { return desc().params[i].map(v); }   // knob i in real units
};
```

### The rules (rigcheck enforces each one: section 6)

1. **Nothing in `process()` allocates, locks or does I/O.** No `new`, no growing `std::vector`, no
   `std::string`, no `printf`. Size every buffer as a fixed member: `DelayLine<65536>`, `float buf[48000]`.
2. **All state lives in members, never in `static` or global variables**, and never `rand()` or the
   clock. The chain can hold two copies (`trem`, `trem2`), and they must not affect each other.
   For randomness, keep a seeded generator in a member.
3. **Smooth every knob that would click** when it jumps: `OnePole` at 20–30 ms per sample, or a
   per-16-sample smoother. Modulation can move any knob every block (128 samples, 2.7 ms).
4. **Output stays finite and sane on any input and any knob setting**: full-scale noise, silence, DC,
   denormals; every knob at 0, at 1, at random. Apply `undenorm()` in feedback paths, keep loop gain
   below 1, and clamp anything that can run away. Output above +36 dBFS fails.
5. **Stereo in, stereo out**, `n` ≤ 128 samples per call, and `n` varies (rig-host plays the audio
   driver's block size, e.g. 32 or 64). The input may be mono on both sides. The input and output
   buffers may be the same memory: read `inL[i]` and `inR[i]` before writing `oL[i]` and `oR[i]`.
6. **Times and frequencies come from `sr`**, never from 48000: the rig runs at 44.1, 48 and 96 kHz.
7. **Knob defaults are normalised.** On a Log knob, √(min·max) sits at 0.5. In `reset()`, set
   smoothers to the defaults' real values so nothing sweeps at start-up.
8. **Up to 8 knobs and 2 meters.** The first 4 knobs are on the card's front, the rest on its back:
   list the most-used first. Meters are display values (the gain now, a delay time in ms), read by the
   face screen.
9. **The id isn't one of the rig's.** `rig.json` lists the built-in effect and source ids (rigcheck and
   `new_plugin.py` refuse them). `trem` and `rand` are this repo's examples.
10. **The header is named after its folder** (`plugins/<id>/<id>.h`), and `plugin.json`'s `struct` is
    the full name, `rig::<Name>`.
11. **It starts with a comment**: one line saying what it does, then its knobs and their defaults.

### Categories (`plugin.json` → `category`)

The tabs of the rig's Add an effect panel (from `rig.json`):

| Category | What goes there |
|---|---|
| `dynamics` | gates, compressors, limiters |
| `drive` | overdrive, distortion, fuzz |
| `cab` | speaker cabinets |
| `filter` | EQs, wahs, filters |
| `synth` | filters from classic synths |
| `voice` | your chords played by other instruments |
| `modulation` | chorus, flanger, phaser, tremolo, vibrato |
| `pitch` | octaves, harmonies, pitch shifting |
| `delay` | echoes and repeats |
| `reverb` | rooms, halls, plates, spaces |
| `utility` | volume, splits, tools |
| `experimental` | anything that doesn't fit a box |

Tags are free-form, lower case, a few per effect. They're filter chips in the add panel.

## 2. Helpers (all allocation-free, safe in `process()`)

`dsp.h` (included by `module.h`):

```cpp
constexpr int MAXBLOCK = 128; constexpr float PI_F;
float clampf(float x, float a, float b);
float db2lin(float dB);
float undenorm(float x);                              // flush tiny values to 0 (feedback, filter state)
struct OnePole { float y, a; void setMs(float sr, float ms); float next(float target); };  // knob smoother
struct Biquad  { void lp(sr,f,q); void hp(sr,f,q); void peak(sr,f,q,dB); float tick(float x); void reset(); };
struct DCBlock { float tick(float x); void reset(); };
template <int N> struct DelayLine {                   // N a power of two; N*4 bytes: make it a member
  void clear(); void push(float x); float tap(float samplesAgo) const;   // linear interpolation, d >= 1
};
struct Allpass { void init(int len, float g); void clear(); float tick(float x); };       // len < 4096
```

Include as needed, from the same folder (read the headers; they're short):

- **`eq.h`**: `Svf` (a stereo state-variable filter, clean while modulated:
  `set(Svf::LP|HP|BP|BELL|LSHELF|HSHELF, sr, f, q, dB)`, `tick(ch, x)`), `Shelf1` (first-order shelves
  and tilt), `Iir3`, and `StepSmooth`, which retunes every 16 samples.
- **`vaf.h`**: `VaLoop<N>` (a zero-delay-feedback analog filter loop), `vaSat()` (a soft saturator),
  `Os2` (2x oversampling), `EnvAmount`, `VaNoise`, `vaG()`. Resonant filters and saturating stages go here.
- **`fft.h`**, **`polytrack.h`** (the rig's polyphonic note tracker), **`notes.h`**: spectral effects
  and pitch-aware effects.

A big delay buffer is fine as a member: `DelayLine<131072>` holds 2.7 s at 48 kHz in 512 KB. Keep each
instance to a few MB.

## 3. A modulation source instead

A source makes one value in −1..1 per block; it moves the knobs it's patched to. Include
`"source.h"`; in `plugin.json`, `"kind": "source"` and one of the categories `time` (LFOs, sequencers,
clocks), `playing` (envelopes, pick triggers, followers), `chaos` (orbits, random walks, simulations)
or `outside` (controllers, sensors). This is `plugins/rand/rand.h`'s heart:

```cpp
//                 symbol   label    min    max     default unit        curve       decimals sign
inline constexpr ParamDesc RAND_PARAMS[] = {
  {"rate", "RATE", 0.5f, 8.f,   0.5f, "per beat", Curve::Log, 1, false},   // 2 per beat
  {"slew", "SLEW", 0.f,  500.f, 0.f,  "ms",       Curve::Lin, 0, false},
};
//                                       id      orb label  one-line description  ext      meters                  knobs
inline constexpr SourceDesc RAND_DESC = {"rand", "RANDOM", "sample and hold", nullptr, 1, {"steps", nullptr}, 2, RAND_PARAMS};

struct RandomSH : Source {
  float ph = 0, target = 0, value = 0; unsigned seed = 0x9E3779B9u; int steps = 0;
  const SourceDesc& desc() const override { return RAND_DESC; }
  void init(float s) override { sr = s; }
  float update(const SourceContext& c) override {         // once per block; return -1..1, always finite
    ph += c.dt * c.bpm / 60.f * knob(c, 0);                // knob(c, i) = knob i's real value, after cross-modulation
    if (ph >= 1.f) { ph -= floorf(ph); seed = seed * 1664525u + 1013904223u; target = (seed >> 8) / 8388608.f - 1.f; steps++; }
    const float slew = knob(c, 1) / 1000.f;
    value = slew <= 0.f ? target : value + (1.f - expf(-c.dt / slew)) * (target - value);
    return clampf(value, -1.f, 1.f);
  }
  void meters(float* out) const override { out[0] = (float)steps; }
};
```

`SourceContext` gives `in` (the dry guitar for this block), `n`, `sr`, `dt` (the block in seconds),
`bpm` and `p` (the knobs, normalised). The same rules apply: no allocation, state in members, finite
output, and a jumping knob mustn't make the output jump (accumulate phases; don't compute them from
the rate). A source id is 12 characters at most. Source plugins can't yet use settings, tables, taps,
`ext()` or `view()`: leave those out.

## 4. A face screen (optional `screen.js`)

Without one, the card's face shows a bar per knob. To draw something of your own:

```js
// screen.js - what it shows, in one line.
screen('trem', (c, b, m, h) => {
  // c: canvas 2D context; b: {x, y, w, h} box; m.id: this instance's id
  // h.eff(i): live knob i (0..1, after modulation); h.param(i).real(v): real units
  // h.meter('gain', fallback): a meter by name; h.T: seconds (for animation)
  // h.col.*: the skin's colours. NEVER write a literal colour: h.col.text, h.col.accent...
  const g = h.meter('gain', 1);
  c.fillRect(b.x + 20, b.y + b.h - 12 - g * (b.h - 40), b.w - 40, g * (b.h - 40));
  c.fillStyle = h.col.text; c.fillText((g * 100).toFixed(0) + '%', b.x + 20, b.y + 26);
});
```

Draw from knobs and meters only; don't re-run the DSP in JavaScript. rigcheck doesn't run screens:
look at it in the preview page (section 7).

## 5. Limits

- **No file slot:** a plugin can't load a NAM capture, an IR or a sample. Generate what it needs in
  `init()`, or keep it as a `constexpr` table in the header.
- **8 knobs, 2 meters, no hidden settings.** Fold choices into a knob with named `stops` (a MODE knob
  with `{0,"CLEAN"},{1,"DIRTY"}`) and round it in `process()`.
- **Portable C++ only.** It compiles for `wasm32-wasi` (with `-msimd128`), `x86_64-windows` and
  `x86_64-linux` with `-std=c++20 -Wall -Wextra -Werror -fno-exceptions -fno-rtti`. Standard C++ and
  `<math.h>`: no threads, files, platform headers, intrinsics or exceptions.
- **Browser CPU:** the browser runs the WebAssembly build at about 1.5x native CPU. FFTs of a few
  thousand points per block at most.

## 6. What `check.py` checks

**1. build**: the plugin compiles cleanly (all three targets, unless `--fast`).

**2. rigcheck** (`tools/rigcheck/rigcheck.cpp`): loads the built `.clap` with the rig's own loader
(`engine/core/plugin_clap.h`, as rig-host does) and plays it as the engine does. FAIL means the rig's
rules say no; WARN is worth a look.

| Check | What it does | Fails when | How to fix |
|---|---|---|---|
| loads, id, category | reads the description | the id is a built-in's; (warn) unknown category, long labels, version not x.y.z | rename the id; pick a category from the table |
| hard cases | 8 inputs (riff, full-scale and quiet noise, silence, DC, denormals, an 80 Hz square, impulses) × 27 knob settings (defaults, all 0, all 1, 24 random) | NaN or Inf anywhere, a meter going NaN, output above +36 dBFS; a source outside −1..1 | guard divisions and logs, clamp, keep feedback below 1, `undenorm()` |
| at its defaults | the riff at the default knobs | (warn) nearly silent, very loud, or identical to the input | better defaults |
| silence in, tail, denormals | silence in; 12 s after the riff stops; the tail's CPU | (warn) noise from silence, a tail that never dies, a tail costing 3x the CPU | `undenorm()` in recursive state |
| clicks | each knob jumping end to end every 170 ms on a 220 Hz sine; the largest sample step against the same knob held still | a step 0.5 above the still one (warn at 3x); mode knobs (with stops) only warn | smooth the knob (`OnePole`, 20–30 ms) |
| allocation | 4 s with every knob moving every block, counting heap calls (malloc, new) inside the plugin | any heap call | fixed-size members, built in `init()` |
| instances | a fresh copy must repeat the first copy exactly; two copies side by side mustn't change each other | outputs differ | move `static`s and globals into members; no `rand()` |
| sample rates | 44.1, 48 and 96 kHz | NaN; (warn) level 6 dB off 48 kHz's | use `sr` |
| block sizes | random blocks of 1–128 samples against 128s | (warn) they sound different | per-sample smoothing; no buffers that assume 128 |
| CPU | 14 s of the riff, at the defaults and with every knob at 1 | over 25% of one core (warn over 5%) | do less per sample; run things per block |

**3. tests**: `plugins/<id>/test.cpp`, your tests of what the effect claims (next section).

Any CLAP effect can be checked the same way: `python3 tools/check.py someone_elses.rigfx`.

### Writing `test.cpp` (`tools/rigtest/rigtest.h`)

The tests compile against your header directly (fast) and run after rigcheck. Test each claim the
effect makes, with a number in the message:

```cpp
#include "rigtest.h"
#include "trem.h"
using namespace rigtest;

TEST("RATE sets the speed in Hz") {
  Fx<rig::Trem> fx; fx.set("depth", 100); fx.set("rate", 4);    // knob values in their own units
  const auto env = envelope(fx.run(sine(48000, 375, 4)).L, 128);  // the level per 128 samples (whole cycles of 375 Hz)
  const Signal tail(env.begin() + 200, env.end());
  const float hz = 48000.f / 128 / period(tail, 25, 410);
  CHECK(fabsf(hz - 4) < 0.12f, "swings at %.2f Hz (asked for 4)", hz);
}
RIGTEST_MAIN
```

- `Fx<T>` plays an effect: `set(symbol, value)` (in the knob's own units), `setNorm`, `setStop(symbol,
  "NAME")`, `run(in)` or `run(L, R, perBlock)` → `{L, R}`, `meters()`.
- `Src<T>` plays a source: `set`, `bpm`, `run(in, secs)` → one value per block.
- Signals (`signals.h`): `riff(sr)` (the rig's test guitar: chords, palm mutes, single notes at
  −12 dBFS), `sine`, `noise`, `square`, `impulses`, `constant`, `concat`, `readWav`/`writeWav`.
- Measurements: `rms`, `peak`, `dB`, `maxStep`, `diff`, `toneLevel(x, sr, hz)` (a frequency's
  level: filter responses, harmonics), `envelope(x, win)`, `period(x, lo, hi)` (an LFO's or an
  echo's spacing), `finite`, and `heapCallsDuring(f)`.

Measure over whole cycles: an envelope window that isn't a whole number of the test tone's periods
ripples on its own. (375 Hz at 48 kHz is exactly 128 samples.) `plugins/trem/test.cpp` and
`plugins/rand/test.cpp` are worked examples; `templates/` holds the starting ones.

## 7. Listening and comparing

- `python3 tools/check.py plugins/<id> --render [--set knob=value ...] [--in guitar.wav] [--out out.wav]`
  plays a WAV (default: the test riff) through the plugin. Values are in the knob's own units, a
  stop's name, or `50%` of its travel. For a source, it writes the value as a control track.
- `python3 tools/wavdiff.py a.wav b.wav` compares two renders: bit-identical, or by how much they differ.
- `preview/rig-test.html` is the rig itself, as one page. Open it in Chrome, press Start, and in **Add
  an effect** press **Load effect file (.rigfx)…**. You hear it in the full rig, with its face screen,
  modulation and presets.

## 8. Revising a released plugin

Presets store knob symbols and values, so a plugin that's out there changes like this:

- **Append** knobs (never insert or reorder), with defaults that keep the old sound.
- **Never rename** a knob `symbol` or the id.
- **Bump `version`** in `plugin.json` (minor for new knobs, patch for fixes).
- **Prove the old sound is kept.** Render the old build and the new one at the same settings, and
  compare: `python3 tools/wavdiff.py old.wav new.wav` should report them bit-identical. If a fix
  changes the sound on purpose, say by how much.

The rig replaces a loaded plugin when a newer file with the same id arrives, keeping each copy's
knobs, so an update reaches a running rig without a restart.

## 9. Handing it over

Deliver `plugins/<id>/` (the header, `plugin.json`, `test.cpp`, an optional `screen.js`) and the
built `dist/plugins/<id>.rigfx`, from a full `check.py` run that ends in PASSED. Report:

- what it does, its knobs and defaults (`check.py ... --info` prints them);
- the measured numbers: rigcheck's CPU and loudest output, and each claim from `test.cpp`;
- any WARN left, and why it's acceptable;
- what you verified, and what you only inferred.

## This repository

| Path | What | Edit? |
|---|---|---|
| `plugins/` | plugins: yours, and the examples `trem` and `rand` | yes |
| `templates/` | what `new_plugin.py` starts from | rarely |
| `tools/check.py`, `new_plugin.py`, `wavdiff.py` | the commands above | – |
| `tools/rigcheck/`, `tools/rigtest/` | the checker, and the test helpers for `test.cpp` | – |
| `tests/` | `selftest.py` checks the checker on `tests/bad/` (one fault each) | – |
| `sdk/`, `engine/core/`, `tools/build_plugin.py`, `third_party/` | **the rig's own files**, copied unchanged by `tools/sync_from_rig.py` | **never** |
| `rig.json`, `RIG_VERSION` | the rig's built-in ids and categories, and the commit they came from | never (synced) |
| `preview/rig-test.html` | the rig, as one page | never (synced) |

If the rig's own files seem wrong, don't patch them here: plugins must build against exactly what the
rig has. Report it instead.
