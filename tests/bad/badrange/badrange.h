// tests/bad/badrange - a source whose output swings to +-2: rigcheck must FAIL "hard cases".
#pragma once
#include "source.h"
namespace rig {
inline constexpr ParamDesc BADRANGE_PARAMS[] = {{"rate", "RATE", 0.1f, 10.f, 0.5f, "Hz", Curve::Log, 1, false}};
inline constexpr SourceDesc BADRANGE_DESC = {"badrange", "BAD RANGE", "out of range", nullptr, 0, {nullptr, nullptr}, 1, BADRANGE_PARAMS};
struct BadRange : Source {
  float ph = 0;
  const SourceDesc& desc() const override { return BADRANGE_DESC; }
  void init(float s) override { sr = s; }
  float update(const SourceContext& c) override { ph += c.dt * knob(c, 0); ph -= floorf(ph); return 2.f * sinf(2 * PI_F * ph); }   // the fault
};
}
