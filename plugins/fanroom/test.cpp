// FAN ROOM: measurable blade-rate tremolo, moving head, and Doppler modulation.
#include "rigtest.h"
#include "fanroom.h"
using namespace rigtest;
using rig::FanRoom;

static void setup(Fx<FanRoom>& f) {
  f.set("rotor",4.f); f.set("blades",3); f.set("chop",100); f.set("color",0);
  f.set("swing",.6f); f.set("arc",0); f.set("doppler",0); f.set("mix",100);
}
static float movementHz(const Signal& v, float sampleRate, int lo, int hi){
  const int p=period(v,lo,hi); return p>0?sampleRate/(float)p:0.f;
}
static float crossingHz(const Signal& y, int start, int end, float sr=48000.f) {
  int n=0; for(int i=start+1;i<end;i++)if(y[(size_t)i-1]<0.f && y[(size_t)i]>=0.f)n++;
  return (float)n*sr/(float)(end-start);
}
static float swingDiff(const Stereo& s, int from){
  float top=0;for(size_t i=(size_t)from;i<s.L.size();i++)top=fmaxf(top,fabsf(s.L[i]-s.R[i]));return top;
}
TEST("ROTOR 4 Hz and 3 BLADES produce twelve blade passes per second") {
  Fx<FanRoom> f;setup(f);
  const auto o=f.run(sine(48000,375,3));
  const auto env=envelope(o.L,128);
  const Signal tail(env.begin()+200,env.end());
  const float hz=movementHz(tail,48000.f/128.f,18,48);
  CHECK(fabsf(hz-12.f)<.5f,"blade passes %.2f Hz, requested 12 Hz",hz);
}
TEST("BLADES changes the number of amplitude interruptions") {
  Fx<FanRoom> f;setup(f);f.set("blades",5);
  const auto o=f.run(sine(48000,375,3));
  const auto env=envelope(o.L,128);
  const Signal tail(env.begin()+200,env.end());
  const float hz=movementHz(tail,48000.f/128.f,10,24);
  CHECK(fabsf(hz-20.f)<1.f,"five blades at four revolutions per second: %.2f Hz (20 expected)",hz);
}
TEST("CHOP controls acoustic blocking depth without hard gate clicks") {
  Fx<FanRoom> a,b;setup(a);setup(b);a.set("chop",0);b.set("chop",100);
  const Signal x=sine(48000,375,3);
  auto z0=envelope(a.run(x).L,128);auto z1=envelope(b.run(x).L,128);
  float min0=10,max0=0,min1=10,max1=0;
  for(size_t i=200;i<z0.size();i++){
    min0=fminf(min0,z0[i]);max0=fmaxf(max0,z0[i]);
    min1=fminf(min1,z1[i]);max1=fmaxf(max1,z1[i]);
  }
  CHECK(max0/(min0+1e-7f)<1.1f && max1/(min1+1e-7f)>12.f,
        "unchopped dynamic ratio %.2fx, full chop %.2fx",max0/(min0+1e-7f),max1/(min1+1e-7f));
}
TEST("COLOR is a moving high-frequency shadow, not a blanket low-pass") {
  Fx<FanRoom> clearL,shadeL,clearH,shadeH;
  for(auto *f:{&clearL,&shadeL,&clearH,&shadeH}){setup(*f);f->set("chop",0);}
  shadeL.set("color",100);shadeH.set("color",100);
  const Signal low=sine(48000,330,2),high=sine(48000,7200,2);
  const float cleanLo=toneLevel(clearL.run(low).L,48000,330,24000),filteredLo=toneLevel(shadeL.run(low).L,48000,330,24000);
  const float cleanHi=toneLevel(clearH.run(high).L,48000,7200,24000),filteredHi=toneLevel(shadeH.run(high).L,48000,7200,24000);
  CHECK(filteredHi/cleanHi < filteredLo/cleanLo*.83f,
       "relative high-frequency transmission %.3f vs low %.3f",filteredHi/cleanHi,filteredLo/cleanLo);
}
TEST("ARC causes an oscillating left/right stereo difference") {
  Fx<FanRoom> staticHead,movingHead;setup(staticHead);setup(movingHead);
  staticHead.set("arc",0);movingHead.set("arc",100);
  const Signal x=sine(48000,330,3);
  const float a=swingDiff(staticHead.run(x),24000),b=swingDiff(movingHead.run(x),24000);
  CHECK(a<.00001f&&b>.05f,"zero ARC stereo difference %.6f, full ARC %.4f",a,b);
}
TEST("SWING rate controls the fan head's side-to-side cycle") {
  for(float rate:{.5f,1.f}){
    Fx<FanRoom> f;setup(f);f.set("swing",rate);f.set("arc",100);
    Signal x(48000*5,0.f);Signal m;
    f.run(x,[&](int, Fx<FanRoom>& fx){m.push_back(fx.meters()[1]);});
    const Signal tail(m.begin()+300,m.end());
    const float hz=movementHz(tail,48000.f/128.f,(int)(375.f/(rate*1.3f)),(int)(375.f/(rate*.8f)));
    CHECK(fabsf(hz-rate)<.025f,"SWING %.2f Hz gives %.3f Hz pan cycle",rate,hz);
  }
}
TEST("DOPPLER changes pitch across approach/recession, not merely volume") {
  Fx<FanRoom> still,moving;setup(still);setup(moving);
  for(auto *f:{&still,&moving}){f->set("arc",100);f->set("swing",1.2f);f->set("chop",0);f->set("color",0);}
  still.set("doppler",0);moving.set("doppler",100);
  const Signal x=sine(48000,440,2);
  const Signal base=still.run(x).L,shift=moving.run(x).L;
  const float refA=crossingHz(base,21600,26400),refB=crossingHz(base,40800,45600);
  const float a=crossingHz(shift,21600,26400),b=crossingHz(shift,40800,45600);
  CHECK(fabsf(a-b)>20.f&&fabsf(refA-refB)<12.f,
        "Doppler 0.45-0.55 s %.1f Hz and 0.85-0.95 s %.1f Hz; static %.1f/%.1f Hz",a,b,refA,refB);
}
TEST("MIX zero returns dry input after control smoothing") {
  Fx<FanRoom> f;setup(f);f.set("mix",0);f.set("arc",100);f.set("doppler",100);
  const auto x=sine(48000,422,2);const auto o=f.run(x);
  float worst=0;for(size_t i=40000;i<x.size();i++)worst=fmaxf(worst,fabsf(x[i]-o.L[i]));
  CHECK(worst<.00003f,"settled dry error %.8f",worst);
}
TEST("The two display meters remain meaningful and bounded") {
  Fx<FanRoom> f;setup(f);f.set("arc",100);
  const Signal x=sine(48000,375,3);float gMin=1,gMax=0,pMin=1,pMax=-1;
  f.run(x,[&](int,Fx<FanRoom>& fx){auto m=fx.meters();gMin=fminf(gMin,m[0]);gMax=fmaxf(gMax,m[0]);pMin=fminf(pMin,m[1]);pMax=fmaxf(pMax,m[1]);});
  CHECK(gMin>=0&&gMax<=1&&gMax-gMin>.6f&&pMin<-.7f&&pMax>.7f,
        "blade gain %.2f..%.2f and oscillation pan %.2f..%.2f",gMin,gMax,pMin,pMax);
}

