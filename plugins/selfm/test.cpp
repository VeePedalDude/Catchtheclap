// SELF FM MATRIX v2: tests for strong audio-rate sidebands and genuine synthesis routing.
#include "rigtest.h"
#include "selfm.h"
using namespace rigtest;
using rig::SelfFmMatrix;
static void base(Fx<SelfFmMatrix>& f){
  f.set("depth",5.5f);f.set("source",337.f);f.set("env",80.f);
  f.set("sustain",25.f);f.set("feedback",20.f);f.set("route",1.f);
  f.set("tone",12000.f);f.set("mix",100.f);
}
static float rmsTail(const Signal& x,size_t from=20000){
  double sum=0;for(size_t i=from;i<x.size();i++)sum+=(double)x[i]*x[i];
  return (float)sqrt(sum/(double)(x.size()-from));
}
static float relativeDiff(const Signal& a,const Signal& b,size_t from=20000){
  double ds=0,bs=0;for(size_t i=from;i<a.size();i++){
    const double d=(double)a[i]-(double)b[i];ds+=d*d;bs+=(double)a[i]*a[i];
  }return (float)sqrt(ds/(bs+1e-20));
}
TEST("INDEX creates strong inharmonic sidebands rather than overdrive"){
  Fx<SelfFmMatrix> zero,hot;base(zero);base(hot);
  for(auto* f:{&zero,&hot}){f->set("sustain",0);f->set("feedback",0);f->set("route",0);}
  zero.set("depth",0);hot.set("depth",5.5f);
  const Signal in=sine(48000,220,2,.3f);
  const auto a=zero.run(in).L,b=hot.run(in).L;
  const float off=toneLevel(a,48000,557,24000),on=toneLevel(b,48000,557,24000);
  CHECK(on>.045f&&on>30.f*(off+1e-6f),
        "220+337Hz sideband: INDEX=0 %.5f, INDEX=5.5 %.4f",off,on);
}
TEST("MOD HZ moves inharmonic sideband frequency"){
  Fx<SelfFmMatrix> f300,f500;base(f300);base(f500);
  for(auto* f:{&f300,&f500}){f->set("depth",5.5f);f->set("sustain",0);f->set("feedback",0);f->set("route",0);}
  f300.set("source",300);f500.set("source",500);
  const Signal x=sine(48000,220,2,.3f);
  const auto a=f300.run(x).L,b=f500.run(x).L;
  const float at520=toneLevel(a,48000,520,24000),at720=toneLevel(b,48000,720,24000);
  const float wrong520=toneLevel(b,48000,520,24000),wrong720=toneLevel(a,48000,720,24000);
  CHECK(at520>.04f&&at720>.04f&&at520>10.f*(wrong520+1e-6f)&&at720>10.f*(wrong720+1e-6f),
        "300Hz creates 520Hz %.3f (off %.3f); 500Hz creates 720Hz %.3f (off %.3f)",at520,wrong520,at720,wrong720);
}
TEST("SELF mixes guitar-derived harmonic modulation"){
  Fx<SelfFmMatrix> osc,gui;base(osc);base(gui);
  for(auto* f:{&osc,&gui}){f->set("feedback",0);f->set("route",0);}
  osc.set("sustain",0);gui.set("sustain",100);
  const auto x=sine(48000,220,2,.3f);
  const float a=toneLevel(osc.run(x).L,48000,440,24000);
  const float b=toneLevel(gui.run(x).L,48000,440,24000);
  CHECK(b>.09f&&b>a*8.f,"guitar harmonic %.3f vs oscillator-only %.4f",b,a);
}
TEST("ROUTE has four audibly different transfer behaviors"){
  Signal out[4];const auto s=sine(48000,220,2,.3f);
  for(int k=0;k<4;k++){Fx<SelfFmMatrix> f;base(f);f.set("route",(float)k);out[k]=f.run(s).L;}
  const float a=relativeDiff(out[0],out[1]),b=relativeDiff(out[1],out[2]),c=relativeDiff(out[2],out[3]);
  CHECK(a>.4f&&b>.4f&&c>.4f,
        "SELF->SERIAL %.2fx, SERIAL->FANIN %.2fx, FANIN->PARALLEL %.2fx RMS",a,b,c);
}
TEST("SERIAL operator changes the guitar carrier substantially"){
  Fx<SelfFmMatrix> a,b;base(a);base(b);a.set("route",0);b.set("route",1);
  const auto x=sine(48000,220,2,.3f);
  const auto v=a.run(x).L,w=b.run(x).L;
  CHECK(relativeDiff(v,w)>.4f,"cascaded operator difference %.2fx RMS",relativeDiff(v,w));
}
TEST("FEEDBACK introduces additional phase complexity"){
  Fx<SelfFmMatrix> a,b;base(a);base(b);a.set("feedback",0);b.set("feedback",80);
  const auto x=sine(48000,250,2,.3f);
  const auto v=a.run(x).L,w=b.run(x).L;
  CHECK(relativeDiff(v,w)>.3f,"feedback 0->80 changes output %.2fx RMS",relativeDiff(v,w));
}
TEST("INDEX zero retains clean fundamental without new sidebands"){
  Fx<SelfFmMatrix> f;base(f);f.set("depth",0);f.set("route",1);
  const auto x=sine(48000,220,2,.3f),y=f.run(x).L;
  const float fundamental=toneLevel(y,48000,220,24000),side=toneLevel(y,48000,557,24000);
  CHECK(fabsf(fundamental-.3f)<.015f&&side<.003f,
        "INDEX=0 fundamental %.3f, inharmonic sideband %.5f",fundamental,side);
}
TEST("TONE shapes high-frequency sidebands rather than mod depth"){
  Fx<SelfFmMatrix> dark,bright;base(dark);base(bright);
  dark.set("tone",400.f);bright.set("tone",15000.f);
  const auto x=sine(48000,220,2,.3f);
  const auto a=dark.run(x).L,b=bright.run(x).L;
  const float loA=toneLevel(a,48000,220,24000),loB=toneLevel(b,48000,220,24000);
  const float hiA=toneLevel(a,48000,1231,24000),hiB=toneLevel(b,48000,1231,24000);
  CHECK(hiA/hiB<loA/loB*.70f,"low retain %.2f vs high retain %.2f",loA/loB,hiA/hiB);
}
TEST("Dry/wet are time-aligned, avoiding the old comb issue"){
  Fx<SelfFmMatrix> a,b,c;base(a);base(b);base(c);
  for(auto* f:{&a,&b,&c}){f->set("depth",0);f->set("tone",15500.f);}
  a.set("mix",100);b.set("mix",50);c.set("mix",0);
  const auto x=sine(48000,220,2,.3f);
  const auto w=a.run(x).L,h=b.run(x).L,d=c.run(x).L;
  const float rel=rmsTail(h)/rmsTail(d);
  CHECK(rel>.93f&&rel<1.04f&&relativeDiff(d,w)<.03f,
        "aligned half mix level %.3fx and wet/dry difference %.4f",rel,relativeDiff(d,w));
}
TEST("Quiet guitar signal still produces synthetic sidebands"){
  Fx<SelfFmMatrix> f;base(f);f.set("depth",8);f.set("route",0);f.set("sustain",0);
  const auto x=sine(48000,330,2,.01f),y=f.run(x).L;
  const float side=toneLevel(y,48000,667,24000);
  CHECK(side>.0007f,"quiet input 667Hz sideband %.5f",side);
}
TEST("Meters report envelope and index activity"){
  Fx<SelfFmMatrix> f;base(f);float mxE=0,mxI=0;
  f.run(sine(48000,220,1,.3f),[&](int,Fx<SelfFmMatrix>& fx){
    auto m=fx.meters();mxE=fmaxf(mxE,m[0]);mxI=fmaxf(mxI,m[1]);});
  CHECK(mxE>.25f&&mxI>.10f&&mxE<=1&&mxI<=1,
        "guitar envelope %.3f, phase index %.3f",mxE,mxI);
}
RIGTEST_MAIN
