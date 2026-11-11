// CLOCKWITCH 8950: HT8950-inspired 8-bit voice shifter with independently abused ADC/DAC clocks.
// VOICE DN3/DN2/DN1/NORM/UP1/UP2/UP3/ROBOT, CLOCK 1..24kHz, SKEW .5..2x,
// VIB SPEED .05..40Hz, VIB DEPTH 0..60%, VIB SHAPE six waves, INSTABILITY 0..100%, MIX 0..100%.
// Original implementation inspired by the Holtek HT8950 specifications; not a bit-exact chip model.
#pragma once
#include "module.h"
#include <stdint.h>

namespace rig {

inline constexpr ParamStop CLOCKWITCH_VOICES[] = {
  {0.f,"DN3 2/3"},{1.f,"DN2 4/5"},{2.f,"DN1 8/9"},{3.f,"NORMAL"},
  {4.f,"UP1 4/3"},{5.f,"UP2 8/5"},{6.f,"UP3 2"},{7.f,"ROBOT"}
};
inline constexpr ParamStop CLOCKWITCH_SHAPES[] = {
  {0.f,"SINE"},{1.f,"TRI"},{2.f,"SQUARE"},{3.f,"RAMP"},
  {4.f,"STEPS"},{5.f,"RANDOM"}
};
inline constexpr ParamDesc CLOCKWITCH_PARAMS[] = {
  {"voice", "VOICE", 0.f, 7.f, 3.f/7.f, "", Curve::Lin, 0, false, CLOCKWITCH_VOICES, 8},
  {"clock", "CLOCK", 1000.f, 24000.f, .6543129f, "Hz", Curve::Log, 0, false},
  {"skew", "SKEW", .5f, 2.f, 1.f/3.f, "x", Curve::Lin, 2, false},
  {"speed", "VIB SPEED", .05f, 40.f, .75939f, "Hz", Curve::Log, 2, false},
  {"depth", "VIB DEPTH", 0.f, 60.f, .20f, "%", Curve::Lin, 0, false},
  {"shape", "VIB SHAPE", 0.f, 5.f, 0.f, "", Curve::Lin, 0, false, CLOCKWITCH_SHAPES, 6},
  {"unstable", "INSTABILITY", 0.f, 100.f, 0.f, "%", Curve::Lin, 0, false},
  {"mix", "MIX", 0.f, 100.f, .8f, "%", Curve::Lin, 0, false}
};
inline constexpr ModuleDesc CLOCKWITCH_DESC = {
  "clockwitch", "CLOCKWITCH 8950", "8-bit dual clock voice modulator", 8, CLOCKWITCH_PARAMS,
  2, {"adc_khz", "dac_khz"}
};

struct Clockwitch8950 : Module {
  static constexpr int SIZE=16384, MASK=SIZE-1;
  int8_t captureL[SIZE],captureR[SIZE];
  int write=0;
  float adcPhase=0.f,grainPhase=0.f,vibPhase=0.f,robotPhase=0.f;
  float vibrHold=0.f,writeJitter=0.f,readJitter=0.f,lowRandom=0.f;
  float adcKHz=8.f,dacKHz=8.f;
  uint32_t rng=0x46E5D1B7u;
  uint64_t captured=0;
  OnePole clockS,skewS,speedS,depthS,unstableS,mixS,ratioS;

