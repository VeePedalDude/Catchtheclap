// tests/bad/badnan - divides by its knob: NaN/Inf at 0. rigcheck must FAIL "hard cases".
#pragma once
#include "module.h"
namespace rig {
inline constexpr ParamDesc BADNAN_PARAMS[] = {{"drive", "DRIVE", 0.f, 10.f, 0.5f, "", Curve::Lin, 1, false}};
inline constexpr ModuleDesc BADNAN_DESC = {"badnan", "BAD NAN", "divides by zero", 1, BADNAN_PARAMS, 0, {nullptr, nullptr}};
struct BadNan : Module {
  const ModuleDesc& desc() const override { return BADNAN_DESC; }
  void init(float s) override { sr = s; }
  void reset() override {}
  void process(const float* inL, const float* inR, float* oL, float* oR, int n, const float* e, const Context&) override {
    const float d = map(0, e[0]);
    for (int i = 0; i < n; i++) { oL[i] = tanhf(inL[i] * d) / tanhf(d); oR[i] = tanhf(inR[i] * d) / tanhf(d); }   // the fault: 0/0 at DRIVE 0
  }
};
}
