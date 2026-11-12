// FAN ROOM: blade-pass guitar tremolo, rotating spectral shadows and oscillating Doppler.
// ROTOR 0.8-16 Hz (default 4), BLADES 2-6 (3), CHOP 0-100% (75), COLOR 0-100% (45),
// SWING 0.08-2.0 Hz (0.35), ARC 0-100% (65), DOPPLER 0-100% (40), MIX 0-100% (80).
#pragma once
#include "module.h"

namespace rig {
inline constexpr ParamStop FANROOM_BLADE_STOPS[] = {
  {2.f,"2"},{3.f,"3"},{4.f,"4"},{5.f,"5"},{6.f,"6"}
};
inline constexpr ParamDesc FANROOM_PARAMS[] = {
  {"rotor", "ROTOR", .8f, 16.f, .537f, "Hz", Curve::Log, 1, false},
  {"blades", "BLADES", 2.f, 6.f, .25f, "", Curve::Lin, 0, false, FANROOM_BLADE_STOPS, 5},
  {"chop", "CHOP", 0.f, 100.f, .75f, "%", Curve::Lin, 0, false},
  {"color", "COLOR", 0.f, 100.f, .45f, "%", Curve::Lin, 0, false},
  {"swing", "SWING", .08f, 2.f, .459f, "Hz", Curve::Log, 2, false},
  {"arc", "ARC", 0.f, 100.f, .65f, "%", Curve::Lin, 0, false},
  {"doppler", "DOPPLER", 0.f, 100.f, .4f, "%", Curve::Lin, 0, false},
  {"mix", "MIX", 0.f, 100.f, .8f, "%", Curve::Lin, 0, false}
};
inline constexpr ModuleDesc FANROOM_DESC = {
  "fanroom", "FAN ROOM", "rotor trem + sweeping Doppler", 8, FANROOM_PARAMS, 2, {"gain","pan"}
};
struct FanRoom : Module {
  // The moving acoustic path ranges from 2 to 22 ms; fixed storage works up to 96 kHz.
  DelayLine<8192> pathL, pathR;
  OnePole rotorS, chopS, colorS, swingS, arcS, dopplerS, mixS;
  float bladePhase=0.f, swingPhase=0.f, airL=0.f, airR=0.f;
  float gain=1.f, pan=0.f;
  int bladeCount=3;
  const ModuleDesc& desc() const override { return FANROOM_DESC; }
  void init(float s) override {
    sr=s;
    rotorS.setMs(sr,30); chopS.setMs(sr,25); colorS.setMs(sr,25);
    swingS.setMs(sr,100); arcS.setMs(sr,30); dopplerS.setMs(sr,30);
    mixS.setMs(sr,25); reset();
  }
  void reset() override {
    pathL.clear(); pathR.clear();
    bladePhase=swingPhase=airL=airR=pan=0.f; gain=1.f;
    rotorS.y=map(0,FANROOM_PARAMS[0].def);
    chopS.y=.75f; colorS.y=.45f;
    swingS.y=map(4,FANROOM_PARAMS[4].def);
    arcS.y=.65f; dopplerS.y=.4f; mixS.y=.8f; bladeCount=3;
  }
  void process(const float* inL,const float* inR,float* outL,float* outR,int n,
               const float* e,const Context&) override {
    const float rotorT=map(0,e[0]);
    const int blades=(int)clampf(floorf(map(1,e[1])+.5f),2.f,6.f);
    const float chopT=map(2,e[2])*.01f;
    const float colorT=map(3,e[3])*.01f;
    const float swingT=map(4,e[4]);
    const float arcT=map(5,e[5])*.01f;
    const float dopT=map(6,e[6])*.01f;
    const float mixT=map(7,e[7])*.01f;
    for(int i=0;i<n;i++) {
      const float l=inL[i],r=inR[i];
      const float rotor=rotorS.next(rotorT), chop=chopS.next(chopT);
      const float color=colorS.next(colorT),swing=swingS.next(swingT);
      const float arc=arcS.next(arcT),doppler=dopplerS.next(dopT);
      const float mix=mixS.next(mixT);
      bladePhase+=rotor/sr;
      if(bladePhase>=1.f){bladePhase-=1.f;bladeCount=blades;} // no blade count jumps mid-revolution
      swingPhase+=swing/sr; if(swingPhase>=1.f)swingPhase-=1.f;
      const float b=.5f+.5f*cosf(2.f*PI_F*bladePhase*(float)bladeCount);
      const float shadow=b*b;
      const float trem=1.f-.93f*chop*shadow;
      const float head=sinf(2.f*PI_F*swingPhase);
      // Variable delay models acoustic travel time and exaggerated Doppler shift.
      const float travelSec=.012f+.010f*arc*doppler*head;
      const float tap=clampf(travelSec*sr,1.f,8190.f);
      pathL.push(l);pathR.push(r);
      const float movedL=pathL.tap(tap),movedR=pathR.tap(tap);
      // Blade shadow modulates upper harmonics as well as the overall amplitude.
      const float shade=color*shadow;
      const float cutoff=1100.f+11500.f*(1.f-shade);
      const float alpha=clampf(2.f*PI_F*cutoff/sr,.002f,1.f);
      airL=undenorm(airL+alpha*(movedL-airL));
      airR=undenorm(airR+alpha*(movedR-airR));
      const float filteredL=movedL+shade*(airL-movedL);
      const float filteredR=movedR+shade*(airR-movedR);
      const float side=arc*head;
      const float wetL=filteredL*trem*(1.f-.5f*side);
      const float wetR=filteredR*trem*(1.f+.5f*side);
      outL[i]=clampf(l*(1.f-mix)+wetL*mix,-4.f,4.f);
      outR[i]=clampf(r*(1.f-mix)+wetR*mix,-4.f,4.f);
      gain=trem;pan=side;
    }
  }
  void meters(float* out)const override {out[0]=gain;out[1]=pan;}
};
} // namespace rig
