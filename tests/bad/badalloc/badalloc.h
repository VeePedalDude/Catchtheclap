// tests/bad/badalloc - allocates in process(): rigcheck must FAIL "allocation".
#pragma once
#include "module.h"
#include <vector>
namespace rig {
inline constexpr ParamDesc BADALLOC_PARAMS[] = {{"gain", "GAIN", 0.f, 1.f, 0.5f, "", Curve::Lin, 2, false}};
inline constexpr ModuleDesc BADALLOC_DESC = {"badalloc", "BAD ALLOC", "allocates", 1, BADALLOC_PARAMS, 0, {nullptr, nullptr}};
struct BadAlloc : Module {
  OnePole g;
  const ModuleDesc& desc() const override { return BADALLOC_DESC; }
  void init(float s) override { sr = s; g.setMs(sr, 20); reset(); }
  void reset() override { g.y = 0.5f; }
  void process(const float* inL, const float* inR, float* oL, float* oR, int n, const float* e, const Context&) override {
    std::vector<float> tmp((size_t)n);                     // the fault
    for (int i = 0; i < n; i++) { tmp[(size_t)i] = g.next(e[0]); oL[i] = inL[i] * tmp[(size_t)i]; oR[i] = inR[i] * tmp[(size_t)i]; }
  }
};
}
