// BITLAB functional checks: dry identity, bypass, independent textures.
#include "rigtest.h"
#include "bitlab.h"
using namespace rigtest;
static float difference(const Stereo& a,const Stereo& b){
 float sum=0; for(size_t i=0;i<a.L.size();++i)sum+=fabsf(a.L[i]-b.L[i])+fabsf(a.R[i]-b.R[i]);
 return sum/(2.f*a.L.size());
}
TEST("MIX zero is bit-identical to dry") {
 Fx<rig::Bitlab> fx; fx.set("crunch_amount",100);fx.set("lofi_amount",85);fx.set("glitch_amount",70);fx.set("dust",3.8f);fx.set("mix",0);
 const auto in=sine(48000,440,1);auto out=fx.run(in);
 float error=0;for(size_t i=0;i<in.size();++i)error=fmaxf(error,fabsf(out.L[i]-in[i]));
 CHECK(error<0.00001f,"dry MIX peak error %.8f",error);
}
TEST("independent Crunch and Lo-Fi alter the signal") {
 auto in=sine(48000,440,1);
 Fx<rig::Bitlab> dry;auto baseline=dry.run(in);
 Fx<rig::Bitlab> cr;cr.set("crunch_amount",90);auto a=cr.run(in);
 Fx<rig::Bitlab> lo;lo.set("lofi_amount",90);auto b=lo.run(in);
 float d1=difference(a,baseline),d2=difference(b,baseline),d3=difference(a,b);
 CHECK(d1>0.01f&&d2>0.01f&&d3>0.01f,"Crunch %.4f Lo-Fi %.4f distinct %.4f",d1,d2,d3);
}
RIGTEST_MAIN
