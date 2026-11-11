// DI SENTINEL behavior tests: front-of-chain filtering, calibrated floor and dynamic expansion.
#include "rigtest.h"
#include "disentinel.h"
using namespace rigtest;

static Signal tone(float hz,float rms=0.025f,float seconds=3.f,float sr=48000.f) {
  Signal a((size_t)(sr*seconds));
  for(size_t i=0;i<a.size();++i)a[i]=rms*1.41421356f*sinf(2.f*rig::PI_F*hz*(float)i/sr);
  return a;
}
static Signal benchNoise(float rms=.001f,float seconds=4.f,float sr=48000.f) {
  Signal a((size_t)(sr*seconds));
  uint32_t rng=0x9e3779b9u;
  for(size_t i=0;i<a.size();++i){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
    a[i]=((float)(rng&0xffffu)/32767.5f-1.f)*rms*1.7320508f;}
  return a;
}
static float rmsAt(const Signal& a,float t0,float t1,float sr=48000.f) {
  const size_t start=(size_t)(t0*sr),end=(size_t)(t1*sr);
  double sum=0.; size_t n=0;
  for(size_t i=start;i<end&&i<a.size();++i){sum+=a[i]*a[i];++n;}
  return n?sqrtf((float)(sum/n)):0.f;
}
static void setup(Fx<rig::DiSentinel>& fx) {
  fx.set("input",0);fx.set("clean",0);fx.set("sense",0);
  fx.set("hold",70);fx.set("release",320);fx.set("subcut",12);
  fx.set("topcut",20000);fx.setStop("learn","AUTO");
}

TEST("SUB CUT removes deep rumble while passing guitar fundamentals") {
  Fx<rig::DiSentinel> lo,hi;setup(lo);setup(hi);
  lo.set("subcut",120);hi.set("subcut",120);
  const auto a=lo.run(tone(30)).L,b=hi.run(tone(440)).L;
  const float slow=rmsAt(a,1.f,2.5f)/.025f;
  const float pass=rmsAt(b,1.f,2.5f)/.025f;
  CHECK(slow<.03f && pass>.90f,"SUB CUT 120 Hz: 30 Hz pass %.4f, 440 Hz pass %.4f",slow,pass);
}

TEST("TOP CUT suppresses ultrasonic-adjacent energy without muffling 1 kHz") {
  Fx<rig::DiSentinel> high,mid;setup(high);setup(mid);
  high.set("topcut",5000);mid.set("topcut",5000);
  const auto a=high.run(tone(14000)).L,b=mid.run(tone(1000)).L;
  const float stop=rmsAt(a,1.f,2.5f)/.025f;
  const float pass=rmsAt(b,1.f,2.5f)/.025f;
  CHECK(stop<.035f && pass>.90f,"TOP CUT 5 kHz: 14 kHz pass %.4f, 1 kHz pass %.4f",stop,pass);
}

TEST("INPUT trim changes level by the requested 12 dB difference") {
  Fx<rig::DiSentinel> quiet,loud;setup(quiet);setup(loud);
  quiet.set("input",-6);loud.set("input",6);
  const Signal s=tone(440,.015f);
  const float a=rmsAt(quiet.run(s).L,1.f,2.5f),b=rmsAt(loud.run(s).L,1.f,2.5f);
  const float delta=20.f*log10f(b/fmaxf(a,1e-9f));
  CHECK(fabsf(delta-12.f)<.3f,"INPUT -6 to +6 dB: measured change %.2f dB",delta);
}

TEST("AUTO background analysis learns a stationary -60 dBFS noise floor") {
  Fx<rig::DiSentinel> fx;setup(fx);fx.set("clean",80);
  fx.run(benchNoise(.001f,5.f));
  const float estimateDb=fx.meters()[0]*80.f-100.f;
  CHECK(estimateDb>-65.f && estimateDb<-57.f,"AUTO floor after 5 s of -60 dBFS hiss: %.2f dBFS",estimateDb);
}

