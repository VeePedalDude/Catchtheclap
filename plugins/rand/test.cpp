// plugins/rand/test.cpp - does RANDOM do what it says? (python3 tools/check.py plugins/rand)
#include "rigtest.h"
#include "rand.h"
using namespace rigtest;

static int steps(const std::vector<float>& v) { int n = 0; for (size_t i = 1; i < v.size(); i++) n += fabsf(v[i] - v[i - 1]) > 1e-6f; return n; }

TEST("RATE picks new values per beat, following the tempo") {
  for (float bpm : {90.f, 120.f}) for (float rate : {1.f, 2.f, 4.f}) {
    Src<rig::RandomSH> s; s.bpm = bpm; s.set("rate", rate); s.set("slew", 0);
    const auto v = s.run({}, 30);
    const float want = 30 * bpm / 60 * rate, got = (float)steps(v);
    CHECK(fabsf(got - want) <= 2, "%.0f bpm, RATE %.0f per beat: %.0f new values in 30 s (expected %.0f)", bpm, rate, got, want);
  }
}

TEST("SLEW glides between values instead of jumping") {
  float jumps[2];
  for (int i = 0; i < 2; i++) {
    Src<rig::RandomSH> s; s.set("rate", 2); s.set("slew", i ? 300.f : 0.f);
    const auto v = s.run({}, 30); float big = 0;
    for (size_t k = 1; k < v.size(); k++) big = fmaxf(big, fabsf(v[k] - v[k - 1]));
    jumps[i] = big;
  }
  CHECK(jumps[0] > 0.5f && jumps[1] < 0.05f, "largest change in one block: %.3f with SLEW 0, %.3f with SLEW 300 ms", jumps[0], jumps[1]);
}

TEST("values spread over -1..1") {
  Src<rig::RandomSH> s; s.set("rate", 8); const auto v = s.run({}, 60);
  float lo = 1, hi = -1, mean = 0; for (float x : v) { lo = fminf(lo, x); hi = fmaxf(hi, x); mean += x / v.size(); }
  CHECK(lo > -1.0001f && hi < 1.0001f && hi - lo > 1.8f && fabsf(mean) < 0.1f, "range %.2f..%.2f, mean %+.3f", lo, hi, mean);
}

RIGTEST_MAIN
