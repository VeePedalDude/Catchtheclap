// plugins/nestriple/nestriple.h - NES TRIPLE: three stepped-volume lo-fi echo stages with 6 FM-style routes.
// TIME 30..300ms, INTERVAL 0.5..2x, DECAY 0..90%, DEGRADE 0..100%, ORDER ABC..CBA,
// FEEDBACK 0..80%, RETURN FRONT/MIDDLE, MIX 0..100%. Defaults: 120ms, 1x, 60%, 20%, ABC, 20%, FRONT, 35%.
#pragma once
#include "module.h"

namespace rig {

inline constexpr ParamStop NESTRIPLE_ORDERS[] = {
  {0.f, "ABC"}, {1.f, "ACB"}, {2.f, "BAC"},
  {3.f, "BCA"}, {4.f, "CAB"}, {5.f, "CBA"}
};
inline constexpr ParamStop NESTRIPLE_RETURNS[] = {{0.f, "FRONT"}, {1.f, "MIDDLE"}};
inline constexpr int NESTRIPLE_ROUTES[6][3] = {
  {0,1,2}, {0,2,1}, {1,0,2}, {1,2,0}, {2,0,1}, {2,1,0}
};
inline constexpr ParamDesc NESTRIPLE_PARAMS[] = {
  {"time",     "TIME",     30.f, 300.f, 0.60206f, "ms", Curve::Log, 0, false},
  {"interval", "INTERVAL", 0.5f, 2.f,   1.f/3.f,  "x",  Curve::Lin, 2, false},
  {"decay",    "DECAY",    0.f,  90.f,  2.f/3.f,  "%", Curve::Lin, 0, false},
  {"degrade",  "DEGRADE",  0.f, 100.f,  0.2f,     "%", Curve::Lin, 0, false},
  {"order",    "ORDER",    0.f,   5.f,  0.f,      "",  Curve::Lin, 0, false, NESTRIPLE_ORDERS, 6},
  {"feedback", "FEEDBACK", 0.f,  80.f,  0.25f,    "%", Curve::Lin, 0, false},
  {"return",   "RETURN",   0.f,   1.f,  0.f,      "",  Curve::Lin, 0, false, NESTRIPLE_RETURNS, 2},
  {"mix",      "MIX",      0.f, 100.f,  0.35f,    "%", Curve::Lin, 0, false}
};
inline constexpr ModuleDesc NESTRIPLE_DESC = {
  "nestriple", "NES TRIPLE", "degrading 3-stage echo", 8, NESTRIPLE_PARAMS, 2, {"order", "feedback"}
};

struct NesTriple : Module {
  static constexpr int MEMORY = 131072; // longest stage: 1.2 s at 96 kHz < 131072 samples
  DelayLine<MEMORY> left[3], right[3];
  OnePole timeS, intervalS, decayS, degradeS, feedbackS, returnS, mixS, alphaS[3];
  float lpL[3] = {0}, lpR[3] = {0}, heldL[3] = {0}, heldR[3] = {0};
  int holdCount[3] = {0};
  float lastL = 0.f, lastR = 0.f, shownFeedback = 0.f;
  int shownOrder = 0;

