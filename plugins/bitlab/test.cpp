// BITLAB tests: independent stages, exact dry mix, and method signatures.
#include "rigtest.h"
#include "bitlab.h"
using namespace rigtest;
static float diff(const Signal& a,const Signal& b){float sum=0;for(size_t i=0;i<a.size();++i)sum+=fabsf(a[i]-b[i]);return sum/a.size();}
TEST("MIX zero is exact bypass") {
 Fx<rig::Bitlab> fx;fx.set("crunch_amount",100);fx.set("lofi_amount",80);fx.set("glitch_amount",80);fx.set("dust",2.8f);fx.set("mix",0);
 const auto s=sine(48000,440,1);auto o=fx.run(s);float err=diff(s,o.L);
 CHECK(err<0.000001f,"MIX zero mean error %.9f",err);
}
TEST("Crunch and Lo-Fi work separately") {
 const auto s=sine(48000,440,1);
 Fx<rig::Bitlab> plain;auto dry=plain.run(s);
 Fx<rig::Bitlab> cr;cr.set("crunch_amount",95);auto a=cr.run(s);
 Fx<rig::Bitlab> lo;lo.set("lofi_amount",95);auto b=lo.run(s);
 CHECK(diff(a.L,dry.L)>0.01f&&diff(b.L,dry.L)>0.01f&&diff(a.L,b.L)>0.01f,
 "Crunch %.4f Lo-Fi %.4f separation %.4f",diff(a.L,dry.L),diff(b.L,dry.L),diff(a.L,b.L));
}
RIGTEST_MAIN
