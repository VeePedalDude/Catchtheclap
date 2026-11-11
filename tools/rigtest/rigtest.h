// tools/rigtest/rigtest.h - a plugin's own tests: does it do what it says? rigcheck already checks the rig's
// rules (NaN, clicks, allocation, shared state...); these check the claims: a tremolo's rate in Hz, an
// echo's time in ms, a filter's response. tools/check.py compiles plugins/<id>/test.cpp against the
// plugin's header and runs it. A test file:
//
//   #include "rigtest.h"
//   #include "trem.h"
//   using namespace rigtest;
//   TEST("RATE sets the tremolo's speed") {
//     Fx<rig::Trem> fx; fx.set("depth", 100); fx.set("rate", 4);   // knob values in their own units
//     const auto o = fx.run(sine(48000, 220, 4));
//     const auto env = envelope(o.L, 48);                          // level every 1 ms
//     const float hz = 1000.f / period(env, 50, 1000);             // its strongest period, in ms
//     CHECK(fabsf(hz - 4) < 0.1f, "swings at %.2f Hz (asked for 4)", hz);
//   }
//   RIGTEST_MAIN
//
// Report numbers in every CHECK message: they are the evidence the effect works.
#pragma once
#include "module.h"
#include "source.h"
#include "signals.h"
#include "heap.h"
#include <functional>
#include <string>
#include <vector>

namespace rigtest {

// ------------------------------------------------------------------ tests
struct Registry { std::vector<std::pair<std::string, std::function<void()>>> tests; int checks = 0, fails = 0; static Registry& get() { static Registry r; return r; } };
struct Reg { Reg(const char* n, std::function<void()> f) { Registry::get().tests.push_back({n, f}); } };
#define RIGTEST_CAT2(a, b) a##b
#define RIGTEST_CAT(a, b) RIGTEST_CAT2(a, b)
#define TEST(name) static void RIGTEST_CAT(rigtest_fn_, __LINE__)(); static rigtest::Reg RIGTEST_CAT(rigtest_reg_, __LINE__)(name, RIGTEST_CAT(rigtest_fn_, __LINE__)); static void RIGTEST_CAT(rigtest_fn_, __LINE__)()
#define CHECK(cond, ...) do { const bool _ok = (cond); rigtest::Registry::get().checks++; printf("  [%s] ", _ok ? "PASS" : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!_ok) rigtest::Registry::get().fails++; } while (0)
#define RIGTEST_MAIN int main() { auto& r = rigtest::Registry::get(); for (auto& t : r.tests) { printf("%s\n", t.first.c_str()); t.second(); } \
  printf("%s: %d checks, %d failed\n", r.fails ? "TESTS FAILED" : "TESTS PASSED", r.checks, r.fails); return r.fails ? 1 : 0; }

// Normalised 0..1 for a value in a knob's own units (clamped), or a stop by name.
inline float normOf(const rig::ParamDesc& p, float v) {
  const float n = p.curve == rig::Curve::Log ? logf(v / p.min) / logf(p.max / p.min) : (v - p.min) / (p.max - p.min);
  return n < 0 ? 0.f : n > 1 ? 1.f : n;
}
inline int knobIndex(const rig::ParamDesc* ps, int n, const char* symbol) {
  for (int i = 0; i < n; i++) if (!strcmp(ps[i].symbol, symbol)) return i;
  fprintf(stderr, "rigtest: no knob \"%s\"\n", symbol); exit(2);
}

// ------------------------------------------------------------------ an effect, played as the engine plays it
struct Stereo { Signal L, R; };
template <class T> struct Fx {
  T m; float sr; float e[rig::MAXPAR] = {0}; rig::Context ctx;
  explicit Fx(float s = 48000.f) : sr(s) { m.init(sr); ctx.sr = sr; for (int k = 0; k < m.desc().nparams; k++) e[k] = m.desc().params[k].def; }
  void set(const char* symbol, float value) { const int k = knobIndex(m.desc().params, m.desc().nparams, symbol); e[k] = normOf(m.desc().params[k], value); }
  void setNorm(const char* symbol, float v) { e[knobIndex(m.desc().params, m.desc().nparams, symbol)] = v; }
  void setStop(const char* symbol, const char* stop) {
    const int k = knobIndex(m.desc().params, m.desc().nparams, symbol); const auto& p = m.desc().params[k];
    for (int s = 0; s < p.nstops; s++) if (!strcmp(p.stops[s].name, stop)) { e[k] = normOf(p, p.stops[s].v); return; }
    fprintf(stderr, "rigtest: %s has no stop \"%s\"\n", symbol, stop); exit(2);
  }
  float real(const char* symbol) const { const int k = knobIndex(m.desc().params, m.desc().nparams, symbol); return m.map(k, e[k]); }
  // Plays in (mono, to both sides) or inL/inR in blocks of `block` (<= 128); perBlock(b, *this) may move knobs.
  Stereo run(const Signal& inL, const Signal& inR, const std::function<void(int, Fx&)>& perBlock = nullptr, int block = rig::MAXBLOCK) {
    Stereo o; o.L.assign(inL.size(), 0); o.R.assign(inL.size(), 0); int b = 0;
    for (size_t i = 0; i < inL.size(); i += (size_t)block, b++) {
      const int n = (int)std::min<size_t>((size_t)block, inL.size() - i);
      if (perBlock) perBlock(b, *this);
      m.process(&inL[i], &inR[i], &o.L[i], &o.R[i], n, e, ctx);
    }
    return o;
  }
  Stereo run(const Signal& in, const std::function<void(int, Fx&)>& perBlock = nullptr) { return run(in, in, perBlock); }
  std::vector<float> meters() const { std::vector<float> v(rig::MAXMODMETERS, 0.f); m.meters(v.data()); return v; }
};

// ------------------------------------------------------------------ a modulation source: one value per block
template <class T> struct Src {
  T s; float sr; float p[rig::MAXPAR] = {0}; float bpm = 120.f;
  explicit Src(float r = 48000.f) : sr(r) { s.init(sr); for (int k = 0; k < s.desc().nparams; k++) p[k] = s.desc().params[k].def; }
  void set(const char* symbol, float value) { const int k = knobIndex(s.desc().params, s.desc().nparams, symbol); p[k] = normOf(s.desc().params[k], value); }
  void setNorm(const char* symbol, float v) { p[knobIndex(s.desc().params, s.desc().nparams, symbol)] = v; }
  // The value each block while `in` plays (silence if empty: `secs` of it). Block = 128 samples.
  std::vector<float> run(const Signal& in, float secs = 0) {
    const size_t len = in.empty() ? (size_t)(secs * sr) : in.size(); std::vector<float> out; static const float zeros[rig::MAXBLOCK] = {0};
    for (size_t i = 0; i < len; i += rig::MAXBLOCK) {
      const int n = (int)std::min<size_t>(rig::MAXBLOCK, len - i);
      rig::SourceContext c{}; c.in = in.empty() ? zeros : &in[i]; c.n = n; c.sr = sr; c.dt = n / sr; c.bpm = bpm; c.p = p;
      out.push_back(s.update(c));
    }
    return out;
  }
  float blockSecs() const { return rig::MAXBLOCK / sr; }
};

} // namespace rigtest