  const ModuleDesc& desc() const override { return NESTRIPLE_DESC; }
  void init(float s) override {
    sr = s;
    timeS.setMs(sr, 35); intervalS.setMs(sr, 35); decayS.setMs(sr, 25);
    degradeS.setMs(sr, 30); feedbackS.setMs(sr, 30); returnS.setMs(sr, 35);
    mixS.setMs(sr, 25);
    for (auto& a : alphaS) a.setMs(sr, 35);
    reset();
  }
  void reset() override {
    for (int k = 0; k < 3; ++k) {
      left[k].clear(); right[k].clear();
      lpL[k] = lpR[k] = heldL[k] = heldR[k] = 0.f;
      holdCount[k] = 0;
    }
    timeS.y = map(0, NESTRIPLE_PARAMS[0].def);
    intervalS.y = map(1, NESTRIPLE_PARAMS[1].def);
    decayS.y = map(2, NESTRIPLE_PARAMS[2].def) * 0.01f;
    degradeS.y = map(3, NESTRIPLE_PARAMS[3].def) * 0.01f;
    feedbackS.y = map(5, NESTRIPLE_PARAMS[5].def) * 0.01f;
    returnS.y = map(6, NESTRIPLE_PARAMS[6].def);
    mixS.y = map(7, NESTRIPLE_PARAMS[7].def) * 0.01f;
    for (auto& a : alphaS) a.y = filterAlpha(degradeS.y, (&a - alphaS) + 1);
    lastL = lastR = shownFeedback = 0.f; shownOrder = 0;
  }
  float filterAlpha(float degradation, int pass) const {
    const float fc = 18000.f * powf(0.03f, degradation * pass / 3.f);
    const float cutoff = clampf(fc, 100.f, sr * 0.42f);
    const float a = 1.f - expf(-2.f * PI_F * cutoff / sr);
    return 1.f - degradation * (1.f - a); // true passthrough at zero degradation
  }
  void process(const float* inL, const float* inR, float* oL, float* oR, int n,
               const float* e, const Context&) override {
    const float timeT = map(0, e[0]), intervalT = map(1, e[1]);
    const float decayT = map(2, e[2]) * 0.01f, damageT = map(3, e[3]) * 0.01f;
    const int order = (int)clampf(floorf(map(4, e[4]) + 0.5f), 0.f, 5.f);
    const float fbT = map(5, e[5]) * 0.01f;
    const float returnT = map(6, e[6]) >= 0.5f ? 1.f : 0.f;
    const float mixT = map(7, e[7]) * 0.01f;
    float alphaT[3];
    for (int p = 0; p < 3; ++p) alphaT[p] = filterAlpha(damageT, p + 1);

    for (int i = 0; i < n; ++i) {
      const float inputL = inL[i], inputR = inR[i]; // safe when input/output alias
      const float baseSamples = timeS.next(timeT) * sr * 0.001f;
      const float ratio = intervalS.next(intervalT);
      const float delay[3] = {baseSamples, baseSamples * ratio, baseSamples * ratio * ratio};
      const float decay = decayS.next(decayT);
      const float damage = degradeS.next(damageT);
      const float fb = feedbackS.next(fbT), ret = returnS.next(returnT);
      const float mix = mixS.next(mixT);
      float stageL = inputL + lastL * fb * (1.f - ret);
      float stageR = inputR + lastR * fb * (1.f - ret);
      float wetL = 0.f, wetR = 0.f, level = 1.f, levelSum = 0.f;
      for (int p = 0; p < 3; ++p) {
        if (p == 1) { stageL += lastL * fb * ret; stageR += lastR * fb * ret; }
        const int k = NESTRIPLE_ROUTES[order][p];
        left[k].push(clampf(stageL, -2.f, 2.f));
        right[k].push(clampf(stageR, -2.f, 2.f));
        const float rawL = left[k].tap(delay[k]), rawR = right[k].tap(delay[k]);
        const float a = alphaS[p].next(alphaT[p]);
        lpL[k] = undenorm(lpL[k] + a * (rawL - lpL[k]));
        lpR[k] = undenorm(lpR[k] + a * (rawR - lpR[k]));
        const int bits = 16 - (int)(damage * (p + 1) * 4.f + 0.5f);
        const float scale = (float)(1 << (bits - 1));
        const float crushedL = damage < 0.001f ? lpL[k] : floorf(lpL[k] * scale + 0.5f) / scale;
        const float crushedR = damage < 0.001f ? lpR[k] : floorf(lpR[k] * scale + 0.5f) / scale;
        const int hold = 1 + (int)(damage * (p + 1) * 2.f + 0.5f);
        if (holdCount[k] <= 0) {
          heldL[k] = crushedL; heldR[k] = crushedR; holdCount[k] = hold - 1;
        } else --holdCount[k];
        stageL = heldL[k]; stageR = heldR[k];
        wetL += stageL * level; wetR += stageR * level; levelSum += level;
        level *= decay;
      }
      lastL = undenorm(stageL); lastR = undenorm(stageR);
      oL[i] = inputL * (1.f - mix) + wetL * (mix / levelSum);
      oR[i] = inputR * (1.f - mix) + wetR * (mix / levelSum);
    }
    shownOrder = order;
    shownFeedback = feedbackS.y;
  }
  void meters(float* out) const override { out[0] = (float)shownOrder; out[1] = shownFeedback; }
};

} // namespace rig
