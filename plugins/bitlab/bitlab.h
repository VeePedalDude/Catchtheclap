// BITLAB: four independent digital degradation textures, eight controls, MIX 100%.
// Crunch/Lo-Fi/Glitch default off; Dust default off; MIX defaults fully wet.
#pragma once
#include "module.h"
#include <cstdint>
#include <cmath>
namespace rig {
inline constexpr ParamStop BL_CR[]={{0,"ROUND"},{1,"TRUNC"},{2,"COMPAND"}};
inline constexpr ParamStop BL_LO[]={{0,"HOLD"},{1,"SMOOTH"},{2,"STEP"}};
inline constexpr ParamStop BL_GL[]={{0,"JITTER"},{1,"SKIP"},{2,"REPEAT"}};
inline constexpr ParamStop BL_DU[]={{0,"OFF"},{1,"DITHER"},{2,"MASK"},{3,"DEADZONE"}};
inline constexpr ParamDesc BL_PARAMS[]={
 {"crunch_method","CR METHOD",0,2,0,"",Curve::Lin,0,false,BL_CR,3},
 {"crunch_amount","CRUNCH",0,100,0,"%",Curve::Lin,0,false},
 {"lofi_method","LO METHOD",0,2,0,"",Curve::Lin,0,false,BL_LO,3},
 {"lofi_amount","LO-FI",0,100,0,"%",Curve::Lin,0,false},
 {"glitch_method","GL METHOD",0,2,0,"",Curve::Lin,0,false,BL_GL,3},
 {"glitch_amount","GLITCH",0,100,0,"%",Curve::Lin,0,false},
 {"dust","DUST",0,4,0,"",Curve::Lin,2,false,BL_DU,4},
 {"mix","MIX",0,100,1,"%",Curve::Lin,0,false}};
inline constexpr ModuleDesc BL_DESC={"bitlab","BITLAB","four digital textures",8,BL_PARAMS,2,{"clock","activity"}};
struct Bitlab : Module {
 float phase=1.f,held[2]={},smooth[2]={},repeat[2]={},activity=0,clock=1;
 int repeatLeft=0; uint32_t rng=0xA341316Cu; OnePole mixS,crS,loS,glS,dustS;
 const ModuleDesc& desc()const override{return BL_DESC;}
 void init(float s)override{sr=s;mixS.setMs(sr,20);crS.setMs(sr,20);loS.setMs(sr,20);glS.setMs(sr,20);dustS.setMs(sr,20);reset();}
 void reset()override{phase=1;held[0]=held[1]=smooth[0]=smooth[1]=repeat[0]=repeat[1]=0;repeatLeft=0;activity=0;clock=1;rng=0xA341316Cu;mixS.y=1;crS.y=loS.y=glS.y=dustS.y=0;}
 float noise(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return float(rng>>8)*(1.f/16777216.f);}
 static float quant(float x,int b,bool trunc){float steps=float((1u<<b)-1u);float u=(clampf(x,-1.f,1.f)+1.f)*0.5f*steps;return 2.f*(trunc?floorf(u):floorf(u+0.5f))/steps-1.f;}
 static int sel(float v,int n){return (int)clampf(floorf(v+0.5f),0.f,float(n-1));}
 void process(const float* l,const float* r,float* ol,float* or_,int n,const float* e,const Context&)override{
 const int cm=sel(map(0,e[0]),3),lm=sel(map(2,e[2]),3),gm=sel(map(4,e[4]),3);
 const float caT=clampf(e[1],0.f,1.f),laT=clampf(e[3],0.f,1.f),gaT=clampf(e[5],0.f,1.f);
 const float dustT=clampf(map(6,e[6]),0.f,4.f);
 for(int i=0;i<n;i++){
  const float ca=crS.next(caT),la=loS.next(laT),ga=glS.next(gaT),mix=mixS.next(clampf(e[7],0.f,1.f)),dust=dustS.next(dustT);
  float dry[2]={l[i],r[i]},x[2]={dry[0],dry[1]};
  if(ca>0.0001f){
   int bits=(int)clampf(floorf(16-ca*15+0.5f),1.f,16.f);
   for(int c=0;c<2;c++){
    if(cm==2){float mu=1+ca*254,logmu=log1pf(mu);float y=copysignf(log1pf(mu*fabsf(clampf(x[c],-1.f,1.f)))/logmu,x[c]);y=quant(y,bits,false);x[c]=copysignf(expm1f(fabsf(y)*logmu)/mu,y);}
    else x[c]=quant(x[c],bits,cm==1);
   }
  }
  float div=1.f+la*la*127.f;
  if(la>0.0001f||ga>0.0001f){
   float inc=1.f/div;
   if(ga>0.0001f&&gm==0)inc*=clampf(1+(noise()*2-1)*ga*0.85f,0.1f,2.f);
   phase+=inc;
   if(phase>=1.f){
    phase-=floorf(phase);
    bool skip=ga>0.0001f&&gm==1&&noise()<ga*0.9f;
    if(!skip){
     if(ga>0.0001f&&gm==2&&repeatLeft>0){held[0]=repeat[0];held[1]=repeat[1];--repeatLeft;}
     else{held[0]=x[0];held[1]=x[1];repeat[0]=held[0];repeat[1]=held[1];if(ga>0.0001f&&gm==2&&noise()<ga*0.2f)repeatLeft=1+(int)(ga*12);}
    }
   }
   for(int c=0;c<2;c++){
    if(lm==1&&la>0.0001f){smooth[c]+=(1.f/(1.f+la*12.f))*(held[c]-smooth[c]);x[c]=smooth[c];}
    else if(lm==2&&la>0.0001f)x[c]=quant(held[c],(int)clampf(16-la*12,2.f,16.f),false);
    else x[c]=held[c];
   }
  }
  int dm=dust<1?0:dust<2?1:dust<3?2:3;float da=dm?dust-float(dm):0;
  if(dm&&da>0.0001f)for(int c=0;c<2;c++){
   if(dm==1)x[c]=clampf(x[c]+(noise()+noise()-1)*da/64.f,-1.f,1.f);
   else if(dm==2){int code=(int)clampf(floorf((clampf(x[c],-1.f,1.f)+1.f)*127.5f+0.5f),0.f,255.f);int mask=(1<<((int)(da*7)+1))-1;code&=~mask;x[c]=2.f*float(code)/255.f-1.f;}
   else if(fabsf(x[c])<da*0.3f)x[c]=0;
  }
  ol[i]=dry[0]+mix*(x[0]-dry[0]);or_[i]=dry[1]+mix*(x[1]-dry[1]);
  activity=0.999f*activity+0.0005f*(fabsf(x[0]-dry[0])+fabsf(x[1]-dry[1]));
  clock=div;
 }
 }
 void meters(float* o)const override{o[0]=clock;o[1]=activity;}
};
} // namespace rig
