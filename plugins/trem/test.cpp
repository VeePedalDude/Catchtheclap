// plugins/trem/test.cpp - does TREMOLO do what it says? (python3 tools/check.py plugins/trem)
#include "rigtest.h"
#include "trem.h"
using namespace rigtest;

// A held note whose cycle is exactly 128 samples (375 Hz), measured over whole cycles, so the level
// readings carry no ripple of their own: what moves is the tremolo.
static Signal steady() { return sine(48000, 375, 4); }
static const size_t WIN = 128; static const float WIN_MS = 1000.f * WIN / 48000;
static float swingDb(const Stereo& o) {                                       // max over min level, after 0.5 s
  const auto env = envelope(o.L, WIN); float lo = 1e9f, hi = 0;
  for (size_t i = 200; i < env.size(); i++) { lo = fminf(lo, env[i]); hi = fmaxf(hi, env[i]); }
  return dB(hi) - dB(lo + 1e-9f);
}

TEST("RATE sets the speed in Hz") {
  for (float want : {1.f, 4.f, 12.f}) {
    Fx<rig::Trem> fx; fx.set("depth", 100); fx.set("rate", want);
    const auto env = envelope(fx.run(steady()).L, WIN);                      // level every 2.7 ms
    const Signal tail(env.begin() + 200, env.end());
    const float hz = 1000.f / (WIN_MS * (float)period(tail, 25, 410));
    CHECK(fabsf(hz - want) < want * 0.03f, "RATE %.0f Hz: swings at %.2f Hz", want, hz);
  }
}

TEST("DEPTH sets how far the level dips") {
  for (float depth : {0.f, 50.f, 100.f}) {
    Fx<rig::Trem> fx; fx.set("depth", depth); fx.set("rate", 4);
    const float sw = swingDb(fx.run(steady()));
    const float want = depth >= 100 ? 40.f : -dB(1 - depth / 100.f);          // gain dips to 1 - depth
    CHECK(depth >= 100 ? sw > want : fabsf(sw - want) < 0.5f, "DEPTH %.0f%%: level swings %.1f dB (%s %.1f)", depth, sw, depth >= 100 ? "over" : "expected", want);
  }
}

TEST("SHAPE goes from sine to square") {
  for (float shape : {0.f, 1.f}) {
    Fx<rig::Trem> fx; fx.set("depth", 100); fx.set("rate", 2); fx.set("shape", shape);
    const auto env = envelope(fx.run(steady()).L, WIN);
    float hi = 0; for (float v : env) hi = fmaxf(hi, v);
    int edge = 0; for (size_t i = 200; i < env.size(); i++) edge += env[i] < 0.1f * hi || env[i] > 0.9f * hi;
    const float frac = edge / (float)(env.size() - 200);
    CHECK(shape == 0 ? frac < 0.5f : frac > 0.85f, "SHAPE %.0f: %.0f%% of the time at full or none (a sine spends ~40%%, a square ~100%%)", shape, frac * 100);
  }
}

TEST("the gain meter follows the level") {
  Fx<rig::Trem> fx; fx.set("depth", 100); float lo = 1, hi = 0;
  fx.run(steady(), [&](int, Fx<rig::Trem>& f) { const float g = f.meters()[0]; lo = fminf(lo, g); hi = fmaxf(hi, g); });
  CHECK(lo >= 0 && hi <= 1 && hi - lo > 0.9f, "meter gain spans %.2f..%.2f", lo, hi);
}

RIGTEST_MAIN