  const ModuleDesc& desc() const override {return CLOCKWITCH_DESC;}
  void init(float s) override {
    sr=s;
    clockS.setMs(sr,35);skewS.setMs(sr,35);speedS.setMs(sr,30);
    depthS.setMs(sr,20);unstableS.setMs(sr,30);mixS.setMs(sr,20);
    ratioS.setMs(sr,22);
    reset();
  }
  void reset() override {
    memset(captureL,0,sizeof captureL);memset(captureR,0,sizeof captureR);
    write=0;adcPhase=grainPhase=vibPhase=robotPhase=0.f;
    vibrHold=writeJitter=readJitter=lowRandom=0.f;
    adcKHz=dacKHz=8.f;captured=0;rng=0x46E5D1B7u;
    clockS.y=map(1,CLOCKWITCH_PARAMS[1].def);
    skewS.y=map(2,CLOCKWITCH_PARAMS[2].def);
    speedS.y=map(3,CLOCKWITCH_PARAMS[3].def);
    depthS.y=map(4,CLOCKWITCH_PARAMS[4].def)*.01f;
    unstableS.y=map(6,CLOCKWITCH_PARAMS[6].def)*.01f;
    mixS.y=map(7,CLOCKWITCH_PARAMS[7].def)*.01f;
    ratioS.y=1.f;
  }
  float noise() { // Deterministic per-instance PRNG. No global state or allocations.
    rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
    return ((float)(rng&0x00ffffffu)*(1.f/8388608.f))-1.f;
  }
  static int8_t quantize8(float x) {
    const float s=clampf(x,-1.f,1.f)*127.f;
    return (int8_t)(int)floorf(s+.5f);
  }
  float tapL(float d) const {return (float)captureL[(write-1-(int)d)&MASK]*(1.f/127.f);}
  float tapR(float d) const {return (float)captureR[(write-1-(int)d)&MASK]*(1.f/127.f);}
  static float tri(float p) {return 1.f-4.f*fabsf(p-.5f);}
  static float voiceRatio(int mode) {
    constexpr float r[7]={2.f/3.f,4.f/5.f,8.f/9.f,1.f,4.f/3.f,8.f/5.f,2.f};
    return r[mode>=0&&mode<7?mode:3];
  }
  void process(const float* inL,const float* inR,float* oL,float* oR,
               int n,const float* e,const Context&) override {
    const int mode=(int)clampf(floorf(map(0,e[0])+.5f),0.f,7.f);
    const int shape=(int)clampf(floorf(map(5,e[5])+.5f),0.f,5.f);
    const float clockTarget=map(1,e[1]);
    const float skewTarget=map(2,e[2]);
    const float speedTarget=map(3,e[3]);
    const float depthTarget=map(4,e[4])*.01f;
    const float unstableTarget=map(6,e[6])*.01f;
    const float mixTarget=map(7,e[7])*.01f;
    const float base=voiceRatio(mode);
    for(int i=0;i<n;i++) {
      const float l=inL[i],r=inR[i]; // In-place processing is allowed.
      const float clk=clockS.next(clockTarget);
      const float skew=skewS.next(skewTarget);
      const float speed=speedS.next(speedTarget);
      const float depth=depthS.next(depthTarget);
      const float instability=unstableS.next(unstableTarget);
      const float mix=mixS.next(mixTarget);
      vibPhase+=speed/sr;
      if(vibPhase>=1.f) {vibPhase-=floorf(vibPhase);lowRandom=noise();}
      float vib=0.f;
      switch(shape) {
        case 0:vib=sinf(2.f*PI_F*vibPhase);break;
        case 1:vib=tri(vibPhase);break;
        case 2:vib=vibPhase<.5f?1.f:-1.f;break;
        case 3:vib=vibPhase*2.f-1.f;break;
        case 4:vib=floorf(vibPhase*8.f)/3.5f-1.f;break;
        default:vib=lowRandom;break;
      }
      // Sampling clock: a real 8-bit sample & hold, not merely a downsample post-filter.
      // Instability alters the capture edge timing; severe settings occasionally repeat codes.
      const float adcHz=clampf(clk*(1.f+writeJitter*instability*.43f),60.f,sr*.95f);
      adcPhase+=adcHz/sr;
      if(adcPhase>=1.f) {
        adcPhase-=1.f;
        const float nextWrite=noise();
        writeJitter+=.12f*(nextWrite-writeJitter);
        readJitter+=.06f*(noise()-readJitter);
        const bool slip=instability>.6f && nextWrite>.98f-(instability-.6f)*.25f;
        const float a=slip?tapL(1.f):l,b=slip?tapR(1.f):r;
        captureL[write]=quantize8(a);
        captureR[write]=quantize8(b);
        write=(write+1)&MASK;
        captured++;
      }
      // A second clock addresses the sample memory at a different speed. The two
      // staggered heads crossfade their unavoidable wrap discontinuities.
      const float targetRatio=clampf(base*skew*(1.f+vib*depth+readJitter*instability*.30f),.12f,4.5f);
      const float ratio=ratioS.next(targetRatio);
      const float win=clampf(clk*.060f,80.f,1800.f);
      const float center=17.f+.5f*fabsf(1.f-ratio)*win;
      grainPhase+=1.f/(sr*.060f);
      if(grainPhase>=1.f)grainPhase-=1.f;
      const float p=grainPhase,q=p<.5f?p+.5f:p-.5f;
      const float delayA=clampf(center+(1.f-ratio)*(p-.5f)*win,2.f,(float)SIZE-3.f);
      const float delayB=clampf(center+(1.f-ratio)*(q-.5f)*win,2.f,(float)SIZE-3.f);
      const float wa=1.f-fabsf(2.f*p-1.f),wb=1.f-wa;
      float wetL=wa*tapL(delayA)+wb*tapL(delayB);
      float wetR=wa*tapR(delayA)+wb*tapR(delayB);
      if(mode==7){ // HT8950-inspired robotic amplitude cadence, intentionally rough.
        robotPhase+=82.f/sr;
        if(robotPhase>=1.f)robotPhase-=1.f;
        const float gate=robotPhase<.5f?1.f:.19f;
        wetL*=gate;wetR*=gate;
      }
      adcKHz=adcHz*.001f;
      dacKHz=adcKHz*ratio;
      oL[i]=clampf(l*(1.f-mix)+wetL*mix,-2.f,2.f);
      oR[i]=clampf(r*(1.f-mix)+wetR*mix,-2.f,2.f);
    }
  }
  void meters(float* out)const override {out[0]=adcKHz;out[1]=dacKHz;}
};
} // namespace rig
