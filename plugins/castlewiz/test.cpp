// Measured behavioral tests for CASTLE ALMOST WIZARD: MIT-inspired 9-mode digital multifx.
#include "rigtest.h"
#include "castlewiz.h"
using namespace rigtest;
using rig::CastleAlmostWizard;

static Signal inputSine(float seconds=2.f){
  Signal x((size_t)(48000.f*seconds));
  for(size_t i=0;i<x.size();i++)x[i]=.3f*sinf(2.f*rig::PI_F*330.f*(float)i/48000.f);
  return x;
}
static Signal impulse(float seconds=2.f){
  Signal x((size_t)(48000.f*seconds),0.f);x[24000]=.7f;return x;
}
static float peakRange(const Signal& v,int center,int radius=150){
  float peak=0.f;
  for(int j=center-radius;j<=center+radius;j++)if(j>=0&&(size_t)j<v.size())peak=fmaxf(peak,fabsf(v[(size_t)j]));
  return peak;
}
static float rmsRange(const Signal& v,size_t from,size_t to){
  double s=0;int count=0;
  for(size_t i=from;i<to&&i<v.size();i++){s+=(double)v[i]*v[i];count++;}
  return count>0?sqrtf((float)(s/(double)count)):0.f;
}
static float maxDifference(const Signal&a,const Signal&b,size_t from=24000){
  float d=0.f;for(size_t i=from;i<a.size()&&i<b.size();i++)d=fmaxf(d,fabsf(a[i]-b[i]));return d;
}
static void setup(Fx<CastleAlmostWizard>& fx,const char*mode){
  fx.setStop("mode",mode);fx.set("time",200);fx.set("feedback",25);fx.set("amount",75);
  fx.set("filter",0);fx.set("stereo",40);fx.setStop("sync","FREE");fx.setStop("trigger","OFF");
}
static Stereo runDynamic(Fx<CastleAlmostWizard>& f,const Signal& x,size_t triggerAt=SIZE_MAX){
  Stereo o;o.L.assign(x.size(),0.f);o.R.assign(x.size(),0.f);
  for(size_t i=0;i<x.size();i+=128){
    if(i>=triggerAt)f.setStop("trigger","ON");
    int n=(int)std::min<size_t>(128,x.size()-i);
    f.m.process(&x[i],&x[i],&o.L[i],&o.R[i],n,f.e,f.ctx);
  }
  return o;
}

