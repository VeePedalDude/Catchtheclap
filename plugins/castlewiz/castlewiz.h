// CASTLE ALMOST WIZARD: nine patch-inspired digital effects for RIG/01.
// Adapted FX mode concepts and sequencer patterns: Bastl Instruments Kastle 2 FX Wizard.
// Copyright (c) 2024 Marek Mach and Vaclav Mach (Bastl Instruments)
// Copyright (c) 2023-2025 Bastl Instruments
// MIT License: permission is granted, free of charge, to use, copy, modify,
// merge, publish, distribute, sublicense, and/or sell copies of this software,
// provided this copyright and permission notice is included in all copies or
// substantial portions. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
// ANY KIND, EXPRESS OR IMPLIED, INCLUDING MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE, OR NONINFRINGEMENT. AUTHORS SHALL NOT BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY ARISING FROM ITS USE OR DISTRIBUTION.
// This is a new host DSP implementation, not a bit-identical firmware port.
// MODE 0..8, TIME 20..1600 ms, FEEDBACK 0..85%, AMOUNT 0..100%, FILTER LP<->HP,
// STEREO 0..100%, SYNC FREE/quarter/eighth/dotted-eighth/sixteenth, TRIGGER OFF/ON.
#pragma once
#include "module.h"
#include <stdint.h>

