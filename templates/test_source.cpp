// plugins/{{ID}}/test.cpp - does {{NAME}} do what it says? `python3 tools/check.py plugins/{{ID}} --fast`
// runs it after the rig's own checks. Test each claim with a number; replace these with your source's.
#include "rigtest.h"
#include "{{ID}}.h"
using namespace rigtest;

TEST("RATE sets the speed in Hz") {
  for (float hz : {0.5f, 2.f, 8.f}) {
    Src<rig::{{STRUCT}}> s; s.set("rate", hz);
    const auto v = s.run({}, 20);
    int up = 0; for (size_t i = 1; i < v.size(); i++) up += v[i - 1] < 0 && v[i] >= 0;   // upward zero crossings = cycles
    CHECK(fabsf(up / 20.f - hz) < 0.1f, "RATE %.1f Hz: %d cycles in 20 s (%.2f Hz)", hz, up, up / 20.f);
  }
}

TEST("the output stays within -1..1") {
  Src<rig::{{STRUCT}}> s; s.set("rate", 20); s.setNorm("shape", 1); float lo = 0, hi = 0;
  for (float x : s.run({}, 10)) { lo = fminf(lo, x); hi = fmaxf(hi, x); }
  CHECK(lo >= -1 && hi <= 1 && hi - lo > 1.9f, "range %.3f..%.3f", lo, hi);
}

RIGTEST_MAIN
