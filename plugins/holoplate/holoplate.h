// HOLOPLATE v2.1 — alternating-phase spatial plate / spectral memory delay + modal plate reverb. A 2D bending-mode plate
// imprinted with an interference mask sits in an energy-limited delay + diffuser.
// TIME 70–1100 ms, FEEDBACK 0–90%, DECAY .35–7s, DIFFUSE 0–100%,
// MORPH 0–100% (natural -> alternating plate phase + upward spectral ghosts),
// ANGLE 0–100%, DAMP, MIX. Low-frequency circulation is filtered in v2.1.
#pragma once
#include "module.h"

namespace rig {
inline constexpr ParamDesc HOLOPLATE_PARAMS[] = {
  {"time", "TIME", 70.f, 1100.f, .53f, "ms", Curve::Log, 0, false},
  {"feedback", "FEEDBACK", 0.f, 90.f, .42f, "%", Curve::Lin, 0, false},
  {"decay", "DECAY", .35f, 7.f, .64f, "s", Curve::Log, 2, false},
  {"diffuse", "DIFFUSE", 0.f, 100.f, .55f, "%", Curve::Lin, 0, false},
  {"morph", "MORPH", 0.f, 100.f, .0f, "%", Curve::Lin, 0, false},
  {"angle", "ANGLE", 0.f, 100.f, .44f, "%", Curve::Lin, 0, false},
  {"damp", "DAMP", 0.f, 100.f, .55f, "%", Curve::Lin, 0, false},
  {"mix", "MIX", 0.f, 100.f, .44f, "%", Curve::Lin, 0, false}
};
inline constexpr ModuleDesc HOLOPLATE_DESC = {
  "holoplate", "HOLOPLATE", "spectral reconstruction / memory delay",
  8, HOLOPLATE_PARAMS, 2, {"plate", "echo"}
};

// Each mode is an independently decaying component of a rectangular bending plate.
// Spatial projection and an interference mask establish excitation/pickup weights.
struct HoloMode {
  float y1=0.f, y2=0.f, a=0.f, b=0.f, injection=0.f, opposite=0.f, left=0.f, right=0.f;
};


// Real 1024-point STFT, 256-sample hop (512 above 70kHz). Fixed storage.
// Memory retains amplitudes and stable phase references of recently heard partials;
// reconstruction repositions bins and phase-rotates them according to MORPH/ANGLE.
// This isn't optical holography: the interference metaphor describes spectral
// encoding, spatially distinct reconstruction, and an acoustic afterimage.
struct HoloSpectrum {
  static constexpr int N=1024, H=256, O=2048;
  float ring[N]={}, win[N]={}, synthesisWindow[N]={}, sineTable[2048]={};
  float olaL[O]={}, olaR[O]={};
  float re[N]={}, im[N]={};
  float leftRe[N]={}, leftIm[N]={}, rightRe[N]={}, rightIm[N]={};
  float memory[N/2+1]={}, phaseRe[N/2+1]={}, phaseIm[N/2+1]={};
  int inputPos=0, readPos=0, hop=0, hopSize=256;
  unsigned frame=0;
  void init(float sr){
    hopSize=sr>=70000.f?512:256;
    for(int i=0;i<N;i++)win[i]=.5f-.5f*cosf(2.f*PI_F*i/N);
    for(int i=0;i<N;i++){
      const float denom=hopSize==256?1.5f:(win[i]*win[i]+win[(i+N/2)&(N-1)]*win[(i+N/2)&(N-1)]);
      synthesisWindow[i]=win[i]/fmaxf(.0001f,denom);
    }
    for(int i=0;i<2048;i++)sineTable[i]=sinf(2.f*PI_F*i/2048.f);
    clear();
  }
  void clear(){
    memset(ring,0,sizeof(ring));memset(olaL,0,sizeof(olaL));memset(olaR,0,sizeof(olaR));
    memset(re,0,sizeof(re));memset(im,0,sizeof(im));
    memset(leftRe,0,sizeof(leftRe));memset(leftIm,0,sizeof(leftIm));
    memset(rightRe,0,sizeof(rightRe));memset(rightIm,0,sizeof(rightIm));
    memset(memory,0,sizeof(memory));memset(phaseRe,0,sizeof(phaseRe));
    memset(phaseIm,0,sizeof(phaseIm));inputPos=readPos=hop=0;frame=0;
  }
  static void fft(float* r,float* j,bool invert){
    for(int i=1,t=0;i<N;i++){
      int bit=N>>1;for(;t&bit;bit>>=1)t^=bit;t^=bit;
      if(i<t){const float a=r[i],b=j[i];r[i]=r[t];j[i]=j[t];r[t]=a;j[t]=b;}
    }
    for(int len=2;len<=N;len<<=1){
      const float step=(invert?2.f:-2.f)*PI_F/len;
      const float cr=cosf(step),ci=sinf(step);
      for(int off=0;off<N;off+=len){
        float wr=1.f,wi=0.f;
        for(int k=0;k<len/2;k++){
          const int a=off+k,b=a+len/2;
          const float vr=r[b]*wr-j[b]*wi,vi=r[b]*wi+j[b]*wr;
          const float ur=r[a],ui=j[a];
          r[a]=ur+vr;j[a]=ui+vi;r[b]=ur-vr;j[b]=ui-vi;
          const float tr=wr*cr-wi*ci;wi=wr*ci+wi*cr;wr=tr;
        }
      }
    }
    if(invert)for(int i=0;i<N;i++){r[i]*=1.f/N;j[i]*=1.f/N;}
  }
  float fastSin(float rad)const{
    float turn=rad*(1.f/(2.f*PI_F));turn-=floorf(turn);
    const float pos=turn*2048.f;const int k=(int)pos;const float frac=pos-k;
    const float a=sineTable[k&2047],b=sineTable[(k+1)&2047];
    return a+frac*(b-a);
  }
  static void accumulate(float* rr,float* ii,int k,float ar,float ai){
    rr[k]+=ar;ii[k]+=ai;
  }
  void build(float sampleRate,float morph,float angle,float decay,float damp){
    for(int n=0;n<N;n++){
      const float v=ring[(inputPos+n)&(N-1)];
      re[n]=v*win[n];im[n]=0.f;
    }
    fft(re,im,false);
    memset(leftRe,0,sizeof(leftRe));memset(leftIm,0,sizeof(leftIm));
    memset(rightRe,0,sizeof(rightRe));memset(rightIm,0,sizeof(rightIm));
    const float strange=morph*morph;
    const float retain=expf(-hopSize/(sampleRate*(.25f+fminf(decay,7.f)*.28f)));
    const float rotation=angle*(2.f*PI_F);
    for(int k=1;k<N/2;k++){
      const float cr=re[k],ci=im[k],mag=sqrtf(cr*cr+ci*ci);
      const float previous=memory[k];
      // Peak-hold spectral memory, with controlled exponential energy loss.
      memory[k]=fmaxf(mag*.90f,previous*retain);
      // Phase reference is held in coordinates of the STFT's advancing window.
      const int q=(int)((frame*(unsigned)k*(unsigned)(hopSize/256))&3u);
      float pr=cr/(mag+1e-12f),pi=ci/(mag+1e-12f);
      if(q==1){const float t=pr;pr=pi;pi=-t;}
      else if(q==2){pr=-pr;pi=-pi;}
      else if(q==3){const float t=pr;pr=-pi;pi=t;}
      if(mag>fmaxf(.00004f,previous*.30f)){phaseRe[k]=pr;phaseIm[k]=pi;}
      float mr=phaseRe[k],mi=phaseIm[k];
      if(q==1){const float t=mr;mr=-mi;mi=t;}
      else if(q==2){mr=-mr;mi=-mi;}
      else if(q==3){const float t=mr;mr=mi;mi=-t;}
      const float persistence=.65f*strange;
      const float srPart=(1.f-persistence)*cr+persistence*memory[k]*mr;
      const float siPart=(1.f-persistence)*ci+persistence*memory[k]*mi;
      // A smooth passage from transparent reconstruction into intentionally
      // nonphysical frequency migration and phase-front rotation.
      const float warp=strange*((2.f+13.f*angle)*fastSin(k*.053f+rotation+frame*.035f)
                       +k*(.017f+.085f*angle)*fastSin(k*.0157f+rotation*.63f+frame*.019f));
      // The positive diffraction order places displaced partials above their
      // source bin instead of sweeping fundamentals toward DC in the loop.
      const float target=clampf(k+fabsf(warp),1.f,(float)(N/2-2));
      const int dest=(int)floorf(target);
      const float frac=target-dest;
      const float w0=fastSin((1.f-frac)*PI_F*.5f),w1=fastSin(frac*PI_F*.5f);
      const float phase=strange*(2.1f*fastSin(k*.067f+rotation+frame*.067f)
                        +1.05f*fastSin(k*.181f-rotation*.7f));
      const float pL=phase+.57f*strange*(angle-.5f);
      const float pR=-phase-.57f*strange*(angle-.5f);
      const float cosL=fastSin(pL+PI_F*.5f),sinL=fastSin(pL);
      const float cosR=fastSin(pR+PI_F*.5f),sinR=fastSin(pR);
      const float dry=1.f-.96f*strange;
      leftRe[k]+=dry*cr;leftIm[k]+=dry*ci;
      rightRe[k]+=dry*cr;rightIm[k]+=dry*ci;
      const float attenuation=1.f/(1.f+.9f*damp*(float)k/(N/2));
      const float gain=1.20f*strange*attenuation;
      const float lre=gain*(srPart*cosL-siPart*sinL),lim=gain*(srPart*sinL+siPart*cosL);
      const float rre=gain*(srPart*cosR-siPart*sinR),rim=gain*(srPart*sinR+siPart*cosR);
      accumulate(leftRe,leftIm,dest,w0*lre,w0*lim);
      accumulate(leftRe,leftIm,dest+1,w1*lre,w1*lim);
      accumulate(rightRe,rightIm,dest,w0*rre,w0*rim);
      accumulate(rightRe,rightIm,dest+1,w1*rre,w1*rim);
    }
    // Real output requires Hermitian symmetry in both synthesized spectra.
    leftRe[0]=leftIm[0]=rightRe[0]=rightIm[0]=0.f;
    leftRe[N/2]=leftIm[N/2]=rightRe[N/2]=rightIm[N/2]=0.f;
    for(int k=1;k<N/2;k++){
      leftRe[N-k]=leftRe[k];leftIm[N-k]=-leftIm[k];
      rightRe[N-k]=rightRe[k];rightIm[N-k]=-rightIm[k];
    }
    fft(leftRe,leftIm,true);fft(rightRe,rightIm,true);
    // Periodic Hann with 4x overlap => sum(window^2)=1.5.
    const int start=(readPos+1)&(O-1);
    for(int n=0;n<N;n++){
      const int dest=(start+n)&(O-1);
      const float scale=synthesisWindow[n];
      olaL[dest]=undenorm(olaL[dest]+leftRe[n]*scale);
      olaR[dest]=undenorm(olaR[dest]+rightRe[n]*scale);
    }
    ++frame;
  }
  void tick(float x,float sr,float morph,float angle,float decay,float damp,
            float& outL,float& outR){
    outL=olaL[readPos];outR=olaR[readPos];
    olaL[readPos]=olaR[readPos]=0.f;
    ring[inputPos]=x;
    inputPos=(inputPos+1)&(N-1);
    if(++hop>=hopSize){hop=0;build(sr,morph,angle,decay,damp);}
    readPos=(readPos+1)&(O-1);
  }
};

// Two cheap cascaded first-order high-passes, for energy circulating in the
// memory loop. Dry input and the first delay echo are never filtered here.
struct HoloHighPass {
  float low1=0.f,low2=0.f,alpha=.018f;
  void init(float sr,float hz){alpha=1.f-expf(-2.f*PI_F*hz/sr);clear();}
  void clear(){low1=low2=0.f;}
  float tick(float x){
    low1=undenorm(low1+alpha*(x-low1));
    const float high=x-low1;
    low2=undenorm(low2+alpha*(high-low2));
    return high-low2;
  }
};

struct HoloPlate final : Module {
  static constexpr int MODES = 36;
  DelayLine<131072> delayL, delayR;      // odd repeats: input -> A -> B -> A ...
  DelayLine<131072> delayBL, delayBR;     // even repeats: opposite plate excitation
  DelayLine<16384> network[4];
  Allpass diffuse1, diffuse2;
  HoloMode modal[MODES];
  HoloSpectrum spectrum;
  OnePole timeS, feedbackS, decayS, diffuseS, imprintS, angleS, dampS, mixS;
  float lp[4]={};
  float echoLevel=0.f, plateLevel=0.f, feedbackL=0.f, feedbackR=0.f;
  float repeatLpL=0.f, repeatLpR=0.f, repeatLpBL=0.f, repeatLpBR=0.f;
  float aEnergy=0.f,bEnergy=0.f;
  HoloHighPass returnHpL, returnHpR, returnHpBL, returnHpBR;
  HoloHighPass plateDriveHp,plateAlternateHp;
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
    spectrum.init(sr);
    returnHpL.init(sr,145.f);returnHpR.init(sr,145.f);
    returnHpBL.init(sr,145.f);returnHpBR.init(sr,145.f);
    plateDriveHp.init(sr,130.f);plateAlternateHp.init(sr,130.f);
    reset();
  }
  void reset() override {
    delayL.clear();delayR.clear();delayBL.clear();delayBR.clear();
    for(auto& x:network)x.clear();
    diffuse1.clear();diffuse2.clear();spectrum.clear();
    returnHpL.clear();returnHpR.clear();returnHpBL.clear();returnHpBR.clear();
    plateDriveHp.clear();plateAlternateHp.clear();
    for(auto& m:modal)m=HoloMode{};
    for(float& x:lp)x=0.f;
    echoLevel=plateLevel=feedbackL=feedbackR=repeatLpL=repeatLpR=repeatLpBL=repeatLpBR=0.f;
    aEnergy=bEnergy=0.f;counter=0;
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
      const float lowTilt=frequency*frequency/(frequency*frequency+155.f*155.f);
      const float gain=.0022f*(48000.f/sr)*(48000.f/sr)*lowTilt;
      mode.injection=gain*inputShape*(1.f+.87f*imprint*interference);
      // Opposing strike location and inverted bending-wave displacement on the
      // second/fourth/etc repeats. At MORPH zero both positions match exactly.
      const float farX=.72f+.08f*cosf(phase),farY=.29f+.11f*sinf(phase);
      const float farShape=sinf(mx*PI_F*farX)*sinf(ny*PI_F*farY);
      const float farInterference=cosf(mx*2.17f-ny*.93f-phase);
      const float flipped=-gain*farShape*(1.f+.87f*imprint*farInterference);
      mode.opposite=(1.f-imprint)*mode.injection+imprint*flipped;
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
    const float morphTarget=map(4,e[4])*.01f,angleTarget=map(5,e[5])*.01f;
    const float dampTarget=map(6,e[6])*.01f,mixTarget=map(7,e[7])*.01f;
    for(int s=0;s<count;s++){
      const float xL=inL[s],xR=inR[s];
      timeS.next(timeTarget);const float fb=feedbackS.next(fbTarget);
      const float decay=decayS.next(decayTarget),diff=diffuseS.next(diffuseTarget);
      const float morph=imprintS.next(morphTarget),angle=angleS.next(angleTarget);
      const float damp=dampS.next(dampTarget),mix=mixS.next(mixTarget);
      if((counter++&63u)==0u)configure(decay,morph,angle,damp);
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
      const float echoAL=delayL.tap(oldTap)*(1.f-interp)+delayL.tap(newTap)*interp;
      const float echoAR=delayR.tap(oldTap)*(1.f-interp)+delayR.tap(newTap)*interp;
      const float echoBL=delayBL.tap(oldTap)*(1.f-interp)+delayBL.tap(newTap)*interp;
      const float echoBR=delayBR.tap(oldTap)*(1.f-interp)+delayBR.tap(newTap)*interp;
      const float echoL=echoAL+echoBL,echoR=echoAR+echoBR;
      delayXfade=fminf(1.f,delayXfade+1.f/(.055f*sr));
      const float monoInput=.5f*(xL+xR),monoEcho=.5f*(echoL+echoR);
      const float monoA=.5f*(echoAL+echoAR),monoB=.5f*(echoBL+echoBR);
      aEnergy=undenorm(aEnergy+.006f*(fabsf(monoA)-aEnergy));
      bEnergy=undenorm(bEnergy+.006f*(fabsf(monoB)-bEnergy));
      const float shareA=(aEnergy+.00001f)/(aEnergy+bEnergy+.00002f);
      const float shareB=1.f-shareA;
      // Spectral memory is excited by the actual echoes: the first repeat is
      // recognizable; later passes are progressively reconstructed incorrectly.
      float ghostL=0.f,ghostR=0.f;
      if(morph>.002f)spectrum.tick(monoEcho,sr,morph,angle,decay,damp,ghostL,ghostR);
      const float ghostMix=morph*morph;
      const float ghostMono=.5f*(ghostL+ghostR);
      const float excitation=plateDriveHp.tick(.45f*monoInput+.60f*((1.f-ghostMix)*monoEcho+ghostMix*ghostMono));
      const float altExcitation=plateAlternateHp.tick(.60f*((1.f-ghostMix)*monoB+ghostMix*ghostMono*shareB));
      const float firstExcitation=excitation-altExcitation;
      float modalL=0.f,modalR=0.f;
      // Adjacent modes are weakly coupled through the hologram. The tiny amount
      // is bounded by damping and produces evolving frequency-selective memory.
      for(int i=0;i<MODES;i++){
        HoloMode& m=modal[i];
        const float neighbor=modal[(i+5)%MODES].y1;
        const float coupling=.00004f*morph*(neighbor-m.y1);
        const float y=clampf(m.a*m.y1-m.b*m.y2+m.injection*firstExcitation+
                             m.opposite*altExcitation+coupling,-2.f,2.f);
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
      const float revL=clampf(modalL*1.40f+.75f*(d[0]+d[2]-d[1]-d[3])+.35f*ghostMix*ghostL,-3.f,3.f);
      const float revR=clampf(modalR*1.40f+.75f*(d[0]-d[2]+d[1]-d[3])+.35f*ghostMix*ghostR,-3.f,3.f);
      // Persistent plate energy returns to the delay only through a bounded tap.
      const float recycle=.22f*diff*morph;
      feedbackL=undenorm(.998f*feedbackL+.002f*revL);
      feedbackR=undenorm(.998f*feedbackR+.002f*revR);
      const float echoAlpha=.15f+.78f*(1.f-damp);
      repeatLpL=undenorm(repeatLpL+echoAlpha*(echoAL-repeatLpL));
      repeatLpR=undenorm(repeatLpR+echoAlpha*(echoAR-repeatLpR));
      repeatLpBL=undenorm(repeatLpBL+echoAlpha*(echoBL-repeatLpBL));
      repeatLpBR=undenorm(repeatLpBR+echoAlpha*(echoBR-repeatLpBR));
      const float directReturn=1.f-.75f*ghostMix;
      const float ghostA=.75f*ghostMix*shareA,ghostB=.75f*ghostMix*shareB;
      const float transformedAL=directReturn*repeatLpL+ghostA*ghostL;
      const float transformedAR=directReturn*repeatLpR+ghostA*ghostR;
      const float transformedBL=directReturn*repeatLpBL+ghostB*ghostL;
      const float transformedBR=directReturn*repeatLpBR+ghostB*ghostR;
      const float loopGain=fb*(1.f-.18f*ghostMix);
      // Cross feed only: an A repeat creates B on the next lap and vice versa.
      // The opposite plate position is driven by the actual B delay buffers.
      delayL.push(clampf(xL+loopGain*returnHpL.tick((1.f-recycle)*transformedBL+.5f*recycle*feedbackL),-3.f,3.f));
      delayR.push(clampf(xR+loopGain*returnHpR.tick((1.f-recycle)*transformedBR+.5f*recycle*feedbackR),-3.f,3.f));
      delayBL.push(clampf(loopGain*returnHpBL.tick((1.f-recycle)*transformedAL+.5f*recycle*feedbackL),-3.f,3.f));
      delayBR.push(clampf(loopGain*returnHpBR.tick((1.f-recycle)*transformedAR+.5f*recycle*feedbackR),-3.f,3.f));
      const float wetL=(1.f-diff)*(echoL+4.0f*ghostMix*shareB*ghostL)+diff*revL;
      const float wetR=(1.f-diff)*(echoR+4.0f*ghostMix*shareB*ghostR)+diff*revR;
      outL[s]=clampf(xL*(1.f-mix)+wetL*mix,-4.f,4.f);
      outR[s]=clampf(xR*(1.f-mix)+wetR*mix,-4.f,4.f);
      echoLevel=undenorm(echoLevel+.001f*(clampf(fmaxf(fabsf(echoL),fabsf(echoR)),0.f,1.f)-echoLevel));
      plateLevel=undenorm(plateLevel+.001f*(clampf(fmaxf(fabsf(revL),fabsf(revR)),0.f,1.f)-plateLevel));
    }
  }
  void meters(float* out) const override {out[0]=plateLevel;out[1]=echoLevel;}
};
} // namespace rig