TEST("COLOR zero retains the original delay path when CHOP and movement are off") {
  Fx<FanRoom> f;setup(f);f.set("chop",0);f.set("color",0);
  const Signal x=sine(48000,375,2),y=f.run(x).L;
  float difference=0.f;
  for(size_t i=48000;i<x.size();i++)
    difference=fmaxf(difference,fabsf(y[i]-x[i-575]));
  CHECK(difference<.00005f,"COLOR 0 baseline: worst sample error %.7f from nominal 12 ms delay",difference);
}
TEST("COLOR creates moving comb cancellations with CHOP disabled") {
  Fx<FanRoom> plain,interfering;setup(plain);setup(interfering);
  plain.set("chop",0);interfering.set("chop",0);interfering.set("color",100);
  const Signal x=sine(48000,750,3);
  const Signal a=envelope(plain.run(x).L,128),b=envelope(interfering.run(x).L,128);
  float alo=10.f,ahi=0.f,blo=10.f,bhi=0.f;
  for(size_t i=200;i<a.size();i++){
    alo=fminf(alo,a[i]);ahi=fmaxf(ahi,a[i]);
    blo=fminf(blo,b[i]);bhi=fmaxf(bhi,b[i]);
  }
  const float dryRatio=ahi/(alo+1e-8f),scatterRatio=bhi/(blo+1e-8f);
  CHECK(dryRatio<1.05f && scatterRatio>2.f,
        "CHOP 0: COLOR 0 ratio %.3fx, moving interference ratio %.3fx",dryRatio,scatterRatio);
}
TEST("COLOR interference moves at blade-pass rate without volume tremolo") {
  Fx<FanRoom> f;setup(f);f.set("chop",0);f.set("color",100);
  const auto o=f.run(sine(48000,750,3));
  const Signal env=envelope(o.L,128),tail(env.begin()+200,env.end());
  const float hz=movementHz(tail,48000.f/128.f,18,48);
  CHECK(fabsf(hz-12.f)<.7f,"COLOR-only movement %.2f Hz for 4 Hz rotor x 3 blades",hz);
}
TEST("Air turbulence produces no independent hiss with silent input") {
  Fx<FanRoom> f;setup(f);f.set("color",100);f.set("chop",0);
  const auto y=f.run(constant(48000,2,0)).L;
  const float p=peak(y);
  CHECK(p<1e-7f,"full COLOR silent input gives peak %.9f",p);
}
RIGTEST_MAIN
