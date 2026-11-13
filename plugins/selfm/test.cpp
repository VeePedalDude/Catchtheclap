// SELF FM MATRIX: measure guitar-derived phase modulation rather than substitute oscillators.
#include "rigtest.h"
#include "selfm.h"
using namespace rigtest;
using rig::SelfFmMatrix;

static void setup(Fx<SelfFmMatrix>& f) {
  f.set("depth",95);f.set("source",60);f.set("env",90);
  f.set("sustain",65);f.set("feedback",50);f.set("route",1);
  f.set("tone",11000);f.set("mix",100);
}
static float tailRms(const Signal& x,size_t from){double sum=0;for(size_t i=from;i<x.size();i++)sum+=(double)x[i]*x[i];return (float)sqrt(sum/(double)(x.size()-from));}
static float harmonic(const Signal& x,float freq){return toneLevel(x,48000.f,freq,24000);}
static float harmonicEnergy(const Signal& x,float base){double sum=0;for(int k=2;k<=12;k++){const float h=harmonic(x,base*(float)k);sum+=(double)h*h;}return (float)sqrt(sum);}

TEST("DEPTH zero has essentially no new harmonics; higher DEPTH creates strong audio-rate sidebands") {
  const Signal x=sine(48000,300,2,.35f);
  Fx<SelfFmMatrix> clean,mod;setup(clean);setup(mod);
  clean.set("depth",0);mod.set("depth",95);
  const auto a=clean.run(x).L,b=mod.run(x).L;
  const float h0=harmonicEnergy(a,300),h1=harmonicEnergy(b,300);
  CHECK(h0<.0005f&&h1>.035f,"300Hz carrier: depth 0 harmonic RMS %.6f; depth 95 %.4f",h0,h1);
}
TEST("ROUTE has four distinguishable harmonic profiles: self, serial, fan-in, parallel") {
  const Signal x=sine(48000,300,2,.35f);
  float feature[4][3]={{0}};
  for(int k=0;k<4;k++){
    Fx<SelfFmMatrix> f;setup(f);f.set("route",(float)k);
    auto y=f.run(x).L;
    for(int h=0;h<3;h++)feature[k][h]=harmonic(y,300.f*(float)(h+1));
  }
  float minDistance=1e6f;
  for(int a=0;a<4;a++)for(int b=a+1;b<4;b++){
    float d=0;for(int h=0;h<3;h++)d+=fabsf(feature[a][h]-feature[b][h]);
    minDistance=fminf(minDistance,d);
  }
  CHECK(minDistance>.04f,"smallest pairwise difference in first three harmonic amplitudes %.4f",minDistance);
}
TEST("SOURCE changes the modulator spectrum, not merely a global output gain") {
  const Signal x=sine(48000,300,2,.35f);
  Fx<SelfFmMatrix> a,b;setup(a);setup(b);a.set("source",0);b.set("source",100);
  const auto A=a.run(x).L,B=b.run(x).L;
  const float hA=harmonic(A,900)/fmaxf(harmonic(A,300),1e-6f);
  const float hB=harmonic(B,900)/fmaxf(harmonic(B,300),1e-6f);
  CHECK(fabsf(hA-hB)>.2f,"third-harmonic/fundamental ratios: direct %.3f, edge %.3f",hA,hB);
}
TEST("FEEDBACK changes modulation spectrum without unstable level growth") {
  const Signal x=sine(48000,300,2,.35f);
  Fx<SelfFmMatrix> a,b;setup(a);setup(b);a.set("feedback",0);b.set("feedback",85);
  const auto A=a.run(x).L,B=b.run(x).L;
  const float hA=harmonic(A,600),hB=harmonic(B,600);
  const float ra=tailRms(A,24000),rb=tailRms(B,24000);
  CHECK(fabsf(hA-hB)>.015f&&rb<.5f,"second harmonic feedback 0 %.4f, feedback 85 %.4f; RMS %.4f -> %.4f",hA,hB,ra,rb);
}
TEST("TONE filters the processed upper harmonic spectrum") {
  const Signal x=sine(48000,850,2,.4f);
  Fx<SelfFmMatrix> lo,hi;setup(lo);setup(hi);lo.set("tone",500);hi.set("tone",11000);
  const auto A=lo.run(x).L,B=hi.run(x).L;
  const float highLo=harmonic(A,4250),highHi=harmonic(B,4250);
  CHECK(highHi>highLo*3.f,"fifth harmonic with 500Hz tone %.5f; 11kHz tone %.5f",highLo,highHi);
}
TEST("SUSTAIN changes the modulation strength on a quiet played note") {
  const Signal x=sine(48000,300,2,.025f);
  Fx<SelfFmMatrix> a,b;setup(a);setup(b);a.set("sustain",0);b.set("sustain",100);
  const auto A=a.run(x).L,B=b.run(x).L;
  const float harmA=harmonicEnergy(A,300),harmB=harmonicEnergy(B,300);
  CHECK(harmB>harmA*1.5f,"quiet note harmonic energy sustain 0 %.5f; sustain 100 %.5f",harmA,harmB);
}
TEST("ENV changes envelope recovery after a note stops") {
  // Compare envelope state after decay in a second run to avoid relying on output tail.
  Fx<SelfFmMatrix> fast,slow;setup(fast);setup(slow);fast.set("env",8);slow.set("env",380);
  Signal burst=concat(sine(48000,300,.2f,.4f),constant(48000,.12f,0.f));
  fast.run(burst);slow.run(burst);
  CHECK(slow.m.L.env>fast.m.L.env*2.f,"envelope after .12s silence: 8ms %.5f, 380ms %.5f",fast.m.L.env,slow.m.L.env);
}
TEST("MIX zero returns exact dry input after smoothing") {
  const Signal x=sine(48000,417,1,.3f);
  Fx<SelfFmMatrix> f;setup(f);f.set("mix",0);
  auto o=f.run(x).L;float error=0;
  for(size_t i=24000;i<o.size();i++)error=fmaxf(error,fabsf(o[i]-x[i]));
  CHECK(error<.00001f,"dry difference after settling %.8f",error);
}
TEST("Silence in creates no internal oscillator or synthesized signal") {
  Fx<SelfFmMatrix> f;setup(f);auto o=f.run(constant(48000,2,0.f));
  float peak=0;for(float x:o.L)peak=fmaxf(peak,fabsf(x));
  CHECK(peak<1e-6f,"absolute silence output peak %.8f",peak);
}
TEST("Stereo input is processed independently with no crosstalk") {
  Fx<SelfFmMatrix> f;setup(f);
  Signal left=sine(48000,330,1,.3f),right=constant(48000,1,0.f);
  const auto o=f.run(left,right);
  CHECK(tailRms(o.L,24000)>.04f&&tailRms(o.R,24000)<1e-7f,
        "left RMS %.4f, right RMS %.9f",tailRms(o.L,24000),tailRms(o.R,24000));
}
TEST("meters report live input envelope and modulation activity") {
  Fx<SelfFmMatrix> f;setup(f);
  auto tone=sine(48000,300,.6f,.35f);f.run(tone);
  const auto m=f.meters();
  CHECK(m[0]>.1f&&m[0]<=1.f&&m[1]>.05f&&m[1]<=1.f,
        "input envelope meter %.3f, phase index meter %.3f",m[0],m[1]);
}
RIGTEST_MAIN