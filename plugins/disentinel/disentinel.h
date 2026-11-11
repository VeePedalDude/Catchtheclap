// DI SENTINEL: front-of-chain guitar input conditioning with adaptive noise-floor learning,
// gentle downward expansion, subsonic and ultrasonic filters, and input trim.
// INPUT -18..+18 dB (0), CLEAN 0..100% (65), SENSE -12..+18 dB (0), HOLD 0..350ms (70),
// RELEASE 40..1500ms (320), SUB CUT 12..140Hz (30), TOP CUT 5..20kHz (18k), LEARN OFF/ON.
#pragma once
#include "module.h"

namespace rig {

inline constexpr ParamStop DISENTINEL_LEARN_STOPS[] = {{0.f, "AUTO"}, {1.f, "LEARN"}};
inline constexpr ParamDesc DISENTINEL_PARAMS[] = {
  {"input", "INPUT", -18.f, 18.f, 0.5f, "dB", Curve::Lin, 1, true},
  {"clean", "CLEAN", 0.f, 100.f, .65f, "%", Curve::Lin, 0, false},
  {"sense", "SENSE", -12.f, 18.f, .4f, "dB", Curve::Lin, 1, true},
  {"hold", "HOLD", 0.f, 350.f, .2f, "ms", Curve::Lin, 0, false},
  {"release", "RELEASE", 40.f, 1500.f, .574f, "ms", Curve::Log, 0, false},
  {"subcut", "SUB CUT", 12.f, 140.f, .3724f, "Hz", Curve::Log, 0, false},
  {"topcut", "TOP CUT", 5000.f, 20000.f, .925f, "Hz", Curve::Log, 0, false},
  {"learn", "LEARN", 0.f, 1.f, 0.f, "", Curve::Lin, 0, false, DISENTINEL_LEARN_STOPS, 2},
};
inline constexpr ModuleDesc DISENTINEL_DESC = {
  "disentinel", "DI SENTINEL", "adaptive input cleanup", 8, DISENTINEL_PARAMS,
  2, {"floor", "reduction"}
};

struct DiSentinel : Module {
  Biquad hp1L, hp2L, hp1R, hp2R, lp1L, lp2L, lp1R, lp2R;
  OnePole inputS, subS, topS;
  float floorRms = .00018f; // conservative -75 dBFS startup estimate
  float detector = 0.f;
  float gain = 1.f, desired = 1.f;
  float reduction = 0.f;
  float peakMonitor = 0.f;
  float noisePowerSum = 0.f;
  float noiseCorr4 = 0.f, noiseCorr8 = 0.f;
  float historyL[16] = {}, historyR[16] = {};
  int historyIndex = 0;
  float lastSub = -1.f, lastTop = -1.f;
  float floorMeter = .3f;
  int winCount = 0, filtCountdown = 0, gainCountdown = 0, holdSamples = 0;
  int quietWindows = 0;
  bool gateOpen = true, learnPrev = false;

