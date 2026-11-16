// BITLAB: independent Crunch, Lo-Fi, Glitch, Dust textures; default all off, MIX 100%.
#pragma once
#include "module.h"
#include <cmath>
#include <cstdint>
namespace rig {
inline constexpr ParamStop B_CR[]={{0,"ROUND"},{1,"TRUNC"},{2,"COMPAND"}};
inline constexpr ParamStop B_LO[]={{0,"HOLD"},{1,"SMOOTH"},{2,"STEP"}};
inline constexpr ParamStop B_GL[]={{0,"JITTER"},{1,"SKIP"},{2,"REPEAT"}};
inline constexpr ParamStop B_DU[]={{0,"OFF"},{1,"DITHER"},{2,"MASK"},{3,"DEADZONE"}};
inline constexpr ParamDesc B_PARAMS[]={
 {"crunch_method","CR METHOD",0,2,0,"",Curve::Lin,0,false,B_CR,3},
 {"crunch_amount","CRUNCH",0,100,0,"%",Curve::Lin,0,false},
 {"lofi_method","LO METHOD",0,2,0,"",Curve::Lin,0,false,B_LO,3},
 {"lofi_amount","LO-FI",0,100,0,"%",Curve::Lin,0,false},
 {"glitch_method","GL METHOD",0,2,0,"",Curve::Lin,0,false,B_GL,3},
 {"glitch_amount","GLITCH",0,100,0,"%",Curve::Lin,0,false},
 {"dust","DUST",0,4,0,"",Curve::Lin,2,false,B_DU,4},
 {"mix","MIX",0,100,1,"%",Curve::Lin,0,false}};
inline constexpr ModuleDesc B_DESC={"bitlab","BITLAB","digital textures",8,B_PARAMS,0,{nullptr,nullptr}};
struct Bitlab:Module {
 OnePole cr,lo,gl,du,mx;
 float phase=1,held[2]={},filtered[2]={},last[2]={};int repeats=0;
 uint32_t seed=0x9E3779B9u;
 const ModuleDesc& desc()const override{return B_DESC;}
 void init(float s)override{sr=s;cr.setMs(s,15);lo.setMs(s,15);gl.setMs(s,15);du.setMs(s,20);mx.setMs(s,15);reset();}
 void reset()override{phase=1;held[0]=held[1]=filtered[0]=filtered[1]=last[0]=last[1]=0;repeats=0;seed=0x9E3779B9u;cr.y=lo.y=gl.y=du.y=0;mx.y=1;}
 float rnd(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return (seed>>8)*(1.f/16777216.f);}
 static int mode(float x){return (int)floorf(x+0.5f);}
 static float q(float x,int bits,bool trunc){
  const float levels=(float)((1u<<bits)-1u);
  const float v=(clampf(x,-1,1)+1)*0.5f*levels;
  return 2*(trunc?floorf(v):floorf(v+0.5f))/levels-1;
 }
 void process(const float* inL,const float* inR,float* outL,float* outR,int n,const float* e,const Context&)override{
  const int cm=mode(map(0,e[0])),lm=mode(map(2,e[2])),gm=mode(map(4,e[4]));
  const float ca=clampf(e[1],0,1),la=clampf(e[3],0,1),ga=clampf(e[5],0,1),dt=clampf(map(6,e[6]),0,4),mt=clampf(e[7],0,1);
  for(int i=0;i<n;++i){
   const float a=cr.next(ca),b=lo.next(la),g=gl.next(ga),d=du.next(dt),m=mx.next(mt);
   const float dry[2]={inL[i],inR[i]};float wet[2]={dry[0],dry[1]};
   if(a>0.001f){
    int bits=(int)clampf(16-floorf(a*15+0.5f),1,16);
    for(int c=0;c<2;++c){
     if(cm==2){const float k=1+15*a;float y=copysignf(log1pf(k*fabsf(clampf(wet[c],-1,1)))/log1pf(k),wet[c]);y=q(y,bits,false);wet[c]=copysignf(expm1f(fabsf(y)*log1pf(k))/k,y);}
     else wet[c]=q(wet[c],bits,cm==1);
    }
   }
   // Shared clock is active only for Lo-Fi or Glitch; independent zero means bypass.
   if(b>0.001f||g>0.001f){
    const float divider=1+127*b*b;
    float advance=1/divider;
    if(g>0.001f&&gm==0)advance*=clampf(1+(rnd()*2-1)*g*0.9f,0.1f,2.f);
    phase+=advance;
    if(phase>=1){phase-=floorf(phase);
     const bool skip=g>0.001f&&gm==1&&rnd()<g*0.8f;
     if(!skip){
      if(g>0.001f&&gm==2&&repeats>0){held[0]=last[0];held[1]=last[1];--repeats;}
      else{held[0]=wet[0];held[1]=wet[1];last[0]=wet[0];last[1]=wet[1];if(g>0.001f&&gm==2&&rnd()<g*0.2f)repeats=1+(int)(g*8);}
     }
    }
    for(int c=0;c<2;++c){
     if(b<=0.001f)wet[c]=held[c];
     else if(lm==1){filtered[c]+=(held[c]-filtered[c])/(1+12*b);wet[c]=filtered[c];}
     else if(lm==2)wet[c]=q(held[c],(int)clampf(16-floorf(12*b),2,16),false);
     else wet[c]=held[c];
    }
   }
   // Dust knob is four one-unit zones: OFF, DITHER, MASK, DEADZONE.
   // At each zone's start, the effect intensity is zero.
   int dm=(int)floorf(d);float strength=d-dm;
   if(dm>=1&&dm<=3&&strength>0.001f)for(int c=0;c<2;++c){
    if(dm==1)wet[c]=clampf(wet[c]+(rnd()+rnd()-1)*strength/32,-1,1);
    else if(dm==2){const int mask=(1<<((int)(strength*6)+1))-1;int v=(int)clampf(floorf((clampf(wet[c],-1,1)+1)*127.5f+0.5f),0,255);wet[c]=2.f*(v&~mask)/255-1;}
    else if(fabsf(wet[c])<strength*0.3f)wet[c]=0;
   }
   // Zero MIX is an exact bypass, even when jumping from 100%.
   const float blend=mt<=0?0:m;
   outL[i]=dry[0]+blend*(wet[0]-dry[0]);outR[i]=dry[1]+blend*(wet[1]-dry[1]);
  }
 }
};
}
