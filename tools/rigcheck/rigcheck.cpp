// tools/rigcheck/rigcheck.cpp - RIG/01's conformance checks, run on a built plugin.
//
//   rigcheck check  <plugin.clap> [--rig rig.json] [--json report.json]   every check below; exit 1 on a FAIL
//   rigcheck info   <plugin.clap>                                          what the rig will show for it
//   rigcheck render <plugin.clap> [--in in.wav] [--out out.wav] [--sr 48000] [--set knob=value ...]
//                                     play a WAV (default: the test riff) through it; values in the knob's
//                                     own units, a stop's name, or "50%" of the knob's travel
//
// The plugin is opened with the rig's own loader (engine/core/plugin_clap.h, as rig-host loads it) and
// played the way the engine plays it: 128-sample stereo blocks, knobs normalised 0..1. tools/check.py
// builds the plugin and runs this on its Linux .clap; it works on any CLAP effect rig-host can load.
//
// Results are PASS, WARN (worth a look, the rig still takes it) or FAIL (the rig's rules say no).
#include "plugin_clap.h"
#include "../rigtest/signals.h"
#include "../rigtest/heap.h"
#include <chrono>
#include <cstdarg>
#include <fstream>
#include <functional>
#include <map>

using namespace rigtest;
using rig::PluginInfo; using rig::ClapLibrary; using rig::MAXPAR; using rig::MAXBLOCK;