TEST("SMART gain reduces steady background noise after auto learning") {
  Fx<rig::DiSentinel> fx;setup(fx);fx.set("clean",100);fx.set("release",90);
  const Signal n=benchNoise(.001f,6.f);const auto y=fx.run(n).L;
  const float before=rmsAt(n,4.5f,5.5f),after=rmsAt(y,4.5f,5.5f);
  const float reductionDb=20.f*log10f(before/fmaxf(after,1e-9f));
  CHECK(reductionDb>15.f,"CLEAN 100: steady -60 dBFS noise reduced %.2f dB",reductionDb);
}

TEST("SMART gain stays open on a sustained guitar note") {
  Fx<rig::DiSentinel> fx;setup(fx);fx.set("clean",100);
  const Signal s=tone(330,.08f,5.f);const auto y=fx.run(s).L;
  const float ratio=rmsAt(y,3.f,4.f)/rmsAt(s,3.f,4.f);
  CHECK(ratio>.92f && ratio<1.08f,"5-second 330 Hz sustain: output/input RMS %.4f",ratio);
}

TEST("AUTO does not classify a very quiet sustained pitched note as background noise") {
  Fx<rig::DiSentinel> fx;setup(fx);fx.set("clean",100);
  const Signal s=tone(440,.003f,7.f);const auto y=fx.run(s).L;
  const float ratio=rmsAt(y,5.f,6.f)/rmsAt(s,5.f,6.f);
  CHECK(ratio>.88f,"7-second quiet 440 Hz note: output/input level %.4f",ratio);
}

TEST("RELEASE changes the amount of background noise immediately after playing") {
  Fx<rig::DiSentinel> fast,slow;setup(fast);setup(slow);
  fast.set("clean",100);slow.set("clean",100);
  fast.set("hold",0);slow.set("hold",0);
  fast.set("release",45);slow.set("release",1100);
  // Pre-learn on noise, play a note, return to noise.
  const Signal a=benchNoise(.001f,2.f),b=tone(330,.16f,1.f),c=benchNoise(.001f,1.4f);
  Signal s=a;s.insert(s.end(),b.begin(),b.end());s.insert(s.end(),c.begin(),c.end());
  const float fa=rmsAt(fast.run(s).L,3.60f,3.82f);
  const float sl=rmsAt(slow.run(s).L,3.60f,3.82f);
  CHECK(sl>fa*2.f,"RELEASE 1100 vs 45 ms: trailing noise RMS %.6f vs %.6f",sl,fa);
}

TEST("LEARN deliberately captures a louder floor that AUTO avoids") {
  Fx<rig::DiSentinel> manual,automatic;setup(manual);setup(automatic);
  manual.setStop("learn","LEARN");
  const Signal n=benchNoise(.014f,3.f);
  manual.run(n);automatic.run(n);
  const float m=manual.meters()[0]*80.f-100.f;
  const float a=automatic.meters()[0]*80.f-100.f;
  CHECK(m>-42.f && m<-34.f && a<-65.f,"LEARN floor %.2f dBFS; AUTO conservative floor %.2f dBFS",m,a);
}

TEST("CLEAN zero retains the filtered signal, unaffected by noise expansion") {
  Fx<rig::DiSentinel> off,on;setup(off);setup(on);
  off.set("clean",0);on.set("clean",100);
  const Signal n=benchNoise(.001f,5.f);
  const float dry=rmsAt(off.run(n).L,4.f,5.f),cleaned=rmsAt(on.run(n).L,4.f,5.f);
  CHECK(dry>cleaned*5.f,"CLEAN 0 noise RMS %.6f, CLEAN 100 %.6f",dry,cleaned);
}

TEST("floor and attenuation meters expose two independent bounded measurements") {
  Fx<rig::DiSentinel> fx;setup(fx);fx.set("clean",100);
  fx.run(benchNoise(.001f,5.f));const auto meters=fx.meters();
  CHECK(meters[0]>=0.f&&meters[0]<=1.f&&meters[1]>=0.f&&meters[1]<=1.f&&meters[1]>.85f,
        "floor %.4f, gain reduction %.4f (normalized)",meters[0],meters[1]);
}

RIGTEST_MAIN