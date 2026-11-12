// DI FINISHER: guitar output conditioning, wide tone EQ, automatic compression and peak limiting.
// LO CUT 15-180 Hz (default ~30), HI CUT 3-20 kHz (default ~18), COMP 0-100% (12%),
// LIMIT 0-15 dB (0), TILT -6..6 dB (0), BODY -6..6 dB (0),
// ATTACK 2-40 ms (22 ms), OUTPUT -18..6 dB (0). Stereo linked dynamics, zero lookahead.
#pragma once
#include "module.h"

namespace rig {
inline constexpr ParamDesc DIFINISH_PARAMS[] = {
  {"lowcut", "LO CUT", 15.f, 180.f, 0.275f, "Hz", Curve::Log, 0, false},
  {"highcut", "HI CUT", 3000.f, 20000.f, 0.94f, "Hz", Curve::Log, 0, false},
  {"comp", "COMP", 0.f, 100.f, 0.12f, "%", Curve::Lin, 0, false},
  {"limit", "LIMIT", 0.f, 15.f, 0.f, "dB", Curve::Lin, 1, false},
  {"tilt", "TILT", -6.f, 6.f, 0.5f, "dB", Curve::Lin, 1, true},
  {"body", "BODY", -6.f, 6.f, 0.5f, "dB", Curve::Lin, 1, true},
  {"attack", "ATTACK", 2.f, 40.f, 0.5263158f, "ms", Curve::Lin, 0, false},
  {"output", "OUTPUT", -18.f, 6.f, 0.75f, "dB", Curve::Lin, 1, true}
};
inline constexpr ModuleDesc DIFINISH_DESC = {
  "difinish", "DI FINISHER", "guitar final EQ / dynamics", 8, DIFINISH_PARAMS,
  2, {"reduction", "peak"}
};

struct DiFinisher : Module {
  struct Channel {
    float sub1=0, sub2=0, top1=0, top2=0;
    float tiltLow=0, bodyLow=0, bodyHigh=0;
    void clear() { sub1=sub2=top1=top2=tiltLow=bodyLow=bodyHigh=0; }
    float process(float x,float hpA,float lpA,float tiltA,float bodyLowA,float bodyHighA,
                  float tiltLoGain,float tiltHiGain,float bodyGain) {
      // Two one-pole high-pass and low-pass sections per channel: 12 dB/octave per cut.
      sub1 += hpA*(x-sub1); float y=x-sub1;
      sub2 += hpA*(y-sub2); y-=sub2;
      top1 += lpA*(y-top1); top2 += lpA*(top1-top2); y=top2;
      // Flat at zero: the filter state is maintained even while controls are flat.
      tiltLow += tiltA*(y-tiltLow);
      y=tiltLow*tiltLoGain + (y-tiltLow)*tiltHiGain;
      bodyLow += bodyLowA*(y-bodyLow);
      bodyHigh += bodyHighA*(y-bodyHigh);
      y += (bodyHigh-bodyLow)*(bodyGain-1.f);
      return y;
    }
  } ch[2];
  OnePole lowS, highS, compS, limitS, tiltS, bodyS, attackS, outputS;
  float hpA=0.005f,lpA=0.5f,tiltA=0.1f,bodyLowA=0.01f,bodyHighA=0.06f;
  float tiltLoGain=1,tiltHiGain=1,bodyGain=1,driveGain=1,outputGain=1;
  float env=0,compTarget=1,compGain=1,limitGain=1,grDb=0,peakDb=-100;
  float attackCoef=0.01f,releaseCoef=0.0001f,limitRelease=0.0002f;
  int controlCounter=0;
  static constexpr float CEILING=0.891250938f;  // -1 dBFS sample-peak ceiling
  const ModuleDesc& desc() const override { return DIFINISH_DESC; }
  float coeff(float hz) const { return 1.f-expf(-2.f*PI_F*clampf(hz,1.f,sr*.49f)/sr); }
  void init(float s) override {
    sr=s;
    lowS.setMs(sr,28.f); highS.setMs(sr,28.f);
    compS.setMs(sr,35.f);limitS.setMs(sr,25.f);
    tiltS.setMs(sr,25.f);bodyS.setMs(sr,25.f);
    attackS.setMs(sr,25.f);outputS.setMs(sr,25.f);
    tiltA=coeff(700.f);bodyLowA=coeff(90.f);bodyHighA=coeff(370.f);
    limitRelease=1.f-expf(-1.f/(.075f*sr));reset();
  }
  void reset() override {
    for(auto& c:ch)c.clear();
    lowS.y=map(0,DIFINISH_PARAMS[0].def);
    highS.y=map(1,DIFINISH_PARAMS[1].def);
    compS.y=map(2,DIFINISH_PARAMS[2].def)*.01f;
    limitS.y=0;tiltS.y=0;bodyS.y=0;
    attackS.y=map(6,DIFINISH_PARAMS[6].def);outputS.y=0;
    env=0;compTarget=compGain=limitGain=1;grDb=0;peakDb=-100.f;
    controlCounter=0;
    hpA=coeff(lowS.y);lpA=coeff(highS.y);
    tiltLoGain=tiltHiGain=bodyGain=driveGain=outputGain=1;
    attackCoef=1.f-expf(-1.f/(attackS.y*.001f*sr));
    releaseCoef=1.f-expf(-1.f/(.180f*sr));
  }
  void process(const float* inL,const float* inR,float* outL,float* outR,
               int n,const float* e,const Context&) override {
    const float low=map(0,e[0]), high=map(1,e[1]);
    const float comp=map(2,e[2])*.01f, limit=map(3,e[3]);
    const float tilt=map(4,e[4]),body=map(5,e[5]);
    const float attack=map(6,e[6]),output=map(7,e[7]);
    for(int i=0;i<n;i++) {
      // Samples may be processed in-place: always read before writing.
      const float xL=inL[i],xR=inR[i];
      const float lc=lowS.next(low),hc=highS.next(high),c=compS.next(comp);
      const float ld=limitS.next(limit),t=tiltS.next(tilt),b=bodyS.next(body);
      const float at=attackS.next(attack),out=outputS.next(output);
      if(controlCounter--<=0) {
        controlCounter=15;
        hpA=coeff(lc);lpA=coeff(hc);
        tiltLoGain=db2lin(-t*.5f);tiltHiGain=db2lin(t*.5f);
        bodyGain=db2lin(b);
        driveGain=db2lin(ld);outputGain=db2lin(out);
        attackCoef=1.f-expf(-1.f/(clampf(at,2.f,40.f)*.001f*sr));
        // Longer release while heavily compressed; avoids dragging quiet tails down abruptly.
        const float releaseMs=130.f+170.f*(1.f-compGain);
        releaseCoef=1.f-expf(-1.f/(releaseMs*.001f*sr));
        const float threshold=db2lin(-18.f-8.f*c);
        const float ratio=1.f+3.5f*c;
        compTarget=(env>threshold && c>.001f) ?
          powf(threshold/(env+1e-9f),1.f-1.f/ratio):1.f;
        compTarget=clampf(compTarget,.06f,1.f);
      }
      float l=ch[0].process(xL,hpA,lpA,tiltA,bodyLowA,bodyHighA,tiltLoGain,tiltHiGain,bodyGain);
      float r=ch[1].process(xR,hpA,lpA,tiltA,bodyLowA,bodyHighA,tiltLoGain,tiltHiGain,bodyGain);
      // Linked envelope avoids wandering stereo image, auto release follows program density.
      const float detector=fmaxf(fabsf(l),fabsf(r));
      env=undenorm(env+(detector-env)*(detector>env?attackCoef:releaseCoef));
      compGain+=clampf(1.f-expf(-1.f/(.004f*sr)),.00001f,1.f)*(compTarget-compGain);
      // Drive and output trim sit before safety limiter so ceiling is never bypassed.
      const float pre=compGain*driveGain*outputGain;
      l*=pre;r*=pre;
      const float pk=fmaxf(fabsf(l),fabsf(r));
      const float required=pk>CEILING?CEILING/pk:1.f;
      if(required<limitGain)limitGain=required;
      else limitGain+=limitRelease*(required-limitGain);
      l*=limitGain;r*=limitGain;
      outL[i]=clampf(l,-CEILING,CEILING);outR[i]=clampf(r,-CEILING,CEILING);
      const float gain=fmaxf(.001f,compGain*limitGain);
      const float gr=-20.f*log10f(gain);
      grDb+=.003f*(clampf(gr,0.f,60.f)-grDb);
      const float pout=fmaxf(fabsf(l),fabsf(r));
      const float db=20.f*log10f(fmaxf(1e-6f,pout));
      if(db>peakDb)peakDb=db;
      else peakDb+=.0001f*(db-peakDb);
    }
  }
  void meters(float* out) const override { out[0]=grDb;out[1]=peakDb; }
};
} // namespace rig