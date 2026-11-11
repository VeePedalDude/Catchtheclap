// CLOCKWITCH 8950: measurable clock, quantization, pitch, vibrato and mixer behavior.
#include "rigtest.h"
#include "clockwitch.h"
using namespace rigtest;
using rig::Clockwitch8950;

static Signal source(float seconds=2.f,float hz=220.f){
  Signal v((size_t)(seconds*48000.f));
  for(size_t i=0;i<v.size();i++)v[i]=.4f*sinf(2.f*rig::PI_F*hz*(float)i/48000.f);
  return v;
}
static void defaults(Fx<Clockwitch8950>& f){
  f.setStop("voice","NORMAL");f.set("clock",8000);f.set("skew",1);
  f.set("speed",8);f.set("depth",0);f.setStop("shape","SINE");
  f.set("unstable",0);f.set("mix",100);
}
static float difference(const Signal& a,const Signal& b,size_t start=24000){
  float m=0.f;for(size_t i=start;i<a.size()&&i<b.size();i++)m=fmaxf(m,fabsf(a[i]-b[i]));return m;
}
static float spectralPeak(const Signal& sig,int fs=48000){
  // Dense enough to distinguish pitch extremes, without a full FFT dependency.
  const size_t start=sig.size()-24000, n=24000;
  float best=0.f,bestHz=0;
  for(int hz=80;hz<=570;hz+=3){
    float sinSum=0,cosSum=0;
    const float w=2.f*rig::PI_F*(float)hz/(float)fs;
    for(size_t i=0;i<n;i+=3){const float x=sig[start+i];sinSum+=x*sinf(w*(float)i);cosSum+=x*cosf(w*(float)i);}
    const float v=sinSum*sinSum+cosSum*cosSum;
    if(v>best){best=v;bestHz=(float)hz;}
  }
  return bestHz;
}
TEST("VOICE exposes all seven HT8950 ratios and a distinct robot mode"){
  const char* stops[]={"DN3 2/3","DN2 4/5","DN1 8/9","NORMAL","UP1 4/3","UP2 8/5","UP3 2","ROBOT"};
  const float want[]={2.f/3.f,4.f/5.f,8.f/9.f,1.f,4.f/3.f,8.f/5.f,2.f,1.f};
  const Signal s=source(1.2f);
  Signal rendered[8];bool ratios=true;
  for(int i=0;i<8;i++){
    Fx<Clockwitch8950> f;defaults(f);f.setStop("voice",stops[i]);
    rendered[i]=f.run(s).L;
    ratios=ratios && fabsf(f.m.dacKHz/f.m.adcKHz-want[i])<.01f;
  }
  float minPair=100.f;
  for(int a=0;a<8;a++)for(int b=a+1;b<8;b++)minPair=fminf(minPair,difference(rendered[a],rendered[b]));
  CHECK(ratios&&minPair>.03f,"VOICE: ratios within 0.01x; least of 28 output pair differences %.4f",minPair);
}
TEST("CLOCK determines physical 8-bit capture edge count"){
  const Signal s=source(2.f);
  Fx<Clockwitch8950> low,high;defaults(low);defaults(high);low.set("clock",1000);high.set("clock",16000);
  low.run(s);high.run(s);
  const float a=(float)low.m.captured,b=(float)high.m.captured;
  CHECK(a>1750&&a<2500&&b>31000&&b<32500,"CLOCK: 1k captures %.0f in 2s; 16k captures %.0f",a,b);
}
TEST("SKEW changes actual pitched frequency above and below the source"){
  const Signal s=source(2.f);
  Fx<Clockwitch8950> down,up;defaults(down);defaults(up);
  down.set("skew",.7f);up.set("skew",1.6f);
  const float lo=spectralPeak(down.run(s).L),hi=spectralPeak(up.run(s).L);
  CHECK(lo<200.f&&hi>285.f&&hi>lo*1.45f,"SKEW: down dominant %.0f Hz, up dominant %.0f Hz (220Hz source)",lo,hi);
}
TEST("VIB SPEED changes modulation rate independently of pitch selection"){
  const Signal s=source(2.f);
  auto cycles=[&](float speed){
    Fx<Clockwitch8950> f;defaults(f);f.set("speed",speed);f.set("depth",40);
    int crossings=0;float prev=0;bool ready=false;
    for(size_t i=0;i<s.size();i+=128){
      const int n=(int)std::min<size_t>(128,s.size()-i);
      float oL[128]={0},oR[128]={0};f.m.process(&s[i],&s[i],oL,oR,n,f.e,f.ctx);
      const float v=f.m.dacKHz-f.m.adcKHz;
      if(i>24000&&ready&&prev<=0&&v>0)crossings++;
      prev=v;ready=true;
    }
    return crossings;
  };
  const int slow=cycles(4.f),fast=cycles(13.f);
  CHECK(slow>=3&&slow<=8&&fast>=13&&fast<=22,"VIB SPEED: rising clock-rate edges in last 1.5s: 4Hz=%d, 13Hz=%d",slow,fast);
}
TEST("VIB SHAPE switches waveform, not a display label"){
  const Signal s=source(1.5f);
  Fx<Clockwitch8950> a,b,c;defaults(a);defaults(b);defaults(c);
  a.set("depth",45);b.set("depth",45);c.set("depth",45);
  a.setStop("shape","SINE");b.setStop("shape","SQUARE");c.setStop("shape","STEPS");
  const auto aa=a.run(s).L,bb=b.run(s).L,cc=c.run(s).L;
  const float ab=difference(aa,bb),ac=difference(aa,cc);
  CHECK(ab>.07f&&ac>.07f,"VIB SHAPE: sine vs square %.3f; sine vs stepped %.3f",ab,ac);
}
TEST("INSTABILITY perturbs the sample clock, not just adds output noise"){
  const Signal s=source(1.8f);
  Fx<Clockwitch8950> clean,broken;defaults(clean);defaults(broken);
  broken.set("unstable",100);
  const auto aa=clean.run(s).L,bb=broken.run(s).L;
  const float delta=difference(aa,bb);
  CHECK(delta>.1f&&fabsf(broken.m.dacKHz-clean.m.dacKHz)>.01f,
    "INSTABILITY: output difference %.3f; clock change %.3f kHz",delta,fabsf(broken.m.dacKHz-clean.m.dacKHz));
}
TEST("8-bit capture stores quantized samples and stereo channels are isolated"){
  Fx<Clockwitch8950> fx;defaults(fx);
  Signal l=source(.2f),r(l.size(),0.f);
  const Stereo o=fx.run(l,r);
  int nonzero=0;float right=0;bool quantized=true;
  for(int i=0;i<Clockwitch8950::SIZE;i++){
    const int code=(int)fx.m.captureL[i];
    nonzero+=(code!=0);quantized=quantized && (code>=-127&&code<=127);
  }
  for(float v:o.R)right=fmaxf(right,fabsf(v));
  CHECK(nonzero>600&&quantized&&right==0.f,"8-bit ADC: %d nonzero codes, right-channel leakage %.6f",nonzero,right);
}
TEST("MIX at zero preserves the dry guitar after the smoothing interval"){
  const Signal s=source(1.1f);
  Fx<Clockwitch8950> f;defaults(f);f.set("mix",0);
  const Signal wet=f.run(s).L;
  const float d=difference(s,wet,24000);
  CHECK(d<.0001f,"MIX 0: largest dry-signal error after 0.5s %.8f",d);
}
RIGTEST_MAIN