TEST("the nine named MODE stops are present and select different effect algorithms"){
  const char* names[]={"DELAY","FLANGER","FREEZER","PANNER","CRUSHER","SLICER","PITCHER","REPLAYER","SHIFTER"};
  Signal rendered[9];const Signal source=inputSine(1.5f);
  for(int m=0;m<9;m++){
    Fx<CastleAlmostWizard> fx;setup(fx,names[m]);
    rendered[m]=runDynamic(fx,source).L;
  }
  float least=1000.f;int distinct=0;
  for(int a=0;a<9;a++)for(int b=a+1;b<9;b++){
    const float delta=maxDifference(rendered[a],rendered[b],24000);
    least=fminf(least,delta);if(delta>.02f)distinct++;
  }
  CHECK(distinct==36,"MODE: %d/36 pairs distinct by >0.02 peak amplitude, minimum difference %.4f",distinct,least);
}
TEST("DELAY TIME controls a measured 100 ms repeat"){
  Fx<CastleAlmostWizard> fx;setup(fx,"DELAY");
  fx.set("time",100);fx.set("feedback",0);fx.set("amount",100);fx.set("stereo",0);
  const Signal out=runDynamic(fx,impulse()).L;
  const float at=peakRange(out,24000+4800,110),wrong=peakRange(out,24000+2400,110);
  CHECK(at>.20f&&wrong<.005f,"DELAY 100ms: repeat %.4f, empty 50ms region %.6f",at,wrong);
}
TEST("SYNC quantizes the quarter-note delay to 500 ms at 120 BPM"){
  Fx<CastleAlmostWizard> fx;setup(fx,"DELAY");fx.set("time",200);
  fx.set("feedback",0);fx.set("amount",100);fx.set("stereo",0);
  fx.setStop("sync","1/4");fx.ctx.bpm=120;
  const Signal out=runDynamic(fx,impulse()).L;
  const float at=peakRange(out,24000+24000,110),wrong=peakRange(out,24000+9600,110);
  CHECK(at>.20f&&wrong<.005f,"SYNC 1/4 @120 BPM: repeat %.4f at 500ms, 200ms %.6f",at,wrong);
}
TEST("FLANGER produces a short comb-modulated wet tone"){
  Fx<CastleAlmostWizard> fx;setup(fx,"FLANGER");fx.set("amount",100);fx.set("feedback",60);
  const Signal source=inputSine(1.5f),out=runDynamic(fx,source).L;
  const float delta=maxDifference(out,source);
  CHECK(delta>.1f,"FLANGER: difference against dry 330 Hz tone %.4f",delta);
}
TEST("FREEZER trigger preserves a captured signal after incoming audio stops"){
  Fx<CastleAlmostWizard> fx;setup(fx,"FREEZER");fx.set("amount",100);fx.set("time",200);
  Signal x=inputSine(1.8f);for(size_t i=48000;i<x.size();i++)x[i]=0.f;
  const Signal out=runDynamic(fx,x,48000).L;
  const float sustain=rmsRange(out,55000,75000),silence=rmsRange(x,55000,75000);
  CHECK(sustain>.02f&&silence==0.f,"FREEZER: held RMS %.4f on silence %.4f",sustain,silence);
}
TEST("PANNER makes left/right levels differ and move"){
  Fx<CastleAlmostWizard> fx;setup(fx,"PANNER");fx.set("amount",100);fx.set("stereo",100);
  const Stereo out=runDynamic(fx,inputSine(2.f));
  float diff=0.f;for(size_t i=24000;i<out.L.size();i++)diff=fmaxf(diff,fabsf(out.L[i]-out.R[i]));
  CHECK(diff>.08f,"PANNER: stereo channel differential %.4f",diff);
}
TEST("CRUSHER produces discrete output levels at full depth"){
  Fx<CastleAlmostWizard> fx;setup(fx,"CRUSHER");fx.set("amount",100);fx.set("feedback",0);
  const Signal out=runDynamic(fx,inputSine(1.5f)).L;
  float residue=0.f;
  for(size_t i=30000;i<out.size();i++){
    const float scaled=out[i]*8.f;
    residue=fmaxf(residue,fabsf(scaled-floorf(scaled+.5f)));
  }
  CHECK(residue<.001f,"CRUSHER: 4-bit quantized output max step error %.6f",residue);
}
TEST("SLICER rhythm creates open and closed windows"){
  Fx<CastleAlmostWizard> fx;setup(fx,"SLICER");fx.set("amount",100);fx.setStop("sync","1/4");
  const Signal out=runDynamic(fx,inputSine(2.5f)).L;
  float lo=1.f,hi=0.f;
  for(int i=4;i<19;i++){
    const float r=rmsRange(out,(size_t)i*6000,(size_t)(i+1)*6000);
    lo=fminf(lo,r);hi=fmaxf(hi,r);
  }
  CHECK(hi>.12f&&lo<.005f,"SLICER: open-window RMS %.4f and closed-window RMS %.4f",hi,lo);
}
TEST("PITCHER and SHIFTER have distinct long-grain and short-sweep responses"){
  Fx<CastleAlmostWizard> a,b;setup(a,"PITCHER");setup(b,"SHIFTER");
  a.set("amount",100);b.set("amount",100);
  const Signal x=inputSine(1.5f);
  const auto aa=runDynamic(a,x).L,bb=runDynamic(b,x).L;
  const float d=maxDifference(aa,bb);
  CHECK(d>.1f,"PITCHER/SHIFTER maximum waveform difference %.4f",d);
}
TEST("REPLAYER trigger holds the recorded tape segment"){
  Fx<CastleAlmostWizard> fx;setup(fx,"REPLAYER");fx.set("amount",100);fx.set("time",600);
  Signal x=inputSine(1.9f);for(size_t i=48000;i<x.size();i++)x[i]=0.f;
  const Signal o=runDynamic(fx,x,48000).L;
  const float heldRms=rmsRange(o,60000,85000);
  CHECK(heldRms>.01f,"REPLAYER: triggered tape playback RMS %.4f after input stopped",heldRms);
}
TEST("AMOUNT zero returns the dry signal for every effect mode"){
  float worst=0.f;
  const char* names[]={"DELAY","FLANGER","FREEZER","PANNER","CRUSHER","SLICER","PITCHER","REPLAYER","SHIFTER"};
  const Signal x=inputSine(1.f);
  for(const char* name:names){Fx<CastleAlmostWizard> fx;setup(fx,name);fx.set("amount",0);fx.m.mixS.y=0;
    const Signal out=runDynamic(fx,x).L;worst=fmaxf(worst,maxDifference(out,x,0));
  }
  CHECK(worst<1e-7f,"AMOUNT zero: worst dry error over 9 modes %.8f",worst);
}
RIGTEST_MAIN
