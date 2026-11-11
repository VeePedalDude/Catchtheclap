// plugins/{{ID}}/{{ID}}.h - {{NAME}}: {{SUB}}. DRIVE 0-24 dB, LEVEL -24..+6 dB. Default 12 dB, 0 dB.
//
// A starting point from tools/new_plugin.py: a soft clipper that passes every check. Replace the knobs
// and process() with your effect; keep the shape (one knob table, one ModuleDesc, one struct). The rules
// are in AGENTS.md; `python3 tools/check.py plugins/{{ID}} --fast` checks them.
#pragma once
#include "module.h"

namespace rig {

// The knobs (up to 8; the first 4 are on the card's front). Defaults are normalised 0..1.
//                    symbol   label    min     max   default unit  curve       decimals sign
inline constexpr ParamDesc {{UPPER}}_PARAMS[] = {
  {"drive", "DRIVE",  0.f,   24.f, 0.5f,  "dB", Curve::Lin, 1, false},
  {"level", "LEVEL", -24.f,  6.f,  0.8f,  "dB", Curve::Lin, 1, true},
};
//                                         id       face title  subtitle   knobs                 meters
inline constexpr ModuleDesc {{UPPER}}_DESC = {"{{ID}}", "{{NAME}}", "{{SUB}}", 2, {{UPPER}}_PARAMS, 1, {"drive", nullptr}};

struct {{STRUCT}} : Module {
  // All state in members (never static): the chain can hold several of these. Nothing in process() allocates.
  OnePole driveS, levelS; float shown = 0;
  const ModuleDesc& desc() const override { return {{UPPER}}_DESC; }
  void init(float s) override { sr = s; driveS.setMs(sr, 20); levelS.setMs(sr, 20); reset(); }
  // Back to silence, with smoothers at the defaults' real values (nothing sweeps at start-up).
  void reset() override { driveS.y = map(0, desc().params[0].def); levelS.y = map(1, desc().params[1].def); shown = 0; }
  // e[i] = knob i for this block, normalised and already modulated; map(i, e[i]) is its real value.
  void process(const float* inL, const float* inR, float* oL, float* oR, int n, const float* e, const Context&) override {
    const float driveT = map(0, e[0]), levelT = map(1, e[1]);
    for (int i = 0; i < n; i++) {
      const float g = db2lin(driveS.next(driveT)), out = db2lin(levelS.next(levelT)) / sqrtf(g);   // smoothed: a jumping knob never clicks
      const float l = inL[i], r = inR[i];                       // read both before writing (the buffers may be the same)
      oL[i] = tanhf(l * g) * out; oR[i] = tanhf(r * g) * out;
    }
    shown = driveS.y;
  }
  void meters(float* out) const override { out[0] = shown; }   // up to 2 values for the face, named in the desc
};

} // namespace rig
