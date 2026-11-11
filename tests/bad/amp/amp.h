// tests/bad/amp - takes the built-in amp's id: rigcheck must FAIL "id".
#pragma once
#include "module.h"
namespace rig {
inline constexpr ParamDesc BADID_PARAMS[] = {{"level", "LEVEL", 0.f, 1.f, 1.f, "", Curve::Lin, 2, false}};
inline constexpr ModuleDesc BADID_DESC = {"amp", "NOT THE AMP", "a taken id", 1, BADID_PARAMS, 0, {nullptr, nullptr}};
struct BadId : Module {
  OnePole g;
  const ModuleDesc& desc() const override { return BADID_DESC; }
  void init(float s) override { sr = s; g.setMs(sr, 20); reset(); }
  void reset() override { g.y = 1; }
  void process(const float* inL, const float* inR, float* oL, float* oR, int n, const float* e, const Context&) override {
    for (int i = 0; i < n; i++) { const float a = g.next(e[0]); oL[i] = inL[i] * a; oR[i] = inR[i] * a; }
  }
};
}
