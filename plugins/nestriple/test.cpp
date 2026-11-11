// NES TRIPLE behavioural claims; tests use the rig's own signal player and measurement helpers.
#include "rigtest.h"
#include "nestriple.h"
using namespace rigtest;

static Signal pulse(float seconds = 2.5f) {
  Signal s((size_t)(48000.f * seconds), 0.f);
  s[24000] = 0.7f; // leave 500 ms for smoothed knob changes to settle
  return s;
}
static float nearPeak(const Signal& s, int sample, int radius = 120) {
  float best = 0.f;
  for (int i = sample-radius; i <= sample+radius; ++i)
    if (i >= 0 && (size_t)i < s.size()) best = fmaxf(best, fabsf(s[(size_t)i]));
  return best;
}
static float nearArea(const Signal& s, int sample, int radius = 120) {
  float total = 0;
  for (int i = sample-radius; i <= sample+radius; ++i)
    if (i >= 0 && (size_t)i < s.size()) total += fabsf(s[(size_t)i]);
  return total;
}
static int nearPosition(const Signal& s, int sample, int radius = 120) {
  int best = sample; float high = 0;
  for (int i = sample-radius; i <= sample+radius; ++i)
    if (i >= 0 && (size_t)i < s.size() && fabsf(s[(size_t)i]) > high) {high=fabsf(s[(size_t)i]);best=i;}
  return best;
}
static void clean(Fx<rig::NesTriple>& fx) {
  fx.set("time",100); fx.set("interval",1); fx.set("decay",50);
  fx.set("degrade",0); fx.set("feedback",0); fx.set("mix",100);
}

TEST("TIME and NES-style DECAY produce three equal-interval geometric-level echoes") {
  Fx<rig::NesTriple> fx; clean(fx);
  const auto out = fx.run(pulse()).L;
  const int start = 24000;
  const int t1 = nearPosition(out,start+4800), t2 = nearPosition(out,start+9600), t3 = nearPosition(out,start+14400);
  const float a=nearArea(out,t1), b=nearArea(out,t2), c=nearArea(out,t3);
  CHECK(fabsf((t1-start)/48.f-100.f)<2.f && fabsf((t2-start)/48.f-200.f)<2.f && fabsf((t3-start)/48.f-300.f)<2.f,
        "taps at %.2f, %.2f, %.2f ms (target 100, 200, 300)", (t1-start)/48.f,(t2-start)/48.f,(t3-start)/48.f);
  CHECK(fabsf(b/a-0.5f)<0.03f && fabsf(c/a-0.25f)<0.03f,
        "volume ratios %.3f, %.3f, %.3f (target 1, .5, .25)",1.f,b/a,c/a);
}

TEST("INTERVAL creates shorter and longer rhythmic spacing") {
  for (float interval : {0.5f,2.f}) {
    Fx<rig::NesTriple> fx; clean(fx); fx.set("interval",interval);
    auto out=fx.run(pulse()).L;
    const float want=100.f*(1.f+interval+interval*interval);
    const int got=nearPosition(out,24000+(int)(want*48.f));
    CHECK(fabsf((got-24000)/48.f-want)<3.f,
          "INTERVAL %.1fx: third tap %.2f ms (target %.2f)",interval,(got-24000)/48.f,want);
  }
}

TEST("ORDER permutes the first operator and changes the earliest echo") {
  for (const char* route : {"ABC","CBA"}) {
    Fx<rig::NesTriple> fx; clean(fx); fx.set("interval",2); fx.set("decay",0); fx.setStop("order",route);
    auto out=fx.run(pulse()).L;
    const float want=route[0]=='A'?100.f:400.f;
    const int got=nearPosition(out,24000+(int)(want*48.f));
    CHECK(fabsf((got-24000)/48.f-want)<3.f && nearPeak(out,got)>0.1f,
          "ORDER %s: first audible tap %.2f ms (target %.0f)",route,(got-24000)/48.f,want);
  }
}

TEST("FEEDBACK RETURN chooses an audible later-to-front or later-to-middle loop") {
  for (const char* route : {"FRONT","MIDDLE"}) {
    Fx<rig::NesTriple> fx; clean(fx); fx.set("interval",2); fx.set("decay",70);
    fx.set("feedback",65); fx.setStop("return",route);
    const auto out=fx.run(pulse()).L;
    const float early=nearPeak(out,24000+38400), late=nearPeak(out,24000+43200); // 800 ms, 900 ms
    CHECK(route[0]=='F'?early>0.04f && late<0.01f:late>0.02f && early<0.01f,
          "RETURN %s: new echoes at 800/900 ms = %.4f/%.4f",route,early,late);
  }
}

TEST("DEGRADE damages repeated echoes cumulatively") {
  auto run = [](float damage) {
    Fx<rig::NesTriple> fx; clean(fx); fx.set("decay",90); fx.set("degrade",damage);
    const auto out=fx.run(pulse()).L;
    const float a=nearArea(out,24000+4800), c=nearArea(out,24000+14400);
    return c/(a+1e-9f);
  };
  const float cleanRatio=run(0), destroyedRatio=run(100);
  CHECK(cleanRatio>0.75f && destroyedRatio < cleanRatio*0.35f,
        "third/first tap ratio clean %.3f, destroyed %.3f",cleanRatio,destroyedRatio);
}

TEST("MIX zero is exact dry passthrough") {
  Fx<rig::NesTriple> fx; fx.set("mix",0); fx.m.mixS.y=0;
  const auto in=sine(48000,375,1,0.2f);
  const auto out=fx.run(in);
  CHECK(peak(diff(out.L,in))<1e-6f,"MIX 0%%: dry difference %.8f",peak(diff(out.L,in)));
}
RIGTEST_MAIN
