// tests/bad/badstatic - its LFO phase is a static, shared by every instance: rigcheck must FAIL "instances".
#pragma once
#include "module.h"
namespace rig {
inline constexpr ParamDesc BADSTATIC_PARAMS[] = {{"rate", "RATE", 0.5f, 8.f, 0.5f, "Hz", Curve::Log, 1, false}};
inline constexpr ModuleDesc BADSTATIC_DESC = {"badstatic", "BAD STATIC", "shares state", 1, BADSTATIC_PARAMS, 0, {nullptr, nullptr}};
inline float g_phase = 0;                                 // the fault
struct BadStatic : Module {
  OnePole r;
  const ModuleDesc& desc() const override { return BADSTATIC_DESC; }
  void init(float s) override { sr = s; r.setMs(sr, 20); reset(); }
  void reset() override { r.y = 2.f; }
  void process(const float* inL, const float* inR, float* oL, float* oR, int n, const float* e, const Context&) override {
    for (int i = 0; i < n; i++) { g_phase += r.next(map(0, e[0])) / sr; if (g_phase >= 1) g_phase -= 1; const float a = 0.5f + 0.5f * sinf(2 * PI_F * g_phase); oL[i] = inL[i] * a; oR[i] = inR[i] * a; }
  }
};
}