  const ModuleDesc& desc() const override { return DISENTINEL_DESC; }
  void init(float sampleRate) override {
    sr = sampleRate;
    inputS.setMs(sr, 20.f); subS.setMs(sr, 30.f); topS.setMs(sr, 30.f);
    reset();
  }
  void reset() override {
    hp1L.reset(); hp2L.reset(); hp1R.reset(); hp2R.reset();
    lp1L.reset(); lp2L.reset(); lp1R.reset(); lp2R.reset();
    inputS.y = map(0, DISENTINEL_PARAMS[0].def);
    subS.y = map(5, DISENTINEL_PARAMS[5].def);
    topS.y = map(6, DISENTINEL_PARAMS[6].def);
    floorRms = .00018f; detector = 0.f; gain = desired = 1.f;
    reduction = peakMonitor = noisePowerSum = noiseCorr4 = noiseCorr8 = 0.f;
    memset(historyL,0,sizeof historyL); memset(historyR,0,sizeof historyR); historyIndex = 0;
    floorMeter = .3f; winCount = quietWindows = 0;
    gainCountdown = 0; holdSamples = 0; gateOpen = true;
    filtCountdown = 0; lastSub = lastTop = -1.f; learnPrev = false;
  }
  void updateFilters(float subHz, float topHz) {
    subHz = clampf(subHz, 8.f, sr * .38f);
    topHz = clampf(topHz, 500.f, sr * .45f);
    if (fabsf(subHz - lastSub) > .02f || fabsf(topHz - lastTop) > 2.f) {
      hp1L.hp(sr, subHz, .7071068f); hp2L.hp(sr, subHz, .7071068f);
      hp1R.hp(sr, subHz, .7071068f); hp2R.hp(sr, subHz, .7071068f);
      lp1L.lp(sr, topHz, .7071068f); lp2L.lp(sr, topHz, .7071068f);
      lp1R.lp(sr, topHz, .7071068f); lp2R.lp(sr, topHz, .7071068f);
      lastSub = subHz; lastTop = topHz;
    }
  }
  void analyzeWindow(bool manualLearn, int length) {
    if (winCount == 0) return;
    const float windowRms = sqrtf(noisePowerSum / (float)winCount + 1e-12f);
    // Strong short-lag correlations identify steady pitched audio; do not learn
    // it as noise in AUTO. Broadband hiss typically has low correlation.
    const float coherence = fmaxf(fabsf(noiseCorr4), fabsf(noiseCorr8)) /
                            fmaxf(noisePowerSum, 1e-12f);
    const bool pitched = coherence > .43f && windowRms > .00003f;
    const float seconds = (float)winCount / sr;
    winCount = 0; noisePowerSum = noiseCorr4 = noiseCorr8 = 0.f;
    // Auto-learning is conservative: a substantial guitar signal freezes the noise estimate.
    // Explicit LEARN can learn a noisier room, but should only be engaged while not playing.
    if (manualLearn) {
      if (!learnPrev) { floorRms = windowRms; quietWindows = 0; }
      else floorRms += (1.f - expf(-seconds / .25f)) * (windowRms - floorRms);
      learnPrev = true;
    } else {
      learnPrev = false;
      if (windowRms < .008f && !pitched) ++quietWindows;
      else quietWindows = 0;
      if (quietWindows >= 3) {
        const float tc = windowRms < floorRms ? .16f : 1.5f;
        floorRms += (1.f - expf(-seconds / tc)) * (windowRms - floorRms);
      }
    }
    floorRms = clampf(floorRms, .000005f, .031622f);
    const float dB = 20.f * log10f(floorRms);
    floorMeter = clampf((dB + 100.f) / 80.f, 0.f, 1.f);
    (void)length;
  }
  void process(const float* inL, const float* inR, float* oL, float* oR,
               int n, const float* e, const Context&) override {
    const float trimDb = map(0, e[0]);
    const float strength = clampf(map(1, e[1]) * .01f, 0.f, 1.f);
    const float biasDb = map(2, e[2]);
    const int hold = (int)(map(3, e[3]) * .001f * sr);
    const float releaseMs = map(4, e[4]);
    const float subHz = map(5, e[5]);
    const float topHz = map(6, e[6]);
    const bool learn = map(7, e[7]) >= .5f;
    const float noiseFloorMinGain = powf(10.f, -60.f * strength / 20.f);
    const float attackDetector = 1.f - expf(-1.f / (.003f * sr));
    const float releaseDetector = 1.f - expf(-1.f / (.05f * sr));
    const float openCoef = 1.f - expf(-1.f / (.00065f * sr));
    const float closeCoef = 1.f - expf(-1.f / (.001f * releaseMs * sr));
    const float thresholdFactor = powf(10.f, biasDb / 20.f);
    const float winLen = fmaxf(64.f, sr * .065f);
    for (int i = 0; i < n; ++i) {
      // Read before writing: in-place host buffers are permitted.
      const float rawL = inL[i], rawR = inR[i];
      const float g = db2lin(inputS.next(trimDb));
      const float xL = rawL * g, xR = rawR * g;
      const float sh = subS.next(subHz), th = topS.next(topHz);
      if (filtCountdown-- <= 0) { updateFilters(sh, th); filtCountdown = 31; }
      const float yL = lp2L.tick(lp1L.tick(hp2L.tick(hp1L.tick(xL))));
      const float yR = lp2R.tick(lp1R.tick(hp2R.tick(hp1R.tick(xR))));
      const float sq = .5f * (yL*yL + yR*yR);
      const int i4=(historyIndex-4)&15, i8=(historyIndex-8)&15;
      noiseCorr4 += .5f*(yL*historyL[i4]+yR*historyR[i4]);
      noiseCorr8 += .5f*(yL*historyL[i8]+yR*historyR[i8]);
      historyL[historyIndex]=yL; historyR[historyIndex]=yR;
      historyIndex=(historyIndex+1)&15;
      noisePowerSum += sq; ++winCount;
      if ((float)winCount >= winLen) analyzeWindow(learn, winCount);
      const float co = sq > detector ? attackDetector : releaseDetector;
      detector = undenorm(detector + co * (sq - detector));
      const float env = sqrtf(detector + 1e-12f);
      const float closeThresh = floorRms * 4.f * thresholdFactor;
      const float openThresh = floorRms * 6.f * thresholdFactor;
      if (env >= openThresh) { gateOpen = true; holdSamples = hold; }
      else if (env < closeThresh) {
        if (holdSamples > 0) --holdSamples;
        else gateOpen = false;
      }
      if (gainCountdown-- <= 0) {
        if (strength <= .0001f || gateOpen) desired = 1.f;
        else {
          const float rel = env / fmaxf(.0000001f, closeThresh);
          float t = clampf((rel - .45f) / .55f, 0.f, 1.f);
          t = t*t*(3.f - 2.f*t);
          desired = noiseFloorMinGain + (1.f-noiseFloorMinGain)*t;
        }
        gainCountdown = 15;
      }
      gain = undenorm(gain + (desired > gain ? openCoef : closeCoef) * (desired - gain));
      oL[i] = yL * gain;
      oR[i] = yR * gain;
      reduction = 1.f - clampf(gain, 0.f, 1.f);
      peakMonitor = fmaxf(peakMonitor * .999f, fmaxf(fabsf(yL), fabsf(yR)));
    }
  }
  void meters(float* out) const override {
    out[0] = floorMeter; // [0,1] mapped from noise floor -100..-20 dBFS for the display
    out[1] = reduction;  // [0,1] gain reduction fraction
  }
};
} // namespace rig