// SELF FM MATRIX 2: audio-rate oscillator-assisted guitar-carrier phase modulation.
// INDEX 0-12 rad, MOD HZ 40-1600, ENV 8-380ms, SELF 0-100%, FDBK 0-85%,
// ROUTE SELF/SERIAL/FAN-IN/PARALLEL, TONE 400-16000Hz, MIX 0-100%.
#pragma once
#include "module.h"

namespace rig {
inline constexpr ParamStop SELFM_ROUTES[] = {
  {0.f,"SELF"},{1.f,"SERIAL"},{2.f,"FAN-IN"},{3.f,"PARALLEL"}
};
inline constexpr ParamDesc SELFM_PARAMS[] = {
  {"depth","INDEX",0.f,12.f,.45f,"",Curve::Lin,2,false},
  {"source","MOD HZ",40.f,1600.f,.56f,"Hz",Curve::Log,0,false},
  {"env","ENV",8.f,380.f,.63f,"ms",Curve::Log,0,false},
  {"sustain","SELF",0.f,100.f,.25f,"%",Curve::Lin,0,false},
  {"feedback","FDBK",0.f,85.f,.32f,"%",Curve::Lin,0,false},
  {"route","ROUTE",0.f,3.f,.3333333f,"",Curve::Lin,0,false,SELFM_ROUTES,4},
  {"tone","TONE",400.f,16000.f,.922f,"Hz",Curve::Log,0,false},
  {"mix","MIX",0.f,100.f,1.f,"%",Curve::Lin,0,false}
};
inline constexpr ModuleDesc SELFM_DESC = {
  "selfm","SELF FM MATRIX","audio-rate phase synthesis",8,SELFM_PARAMS,2,{"env","index"}
};

// A 511-tap windowed Hilbert transformer creates an analytic version of the
// actual guitar. Its time-aligned real and quadrature branches undergo a
// complex phase rotation; no pitch tracking or delay-time modulation is used.
struct SelfFmChannel {
  float ring[512]={};
  int write=0;
  float env=0.f,low=0.f,prevStage=0.f,outLP=0.f,lowCoef=.0025f;
  float activity=0.f;
  void clear(){memset(ring,0,sizeof(ring));write=0;env=low=prevStage=outLP=activity=0.f;}
  static float limitShape(float v){return v/(1.f+fabsf(v));}
  static float rotateReal(float real,float quad,float angle){
    return real*cosf(angle)-quad*sinf(angle);
  }
  float tick(float x,const float* coeff,float phA,float phB,float index,
             float selfBlend,float feedback,float envelopeAttack,float envelopeRelease,
             float toneCoef,float route,float mix,float& eDisplay,float& iDisplay){
    const float mag=fabsf(x);
    env=undenorm(env+(mag-env)*(mag>env?envelopeAttack:envelopeRelease));
    low=undenorm(low+lowCoef*(x-low));
    const float selfSignal=clampf((x-low)/(env*1.30f+.008f),-1.3f,1.3f);
    ring[write]=x;
    const int newest=write;
    write=(write+1)&511;
    const float real=ring[(newest-255)&511];
    float quad=0.f;
    for(int j=0;j<128;j++){
      const int d=j*2+1;
      const float near=ring[(newest-255+d)&511];
      const float far=ring[(newest-255-d)&511];
      quad+=coeff[j]*(near-far);
    }
    const float gate=clampf((env-.00035f)*260.f,0.f,1.f);
    const float ampIndex=index*gate;
    const float oscA=sinf(phA+feedback*2.0f*prevStage);
    const float modA=(1.f-selfBlend)*oscA+selfBlend*selfSignal;
    const float thetaA=ampIndex*modA;
    const float sinA=sinf(thetaA),cosA=cosf(thetaA);
    const float stageA=real*cosA-quad*sinA;
    const float qA=real*sinA+quad*cosA;
    const float stageNorm=clampf(stageA/(env*1.4f+.015f),-1.3f,1.3f);
    const float modB=sinf(phB+1.35f*stageNorm+feedback*2.1f*prevStage);
    const float modBIndependent=sinf(phB+feedback*.70f*prevStage);
    const float thetaB=ampIndex*.79f*modB;
    const float thetaIndependent=ampIndex*.92f*modBIndependent;
    float wet=stageA;
    const int r0=(int)floorf(route);
    const float blend=route-(float)r0;
    float serial=0.f,fanin=0.f,parallel=0.f;
    if(r0==0||r0==1){
      serial=rotateReal(stageA,qA,thetaB);
      wet=stageA+(serial-stageA)*clampf(route,0.f,1.f);
    }
    if(r0==1||r0==2){
      const float angle=.75f*thetaA+.80f*thetaIndependent;
      fanin=rotateReal(real,quad,angle);
      if(r0==1)wet=serial+(fanin-serial)*blend;
    }
    if(r0>=2){
      parallel=.52f*(stageA+rotateReal(real,quad,-thetaIndependent));
      wet=fanin+(parallel-fanin)*clampf(route-2.f,0.f,1.f);
    }
    if(route>=2.999f)wet=parallel;
    prevStage=undenorm(limitShape(stageNorm*.65f+modB*.40f));
    const float filtered=undenorm(outLP+toneCoef*(wet-outLP));
    outLP=filtered;
    eDisplay=fmaxf(eDisplay,clampf(env*3.f,0.f,1.f));
    iDisplay=fmaxf(iDisplay,clampf(fabsf(thetaA)*.08f,0.f,1.f));
    activity=ampIndex;
    return clampf(real*(1.f-mix)+filtered*mix,-3.f,3.f);
  }
};

struct SelfFmMatrix:Module {
  SelfFmChannel L,R;
  float coeff[128]={};
  OnePole depthS,sourceS,envS,selfS,fbS,routeS,toneS,mixS;
  float oscillatorPhaseA=0.f,oscillatorPhaseB=0.f;
  float attackCoef=0.f,releaseCoef=0.f,toneCoef=0.f;
  unsigned sampleCounter=0;
  float envMeter=0.f,indexMeter=0.f;
  const ModuleDesc& desc()const override{return SELFM_DESC;}
  void init(float s)override{
    sr=s;
    depthS.setMs(sr,30);sourceS.setMs(sr,40);envS.setMs(sr,40);
    selfS.setMs(sr,35);fbS.setMs(sr,35);routeS.setMs(sr,55);
    toneS.setMs(sr,40);mixS.setMs(sr,25);
    for(int j=0;j<128;j++){
      const float d=(float)(2*j+1),v=PI_F*d/256.f;
      const float window=.42f+.50f*cosf(v)+.08f*cosf(2.f*v);
      coeff[j]=(2.f/(PI_F*d))*window;
    }
    reset();
  }
  void reset()override{
    L.clear();R.clear();oscillatorPhaseA=oscillatorPhaseB=0.f;
    L.lowCoef=R.lowCoef=1.f-expf(-2.f*PI_F*20.f/sr);
    envMeter=indexMeter=0.f;sampleCounter=0;
    depthS.y=map(0,SELFM_PARAMS[0].def);
    sourceS.y=map(1,SELFM_PARAMS[1].def);
    envS.y=map(2,SELFM_PARAMS[2].def);
    selfS.y=map(3,SELFM_PARAMS[3].def)*.01f;
    fbS.y=map(4,SELFM_PARAMS[4].def)*.01f;
    routeS.y=map(5,SELFM_PARAMS[5].def);
    toneS.y=map(6,SELFM_PARAMS[6].def);
    mixS.y=map(7,SELFM_PARAMS[7].def)*.01f;
    attackCoef=1.f-expf(-1.f/(.0025f*sr));
    releaseCoef=1.f-expf(-1.f/(.001f*envS.y*sr));
    toneCoef=1.f-expf(-2.f*PI_F*toneS.y/sr);
  }
  void process(const float* inL,const float* inR,float* oL,float* oR,int n,
               const float* e,const Context&)override{
    const float dT=map(0,e[0]),fT=map(1,e[1]),envT=map(2,e[2]);
    const float sT=map(3,e[3])*.01f,fbT=map(4,e[4])*.01f;
    const float rT=map(5,e[5]),tT=map(6,e[6]),mT=map(7,e[7])*.01f;
    for(int i=0;i<n;i++){
      const float xL=inL[i],xR=inR[i];
      const float index=depthS.next(dT),hz=sourceS.next(fT);
      const float envMs=envS.next(envT),self=selfS.next(sT);
      const float feedback=fbS.next(fbT),route=routeS.next(rT);
      const float toneHz=toneS.next(tT),mix=mixS.next(mT);
      if((sampleCounter++&15u)==0u){
        releaseCoef=1.f-expf(-1.f/(.001f*envMs*sr));
        toneCoef=1.f-expf(-2.f*PI_F*fminf(toneHz,sr*.46f)/sr);
      }
      oscillatorPhaseA+=hz/sr;
      if(oscillatorPhaseA>=1.f)oscillatorPhaseA-=1.f;
      oscillatorPhaseB+=1.61803398875f*hz/sr;
      if(oscillatorPhaseB>=1.f)oscillatorPhaseB-=1.f;
      const float pA=2.f*PI_F*oscillatorPhaseA,pB=2.f*PI_F*oscillatorPhaseB;
      float em=0.f,im=0.f;
      oL[i]=L.tick(xL,coeff,pA,pB,index,self,feedback,attackCoef,releaseCoef,
                   toneCoef,route,mix,em,im);
      oR[i]=R.tick(xR,coeff,pA,pB,index,self,feedback,attackCoef,releaseCoef,
                   toneCoef,route,mix,em,im);
      envMeter=undenorm(envMeter+.006f*(em-envMeter));
      indexMeter=undenorm(indexMeter+.012f*(im-indexMeter));
    }
  }
  void meters(float* out)const override{out[0]=envMeter;out[1]=indexMeter;}
};
} // namespace rig
