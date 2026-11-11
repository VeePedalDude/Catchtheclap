// tools/rigtest/signals.h - test signals and measurements, shared by rigcheck (the conformance checks)
// and by a plugin's own tests (rigtest.h). The guitar is RIG/01's own test riff (tests/native/framework.h):
// Karplus-Strong strings, chords, palm mutes and single notes, peaking at -12 dBFS like a guitar into an
// interface's instrument input.
#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

namespace rigtest {

using Signal = std::vector<float>;

// ------------------------------------------------------------------ guitar (Karplus-Strong)
struct Note { float t, f, vel; bool mute; };
inline Signal guitar(float sr, float secs, const std::vector<Note>& notes, unsigned seed = 1) {
  const int N = (int)(secs * sr);
  Signal out((size_t)N, 0.f);
  unsigned s = seed;
  auto rnd = [&] { s = s * 1664525u + 1013904223u; return (s >> 8) / 8388608.f - 1.f; };
  for (const auto& nt : notes) {
    const int start = (int)(nt.t * sr), period = std::max(2, (int)(sr / nt.f));
    std::vector<float> line((size_t)period);
    for (auto& v : line) v = nt.vel * rnd();
    const float damp = nt.mute ? 0.93f : 0.996f;
    const int dur = (int)((nt.mute ? 0.18f : 2.2f) * sr);
    int idx = 0;
    for (int i = 0; i < dur && start + i < N; i++) {
      const float a = line[(size_t)idx], b = line[(size_t)((idx + 1) % period)];
      line[(size_t)idx] = 0.5f * (a + b) * damp; idx = (idx + 1) % period;
      const float fade = i > dur - 480 ? (float)(dur - i) / 480.f : 1.f;
      out[(size_t)(start + i)] += a * fade;
    }
  }
  float pk = 0; for (float v : out) pk = std::max(pk, std::fabs(v));
  if (pk > 0) for (auto& v : out) v *= 0.25f / pk;                // peaks at -12 dBFS
  return out;
}
// Four bars: a power chord, four palm mutes, a single note; E2 and A2 roots. 14 s at sr.
inline std::vector<Note> riffNotes() {
  std::vector<Note> n; const float E2 = 82.41f, A2 = 110.f, B2 = 123.47f, D3 = 146.83f; float t = 0.2f;
  for (int bar = 0; bar < 4; bar++) {
    const float root = bar % 2 ? A2 : E2;
    n.push_back({t, root, 1.f, false}); n.push_back({t + 0.008f, root * 1.4983f, .8f, false}); n.push_back({t + 0.016f, root * 2, .6f, false});
    t += 0.75f;
    for (int k = 0; k < 4; k++) { n.push_back({t, root, .7f, true}); t += 0.125f; }
    n.push_back({t, bar % 2 ? D3 : B2, .9f, false}); t += 0.75f;
  }
  return n;
}
inline Signal riff(float sr = 48000.f, float secs = 14.f) { return guitar(sr, secs, riffNotes()); }

// ------------------------------------------------------------------ plain signals
inline Signal sine(float sr, float hz, float secs, float amp = 0.3f) {
  Signal v((size_t)(secs * sr)); for (size_t i = 0; i < v.size(); i++) v[i] = amp * sinf(6.2831853f * hz * (float)i / sr); return v;
}
inline Signal noise(float sr, float secs, float amp = 1.f, unsigned seed = 1) {
  Signal v((size_t)(secs * sr)); unsigned s = seed; for (auto& x : v) { s = s * 1664525u + 1013904223u; x = amp * ((s >> 8) / 8388608.f - 1.f); } return v;
}
inline Signal constant(float sr, float secs, float value) { return Signal((size_t)(secs * sr), value); }
inline Signal impulses(float sr, float secs, float every, float amp = 1.f) {
  Signal v((size_t)(secs * sr), 0.f); const size_t step = (size_t)(every * sr); for (size_t i = 0; i < v.size(); i += step) v[i] = amp; return v;
}
inline Signal square(float sr, float hz, float secs, float amp = 1.f) {
  Signal v((size_t)(secs * sr)); const float per = sr / hz; for (size_t i = 0; i < v.size(); i++) v[i] = fmodf((float)i, per) < per / 2 ? amp : -amp; return v;
}
inline Signal concat(Signal a, const Signal& b) { a.insert(a.end(), b.begin(), b.end()); return a; }

// ------------------------------------------------------------------ measurements
inline float dB(double x) { return 20.f * (float)std::log10(x + 1e-12); }
inline double rms(const Signal& x, size_t a = 0, size_t b = SIZE_MAX) {
  b = std::min(b, x.size()); if (b <= a) return 0; double s = 0; for (size_t i = a; i < b; i++) s += (double)x[i] * x[i]; return std::sqrt(s / (double)(b - a));
}
inline float peak(const Signal& x, size_t a = 0, size_t b = SIZE_MAX) {
  b = std::min(b, x.size()); float p = 0; for (size_t i = a; i < b; i++) p = std::max(p, std::fabs(x[i])); return p;
}
inline bool finite(const Signal& x) { for (float v : x) if (!std::isfinite(v)) return false; return true; }
// The largest sample-to-sample jump: a click shows as a step far bigger than the signal's own.
inline float maxStep(const Signal& x, size_t a = 1) { float s = 0; for (size_t i = std::max<size_t>(a, 1); i < x.size(); i++) s = std::max(s, std::fabs(x[i] - x[i - 1])); return s; }
inline Signal diff(const Signal& a, const Signal& b) { Signal d(std::min(a.size(), b.size())); for (size_t i = 0; i < d.size(); i++) d[i] = a[i] - b[i]; return d; }
// Level of a sine at hz in x (from a ~ hz-long window onwards): the magnitude of the projection.
inline float toneLevel(const Signal& x, float sr, float hz, size_t a = 0, size_t b = SIZE_MAX) {
  b = std::min(b, x.size()); double c = 0, s = 0; for (size_t i = a; i < b; i++) { const double w = 6.283185307 * hz * (double)i / sr; c += x[i] * std::cos(w); s += x[i] * std::sin(w); }
  return b > a ? (float)(2.0 * std::sqrt(c * c + s * s) / (double)(b - a)) : 0.f;
}
// RMS per window of `win` samples: a level envelope (for tremolo depth, decay, gating...).
inline Signal envelope(const Signal& x, size_t win) { Signal e; for (size_t i = 0; i + win <= x.size(); i += win) e.push_back((float)rms(x, i, i + win)); return e; }
// The strongest period (in samples) between lo and hi, by autocorrelation (mean removed, normalised by
// the overlap): an LFO's, an envelope's or an echo's spacing. Keep hi under half of x's length.
inline int period(const Signal& x, int lo, int hi) {
  double mean = 0; for (float v : x) mean += v; mean /= std::max<size_t>(x.size(), 1);
  std::vector<double> c; double top = -1e30;
  for (int lag = lo; lag <= hi && lag < (int)x.size(); lag++) {
    double s = 0; for (size_t i = (size_t)lag; i < x.size(); i++) s += ((double)x[i] - mean) * ((double)x[i - (size_t)lag] - mean);
    c.push_back(s / (double)(x.size() - (size_t)lag)); top = std::max(top, c.back());
  }
  for (size_t k = 1; k + 1 < c.size(); k++)                     // the shortest peak near the best (not a multiple of it)
    if (c[k] >= 0.9 * top && c[k] >= c[k - 1] && c[k] >= c[k + 1]) return lo + (int)k;
  for (size_t k = 0; k < c.size(); k++) if (c[k] == top) return lo + (int)k;
  return lo;
}

// ------------------------------------------------------------------ WAV files (16/24-bit PCM, 32-bit float; mono or stereo)
inline bool readWav(const std::string& path, Signal& L, Signal& R, float& sr) {
  FILE* f = fopen(path.c_str(), "rb"); if (!f) return false;
  std::vector<unsigned char> d; unsigned char buf[65536]; size_t k; while ((k = fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + k); fclose(f);
  auto u32 = [&](size_t o) { return (uint32_t)d[o] | (uint32_t)d[o + 1] << 8 | (uint32_t)d[o + 2] << 16 | (uint32_t)d[o + 3] << 24; };
  auto u16 = [&](size_t o) { return (uint16_t)(d[o] | d[o + 1] << 8); };
  if (d.size() < 12 || memcmp(&d[0], "RIFF", 4) || memcmp(&d[8], "WAVE", 4)) return false;
  int fmt = 0, ch = 0, bits = 0; size_t data = 0, len = 0;
  for (size_t o = 12; o + 8 <= d.size(); ) {
    const uint32_t n = u32(o + 4);
    if (!memcmp(&d[o], "fmt ", 4) && o + 24 <= d.size()) { fmt = u16(o + 8); ch = u16(o + 10); sr = (float)u32(o + 12); bits = u16(o + 22); if (fmt == 0xFFFE && o + 34 <= d.size()) fmt = u16(o + 32); }
    if (!memcmp(&d[o], "data", 4)) { data = o + 8; len = std::min<size_t>(n, d.size() - data); break; }
    o += 8 + n + (n & 1);
  }
  if (!data || ch < 1 || !(fmt == 1 || fmt == 3)) return false;
  const int bps = bits / 8; const size_t frames = len / (size_t)(bps * ch);
  L.assign(frames, 0); R.assign(frames, 0);
  for (size_t i = 0; i < frames; i++) for (int c = 0; c < std::min(ch, 2); c++) {
    const size_t o = data + (i * (size_t)ch + (size_t)c) * (size_t)bps; float v = 0;
    if (fmt == 3 && bits == 32) { uint32_t b = u32(o); memcpy(&v, &b, 4); }
    else if (bits == 16) v = (int16_t)u16(o) / 32768.f;
    else if (bits == 24) v = (float)((int32_t)((uint32_t)d[o] << 8 | (uint32_t)d[o + 1] << 16 | (uint32_t)d[o + 2] << 24) >> 8) / 8388608.f;
    else if (bits == 32) v = (int32_t)u32(o) / 2147483648.f;
    (c ? R : L)[i] = v;
  }
  if (ch == 1) R = L;
  return true;
}
inline bool writeWav(const std::string& path, const Signal& L, const Signal& R, float sr) {   // 32-bit float stereo
  FILE* f = fopen(path.c_str(), "wb"); if (!f) return false;
  const uint32_t frames = (uint32_t)std::min(L.size(), R.size()), bytes = frames * 8;
  auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
  fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(3); w16(2); w32((uint32_t)sr); w32((uint32_t)sr * 8); w16(8); w16(32);
  fwrite("data", 1, 4, f); w32(bytes);
  for (uint32_t i = 0; i < frames; i++) { fwrite(&L[i], 4, 1, f); fwrite(&R[i], 4, 1, f); }
  return fclose(f) == 0;
}

} // namespace rigtest
