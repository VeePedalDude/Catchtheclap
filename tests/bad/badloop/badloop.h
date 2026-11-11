// tests/bad/badloop - an echo whose feedback goes above 1 with nothing to stop it: rigcheck must FAIL "hard cases".
#pragma once
#include "module.h"
namespace rig {
inline constexpr ParamDesc BADLOOP_PARAMS[] = {{"fb", "FEEDBACK", 0.f, 130.f, 0.3f, "%", Curve::Lin, 0, false}};
inline constexpr ModuleDesc BADLOOP_DESC = {"badloop", "BAD LOOP", "runs away", 1, BADLOOP_PARAMS, 0, {nullptr, nullptr}};
struct BadLoop : Module {
  DelayLine<8192> dl; OnePole fb;
  const ModuleDesc& desc() const override { return BADLOOP_DESC; }
  void init(float s) override { sr = s; fb.setMs(sr, 30); reset(); }
  void reset() override { dl.clear(); fb.y = 0.39f; }
  void process(const float* inL, const float* inR, float* oL, float* oR, int n, const float* e, const Context&) override {
    for (int i = 0; i < n; i++) { const float y = dl.tap(4800.f), x = 0.5f * (inL[i] + inR[i]) + fb.next(map(0, e[0]) * 0.01f) * y; dl.push(x); oL[i] = inL[i] + y; oR[i] = inR[i] + y; }   // the fault: no clamp, gain up to 1.3
  }
};
}
