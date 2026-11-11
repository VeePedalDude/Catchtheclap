// plugins/{{ID}}/{{ID}}.h - {{NAME}}: {{SUB}}. RATE 0.05-20 Hz, SHAPE sine (0) to triangle (1).
// Default 1 Hz, sine.
//
// A starting point from tools/new_plugin.py: a free-running LFO that passes every check. Replace the
// knobs and update() with your source; keep the shape. Rules: AGENTS.md. Check: tools/check.py.
#pragma once
#include "source.h"

namespace rig {

//                    symbol   label    min    max    default unit  curve       decimals sign
inline constexpr ParamDesc {{UPPER}}_PARAMS[] = {
  {"rate",  "RATE",  0.05f, 20.f, 0.5f,  "Hz", Curve::Log, 2, false},
  {"shape", "SHAPE", 0.f,   1.f,  0.f,   "",   Curve::Lin, 2, false},
};
//                                         id       orb label   description  ext      meters                knobs
inline constexpr SourceDesc {{UPPER}}_DESC = {"{{ID}}", "{{NAME}}", "{{SUB}}", nullptr, 1, {"phase", nullptr}, 2, {{UPPER}}_PARAMS};

struct {{STRUCT}} : Source {
  float ph = 0;                                                 // state in members, never static
  const SourceDesc& desc() const override { return {{UPPER}}_DESC; }
  void init(float s) override { sr = s; ph = 0; }
  // Once per block (c.n samples, c.dt seconds). Return -1..1, always finite. knob(c, i) = knob i's real value.
  float update(const SourceContext& c) override {
    ph += c.dt * knob(c, 0); ph -= floorf(ph);                  // accumulate the phase: a jumping RATE never jumps the output
    const float q = ph + 0.25f - floorf(ph + 0.25f);                // the triangle in step with the sine, so SHAPE morphs smoothly
    const float sh = c.p[1], s = sinf(2 * PI_F * ph), tri = 1.f - 4.f * fabsf(q - 0.5f);
    return clampf((1 - sh) * s + sh * tri, -1.f, 1.f);
  }
  void meters(float* out) const override { out[0] = ph; }
};

} // namespace rig
