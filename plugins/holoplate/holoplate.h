// HOLOPLATE — stereo hybrid delay / virtual plate reverb. A 2D bending-mode plate
// imprinted with an interference mask sits in an energy-limited delay + diffuser.
// TIME 70–1100 ms, FEEDBACK 0–90%, DECAY .35–7s, DIFFUSE 0–100%,
// IMPRINT 0–100%, ANGLE 0–100%, DAMP 0–100%, MIX 0–100%.
#pragma once
#include "module.h"

namespace rig {
inline constexpr ParamDesc HOLOPLATE_PARAMS[] = {
  {"time", "TIME", 70.f, 1100.f, .53f, "ms", Curve::Log, 0, false},
  {"feedback", "FEEDBACK", 0.f, 90.f, .42f, "%", Curve::Lin, 0, false},
  {"decay", "DECAY", .35f, 7.f, .64f, "s", Curve::Log, 2, false},
  {"diffuse", "DIFFUSE", 0.f, 100.f, .55f, "%", Curve::Lin, 0, false},
  {"imprint", "IMPRINT", 0.f, 100.f, .48f, "%", Curve::Lin, 0, false},
  {"angle", "ANGLE", 0.f, 100.f, .44f, "%", Curve::Lin, 0, false},
  {"damp", "DAMP", 0.f, 100.f, .55f, "%", Curve::Lin, 0, false},
  {"mix", "MIX", 0.f, 100.f, .44f, "%", Curve::Lin, 0, false}
};
inline constexpr ModuleDesc HOLOPLATE_DESC = {
  "holoplate", "HOLOPLATE", "interference plate / memory delay",
  8, HOLOPLATE_PARAMS, 2, {"plate", "echo"}
};

// Each mode is an independently decaying component of a rectangular bending plate.
// Spatial projection and an interference mask establish excitation/pickup weights.
struct HoloMode {
  float y1=0.f, y2=0.f, a=0.f, b=0.f, injection=0.f, left=0.f, right=0.f;
};

struct HoloPlate final : Module {
  static constexpr int MODES = 36;
  DelayLine<131072> delayL, delayR;      // up to 1.1 s at 96 kHz
  DelayLine<16384> network[4];
  Allpass diffuse1, diffuse2;
  HoloMode modal[MODES];
  OnePole timeS, feedbackS, decayS, diffuseS, imprintS, angleS, dampS, mixS;
  float lp[4]={};
  float echoLevel=0.f, plateLevel=0.f, feedbackL=0.f, feedbackR=0.f;
  float repeatLpL=0.f, repeatLpR=0.f;
  float delayOldMs=0.f,delayNewMs=0.f,delayXfade=1.f;
  unsigned counter=0;
  float lengths[4]={}, rt[4]={};
  const ModuleDesc& desc() const override {return HOLOPLATE_DESC;}
  void init(float rate) override {
    sr=rate;
    timeS.setMs(sr,45); feedbackS.setMs(sr,30); decayS.setMs(sr,80);
    diffuseS.setMs(sr,35); imprintS.setMs(sr,70); angleS.setMs(sr,85);
    dampS.setMs(sr,80); mixS.setMs(sr,25);
    diffuse1.init((int)(.0063f*sr),.62f);
    diffuse2.init((int)(.0091f*sr),.57f);
    reset();
  }
  void reset() override {
    delayL.clear();delayR.clear();for(auto& x:network)x.clear();
    diffuse1.clear();diffuse2.clear();
    for(auto& m:modal)m=HoloMode{};
    for(float& x:lp)x=0.f;
    echoLevel=plateLevel=feedbackL=feedbackR=repeatLpL=repeatLpR=0.f; counter=0;
    timeS.y=map(0,HOLOPLATE_PARAMS[0].def);
    delayOldMs=delayNewMs=timeS.y;delayXfade=1.f;
    feedbackS.y=map(1,HOLOPLATE_PARAMS[1].def)*.01f;
    decayS.y=map(2,HOLOPLATE_PARAMS[2].def);
    diffuseS.y=map(3,HOLOPLATE_PARAMS[3].def)*.01f;
    imprintS.y=map(4,HOLOPLATE_PARAMS[4].def)*.01f;
    angleS.y=map(5,HOLOPLATE_PARAMS[5].def)*.01f;
    dampS.y=map(6,HOLOPLATE_PARAMS[6].def)*.01f;
    mixS.y=map(7,HOLOPLATE_PARAMS[7].def)*.01f;
    configure(decayS.y,imprintS.y,angleS.y,dampS.y);
  }
  void configure(float decay,float imprint,float angle,float damp) {
    // The virtual plate follows the bending-wave dispersion f ~ k_x^2 + k_y^2.
    // The "hologram" is explicitly a virtual interference mask, not optical physics.
    const float phase=(angle-.5f)*2.f*PI_F;
    for(int i=0;i<MODES;i++){
      const int m=1+(i%6),n=1+(i/6);
      const float mx=(float)m,ny=(float)n;
      const float f=68.f*(mx*mx+.65f*ny*ny);
      const float frequency=fminf(f,sr*.44f);
      const float x=frequency/3500.f;
      const float modalDecay=decay/(1.f+2.8f*damp*x*x);
      const float r=expf(-6.907755f/(sr*fmaxf(.09f,modalDecay)));
      const float w=2.f*PI_F*frequency/sr;
      HoloMode& mode=modal[i];
      mode.a=2.f*r*cosf(w);mode.b=r*r;
      const float interference=cosf(mx*1.7f+ny*2.13f+phase);
      const float inputShape=sinf(mx*PI_F*.39f)*sinf(ny*PI_F*.43f);
      const float pupilL=sinf(mx*PI_F*.19f)*sinf(ny*PI_F*.62f);
      const float pupilR=sinf(mx*PI_F*.72f)*sinf(ny*PI_F*.28f);
      mode.injection=.0022f*(48000.f/sr)*inputShape*(1.f+.87f*imprint*interference);
      mode.left=.22f*pupilL*(1.f+.8f*imprint*cosf(phase+mx*.62f-ny*1.23f));
      mode.right=.22f*pupilR*(1.f+.8f*imprint*cosf(phase-mx*1.11f+ny*.49f));
    }
    constexpr float ms[4]={.0311f,.0411f,.0563f,.0719f};
    for(int i=0;i<4;i++){
      lengths[i]=clampf(ms[i]*sr,8.f,16380.f);
      rt[i]=clampf(powf(.001f,lengths[i]/(sr*decay)),0.f,.99985f);
    }
  }
  void process(const float* inL,const float* inR,float* outL,float* outR,int count,
               const float* e,const Context&) override {
    const float timeTarget=map(0,e[0]),fbTarget=map(1,e[1])*.01f;
    const float decayTarget=map(2,e[2]),diffuseTarget=map(3,e[3])*.01f;
    const float imprintTarget=map(4,e[4])*.01f,angleTarget=map(5,e[5])*.01f;
    const float dampTarget=map(6,e[6])*.01f,mixTarget=map(7,e[7])*.01f;
    for(int s=0;s<count;s++){
      const float xL=inL[s],xR=inR[s];
      timeS.next(timeTarget);const float fb=feedbackS.next(fbTarget);
      const float decay=decayS.next(decayTarget),diff=diffuseS.next(diffuseTarget);
      const float imprint=imprintS.next(imprintTarget),angle=angleS.next(angleTarget);
      const float damp=dampS.next(dampTarget),mix=mixS.next(mixTarget);
      if((counter++&63u)==0u)configure(decay,imprint,angle,damp);
      // Two stationary read heads: crossfade substantial TIME changes rather than
      // sweeping one head across recorded audio (audible Doppler and discontinuities).
      if(fabsf(timeTarget-delayNewMs)>12.f){
        delayOldMs=delayOldMs*(1.f-delayXfade)+delayNewMs*delayXfade;
        delayNewMs=timeTarget;
        delayXfade=0.f;
      }
      const float interp=delayXfade;
      const float oldTap=clampf(delayOldMs*.001f*sr,2.f,131066.f);
      const float newTap=clampf(delayNewMs*.001f*sr,2.f,131066.f);
      const float echoL=delayL.tap(oldTap)*(1.f-interp)+delayL.tap(newTap)*interp;
      const float echoR=delayR.tap(oldTap)*(1.f-interp)+delayR.tap(newTap)*interp;
      delayXfade=fminf(1.f,delayXfade+1.f/(.055f*sr));
      const float monoInput=.5f*(xL+xR),monoEcho=.5f*(echoL+echoR);
      const float excitation=.45f*monoInput+.60f*monoEcho;
      float modalL=0.f,modalR=0.f;
      // Adjacent modes are weakly coupled through the hologram. The tiny amount
      // is bounded by damping and produces evolving frequency-selective memory.
      for(int i=0;i<MODES;i++){
        HoloMode& m=modal[i];
        const float neighbor=modal[(i+5)%MODES].y1;
        const float coupling=.00004f*imprint*(neighbor-m.y1);
        const float y=clampf(m.a*m.y1-m.b*m.y2+m.injection*excitation+coupling,-2.f,2.f);
        m.y2=m.y1;m.y1=undenorm(y);
        modalL+=y*m.left;modalR+=y*m.right;
      }
      // Fixed-size diffusion network (orthogonal 4x4 feedback matrix).
      const float modalMono=.5f*(modalL+modalR);
      const float seed=diffuse2.tick(diffuse1.tick(.35f*monoInput+.65f*monoEcho+.35f*modalMono));
      float d[4];for(int i=0;i<4;i++)d[i]=network[i].tap(lengths[i]);
      const float w0=.5f*(d[0]+d[1]+d[2]+d[3]);
      const float w1=.5f*(d[0]-d[1]+d[2]-d[3]);
      const float w2=.5f*(d[0]+d[1]-d[2]-d[3]);
      const float w3=.5f*(d[0]-d[1]-d[2]+d[3]);
      const float v[4]={w0,w1,w2,w3};
      const float alpha=.1f+.82f*(1.f-damp);
      for(int i=0;i<4;i++){
        lp[i]=undenorm(lp[i]+alpha*(v[i]-lp[i]));
        network[i].push(clampf(.41f*seed+rt[i]*lp[i],-3.f,3.f));
      }
      const float revL=clampf(modalL*1.40f+.75f*(d[0]+d[2]-d[1]-d[3]),-3.f,3.f);
      const float revR=clampf(modalR*1.40f+.75f*(d[0]-d[2]+d[1]-d[3]),-3.f,3.f);
      // Persistent plate energy returns to the delay only through a bounded tap.
      const float recycle=.22f*diff*imprint;
      feedbackL=undenorm(.998f*feedbackL+.002f*revL);
      feedbackR=undenorm(.998f*feedbackR+.002f*revR);
      const float echoAlpha=.15f+.78f*(1.f-damp);
      repeatLpL=undenorm(repeatLpL+echoAlpha*(echoL-repeatLpL));
      repeatLpR=undenorm(repeatLpR+echoAlpha*(echoR-repeatLpR));
      delayL.push(clampf(xL+fb*((1.f-recycle)*repeatLpL+recycle*feedbackL),-3.f,3.f));
      delayR.push(clampf(xR+fb*((1.f-recycle)*repeatLpR+recycle*feedbackR),-3.f,3.f));
      const float wetL=(1.f-diff)*echoL+diff*revL;
      const float wetR=(1.f-diff)*echoR+diff*revR;
      outL[s]=clampf(xL*(1.f-mix)+wetL*mix,-4.f,4.f);
      outR[s]=clampf(xR*(1.f-mix)+wetR*mix,-4.f,4.f);
      echoLevel=undenorm(echoLevel+.001f*(clampf(fmaxf(fabsf(echoL),fabsf(echoR)),0.f,1.f)-echoLevel));
      plateLevel=undenorm(plateLevel+.001f*(clampf(fmaxf(fabsf(revL),fabsf(revR)),0.f,1.f)-plateLevel));
    }
  }
  void meters(float* out) const override {out[0]=plateLevel;out[1]=echoLevel;}
};
} // namespace rig
