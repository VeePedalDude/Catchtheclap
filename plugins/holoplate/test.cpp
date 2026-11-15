// HOLOPLATE behavior checks: measurable echoes, modal reverb and interference memory.
#include "rigtest.h"
#include "holoplate.h"
#include "../platemem/platemem.h"
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
  Fx<HoloPlate> a;settings(a,100);a.set("morph",100);a.set("angle",0);
  const Signal s=strike(2.0f);
  const auto x=a.run(s).L;
  Fx<HoloPlate> b;settings(b,100);b.set("morph",100);b.set("angle",50);
  const auto y=b.run(s).L;
  double err=0,energy=0;size_t n=0;
  for(size_t i=size_t(.35f*SR);i<size_t(1.4f*SR);i++){const double d=x[i]-y[i];err+=d*d;energy+=y[i]*y[i];n++;}
  const double ratio=sqrt(err/double(n)),signal=sqrt(energy/double(n));
  CHECK(ratio>.0004 && ratio>signal*.2,"angle projection difference %.6f RMS / signal %.6f",ratio,signal);
}
TEST("MORPH changes sound, not just graphics") {
  Fx<HoloPlate> a;settings(a,100);a.set("morph",0);
  const Signal s=strike(2.0f);const auto x=a.run(s).L;
  Fx<HoloPlate> b;settings(b,100);b.set("morph",100);
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

static Signal noteBurst(float duration=1.7f) {
  Signal x(size_t(duration*SR),0.f);
  const int length=int(.085f*SR),offset=int(.20f*SR);
  for(int i=0;i<length;i++) {
    const float ramp=fminf(1.f,fminf(i/160.f,(length-i)/160.f));
    x[(size_t)(offset+i)]=.5f*sinf(2.f*rig::PI_F*441.f*i/SR)*ramp;
  }
  return x;
}
static float proj(const Signal& s,float hz,float a,float b) {
  const int lo=int(a*SR), hi=int(b*SR);double real=0,imag=0;
  for(int i=lo;i<hi;i++) {
    const double phase=2.0*3.141592653589793*hz*i/SR;
    real+=s[(size_t)i]*cos(phase);imag+=s[(size_t)i]*sin(phase);
  }
  return (float)(2.0*sqrt(real*real+imag*imag)/(hi-lo));
}
static Signal repeatNote(float morph,float angle=70.f){
  Fx<HoloPlate> fx;settings(fx,0,85,3.f);
  fx.set("morph",morph);fx.set("angle",angle);fx.set("damp",10);
  return fx.run(noteBurst()).L;
}
TEST("MORPH zero preserves the natural first delay repeat despite the LF return filter") {
  Fx<HoloPlate> v2;Fx<rig::PlateMemory> v1;
  for(auto* f:{&v2}){settings(*f,0,75);f->set("morph",0);f->set("angle",80);}
  v1.set("time",250);v1.set("feedback",75);v1.set("diffuse",0);
  v1.set("imprint",0);v1.set("angle",80);v1.set("mix",100);
  const Signal in=strike(1.2f);
  const Signal y=v2.run(in).L,x=v1.run(in).L;
  float error=0;
  for(size_t i=size_t(.42f*SR);i<size_t(.49f*SR);i++)error=fmaxf(error,fabsf(y[i]-x[i]));
  CHECK(error<.00001f,"uncolored first echo compared with classic: maximum error %.8f",error);
}
TEST("A and B are actual alternating delay generations") {
  Fx<HoloPlate> f;settings(f,0,80);f.set("morph",100);f.set("damp",0);
  Signal in(size_t(SR),0.f);in[size_t(.20f*SR)]=1.f;
  f.run(in);
  const float a0=f.m.delayL.tap(.80f*SR),b0=f.m.delayBL.tap(.80f*SR);
  const float a1=f.m.delayL.tap(.55f*SR),b1=f.m.delayBL.tap(.55f*SR);
  const float a2=f.m.delayL.tap(.30f*SR),b2=f.m.delayBL.tap(.30f*SR);
  CHECK(a0>.9f&&fabsf(b0)<.00001f&&fabsf(a1)<.0001f&&b1>.08f&&
        a2>.015f&&fabsf(b2)<a2*.10f,
        "repeat generation A input %.3f / B %.3f; B second %.3f / A %.4f; A third %.3f / B %.4f",
        a0,b0,b1,a1,a2,b2);
}
TEST("Plate excitation alternates position and polarity as MORPH increases") {
  rig::HoloPlate f;f.init(SR);
  f.configure(3.f,0.f,.83f,.3f);
  float unshifted=0;
  for(const auto& m:f.modal)unshifted=fmaxf(unshifted,fabsf(m.injection-m.opposite));
  f.configure(3.f,1.f,.83f,.3f);
  int signSwaps=0;float difference=0;
  for(const auto& m:f.modal){
    if(m.injection*m.opposite<0)signSwaps++;
    difference+=fabsf(m.injection-m.opposite);
  }
  CHECK(unshifted<1.e-9f&&signSwaps>=14&&difference>.02f,
        "natural imprint mismatch %.8f; flipped modes %d/36, summed excitation change %.5f",
        unshifted,signSwaps,difference);
}
TEST("110Hz high-feedback input decays rather than running away") {
  for(float diff:{0.f,100.f}) {
    Fx<HoloPlate> f;settings(f,diff,88,7);f.set("time",220);f.set("morph",100);
    f.set("angle",85);f.set("damp",75);
    Signal input(size_t(8*SR),0.f);
    for(int i=0;i<2*int(SR);i++) {
      const float attack=fminf(1.f,i/(.1f*SR));
      input[(size_t)i]=.28f*sinf(2.f*rig::PI_F*110.f*i/SR)*attack;
    }
    const Signal y=f.run(input).L;
    const double early=between(y,2.f,3.f),late=between(y,6.f,8.f);
    const float level=peak(y);
    CHECK(late<.065&&late<early*.25&&level<1.5f,
          "DIFFUSE %.0f%%: 110Hz RMS 2-3s %.5f, 6-8s %.5f, peak %.4f",
          diff,early,late,level);
  }
}
TEST("Spectral reconstruction generates strong inharmonic components") {
  const Signal natural=repeatNote(0),strange=repeatNote(100);
  const float nBase=proj(natural,441,.72f,.80f),nSide=proj(natural,950,.72f,.80f);
  const float sBase=proj(strange,441,.72f,.80f),sSide=proj(strange,950,.72f,.80f);
  const float ratioOld=nSide/(nBase+1e-8f),ratioNew=sSide/(sBase+1e-8f);
  CHECK(sSide>.010f && ratioNew>ratioOld*15.f,
        "950 Hz inharmonic partial %.5f (classic %.5f), spectral ratio %.3f (classic %.3f)",
        sSide,nSide,ratioNew,ratioOld);
}
TEST("MORPH varies tone, not just loudness, across the middle") {
  const Signal natural=repeatNote(0),half=repeatNote(50),strange=repeatNote(100);
  auto cosine=[&](const Signal& b){double ab=0,aa=0,bb=0;
    for(size_t i=size_t(.725f*SR);i<size_t(.80f*SR);i++){
      ab+=natural[i]*b[i];aa+=natural[i]*natural[i];bb+=b[i]*b[i];
    }
    return (float)(ab/sqrt(aa*bb+1e-30));
  };
  const float moderate=cosine(half),absurd=cosine(strange);
  CHECK(moderate<.95f && absurd<.5f && moderate>absurd+.20f,
        "repeat correlation to natural: midpoint %.3f, absurd %.3f",moderate,absurd);
}
TEST("The first repeat remains recognizable while later echoes mutate") {
  const auto natural=repeatNote(0),strange=repeatNote(100);
  const float firstA=(float)between(natural,.46f,.52f),firstB=(float)between(strange,.46f,.52f);
  const float laterA=proj(natural,441,.72f,.80f),laterB=proj(strange,441,.72f,.80f);
  CHECK(fabsf(firstA-firstB)<.002f && laterB<laterA*.45f,
        "first-repeat levels %.4f / %.4f; later 441 Hz fundamental %.4f / %.4f",firstA,firstB,laterA,laterB);
}
TEST("ANGLE changes actual reconstructed spectral data") {
  const Signal near=repeatNote(100,0),far=repeatNote(100,100);
  double d=0,e=0;
  for(size_t i=size_t(.70f*SR);i<size_t(1.1f*SR);i++){
    const double diff=near[i]-far[i];d+=diff*diff;e+=near[i]*near[i];
  }
  const float ratio=(float)sqrt(d/(e+1e-30));
  CHECK(ratio>.45f,"spectral reconstruction angle changes output: normalized RMS difference %.3f",ratio);
}

RIGTEST_MAIN