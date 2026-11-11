// BITROT MATRIX: measured timing, topologies, feedback, decay, degradation and dry mix.
#include "rigtest.h"
#include "nestriple.h"
using namespace rigtest;

static Signal pulse(float seconds=3.f){
  Signal s((size_t)(48000.f*seconds),0.f);
  s[24000]=0.7f;
  return s;
}
static float nearPeak(const Signal& s,int at,int r=140){
  float v=0;
  for(int i=at-r;i<=at+r;i++) if(i>=0 && (size_t)i<s.size()) v=fmaxf(v,fabsf(s[(size_t)i]));
  return v;
}
static float nearArea(const Signal& s,int at,int r=140){
  float v=0;
  for(int i=at-r;i<=at+r;i++) if(i>=0 && (size_t)i<s.size()) v+=fabsf(s[(size_t)i]);
  return v;
}
static float difference(const Signal& a,const Signal& b){
  float v=0;
  for(size_t i=0;i<a.size()&&i<b.size();i++) v=fmaxf(v,fabsf(a[i]-b[i]));
  return v;
}
static void configure(Fx<rig::NesTriple>& f){
  f.set("time",100);f.set("interval",2);f.set("decay",70);
  f.set("degrade",0);f.set("feedback",0);f.set("mix",100);
}

TEST("TIME now reaches 1600 ms and has a measured long echo") {
  Fx<rig::NesTriple> f; configure(f);
  f.set("time",1600);f.set("interval",1);f.set("decay",0);f.setStop("order","SERIES");
  const auto o=f.run(pulse(3.5f)).L;
  const float a=nearPeak(o,24000+1600*48);
  CHECK(a>0.02f && nearPeak(o,24000+1400*48)<0.003f,
        "TIME 1600 ms: echo level %.4f at 1600 ms, early level %.4f",a,nearPeak(o,24000+1400*48));
}

TEST("maximum INTERVAL supports an eight-second third stage") {
  Fx<rig::NesTriple> f;configure(f);f.set("time",2000);f.set("interval",2);
  f.set("decay",90);f.setStop("order","PARALLEL");
  const auto o=f.run(pulse(8.9f)).L;
  const float a=nearPeak(o,24000+8000*48);
  CHECK(a>0.005f,"TIME 2000 ms, INTERVAL 2: last delay at 8000 ms, peak %.5f",a);
}

TEST("NES volume decay is geometric on three independent taps") {
  Fx<rig::NesTriple> f;configure(f);f.set("decay",50);f.setStop("order","PARALLEL");
  const auto o=f.run(pulse()).L;
  const float a=nearArea(o,24000+4800),b=nearArea(o,24000+9600),c=nearArea(o,24000+19200);
  CHECK(a>0 && fabsf(b/a-.5f)<.035f && fabsf(c/a-.25f)<.035f,
        "tap amplitudes: first %.4f, second/first %.3f, third/first %.3f (target 1,.5,.25)",a,b/a,c/a);
}

TEST("all eight ORDER algorithms produce different waveforms") {
  const char* routes[]={"SERIES","PARALLEL","FORK","MERGE","REVERSE","LADDER","CENTER","BRIDGE"};
  Signal renders[8];
  for(int k=0;k<8;k++){
    Fx<rig::NesTriple> f;configure(f);f.set("decay",85);f.setStop("order",routes[k]);
    renders[k]=f.run(pulse()).L;
  }
  int pairs=0;float closest=1000;
  for(int a=0;a<8;a++)for(int b=a+1;b<8;b++){
    const float d=difference(renders[a],renders[b]);
    closest=fminf(closest,d);if(d>.008f)pairs++;
  }
  CHECK(pairs==28,"8 route algorithms: %d/28 pairwise distinct; closest waveform difference %.4f",pairs,closest);
}

TEST("all six feedback RETURN patterns have distinct regeneration") {
  const char* returns[]={"WRAP","MIDDLE","SPLIT","LOCAL","CHAIN","CROSS"};
  Signal renders[6];
  for(int k=0;k<6;k++){
    Fx<rig::NesTriple> f;configure(f);f.setStop("order","SERIES");f.set("feedback",75);f.setStop("return",returns[k]);
    renders[k]=f.run(pulse(3.4f)).L;
  }
  int pairs=0;float closest=1000;
  for(int a=0;a<6;a++)for(int b=a+1;b<6;b++){
    const float d=difference(renders[a],renders[b]);
    closest=fminf(closest,d);if(d>.002f)pairs++;
  }
  CHECK(pairs==15,"6 return patterns: %d/15 pairwise distinct; closest waveform difference %.5f",pairs,closest);
}

TEST("higher DEGRADE destroys later echoes more than earlier echoes") {
  float ratios[2];
  for(int k=0;k<2;k++){
    Fx<rig::NesTriple> f;configure(f);f.set("interval",1);f.set("decay",90);
    f.set("degrade",k?100:0);f.setStop("order","SERIES");
    const auto o=f.run(pulse()).L;
    ratios[k]=nearArea(o,24000+14400)/(nearArea(o,24000+4800)+1e-9f);
  }
  CHECK(ratios[0]>.65f && ratios[1]<ratios[0]*.8f,
        "third/first echo area: undamaged %.3f, maximum DEGRADE %.3f",ratios[0],ratios[1]);
}

TEST("MIX 0 percent passes dry audio unchanged") {
  Fx<rig::NesTriple> f;f.set("mix",0);f.m.mixS.y=0;
  const auto s=sine(48000,375,1,0.2f);
  const auto o=f.run(s);
  const float d=difference(o.L,s);
  CHECK(d<1e-6f,"dry passthrough maximum absolute difference %.8f",d);
}
RIGTEST_MAIN
