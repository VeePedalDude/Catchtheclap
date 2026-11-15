// HOLOPLATE behavior checks: measurable echoes, modal reverb and interference memory.
#include "rigtest.h"
#include "holoplate.h"
using namespace rigtest;
using rig::HoloPlate;
static constexpr float SR=48000.f;
static Signal strike(float seconds=4.f,float at=.20f){Signal s(size_t(seconds*SR),0);s[size_t(at*SR)]=1.f;return s;}
static float maxNear(const Signal& s,float sec,float width=.002f){return peak(s,size_t((sec-width)*SR),size_t((sec+width)*SR));}
static double between(const Signal& s,float a,float b){return rms(s,size_t(a*SR),size_t(b*SR));}
static void settings(Fx<HoloPlate>& f,float diffuse,float feedback=0,float decay=2.5f){
  f.set("time",250.f);f.set("feedback",feedback);f.set("decay",decay);
  f.set("diffuse",diffuse);f.set("mix",100.f);
}
TEST("TIME yields distinct sample-accurate echoes with DIFFUSE at zero") {
  Fx<HoloPlate> f;settings(f,0);
  const auto y=f.run(strike(2)).L;
  const float first=maxNear(y,.450f),before=peak(y,size_t(.26f*SR),size_t(.42f*SR));
  CHECK(first>.4f && before<.0001f,"first 250ms echo %.4f; pre-echo peak %.7f",first,before);
}
TEST("FEEDBACK produces a controlled second echo") {
  Fx<HoloPlate> f;settings(f,0,65);
  const auto y=f.run(strike(2)).L;
  const float a=maxNear(y,.450f),b=maxNear(y,.700f);
  CHECK(a>.4f && b>.18f && b<a*.8f,"250ms echo %.4f, 500ms echo %.4f (ratio %.2f)",a,b,b/(a+.00001f));
}
TEST("DIFFUSE morphs distinct repeats into a dense reverb") {
  Fx<HoloPlate> a;settings(a,0,55);const Signal in=strike(2);
  const auto plain=a.run(in).L;
  Fx<HoloPlate> b;settings(b,100,55);const auto dense=b.run(in).L;
  const double sparse=between(plain,.53f,.63f),filled=between(dense,.53f,.63f);
  CHECK(filled>sparse*8.f && filled>.0001,"between echoes: dry-delay RMS %.7f vs diffuse RMS %.7f",sparse,filled);
}
TEST("A plate tail starts from the attack rather than waiting for the first echo") {
  Fx<HoloPlate> f;settings(f,100);
  const Signal y=f.run(strike(1)).L;
  const double pre=between(y,.24f,.37f);
  CHECK(pre>.00001,"pre-echo plate tail RMS %.7f",pre);
}
TEST("DECAY controls modal and diffused late energy") {
  Fx<HoloPlate> shortFx;settings(shortFx,100,0,.5f);
  const Signal s=strike(4.5f);
  const auto shortTail=shortFx.run(s).L;
  Fx<HoloPlate> longFx;settings(longFx,100,0,3.5f);
  const auto longTail=longFx.run(s).L;
  const double weak=between(shortTail,1.65f,2.10f),strong=between(longTail,1.65f,2.10f);
  CHECK(strong>weak*10 && strong>.00001,"tail short %.8f, tail long %.8f RMS",weak,strong);
}
TEST("ANGLE changes the modal interference projection") {
  Fx<HoloPlate> a;settings(a,100);a.set("imprint",100);a.set("angle",0);
  const Signal s=strike(2.0f);
  const auto x=a.run(s).L;
  Fx<HoloPlate> b;settings(b,100);b.set("imprint",100);b.set("angle",50);
  const auto y=b.run(s).L;
  double err=0,energy=0;size_t n=0;
  for(size_t i=size_t(.35f*SR);i<size_t(1.4f*SR);i++){const double d=x[i]-y[i];err+=d*d;energy+=y[i]*y[i];n++;}
  const double ratio=sqrt(err/double(n)),signal=sqrt(energy/double(n));
  CHECK(ratio>.0004 && ratio>signal*.2,"angle projection difference %.6f RMS / signal %.6f",ratio,signal);
}
TEST("IMPRINT changes sound, not just graphics") {
  Fx<HoloPlate> a;settings(a,100);a.set("imprint",0);
  const Signal s=strike(2.0f);const auto x=a.run(s).L;
  Fx<HoloPlate> b;settings(b,100);b.set("imprint",100);
  const auto y=b.run(s).L;
  double dist=0;size_t n=0;
  for(size_t i=size_t(.30f*SR);i<size_t(1.2f*SR);i++){const double d=x[i]-y[i];dist+=d*d;n++;}
  const double difference=sqrt(dist/n);
  CHECK(difference>.0002,"hologram on/off changes wet sound by %.6f RMS",difference);
}
TEST("Spatial plate pickups produce stereo from mono input") {
  Fx<HoloPlate> f;settings(f,100);
  const auto o=f.run(strike(1.6f));double d=0;size_t n=0;
  for(size_t i=size_t(.30f*SR);i<size_t(.9f*SR);i++){const double v=o.L[i]-o.R[i];d+=v*v;n++;}
  CHECK(sqrt(d/n)>.0004,"mono input yields left/right difference %.6f RMS",sqrt(d/n));
}
TEST("DAMP removes high frequencies from later feedback repeats") {
  const Signal s=sine(SR,7000.f,.018f,.5f);
  Signal x(size_t(1.2f*SR),0.f);for(size_t i=0;i<s.size();i++)x[size_t(.2f*SR)+i]=s[i];
  Fx<HoloPlate> open;settings(open,0,70);open.set("damp",0);const auto a=open.run(x).L;
  Fx<HoloPlate> closed;settings(closed,0,70);closed.set("damp",100);const auto b=closed.run(x).L;
  const double oa=between(a,.705f,.735f),ob=between(b,.705f,.735f);
  CHECK(ob<oa*.68 && oa>.002,"second high-frequency repeat damping %.6f vs %.6f RMS",ob,oa);
}
TEST("Zero MIX returns the original audio after normal knob smoothing") {
  Fx<HoloPlate> f;settings(f,100,90);f.set("mix",0);
  const Signal x=sine(SR,327.f,2.0f,.2f);const auto y=f.run(x).L;
  float err=0;for(size_t i=size_t(.6f*SR);i<x.size();i++)err=fmaxf(err,fabsf(x[i]-y[i]));
  CHECK(err<.00001f,"settled dry-path maximum error %.9f",err);
}
TEST("Silence remains silent without internal generated noise") {
  Fx<HoloPlate> f;settings(f,100,75,7);
  const auto y=f.run(Signal(size_t(1.3f*SR),0.f)).L;
  CHECK(peak(y)<1e-7,"silent input produces peak %.9f",peak(y));
}
TEST("Longest TIME fits at 96 kHz without cutting the delayed impulse") {
  constexpr float rate=96000.f;
  Fx<HoloPlate> f(rate);f.set("time",1100);f.set("feedback",0);f.set("diffuse",0);f.set("mix",100);
  Signal x(size_t(rate*1.7f),0);x[size_t(rate*.2f)]=1;
  const auto y=f.run(x).L;
  const float peak1=peak(y,size_t(rate*1.298f),size_t(rate*1.302f));
  CHECK(peak1>.2,"96 kHz, 1.1 s TIME peak %.5f",peak1);
}
RIGTEST_MAIN
