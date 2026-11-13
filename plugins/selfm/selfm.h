// SELF FM MATRIX — guitar-sourced phase modulation, no pitch tracker and no replacement oscillator.
// DEPTH 0–100%, SOURCE 0–100%, ENV 8–380ms, SUSTAIN 0–100%, FEEDBACK 0–85%,
// ROUTE SELF/SERIAL/FAN-IN/PARALLEL, TONE 400–12000Hz, MIX 0–100%.
#pragma once
#include "module.h"

namespace rig {
inline constexpr ParamStop SELFM_ROUTES[] = {
  {0.f,"SELF"},{1.f,"SERIAL"},{2.f,"FAN-IN"},{3.f,"PARALLEL"}
};
inline constexpr ParamDesc SELFM_PARAMS[] = {
  {"depth","DEPTH",0.f,100.f,.62f,"%",Curve::Lin,0,false},
  {"source","SOURCE",0.f,100.f,.46f,"%",Curve::Lin,0,false},
  {"env","ENV",8.f,380.f,.5f,"ms",Curve::Log,0,false},
  {"sustain","SUSTAIN",0.f,100.f,.6f,"%",Curve::Lin,0,false},
  {"feedback","FEEDBACK",0.f,85.f,.27f,"%",Curve::Lin,0,false},
  {"route","ROUTE",0.f,3.f,.3333333f,"",Curve::Lin,0,false,SELFM_ROUTES,4},
  {"tone","TONE",400.f,12000.f,.81f,"Hz",Curve::Log,0,false},
  {"mix","MIX",0.f,100.f,.85f,"%",Curve::Lin,0,false}
};
inline constexpr ModuleDesc SELFM_DESC = {
  "selfm", "SELF FM MATRIX", "guitar-driven PM cascade", 8, SELFM_PARAMS, 2, {"env","index"}
};

// Two short time-varying delays are the operators. The guitar remains the carrier.
// A runs on filtered guitar; B is phase-modulated by A; B then modulates the carrier.
// This is signal-driven PM, not a DX-style oscillator FM implementation.
struct SelfFmChannel {
  DelayLine<4096> carrier, operatorB;
  DCBlock dc;
  float low = 0.f, mid = 0.f, env = 0.f, prevB = 0.f, tone = 0.f;
  void clear() {
    carrier.clear(); operatorB.clear(); dc.reset();
    low=mid=env=prevB=tone=0.f;
  }
  static float shape(float x) { return x / (1.f + fabsf(x)); }
  float tick(float input,float sr,float source,float depth,float sustain,float fb,
             float attack,float release,float lowCoef,float midCoef,float toneCoef,
             float route,float& activity) {
    const float x = dc.tick(input);
    const float absx = fabsf(x);
    env=undenorm(env+(absx-env)*(absx>env?attack:release));
    // One evolving filter bank: low, bandpass and edge outputs from the guitar.
    low=undenorm(low+lowCoef*(x-low));
    mid=undenorm(mid+midCoef*(x-mid));
    const float norm = 1.f/(.025f+env*.8f);
    const float direct=shape(x*norm*2.2f);
    const float band=shape((mid-low)*norm*3.8f);
    const float edge=shape((x-mid)*norm*2.8f);
    const float blend=source*2.f;
    const float a=blend<1.f?direct+(band-direct)*blend:band+(edge-band)*(blend-1.f);
    // Quiet background is not amplified by the self-modulator normalization.
    const float gate=clampf((env-.0015f)*160.f,0.f,1.f);
    const float modA=a*gate;
    const float dynamics=clampf(env*4.f,0.f,1.f);
    const float effectiveDepth=depth*(sustain+(1.f-sustain)*dynamics);
    // Operator B is a genuine second variable-delay PM stage; its output influences
    // the carrier read phase. Its regeneration is gain-bounded and soft-clipped.
    operatorB.push(shape(modA+fb*.32f*prevB));
    const float bTap=clampf(.00115f*sr+effectiveDepth*.00050f*sr*modA,2.f,4092.f);
    const float delayed=operatorB.tap(bTap);
    const float modB=shape(2.0f*delayed+fb*.52f*prevB);
    prevB=undenorm(modB);
    carrier.push(x);
    const float base=sr*.0045f, span=sr*.0014f*effectiveDepth;
    const float aTap=clampf(base+span*modA,2.f,4092.f);
    const float bCarrierTap=clampf(base+span*modB,2.f,4092.f);
    const float combTap=clampf(base+span*clampf(.70f*modA+.70f*modB,-1.f,1.f),2.f,4092.f);
    const float pTap=clampf(base-span*modB,2.f,4092.f);
    const float self=carrier.tap(aTap);
    const float serial=carrier.tap(bCarrierTap);
    const float fanin=carrier.tap(combTap);
    const float parallel=.5f*(self+carrier.tap(pTap));
    // Adjacent routing morphs are smoothed for automation, not sudden switches.
    float out;
    if(route<1.f)out=self+(serial-self)*route;
    else if(route<2.f)out=serial+(fanin-serial)*(route-1.f);
    else out=fanin+(parallel-fanin)*clampf(route-2.f,0.f,1.f);
    tone=undenorm(tone+toneCoef*(out-tone));
    activity=fmaxf(activity,fabsf(modA)*effectiveDepth);
    return tone;
  }
};

struct SelfFmMatrix: Module {
  SelfFmChannel L,R;
  OnePole depthS,sourceS,envS,sustainS,feedbackS,routeS,toneS,mixS;
  float envMeter=0.f,indexMeter=0.f;
  const ModuleDesc& desc() const override {return SELFM_DESC;}
  void init(float s) override {
    sr=s;
    depthS.setMs(sr,25);sourceS.setMs(sr,30);envS.setMs(sr,40);
    sustainS.setMs(sr,30);feedbackS.setMs(sr,30);routeS.setMs(sr,55);
    toneS.setMs(sr,35);mixS.setMs(sr,25);
    reset();
  }
  void reset() override {
    L.clear();R.clear();envMeter=indexMeter=0.f;
    depthS.y=map(0,SELFM_PARAMS[0].def)*.01f;
    sourceS.y=map(1,SELFM_PARAMS[1].def)*.01f;
    envS.y=map(2,SELFM_PARAMS[2].def);
    sustainS.y=map(3,SELFM_PARAMS[3].def)*.01f;
    feedbackS.y=map(4,SELFM_PARAMS[4].def)*.01f;
    routeS.y=map(5,SELFM_PARAMS[5].def);
    toneS.y=map(6,SELFM_PARAMS[6].def);
    mixS.y=map(7,SELFM_PARAMS[7].def)*.01f;
  }
  void process(const float* inL,const float* inR,float* oL,float* oR,int n,
               const float* e,const Context&) override {
    const float depthT=map(0,e[0])*.01f;
    const float sourceT=map(1,e[1])*.01f;
    const float envT=map(2,e[2]);
    const float sustainT=map(3,e[3])*.01f;
    const float fbT=map(4,e[4])*.01f;
    const float routeT=map(5,e[5]);
    const float toneT=map(6,e[6]);
    const float mixT=map(7,e[7])*.01f;
    for(int i=0;i<n;i++) {
      // Buffer aliases are permitted: read both input channels first.
      const float xL=inL[i],xR=inR[i];
      const float depth=depthS.next(depthT),source=sourceS.next(sourceT);
      const float envMs=envS.next(envT),sustain=sustainS.next(sustainT);
      const float fb=feedbackS.next(fbT),route=routeS.next(routeT);
      const float toneHz=toneS.next(toneT),mix=mixS.next(mixT);
      // Coefficients need only track slow controls; using a stable low-cost update
      // at 16-sample intervals avoids per-sample exp() in the audio loop.
      if((i&15)==0){
        attackCoef=1.f-expf(-1.f/(.0015f*sr));
        releaseCoef=1.f-expf(-1.f/(.001f*envMs*sr));
        const float lowHz=100.f+source*520.f;
        const float midHz=750.f+source*5000.f;
        lowCoef=1.f-expf(-2.f*PI_F*lowHz/sr);
        midCoef=1.f-expf(-2.f*PI_F*midHz/sr);
        toneCoef=1.f-expf(-2.f*PI_F*fminf(toneHz,sr*.46f)/sr);
      }
      float activity=0.f;
      const float wL=L.tick(xL,sr,source,depth,sustain,fb,attackCoef,releaseCoef,
                             lowCoef,midCoef,toneCoef,route,activity);
      const float wR=R.tick(xR,sr,source,depth,sustain,fb,attackCoef,releaseCoef,
                             lowCoef,midCoef,toneCoef,route,activity);
      oL[i]=clampf(xL*(1.f-mix)+wL*mix,-3.f,3.f);
      oR[i]=clampf(xR*(1.f-mix)+wR*mix,-3.f,3.f);
      envMeter=undenorm(envMeter+.006f*(clampf(fmaxf(L.env,R.env)*3.f,0.f,1.f)-envMeter));
      indexMeter=undenorm(indexMeter+.015f*(clampf(activity,0.f,1.f)-indexMeter));
    }
  }
  void meters(float* out)const override {out[0]=envMeter;out[1]=indexMeter;}
private:
  float attackCoef=0.f,releaseCoef=0.f,lowCoef=0.f,midCoef=0.f,toneCoef=0.f;
};
}