namespace rig {
inline constexpr ParamStop CASTLE_MODES[] = {
  {0.f,"DELAY"},{1.f,"FLANGER"},{2.f,"FREEZER"},{3.f,"PANNER"},{4.f,"CRUSHER"},
  {5.f,"SLICER"},{6.f,"PITCHER"},{7.f,"REPLAYER"},{8.f,"SHIFTER"}
};
inline constexpr ParamStop CASTLE_SYNCS[] = {
  {0.f,"FREE"},{1.f,"1/4"},{2.f,"1/8"},{3.f,"D8"},{4.f,"1/16"}
};
inline constexpr ParamStop CASTLE_TRIGGERS[] = {{0.f,"OFF"},{1.f,"ON"}};
inline constexpr ParamDesc CASTLE_PARAMS[] = {
  {"mode","MODE",0.f,8.f,0.f,"",Curve::Lin,0,false,CASTLE_MODES,9},
  {"time","TIME",20.f,1600.f,0.5269f,"ms",Curve::Log,0,false},
  {"feedback","FEEDBACK",0.f,85.f,.28f,"%",Curve::Lin,0,false},
  {"amount","AMOUNT",0.f,100.f,.55f,"%",Curve::Lin,0,false},
  {"filter","FILTER",-100.f,100.f,.5f,"%",Curve::Lin,0,true},
  {"stereo","STEREO",0.f,100.f,.35f,"%",Curve::Lin,0,false},
  {"sync","SYNC",0.f,4.f,0.f,"",Curve::Lin,0,false,CASTLE_SYNCS,5},
  {"trigger","TRIGGER",0.f,1.f,0.f,"",Curve::Lin,0,false,CASTLE_TRIGGERS,2}
};
inline constexpr ModuleDesc CASTLE_DESC={"castlewiz","CASTLE ALMOST WIZARD","9 patch-inspired effects",8,CASTLE_PARAMS,2,{"activity","held"}};

// Ring buffer is downsampled to 16 kHz for a deliberate portable-digital sound.
struct CastleRing {
  static constexpr int LEN=32768, MASK=LEN-1;
  float l[LEN], r[LEN]; int w=0;
  void clear(){memset(l,0,sizeof l);memset(r,0,sizeof r);w=0;}
  void push(float a,float b){l[w]=a;r[w]=b;w=(w+1)&MASK;}
  float readL(int d)const{return l[(w-d)&MASK];}
  float readR(int d)const{return r[(w-d)&MASK];}
  float readLF(float d)const{int i=(int)d;float q=d-(float)i;return readL(i)*(1.f-q)+readL(i+1)*q;}
  float readRF(float d)const{int i=(int)d;float q=d-(float)i;return readR(i)*(1.f-q)+readR(i+1)*q;}
};

struct CastleAlmostWizard : Module {
  static constexpr float RATE=16000.f;
  CastleRing ring;
  OnePole timeS, feedbackS, amountS, filterS, stereoS, mixS;
  float phase=0.f, sumL=0.f,sumR=0.f, wetL=0.f,wetR=0.f, activity=0.f;
  int summed=0;
  float lfo=0.f, seq=0.f, crushPhaseL=0.f,crushPhaseR=0.f;
  float crushL=0.f,crushR=0.f, filterLowL=0.f,filterLowR=0.f;
  float grain=0.f,replay=0.f, shifter=0.f, feedbackL=0.f,feedbackR=0.f;
  float frozenAt=0.f, frozenPhase=0.f;
  int freezeLength=1600;
  bool frozen=false,triggerPrev=false;
  int lastMode=-1;
  unsigned seed=0x7f4a7c15u;
  float heldMeter=0.f;
  const ModuleDesc& desc()const override{return CASTLE_DESC;}
  void init(float s)override{
    sr=s;timeS.setMs(RATE,35);feedbackS.setMs(RATE,25);amountS.setMs(RATE,25);
    filterS.setMs(RATE,25);stereoS.setMs(RATE,25);mixS.setMs(sr,20);reset();
  }
  void reset()override{
    ring.clear();phase=sumL=sumR=wetL=wetR=activity=lfo=seq=0.f;summed=0;
    crushPhaseL=crushPhaseR=crushL=crushR=filterLowL=filterLowR=0.f;
    grain=replay=shifter=feedbackL=feedbackR=frozenAt=frozenPhase=0.f;
    freezeLength=1600;frozen=triggerPrev=false;lastMode=-1;heldMeter=0.f;
    seed=0x7f4a7c15u;
    timeS.y=map(1,CASTLE_PARAMS[1].def);feedbackS.y=map(2,CASTLE_PARAMS[2].def)*.01f;
    amountS.y=.55f;filterS.y=0.f;stereoS.y=.35f;mixS.y=.55f;
  }
  static float tri(float p){return 1.f-4.f*fabsf(p-.5f);}
  static float saturate(float x){return x/(1.f+fabsf(x)*.25f);}
  static float smoothBlend(float a,float b,float amount){return a+(b-a)*amount;}
  int boundedTap(float d)const{return (int)clampf(d,2.f,32765.f);}
  float modeTime(float time,int sync,float bpm)const{
    if(sync==0)return time;
    const float beat=60000.f/clampf(bpm,30.f,300.f);
    const float divisors[4]={1.f,.5f,.75f,.25f};
    return clampf(beat*divisors[sync-1]*time/200.f,20.f,1600.f);
  }
  void tick(float inL,float inR,int mode,float time,float feedback,float amount,
            float filter,float stereo,int sync,bool trigger,float bpm){
    if(mode!=lastMode){ // independent mode state, same ring can preserve incoming history
      lastMode=mode;feedbackL=feedbackR=0.f;grain=replay=shifter=0.f;
      frozen=false;frozenPhase=0.f;crushPhaseL=crushPhaseR=0.f;
      lfo=seq=0.f;triggerPrev=false;
    }
    const bool rise=trigger&&!triggerPrev;triggerPrev=trigger;
    const float t=modeTime(timeS.next(time),sync,bpm);
    const float fb=feedbackS.next(feedback), amt=amountS.next(amount);
    const float stereoAmt=stereoS.next(stereo);
    const float tone=filterS.next(filter);
    const int samples=boundedTap(t*RATE*.001f);
    if(rise && (mode==2 || mode==7)){
      frozen=true;frozenAt=(float)ring.w;frozenPhase=0.f;replay=0.f;
      freezeLength=boundedTap(t*RATE*.001f);
    }
    if(!trigger&&(mode==2||mode==7))frozen=false;
    const float dt=1.f/RATE;
    float processedL=inL,processedR=inR;
    const float clockPeriod=sync ? modeTime(200.f,sync,bpm)*.001f : clampf(t*.001f,.02f,1.6f);
    switch(mode){
    case 0:{ // DELAY: stereo-tuned tap and filtered feedback
      const int dL=samples,dR=boundedTap((float)samples*(1.f+.08f*stereoAmt));
      processedL=ring.readL(dL);processedR=ring.readR(dR);
      ring.push(clampf(inL+processedL*fb,-1.6f,1.6f),clampf(inR+processedR*fb,-1.6f,1.6f));
      break;
    }
    case 1:{ // FLANGER: short fractional sweep, stereo dephasing
      const float hz=clampf(1000.f/t*.16f,.08f,5.f);
      lfo+=hz*dt;if(lfo>=1.f)lfo-=1.f;
      const float dl=9.f+93.f*(.5f+.5f*tri(lfo));
      const float dr=9.f+93.f*(.5f+.5f*tri(lfo+stereoAmt*.25f>1.f?lfo+stereoAmt*.25f-1.f:lfo+stereoAmt*.25f));
      processedL=ring.readLF(dl);processedR=ring.readRF(dr);
      ring.push(clampf(inL+processedL*fb*.85f,-1.6f,1.6f),clampf(inR+processedR*fb*.85f,-1.6f,1.6f));
      // mixed comb sound, positive/negative feedback character
      processedL=smoothBlend(inL,processedL,.75f);processedR=smoothBlend(inR,processedR,.75f);
      break;
    }
    case 2:{ // FREEZER: capture/repeat previously recorded buffer, no heap
      if(!frozen){ring.push(inL,inR);processedL=inL;processedR=inR;}
      else{
        const int length=freezeLength;
        const float speed=.75f+.5f*stereoAmt;
        frozenPhase+=speed;if(frozenPhase>=length)frozenPhase-=length;
        const int pos=(int)frozenPhase;
        const int ix=((int)frozenAt-length+pos)&CastleRing::MASK;
        const int jx=((int)frozenAt-length+((pos+length/2)%length))&CastleRing::MASK;
        const float fade=fminf(1.f,fminf((float)pos,(float)(length-pos))*0.01f);
        processedL=ring.l[ix]*fade+ring.l[((int)frozenAt-length)&CastleRing::MASK]*(1.f-fade);
        processedR=ring.r[ix]*fade+ring.r[jx]*(1.f-fade)*fb;
      }
      break;
    }
    case 3:{ // PANNER: out-of-phase amplitude modulation
      const float hz=1.f/(2.f*clockPeriod);
      lfo+=hz*dt;if(lfo>=1.f)lfo-=1.f;
      const float wave=sinf(2.f*PI_F*lfo);
      const float depth=clampf(amt*.95f+fb*.05f,0.f,1.f);
      processedL=inL*(1.f-depth*(.5f+.5f*wave));
      processedR=inR*(1.f-depth*(.5f-.5f*wave)*(stereoAmt*.8f+.2f));
      break;
    }
    case 4:{ // CRUSHER: sample-rate / bit resolution reduction, stereo divergence
      const float rate=clampf(16000.f/(1.f+t*.17f),25.f,RATE);
      const int bits=4+(int)(12.f*(1.f-amt)+.5f);
      const float scale=(float)(1<<(bits-1));
      crushPhaseL+=rate*dt;crushPhaseR+=rate*(1.f+.25f*stereoAmt)*dt;
      if(crushPhaseL>=1.f){crushPhaseL-=floorf(crushPhaseL);crushL=floorf(inL*scale+.5f)/scale;}
      if(crushPhaseR>=1.f){crushPhaseR-=floorf(crushPhaseR);crushR=floorf(inR*scale+.5f)/scale;}
      processedL=crushL;processedR=crushR;
      // a small amount of regenerative digital grit
      processedL=fb>.0001f?saturate(processedL+feedbackL*fb*.15f):crushL;
      processedR=fb>.0001f?saturate(processedR+feedbackR*fb*.15f):crushR;
      break;
    }
    case 5:{ // SLICER: 8-step pattern with alternate stereo and probability of glitches
      const uint8_t patterns[8]={0x80,0x88,0x22,0x84,0x92,0xAA,0xAC,0xFF}; // Bastl source pattern table
      const float stepSec=clockPeriod*.5f;
      seq+=dt/stepSec;
      if(seq>=8.f)seq-=8.f;
      const int step=(int)seq&7;
      int pattern=t<35.f?0:t<60.f?1:t<100.f?2:t<160.f?3:t<250.f?4:t<400.f?5:t<650.f?6:7;
      const int b=(patterns[pattern]>>(7-step))&1;
      const int alt=(patterns[(pattern+3)&7]>>(7-step))&1;
      const float envelope=clampf((seq-floorf(seq))*12.f,0.f,1.f);
      const float gateL=b?envelope:0.f;
      const float gateR=stereoAmt>.5f?(alt?envelope:0.f):gateL;
      processedL=inL*gateL;processedR=inR*gateR;
      if(fb>0.f){processedL+=ring.readL(200)*fb*.28f;processedR+=ring.readR(200)*fb*.28f;}
      ring.push(clampf(processedL,-1.6f,1.6f),clampf(processedR,-1.6f,1.6f));
      break;
    }
    case 6:{ // PITCHER: granular dual-head swept-delay pitch translation
      ring.push(inL,inR);
      const float rate=(t<200.f)?1.5f:((t>650.f)?.7f:1.23f); // ratio fixed bands avoid jitter
      const float span=clampf(130.f+amt*1500.f,130.f,1800.f);
      grain+=1.f/span;if(grain>=1.f)grain-=1.f;
      const float p=grain, q=p+.5f>=1.f?p-.5f:p+.5f;
      const float read1=20.f+span*p*(1.f-rate)+span;
      const float read2=20.f+span*q*(1.f-rate)+span;
      const float win1=1.f-fabsf(2.f*p-1.f),win2=1.f-fabsf(2.f*q-1.f);
      const float norm=1.f/(win1+win2+1e-4f);
      processedL=(ring.readLF(clampf(read1,2.f,32000.f))*win1+ring.readLF(clampf(read2,2.f,32000.f))*win2)*norm;
      processedR=(ring.readRF(clampf(read1*(1.f+.02f*stereoAmt),2.f,32000.f))*win1+
                  ring.readRF(clampf(read2*(1.f+.02f*stereoAmt),2.f,32000.f))*win2)*norm;
      break;
    }
    case 7:{ // REPLAYER: reverse/forward tape-head from captured history
      if(!frozen)ring.push(inL,inR);
      const float pos=clampf(replay,0.f,(float)freezeLength-1.f);
      const int length=freezeLength;
      const float direction=time<350.f?-1.f:1.f;
      const float speed=(time<350.f)?(.5f+(350.f-time)/350.f):clampf(time/700.f,.4f,2.f);
      const int ix=((int)(frozen?frozenAt:(float)ring.w)-length+(int)pos)&CastleRing::MASK;
      processedL=ring.l[ix];processedR=ring.r[ix];
      replay+=direction*speed;
      if(replay<0.f)replay+=(float)length;
      if(replay>=(float)length)replay-=(float)length;
      if(!frozen){ // feedback controls how strongly the live recording is retained
        processedL=smoothBlend(inL,processedL,clampf(amt+.25f*fb,0.f,1.f));
        processedR=smoothBlend(inR,processedR,clampf(amt+.25f*fb,0.f,1.f));
      }
      break;
    }
    default:{ // SHIFTER: rapid swept delay with variable direction (digital pitch warble)
      ring.push(inL,inR);
      const float hz=clampf(400.f/t,.3f,16.f);
      shifter+=hz*dt;if(shifter>=1.f)shifter-=1.f;
      const float p=time<250.f?1.f-shifter:shifter;
      const float d=8.f+amt*200.f*p;
      processedL=ring.readLF(d);
      processedR=ring.readRF(d*(1.f+.08f*stereoAmt));
      const float win=clampf(fminf(shifter,1.f-shifter)*40.f,0.f,1.f);
      processedL*=win;processedR*=win;
      break;
    }
    }
    // Tone is a shared DJ-style lowpass / highpass. Modulation remains block-rate cheap.
    const float cut=(tone<0.f)?(8000.f*(1.f+tone)+160.f):(160.f+5000.f*tone);
    const float a=clampf(2.f*PI_F*cut/RATE,.01f,1.f);
    filterLowL=undenorm(filterLowL+a*(processedL-filterLowL));
    filterLowR=undenorm(filterLowR+a*(processedR-filterLowR));
    if(tone<-.025f){processedL=filterLowL;processedR=filterLowR;}
    if(tone>.025f){processedL-=filterLowL;processedR-=filterLowR;}
    feedbackL=undenorm(clampf(processedL,-2.f,2.f));
    feedbackR=undenorm(clampf(processedR,-2.f,2.f));
    wetL=clampf(processedL,-2.f,2.f);wetR=clampf(processedR,-2.f,2.f);
    activity=activity*.98f+.02f*fminf(1.f,(fabsf(wetL)+fabsf(wetR))*.5f);
    heldMeter=frozen?1.f:0.f;
  }
  void process(const float* inL,const float* inR,float* outL,float* outR,
               int n,const float* e,const Context& ctx)override{
    const int mode=(int)clampf(floorf(map(0,e[0])+.5f),0.f,8.f);
    const float time=map(1,e[1]),feedback=map(2,e[2])*.01f,amount=map(3,e[3])*.01f;
    const float filter=map(4,e[4])*.01f,stereo=map(5,e[5])*.01f;
    const int sync=(int)clampf(floorf(map(6,e[6])+.5f),0.f,4.f);
    const bool trigger=map(7,e[7])>=.5f;
    const float step=RATE/sr;
    for(int i=0;i<n;i++){
      const float l=inL[i],r=inR[i]; // buffers may alias
      sumL+=l;sumR+=r;summed++;
      phase+=step;
      if(phase>=1.f){phase-=1.f;
        tick(sumL/(float)summed,sumR/(float)summed,mode,time,feedback,amount,
             filter,stereo,sync,trigger,ctx.bpm);
        sumL=sumR=0.f;summed=0;
      }
      const float amt=mixS.next(amount);
      const float effectiveMix=(mode==3||mode==5)?1.f:amt;
      const float wetMix=(mode==3||mode==5)?amt:effectiveMix;
      outL[i]=clampf(l*(1.f-wetMix)+wetL*wetMix,-3.f,3.f);
      outR[i]=clampf(r*(1.f-wetMix)+wetR*wetMix,-3.f,3.f);
    }
  }
  void meters(float* out) const override{out[0]=activity;out[1]=heldMeter;}
};
} // namespace rig
