// tests/bad/badclick - a 0-4x gain with no smoothing: rigcheck must FAIL "clicks".
#pragma once
#include "module.h"
namespace rig {
inline constexpr ParamDesc BADCLICK_PARAMS[] = {{"gain", "GAIN", 0.f, 4.f, 0.25f, "x", Curve::Lin, 2, false}};
inline constexpr ModuleDesc BADCLICK_DESC = {"badclick", "BAD CLICK", "unsmoothed", 1, BADCLICK_PARAMS, 0, {nullptr, nullptr}};
struct BadClick : Module {
  const ModuleDesc& desc() const override { return BADCLICK_DESC; }
  void init(float s) override { sr = s; }
  void reset() override {}
  void process(const float* inL, const float* inR, float* oL, float* oR, int n, const float* e, const Context&) override {
    const float g = map(0, e[0]);                           // the fault: jumps with the knob
    for (int i = 0; i < n; i++) { oL[i] = inL[i] * g; oR[i] = inR[i] * g; }
  }
};
}
