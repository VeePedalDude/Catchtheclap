// DI FINISHER: measurable guitar output/mastering behavior tests.
#include "rigtest.h"
#include "difinish.h"
using namespace rigtest;
using rig::DiFinisher;

static float tailRms(const Signal& a,float sr=48000.f){return (float)rms(a,(size_t)(sr*.6f),a.size());}
static void neutral(Fx<DiFinisher>& f){
  f.set("lowcut",15);f.set("highcut",20000);f.set("comp",0);
  f.set("limit",0);f.set("tilt",0);f.set("body",0);f.set("output",0);
}
TEST("LOW CUT removes rumble without disproportionately cutting guitar midrange"){
  Fx<DiFinisher> a,b,c,d;neutral(a);neutral(b);neutral(c);neutral(d);
  a.set("lowcut",15);b.set("lowcut",160);c.set("lowcut",15);d.set("lowcut",160);
  float lowRatio=tailRms(b.run(sine(48000,35,1.1f,.15f)).L)/tailRms(a.run(sine(48000,35,1.1f,.15f)).L);
  float midRatio=tailRms(d.run(sine(48000,400,1.1f,.15f)).L)/tailRms(c.run(sine(48000,400,1.1f,.15f)).L);
  CHECK(lowRatio<.22f && midRatio>.70f,"LO CUT 160 vs 15 Hz: 35 Hz ratio %.3f, 400 Hz ratio %.3f",lowRatio,midRatio);
}
TEST("HIGH CUT reduces fizz while keeping fundamentals"){
  Fx<DiFinisher> a,b,c,d;neutral(a);neutral(b);neutral(c);neutral(d);
  a.set("highcut",20000);b.set("highcut",4000);c.set("highcut",20000);d.set("highcut",4000);
  float highRatio=tailRms(b.run(sine(48000,11000,1.1f,.1f)).L)/tailRms(a.run(sine(48000,11000,1.1f,.1f)).L);
  float midRatio=tailRms(d.run(sine(48000,500,1.1f,.1f)).L)/tailRms(c.run(sine(48000,500,1.1f,.1f)).L);
  CHECK(highRatio<.55f && midRatio>.90f,"HI CUT 4k vs 20k: 11 kHz ratio %.3f, 500 Hz ratio %.3f",highRatio,midRatio);
}
TEST("TILT brightens or darkens broadband guitar harmonics"){
  Fx<DiFinisher> bright,dark,brightLow,darkLow;
  neutral(bright);neutral(dark);neutral(brightLow);neutral(darkLow);
  bright.set("tilt",6);dark.set("tilt",-6);brightLow.set("tilt",6);darkLow.set("tilt",-6);
  float hi=tailRms(bright.run(sine(48000,3000,1.1f,.09f)).L)/tailRms(dark.run(sine(48000,3000,1.1f,.09f)).L);
  float lo=tailRms(brightLow.run(sine(48000,140,1.1f,.09f)).L)/tailRms(darkLow.run(sine(48000,140,1.1f,.09f)).L);
  CHECK(hi>1.3f && lo<.9f,"TILT +6 vs -6 dB: 3 kHz ratio %.3f, 140 Hz ratio %.3f",hi,lo);
}
TEST("BODY changes guitar low-mid weight more than upper midrange"){
  Fx<DiFinisher> a,b,c,d;neutral(a);neutral(b);neutral(c);neutral(d);
  b.set("body",6);d.set("body",6);
  const float bodyRatio=tailRms(b.run(sine(48000,170,1.1f,.1f)).L)/tailRms(a.run(sine(48000,170,1.1f,.1f)).L);
  const float upperRatio=tailRms(d.run(sine(48000,3000,1.1f,.1f)).L)/tailRms(c.run(sine(48000,3000,1.1f,.1f)).L);
  CHECK(bodyRatio>1.2f && bodyRatio>upperRatio*1.15f,"BODY +6 dB: 170 Hz ratio %.3f, 3 kHz ratio %.3f",bodyRatio,upperRatio);
}
TEST("COMP narrows the difference between loud and quiet notes"){
  Fx<DiFinisher> quietDry,loudDry,quietComp,loudComp;
  neutral(quietDry);neutral(loudDry);neutral(quietComp);neutral(loudComp);
  quietComp.set("comp",100);loudComp.set("comp",100);
  const float dryRatio=tailRms(loudDry.run(sine(48000,440,1.2f,.4f)).L)/tailRms(quietDry.run(sine(48000,440,1.2f,.01f)).L);
  const float compRatio=tailRms(loudComp.run(sine(48000,440,1.2f,.4f)).L)/tailRms(quietComp.run(sine(48000,440,1.2f,.01f)).L);
  CHECK(dryRatio>35.f&&compRatio<dryRatio*.7f,"COMP loud/quiet dynamics: dry %.2fx, compressed %.2fx",dryRatio,compRatio);
}
TEST("ATTACK setting changes pick transient strength"){
  Fx<DiFinisher> quick,slow;neutral(quick);neutral(slow);
  quick.set("comp",100);slow.set("comp",100);quick.set("attack",2);slow.set("attack",40);
  Signal in((size_t)(48000*1.0),0.f);
  for(size_t i=4800;i<in.size();i++)in[i]=.5f*sinf(2.f*rig::PI_F*330.f*(float)i/48000.f);
  const auto a=quick.run(in).L, b=slow.run(in).L;
  const float fastR=(float)rms(a,5100,6000),slowR=(float)rms(b,5100,6000);
  CHECK(slowR>fastR*1.05f,"ATTACK 40 vs 2 ms: first transient %.4f vs %.4f RMS",slowR,fastR);
}
TEST("LIMIT maintains fixed -1 dBFS sample ceiling on loud input"){
  Fx<DiFinisher> fx;neutral(fx);fx.set("limit",15);fx.set("output",6);
  const auto o=fx.run(sine(48000,1000,1.1f,1.6f));
  const float pk=fmaxf(peak(o.L),peak(o.R));
  CHECK(pk<=DiFinisher::CEILING+1e-5f&&pk>.7f,"LIMIT: max sample peak %.5f (ceiling %.5f)",pk,DiFinisher::CEILING);
}
TEST("OUTPUT trim controls level below the limiter threshold"){
  Fx<DiFinisher> a,b;neutral(a);neutral(b);a.set("output",-12);b.set("output",0);
  const Signal in=sine(48000,550,1.2f,.012f);
  const float ratio=tailRms(b.run(in).L)/tailRms(a.run(in).L);
  CHECK(fabsf(ratio-3.981f)<.1f,"OUTPUT 0 vs -12 dB: RMS ratio %.3fx (target 3.981x)",ratio);
}
TEST("linked stereo dynamics preserves proportional channel balance"){
  Fx<DiFinisher> fx;neutral(fx);fx.set("comp",100);fx.set("limit",10);
  const Signal l=sine(48000,350,1.3f,.45f),r=sine(48000,350,1.3f,.225f);
  const auto o=fx.run(l,r);
  float ratio=tailRms(o.L)/tailRms(o.R);
  CHECK(fabsf(ratio-2.f)<.04f,"Linked stereo gain preserves left/right amplitude ratio %.3f",ratio);
}
TEST("meters expose reduction and post-limiter output peak"){
  Fx<DiFinisher> fx;neutral(fx);fx.set("limit",15);fx.set("output",6);
  fx.run(sine(48000,440,1.f,.9f));
  const auto v=fx.meters();
  CHECK(v[0]>1.f&&v[1]<=-1.f+0.15f&&v[1]>-10.f,"Meters: gain reduction %.2f dB, peak %.2f dBFS",v[0],v[1]);
}
TEST("quiet guitar tail is not muted by a post-chain noise gate"){
  Fx<DiFinisher> fx;neutral(fx);fx.set("comp",50);
  const auto o=fx.run(sine(48000,500,1.f,.004f));
  const float level=(float)rms(o.L,20000);
  CHECK(level>.0008f,"Low-level sustain remains audible: RMS %.5f",level);
}
RIGTEST_MAIN