namespace {

// ------------------------------------------------------------------ results
struct Result { std::string status, name, detail; };
std::vector<Result> results;
void report(const char* status, const std::string& name, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
void report(const char* status, const std::string& name, const char* fmt, ...) {
  char b[1024]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
  results.push_back({status, name, b});
  printf("  [%s] %s: %s\n", status, name.c_str(), b); fflush(stdout);
}
#define PASS(n, ...) report("PASS", n, __VA_ARGS__)
#define WARN(n, ...) report("WARN", n, __VA_ARGS__)
#define FAIL(n, ...) report("FAIL", n, __VA_ARGS__)
std::string jesc(const std::string& s) { std::string o; for (char c : s) { if (c == '"' || c == '\\') o += '\\'; if ((unsigned char)c >= 0x20) o += c; } return o; }

// ------------------------------------------------------------------ the plugin, as the engine runs it
struct Plug {
  ClapLibrary lib; PluginInfo info; std::string err; bool ok = false;
  int np() const { return (int)info.params.size(); }
  std::vector<float> defaults() const { std::vector<float> d(MAXPAR, 0.f); for (int k = 0; k < np(); k++) d[(size_t)k] = info.params[(size_t)k].def; return d; }
};
struct Out { Signal L, R, src; std::vector<float> meters; bool meterFinite = true; double secs = 0; };
// Knobs for block b (normalised); null = the defaults throughout.
using Knobs = std::function<void(int block, float* norm)>;

struct Inst {
  Plug& P; void* h = nullptr; float sr;
  Inst(Plug& p, float s) : P(p), sr(s) { h = P.lib.create(sr); }
  ~Inst() { if (h) P.lib.destroy(h); }
  Inst(const Inst&) = delete; Inst& operator=(const Inst&) = delete;
  // Plays inL/inR; `sizes` (if set) gives each block's length (1..128), else 128.
  Out run(const Signal& inL, const Signal& inR, const Knobs& knobs = nullptr, float bpm = 120.f, const std::function<int(int)>& sizes = nullptr) {
    Out o; o.L.assign(inL.size(), 0.f); o.R.assign(inL.size(), 0.f);
    std::vector<float> norm = P.defaults();
    float m[rig::MAXMODMETERS];
    const auto t0 = std::chrono::steady_clock::now();
    int b = 0;
    for (size_t i = 0; i < inL.size(); b++) {
      const int n = (int)std::min<size_t>(sizes ? (size_t)sizes(b) : (size_t)MAXBLOCK, inL.size() - i);
      if (knobs) knobs(b, norm.data());
      P.lib.process(h, &inL[i], &inR[i], &o.L[i], &o.R[i], n, norm.data(), P.np(), bpm);
      if (P.info.source) o.src.push_back(P.lib.output(h));
      const int nm = P.lib.meters(h, m, rig::MAXMODMETERS);
      for (int k = 0; k < nm; k++) { if (!std::isfinite(m[k])) o.meterFinite = false; }
      i += (size_t)n;
    }
    o.secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return o;
  }
  Out run(const Signal& in, const Knobs& knobs = nullptr, float bpm = 120.f) { return run(in, in, knobs, bpm); }
};
bool finiteOut(const Out& o) { return finite(o.L) && finite(o.R) && finite(o.src); }
float peakOut(const Out& o) { return std::max(peak(o.L), peak(o.R)); }
float srcRange(const Out& o, size_t from = 0) { float lo = 1e9f, hi = -1e9f; for (size_t i = from; i < o.src.size(); i++) { lo = std::min(lo, o.src[i]); hi = std::max(hi, o.src[i]); } return o.src.size() > from ? hi - lo : 0.f; }
float srcPeak(const Out& o) { float p = 0; for (float v : o.src) p = std::max(p, std::fabs(v)); return p; }
float srcStep(const Out& o, size_t from = 1) { float s = 0; for (size_t i = std::max<size_t>(from, 1); i < o.src.size(); i++) s = std::max(s, std::fabs(o.src[i] - o.src[i - 1])); return s; }

// A knob's value in its own units, for messages.
std::string real(const PluginInfo& I, int k, float v) {
  const auto& p = I.params[(size_t)k]; char b[64]; snprintf(b, sizeof b, "%.*f%s%s", p.decimals, p.map(v), *p.unit ? " " : "", p.unit); return b;
}
std::vector<std::string> readList(const std::string& rigJson, const char* key) {
  std::vector<std::string> out; std::ifstream f(rigJson); if (!f) return out;
  const nlohmann::json j = nlohmann::json::parse(f, nullptr, false); if (j.is_discarded() || !j.contains(key)) return out;
  for (const auto& x : j[key]) out.push_back(x.is_string() ? x.get<std::string>() : x.value("id", ""));
  return out;
}

// ------------------------------------------------------------------ the checks
const float SR = 48000.f;

void checkDescription(Plug& P, const std::string& rigJson) {
  const PluginInfo& I = P.info; const char* what = I.source ? "source" : "effect";
  PASS("loads", "%s \"%s\" (%s), %d knob%s, %d meter%s, category %s, version %s", what, I.id.c_str(), I.name.c_str(), P.np(), P.np() == 1 ? "" : "s",
       I.source ? I.sdesc.nmeters : I.mdesc.nmeters, (I.source ? I.sdesc.nmeters : I.mdesc.nmeters) == 1 ? "" : "s", I.category.c_str(), I.version.c_str());
  const auto builtins = readList(rigJson, I.source ? "builtinSources" : "builtinEffects"), others = readList(rigJson, I.source ? "builtinEffects" : "builtinSources");
  if (builtins.empty()) WARN("id", "no rig.json to check the id against the rig's built-ins (--rig)");
  else if (std::find(builtins.begin(), builtins.end(), I.id) != builtins.end() || std::find(others.begin(), others.end(), I.id) != others.end())
    FAIL("id", "\"%s\" is a built-in's id: the rig would refuse it. Pick another (rig.json lists the taken ones)", I.id.c_str());
  else if (I.source && I.id.size() > 12) FAIL("id", "a source id is at most 12 characters (\"%s\" is %zu)", I.id.c_str(), I.id.size());
  else if (I.id.size() > 12) WARN("id", "\"%s\" is %zu characters; keep it to 12 so it fits the card", I.id.c_str(), I.id.size());
  else PASS("id", "\"%s\" isn't taken by a built-in", I.id.c_str());
  const auto cats = readList(rigJson, I.source ? "sourceCategories" : "effectCategories");
  if (!cats.empty() && std::find(cats.begin(), cats.end(), I.category) == cats.end())
    WARN("category", "\"%s\" isn't a tab of Add %s: it lands in %s", I.category.c_str(), I.source ? "a source" : "an effect", I.source ? "the first tab" : "Experimental");
  else if (!cats.empty()) PASS("category", "%s", I.category.c_str());
  std::string longLabels;
  for (const auto& p : I.params) if (strlen(p.label) > 10) longLabels += std::string(longLabels.empty() ? "" : ", ") + p.label;
  if (!longLabels.empty()) WARN("labels", "long knob labels may not fit a knob: %s (10 characters or fewer reads best)", longLabels.c_str());
  int dots = 0; for (char c : I.version) dots += c == '.';
  if (I.version.empty() || dots != 2) WARN("version", "\"%s\" isn't major.minor.patch (plugin.json \"version\")", I.version.c_str());
  if (P.np() > 4 && !I.source) PASS("layout", "knobs 1-4 (%s...) on the card's front, %d on its back", I.params[0].label, P.np() - 4);
}

// Knob settings to try: defaults, all at 0, all at 1, then random ones.
std::vector<std::vector<float>> settings(Plug& P, int randoms) {
  std::vector<std::vector<float>> s = {P.defaults(), std::vector<float>(MAXPAR, 0.f), std::vector<float>(MAXPAR, 1.f)};
  unsigned r = 12345; auto rnd = [&] { r = r * 1664525u + 1013904223u; return (r >> 8) / 16777216.f; };
  for (int i = 0; i < randoms; i++) { std::vector<float> v(MAXPAR); for (auto& x : v) x = rnd(); s.push_back(v); }
  return s;
}
std::string settingName(Plug& P, size_t i, const std::vector<float>& v) {
  if (i < 3) return i == 0 ? "defaults" : i == 1 ? "all knobs at 0" : "all knobs at 1";
  std::string o = "random:"; for (int k = 0; k < P.np(); k++) o += std::string(" ") + P.info.params[(size_t)k].symbol + "=" + real(P.info, k, v[(size_t)k]);
  return o;
}

void checkHardCases(Plug& P, bool quick) {
  struct Sig { const char* name; Signal s; };
  std::vector<Sig> sigs = {{"the riff", riff(SR, quick ? 4.f : 8.f)}, {"full-scale noise", noise(SR, 3)}, {"quiet noise (-60 dB)", noise(SR, 3, 0.001f)},
                           {"silence", constant(SR, 3, 0)}, {"DC 0.5", constant(SR, 3, 0.5f)}, {"denormals", constant(SR, 3, 1e-39f)},
                           {"full-scale square 80 Hz", square(SR, 80, 3)}, {"impulses", impulses(SR, 3, 0.25f)}};
  const auto S = settings(P, quick ? 6 : 24);
  int runs = 0; float worst = 0; std::string worstAt, bad;
  for (const auto& g : sigs) for (size_t i = 0; i < S.size(); i++) {
    Inst x(P, SR); if (!x.h) { FAIL("hard cases", "the plugin wouldn't start an instance"); return; }
    const auto& v = S[i];
    const Out o = x.run(g.s, [&](int, float* n) { for (int k = 0; k < P.np(); k++) n[k] = v[(size_t)k]; });
    runs++;
    const float pk = P.info.source ? srcPeak(o) : peakOut(o);
    if (!finiteOut(o) && bad.empty()) bad = std::string("NaN or Inf with ") + g.name + ", " + settingName(P, i, v);
    if (!o.meterFinite && bad.empty()) bad = std::string("a meter went NaN or Inf with ") + g.name + ", " + settingName(P, i, v);
    if (P.info.source && pk > 1.0001f && bad.empty()) { char b[64]; snprintf(b, sizeof b, "output %.3f, outside -1..1, with ", pk); bad = b + std::string(g.name) + ", " + settingName(P, i, v); }
    if (pk > worst) { worst = pk; worstAt = std::string(g.name) + ", " + settingName(P, i, v); }
  }
  if (!bad.empty()) FAIL("hard cases", "%s", bad.c_str());
  else if (!P.info.source && worst > 63.1f) FAIL("hard cases", "output reached %+.1f dBFS (over the +36 dBFS limit) with %s", dB(worst), worstAt.c_str());
  else if (P.info.source) PASS("hard cases", "%d runs (%zu inputs x %zu knob settings): always finite, within -1..1", runs, sigs.size(), S.size());
  else PASS("hard cases", "%d runs (%zu inputs x %zu knob settings): always finite; loudest %+.1f dBFS (%s)", runs, sigs.size(), S.size(), dB(worst), worstAt.c_str());
}

void checkDefaults(Plug& P) {
  const Signal g = riff(SR);
  Inst x(P, SR); const Out o = x.run(g);
  if (P.info.source) {
    const float range = srcRange(o, 10);
    if (range < 0.05f) WARN("at its defaults", "it hardly moves while the riff plays (range %.3f of -1..1): is that intended?", range);
    else PASS("at its defaults", "moves over %.2f of -1..1 while the riff plays (min..max)", range);
    return;
  }
  const double in = rms(g), out = rms(o.L), d = rms(diff(o.L, g));
  const float pk = peakOut(o);
  if (out < in * 0.063) WARN("at its defaults", "output %.1f dB below its input: nearly silent at the defaults", dB(in) - dB(out));
  else if (pk > 4.f) WARN("at its defaults", "peaks at %+.1f dBFS from a -12 dBFS guitar: very loud at the defaults", dB(pk));
  else if (d < in * 0.001) WARN("at its defaults", "the output is the input (difference %.0f dB below it): it does nothing at its defaults", dB(in) - dB(d));
  else PASS("at its defaults", "the riff in %.1f dBFS RMS, out %.1f dBFS (L) / %.1f (R), peak %+.1f dBFS; wet differs from dry by %.1f dB", dB(in), dB(out), dB(rms(o.R)), dB(pk), dB(d) - dB(in));
}

void checkSilence(Plug& P) {
  if (P.info.source) return;
  { Inst x(P, SR); const Out o = x.run(constant(SR, 2, 0));
    const float pk = peakOut(o);
    if (pk > 1e-4f) WARN("silence in", "silence in, %.1f dBFS out at the defaults (hiss or self-oscillation?)", dB(pk));
    else PASS("silence in", "silence out (peak %.0f dBFS) at the defaults", dB(std::max(pk, 1e-10f))); }
  { const Signal g = concat(riff(SR, 4), constant(SR, 12, 0));
    Inst x(P, SR); const Out o = x.run(g);
    const size_t s = (size_t)(14 * SR);
    const double tail = std::max(rms(o.L, s), rms(o.R, s));
    // denormals: the tail's CPU, against the riff's
    Inst y(P, SR); const auto t0 = std::chrono::steady_clock::now(); y.run(riff(SR, 4)); const double tr = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() / 4;
    Inst z(P, SR); z.run(riff(SR, 4)); const auto t1 = std::chrono::steady_clock::now(); z.run(constant(SR, 8, 0)); const double ts = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count() / 8;
    if (tail > 1e-4) WARN("tail", "10 s after the riff stops, still %.1f dBFS RMS at the defaults: does it ever die away?", dB(tail));
    else PASS("tail", "dies away after the riff: %.0f dBFS RMS 10-12 s later", dB(std::max(tail, 1e-10)));
    if (ts > 3 * tr && ts > 2e-4) WARN("denormals", "the silent tail costs %.1fx the CPU of playing (%.2f%% vs %.2f%% of a core): flush tiny values with undenorm()", ts / tr, ts * 100, tr * 100);
    else PASS("denormals", "the silent tail costs %.1fx the CPU of playing", ts / std::max(tr, 1e-9)); }
}

void checkClicks(Plug& P) {
  const Signal g = P.info.source ? riff(SR, 6) : sine(SR, 220, 3, 0.3f);
  std::string fails, warns, info; float worstR = 0;
  for (int k = 0; k < P.np(); k++) {
    auto fixed = [&](float v) { Inst x(P, SR); return x.run(g, [&](int, float* n) { n[k] = v; }); };
    const Out a = fixed(0.f), b = fixed(1.f);
    Inst x(P, SR);
    const Out j = x.run(g, [&](int blk, float* n) { n[k] = (blk / 64) % 2 ? 1.f : 0.f; });   // jumps end to end every 64 blocks (170 ms)
    const auto& p = P.info.params[(size_t)k];
    const bool mode = p.nstops > 0;
    char b2[160];
    if (P.info.source) {
      const float base = std::max(srcStep(a, 4), srcStep(b, 4)), step = srcStep(j, 4);
      snprintf(b2, sizeof b2, "%s %.3f per block (still: %.3f)", p.label, step, base);
      if (step > 3 * base + 0.2f) (warns += warns.empty() ? "" : "; ") += b2; else (info += info.empty() ? "" : ", ") += b2;
      continue;
    }
    const size_t from = (size_t)(0.05f * SR);
    const float base = std::max(maxStep(a.L, from), maxStep(b.L, from)), step = std::max(maxStep(j.L, from), maxStep(j.R, from));
    worstR = std::max(worstR, step / std::max(base, 1e-6f));
    snprintf(b2, sizeof b2, "%s %.3f (still: %.3f)", p.label, step, base);
    if (step > base + 0.5f && !mode) (fails += fails.empty() ? "" : "; ") += b2;
    else if (step > 3 * base + 0.02f) (warns += warns.empty() ? "" : "; ") += std::string(b2) + (mode ? " (a mode knob)" : "");
    else (info += info.empty() ? "" : ", ") += b2;
  }
  const char* what = P.info.source ? "the output's largest step when a knob jumps end to end" : "the largest sample step when a knob jumps end to end, on a 220 Hz sine";
  if (!fails.empty()) FAIL("clicks", "%s: %s. Smooth it (OnePole at 20-30 ms)", what, fails.c_str());
  else if (!warns.empty()) WARN("clicks", "%s: %s. A smoother would hide it%s", what, warns.c_str(), info.empty() ? "" : (". Fine: " + info).c_str());
  else PASS("clicks", "%s: %s", what, info.empty() ? "no knobs" : info.c_str());
}

void checkHeap(Plug& P) {
  if (!heapCounting()) { WARN("allocation", "can't count heap calls on this system (glibc only)"); return; }
  const long self = heapCallsDuring([] { void* p = malloc(16); free(p); });
  if (self != 2) { WARN("allocation", "heap counting isn't working here (%ld calls seen for 2)", self); return; }
  Inst x(P, SR); const Signal g = riff(SR, 4);
  Out o; o.L.assign(g.size(), 0); o.R.assign(g.size(), 0);
  std::vector<float> norm = P.defaults(); unsigned r = 99;
  x.run(Signal(MAXBLOCK * 4, 0.f));                              // first blocks outside the count (a lazy init would still be a bug, but say it apart)
  const long calls = heapCallsDuring([&] {
    for (size_t i = 0; i + MAXBLOCK <= g.size(); i += MAXBLOCK) {
      for (int k = 0; k < P.np(); k++) { r = r * 1664525u + 1013904223u; norm[(size_t)k] = (r >> 8) / 16777216.f; }   // knobs move every block, as modulation moves them
      P.lib.process(x.h, &g[i], &g[i], &o.L[i], &o.R[i], MAXBLOCK, norm.data(), P.np(), 120.f);
      if (P.info.source) P.lib.output(x.h);
      float m[rig::MAXMODMETERS]; P.lib.meters(x.h, m, rig::MAXMODMETERS);
    }
  });
  if (calls) FAIL("allocation", "%ld heap calls while processing 4 s with every knob moving each block: nothing in process() may allocate", calls);
  else PASS("allocation", "0 heap calls while processing 4 s with every knob moving each block");
}

void checkInstances(Plug& P) {
  const Signal g = riff(SR, 6), h = noise(SR, 6, 0.2f, 7);
  auto setA = [&](int, float* n) { for (int k = 0; k < P.np(); k++) n[k] = 0.3f + 0.4f * (float)(k % 2); };
  auto setB = [&](int, float* n) { for (int k = 0; k < P.np(); k++) n[k] = 0.9f - 0.8f * (float)(k % 2); };
  Out alone; { Inst a(P, SR); alone = a.run(g, setA); }
  Out again; { Inst a(P, SR); again = a.run(g, setA); }
  // two at once, block by block
  Inst a(P, SR), b(P, SR); Out both; both.L.assign(g.size(), 0); both.R.assign(g.size(), 0); Signal bl(g.size()), br(g.size());
  std::vector<float> na = P.defaults(), nb = P.defaults(); setA(0, na.data()); setB(0, nb.data());
  for (size_t i = 0; i < g.size(); i += MAXBLOCK) {
    const int n = (int)std::min<size_t>(MAXBLOCK, g.size() - i);
    P.lib.process(a.h, &g[i], &g[i], &both.L[i], &both.R[i], n, na.data(), P.np(), 120.f); if (P.info.source) both.src.push_back(P.lib.output(a.h));
    P.lib.process(b.h, &h[i], &h[i], &bl[i], &br[i], n, nb.data(), P.np(), 120.f); if (P.info.source) P.lib.output(b.h);
  }
  const bool same = P.info.source ? alone.src == again.src : alone.L == again.L && alone.R == again.R;
  const bool indep = P.info.source ? alone.src == both.src : alone.L == both.L && alone.R == both.R;
  if (!same) FAIL("instances", "a fresh instance doesn't repeat the first one's output exactly: state in a static (or rand()/time) is shared between instances");
  else if (!indep) FAIL("instances", "an instance's output changes when a second one plays beside it: the two share state (a static?)");
  else PASS("instances", "a fresh instance repeats the first exactly, and two side by side with other settings and inputs don't touch each other");
}

void checkRatesAndBlocks(Plug& P) {
  std::string bad, info; double lvl48 = 0;
  for (float sr : {48000.f, 44100.f, 96000.f}) {
    const Signal g = riff(sr, 6); Inst x(P, sr); const Out o = x.run(g);
    if (!finiteOut(o)) { bad += std::string(bad.empty() ? "" : "; ") + "NaN or Inf at " + std::to_string((int)sr) + " Hz"; continue; }
    const double lv = P.info.source ? srcRange(o, 10) : rms(o.L);
    if (sr == 48000.f) lvl48 = lv;
    char b[96]; snprintf(b, sizeof b, P.info.source ? "%.0f Hz: range %.2f" : "%.0f Hz: %.1f dBFS RMS", sr, P.info.source ? lv : dB(lv)); info += std::string(info.empty() ? "" : ", ") + b;
    if (!P.info.source && sr != 48000.f && lvl48 > 0 && std::fabs(dB(lv) - dB(lvl48)) > 6) bad += std::string(bad.empty() ? "" : "; ") + b + " (6 dB off 48 kHz)";
  }
  if (bad.find("NaN") != std::string::npos) FAIL("sample rates", "%s", bad.c_str());
  else if (!bad.empty()) WARN("sample rates", "%s. Times and frequencies should come from sr, not 48000", bad.c_str());
  else PASS("sample rates", "%s", info.c_str());
  // blocks of any length up to 128 (rig-host plays a driver's block size, e.g. 64 or 32)
  const Signal g = riff(SR, 6);
  Inst a(P, SR), b(P, SR);
  const Out o128 = a.run(g, g);
  unsigned r = 5; const Out ov = b.run(g, g, nullptr, 120.f, [&](int) { r = r * 1664525u + 1013904223u; return 1 + (int)((r >> 8) % 128); });
  const double ref = P.info.source ? 1.0 : std::max(rms(o128.L), 1e-9);
  double dd = 0;
  if (P.info.source) { /* a source runs once per block: its blocks are the engine's, nothing to compare */ }
  else dd = rms(diff(o128.L, ov.L));
  if (!finiteOut(ov)) FAIL("block sizes", "NaN or Inf with blocks of 1-128 samples");
  else if (!P.info.source && dd > ref * 0.001) WARN("block sizes", "blocks of 1-128 samples sound different from 128s (%.0f dB below the output): per-block smoothing or a buffer that assumes 128?", dB(ref) - dB(dd));
  else PASS("block sizes", P.info.source ? "finite with blocks of 1-128 samples" : "blocks of 1-128 samples match 128s (difference %.0f dB below the output)", dB(ref) - dB(std::max(dd, 1e-12)));
}

void checkCpu(Plug& P) {
  const Signal g = riff(SR, 14);
  double best = 1e9, worst = 0;
  for (const auto& v : {P.defaults(), std::vector<float>(MAXPAR, 1.f)}) {
    double t = 1e9;
    for (int rep = 0; rep < 3; rep++) { Inst x(P, SR); const Out o = x.run(g, [&](int, float* n) { for (int k = 0; k < P.np(); k++) n[k] = v[(size_t)k]; }); t = std::min(t, o.secs); }
    best = std::min(best, t / 14.0); worst = std::max(worst, t / 14.0);
  }
  const double pc = worst * 100;
  if (pc > 25) FAIL("CPU", "%.1f%% of one core at worst (%.2f%% at the defaults); the whole chain shares one: keep it under 5%%", pc, best * 100);
  else if (pc > 5) WARN("CPU", "%.1f%% of one core at worst (%.2f%% at the defaults), about %.1f%% in the browser; heavy for one effect", pc, best * 100, pc * 1.5);
  else PASS("CPU", "%.2f%% of one core at the defaults, %.2f%% with every knob at 1 (native; the browser runs about 1.5x that)", best * 100, pc);
}

// ------------------------------------------------------------------ commands
bool load(Plug& P, const std::string& path) {
  P.ok = P.lib.open(path, P.info, P.err);
  if (!P.ok) { fprintf(stderr, "rigcheck: %s: %s\n", path.c_str(), P.err.c_str()); return false; }
  return true;
}

int info(Plug& P) {
  const PluginInfo& I = P.info;
  printf("%s \"%s\": %s%s%s (%s, by %s, version %s)\n", I.source ? "source" : "effect", I.id.c_str(), I.name.c_str(), I.sub.empty() ? "" : " - ", I.sub.c_str(), I.category.c_str(), I.author.c_str(), I.version.c_str());
  for (int k = 0; k < P.np(); k++) {
    const auto& p = I.params[(size_t)k];
    printf("  %d %-10s %-8s %s .. %s, default %s%s", k + 1, p.symbol, p.label, real(I, k, 0).c_str(), real(I, k, 1).c_str(), real(I, k, p.def).c_str(), p.curve == rig::Curve::Log ? " (log)" : "");
    for (int s = 0; s < p.nstops; s++) printf("%s%s", s ? ", " : "  stops: ", p.stops[s].name);
    printf("%s\n", !I.source && k == 3 && P.np() > 4 ? "\n    (knobs below are on the card's back)" : "");
  }
  const auto& md = I.source ? I.sdesc.meters : I.mdesc.meters; const int nm = I.source ? I.sdesc.nmeters : I.mdesc.nmeters;
  for (int m = 0; m < nm; m++) printf("  meter %s\n", md[m]);
  return 0;
}

// knob=value: in the knob's units, a stop's name, or N% of its travel. -> normalised
bool parseSet(const PluginInfo& I, const std::string& s, int& k, float& norm, std::string& err) {
  const size_t eq = s.find('='); if (eq == std::string::npos) { err = "--set wants knob=value"; return false; }
  const std::string sym = s.substr(0, eq), val = s.substr(eq + 1);
  k = -1; for (int i = 0; i < (int)I.params.size(); i++) if (sym == I.params[(size_t)i].symbol) k = i;
  if (k < 0) { err = "no knob \"" + sym + "\""; return false; }
  const auto& p = I.params[(size_t)k];
  for (int st = 0; st < p.nstops; st++) if (val == p.stops[st].name) { const float v = p.stops[st].v; norm = p.curve == rig::Curve::Log ? logf(v / p.min) / logf(p.max / p.min) : (v - p.min) / (p.max - p.min); return true; }
  char* end = nullptr; const float v = strtof(val.c_str(), &end);
  if (end == val.c_str()) { err = "\"" + val + "\" isn't a number or one of " + sym + "'s stops"; return false; }
  if (*end == '%') { norm = v / 100.f; }
  else if (p.curve == rig::Curve::Log) { if (v <= 0) { err = sym + " is a log knob: its value must be above 0"; return false; } norm = logf(v / p.min) / logf(p.max / p.min); }
  else norm = (v - p.min) / (p.max - p.min);
  norm = std::min(1.f, std::max(0.f, norm));
  return true;
}

int render(Plug& P, const std::map<std::string, std::string>& o, const std::vector<std::string>& sets) {
  float sr = o.count("sr") ? std::stof(o.at("sr")) : SR; Signal L, R;
  if (o.count("in")) { if (!readWav(o.at("in"), L, R, sr)) { fprintf(stderr, "rigcheck: can't read %s (16/24-bit PCM or 32-bit float WAV)\n", o.at("in").c_str()); return 2; } }
  else { L = riff(sr); R = L; }
  std::vector<float> norm = P.defaults();
  for (const auto& s : sets) { int k; float v; std::string err; if (!parseSet(P.info, s, k, v, err)) { fprintf(stderr, "rigcheck: %s\n", err.c_str()); return 2; } norm[(size_t)k] = v; }
  Inst x(P, sr); if (!x.h) { fprintf(stderr, "rigcheck: the plugin wouldn't start\n"); return 1; }
  const Out out = x.run(L, R, [&](int, float* n) { for (int k = 0; k < P.np(); k++) n[k] = norm[(size_t)k]; });
  for (int k = 0; k < P.np(); k++) printf("  %s = %s\n", P.info.params[(size_t)k].symbol, real(P.info, k, norm[(size_t)k]).c_str());
  const std::string path = o.count("out") ? o.at("out") : "out.wav";
  if (P.info.source) {                                           // the modulation value, held per block, as a mono "control" track
    Signal c(L.size()); for (size_t i = 0; i < c.size(); i++) c[i] = out.src.empty() ? 0.f : out.src[std::min(out.src.size() - 1, i / MAXBLOCK)];
    writeWav(path, c, c, sr);
    printf("  wrote %s: the source's value (-1..1) as audio, %.1f s; range %.3f, peak %.3f\n", path.c_str(), c.size() / sr, srcRange(out), srcPeak(out));
  } else {
    writeWav(path, out.L, out.R, sr);
    printf("  wrote %s: %.1f s at %.0f Hz; in %.1f dBFS RMS, out %.1f / %.1f dBFS RMS, peak %+.1f dBFS%s\n", path.c_str(), L.size() / sr, sr, dB(rms(L)), dB(rms(out.L)), dB(rms(out.R)), dB(peakOut(out)), finiteOut(out) ? "" : " (NaN/Inf found!)");
  }
  return finiteOut(out) ? 0 : 1;
}

int check(Plug& P, const std::map<std::string, std::string>& o) {
  const bool quick = o.count("quick") > 0;
  printf("rigcheck: %s\n", o.at("file").c_str());
  checkDescription(P, o.count("rig") ? o.at("rig") : "rig.json");
  { Inst probe(P, SR); if (!probe.h) { FAIL("instance", "the plugin wouldn't activate at 48 kHz with 128-sample blocks"); goto done; } }
  checkHardCases(P, quick);
  checkDefaults(P);
  checkSilence(P);
  checkClicks(P);
  checkHeap(P);
  checkInstances(P);
  checkRatesAndBlocks(P);
  if (!quick) checkCpu(P);
done:
  int f = 0, w = 0; for (const auto& r : results) { f += r.status == "FAIL"; w += r.status == "WARN"; }
  printf("%s: %d passed, %d warning%s, %d failed\n", f ? "FAILED" : w ? "PASSED with warnings" : "PASSED", (int)results.size() - f - w, w, w == 1 ? "" : "s", f);
  if (o.count("json")) {
    std::ofstream j(o.at("json"));
    j << "{\"id\":\"" << jesc(P.info.id) << "\",\"kind\":\"" << (P.info.source ? "source" : "effect") << "\",\"failed\":" << f << ",\"warnings\":" << w << ",\"results\":[";
    for (size_t i = 0; i < results.size(); i++) j << (i ? "," : "") << "{\"status\":\"" << results[i].status << "\",\"check\":\"" << jesc(results[i].name) << "\",\"detail\":\"" << jesc(results[i].detail) << "\"}";
    j << "]}\n";
  }
  return f ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 3) { fprintf(stderr, "usage: rigcheck check|info|render <plugin.clap> [options] (see the top of tools/rigcheck/rigcheck.cpp)\n"); return 2; }
  const std::string cmd = argv[1];
  std::map<std::string, std::string> o; std::vector<std::string> sets; o["file"] = argv[2];
  for (int i = 3; i < argc; i++) {
    const std::string a = argv[i];
    if (a == "--quick") o["quick"] = "1";
    else if (a == "--set" && i + 1 < argc) sets.push_back(argv[++i]);
    else if (a.rfind("--", 0) == 0 && i + 1 < argc) o[a.substr(2)] = argv[++i];
    else { fprintf(stderr, "rigcheck: what is %s?\n", a.c_str()); return 2; }
  }
  static Plug P;                                                  // outlives every instance
  if (!load(P, o["file"])) return 1;
  if (cmd == "info") return info(P);
  if (cmd == "render") return render(P, o, sets);
  if (cmd == "check") return check(P, o);
  fprintf(stderr, "rigcheck: commands are check, info, render\n"); return 2;
}
