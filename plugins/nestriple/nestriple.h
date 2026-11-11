// plugins/nestriple/nestriple.h - BITROT MATRIX: eight 3-node echo algorithms, six regenerative paths.
// TIME 30..2000 ms, INTERVAL 0.5..2x, DECAY 0..90%, DEGRADE 0..100%, ORDER 8 algorithms,
// FEEDBACK 0..80%, RETURN 6 patterns, MIX 0..100%. Defaults: 120ms, 1x, 60%, 20%, SERIES, 20%, WRAP, 35%.
#pragma once
#include "module.h"

namespace rig {

inline constexpr ParamStop BITROT_ORDERS[] = {
  {0.f,"SERIES"}, {1.f,"PARALLEL"}, {2.f,"FORK"}, {3.f,"MERGE"},
  {4.f,"REVERSE"}, {5.f,"LADDER"}, {6.f,"CENTER"}, {7.f,"BRIDGE"}
};
inline constexpr ParamStop BITROT_RETURNS[] = {
  {0.f,"WRAP"}, {1.f,"MIDDLE"}, {2.f,"SPLIT"},
  {3.f,"LOCAL"}, {4.f,"CHAIN"}, {5.f,"CROSS"}
};
// Destination row, source column: the graph only accesses nodes already calculated in 'seq'.
struct BitrotRoute { unsigned char seq[3]; float dry[3]; float link[3][3]; };
inline constexpr BitrotRoute BITROT_ROUTES[] = {
  {{0,1,2},{1,0,0},{{0,0,0},{1,0,0},{0,1,0}}}, // A -> B -> C
  {{0,1,2},{1,1,1},{{0,0,0},{0,0,0},{0,0,0}}}, // three independent taps
  {{0,1,2},{1,0,0},{{0,0,0},{1,0,0},{1,0,0}}}, // A forks to B and C
  {{0,1,2},{1,1,0},{{0,0,0},{0,0,0},{.5f,.5f,0}}}, // A and B merge into C
  {{2,1,0},{0,0,1},{{0,1,0},{0,0,1},{0,0,0}}}, // C -> B -> A
  {{0,1,2},{1,0,0},{{0,0,0},{1,0,0},{.5f,.5f,0}}}, // A -> B, A+B -> C
  {{1,0,2},{0,1,0},{{0,1,0},{0,0,0},{0,1,0}}}, // B forks to A and C
  {{0,1,2},{1,0,.5f},{{0,0,0},{1,0,0},{0,.5f,0}}} // A->B->C plus dry to C
};
inline constexpr ParamDesc BITROT_PARAMS[] = {
  {"time",     "TIME",     30.f, 2000.f, .33011f, "ms", Curve::Log, 0, false},
  {"interval", "INTERVAL", .5f,   2.f,    1.f/3.f,  "x", Curve::Lin, 2, false},
  {"decay",    "DECAY",    0.f,   90.f,   2.f/3.f,  "%", Curve::Lin, 0, false},
  {"degrade",  "DEGRADE",  0.f,  100.f,    .2f,     "%", Curve::Lin, 0, false},
  {"order",    "ROUTE",    0.f,    7.f,    0.f,     "", Curve::Lin, 0, false, BITROT_ORDERS, 8},
  {"feedback", "FEEDBACK", 0.f,   80.f,    .25f,    "%", Curve::Lin, 0, false},
  {"return",   "RETURN",   0.f,    5.f,    0.f,     "", Curve::Lin, 0, false, BITROT_RETURNS, 6},
  {"mix",      "MIX",      0.f,  100.f,    .35f,    "%", Curve::Lin, 0, false}
};
inline constexpr ModuleDesc BITROT_DESC = {
  "nestriple", "BITROT MATRIX", "8 routes / 6 return paths", 8, BITROT_PARAMS, 2, {"order","feedback"}
};

// The 8 kHz internal audio clock is deliberate: six low-resolution delay lines for 1.5 MB total.
// A 2 s base multiplied by the maximum interval (x4 on C) reaches 8 s per stage.
struct BitrotLine {
  static constexpr int SIZE=65536, MASK=SIZE-1;
  float l[SIZE], r[SIZE];
  int index=0;
  void clear() { memset(l,0,sizeof l); memset(r,0,sizeof r); index=0; }
  void push(float a,float b) { l[index]=a; r[index]=b; index=(index+1)&MASK; }
  float tapL(int samples) const { return l[(index-samples)&MASK]; }
  float tapR(int samples) const { return r[(index-samples)&MASK]; }
};
struct NesTriple : Module {
  static constexpr float INTERNAL_SR=8000.f;
  BitrotLine delay[3];
  OnePole timeS, intervalS, decayS, damageS, feedbackS, mixS;
  float clockStep=1.f/6.f, phase=0.f, accL=0.f, accR=0.f, wetL=0.f, wetR=0.f;
  int accCount=0, heldCount[3]={0};
  float lpL[3]={0}, lpR[3]={0}, heldL[3]={0}, heldR[3]={0};
  float prevL[3]={0}, prevR[3]={0};
  int displayOrder=0; float displayFeedback=0;

  const ModuleDesc& desc() const override { return BITROT_DESC; }
  void init(float s) override {
    sr=s; clockStep=INTERNAL_SR/sr;
    timeS.setMs(INTERNAL_SR,30); intervalS.setMs(INTERNAL_SR,30);
    decayS.setMs(INTERNAL_SR,25); damageS.setMs(INTERNAL_SR,30);
    feedbackS.setMs(INTERNAL_SR,30); mixS.setMs(sr,25);
    reset();
  }
  void reset() override {
    for(int k=0;k<3;k++) {
      delay[k].clear(); lpL[k]=lpR[k]=heldL[k]=heldR[k]=prevL[k]=prevR[k]=0.f;
      heldCount[k]=0;
    }
    phase=accL=accR=wetL=wetR=0.f; accCount=0;
    timeS.y=map(0,BITROT_PARAMS[0].def); intervalS.y=1.f;
    decayS.y=.6f; damageS.y=.2f; feedbackS.y=.2f; mixS.y=.35f;
    displayOrder=0; displayFeedback=0;
  }
  void slowTick(float inputL,float inputR,float time,float interval,float decay,float damage,
                int algo,int ret,float feedback) {
    const float t=timeS.next(time)*INTERNAL_SR*.001f;
    const float ratio=intervalS.next(interval);
    int tap[3] = {
      (int)(t+.5f), (int)(t*ratio+.5f), (int)(t*ratio*ratio+.5f)
    };
    for(int k=0;k<3;k++) if(tap[k]>65534) tap[k]=65534; else if(tap[k]<1) tap[k]=1;
    const float dec=decayS.next(decay);
    const float dmg=damageS.next(damage);
    const float fb=feedbackS.next(feedback);
    float fL[3]={0}, fR[3]={0};
    // Feedback always uses the previous slow-tick output, avoiding instantaneous algebraic loops.
    switch(ret) {
      case 0: fL[0]=prevL[2]*fb; fR[0]=prevR[2]*fb; break;              // WRAP C -> A
      case 1: fL[1]=prevL[2]*fb; fR[1]=prevR[2]*fb; break;              // MIDDLE C -> B
      case 2: fL[0]=fL[1]=prevL[2]*fb*.5f; fR[0]=fR[1]=prevR[2]*fb*.5f; break; // SPLIT
      case 3: for(int k=0;k<3;k++){fL[k]=prevL[k]*fb;fR[k]=prevR[k]*fb;} break; // LOCAL
      case 4: fL[0]=prevL[1]*fb; fL[1]=prevL[2]*fb;                     // CHAIN B -> A, C -> B
              fR[0]=prevR[1]*fb; fR[1]=prevR[2]*fb; break;
      default:fL[0]=prevL[2]*fb; fL[2]=prevL[0]*fb;                     // CROSS C <-> A
              fR[0]=prevR[2]*fb; fR[2]=prevR[0]*fb; break;
    }
    float nowL[3]={0}, nowR[3]={0};
    const BitrotRoute& graph=BITROT_ROUTES[algo];
    for(int i=0;i<3;i++) {
      const int k=graph.seq[i];
      float xL=inputL*graph.dry[k]+fL[k];
      float xR=inputR*graph.dry[k]+fR[k];
      for(int j=0;j<3;j++) {
        xL+=graph.link[k][j]*nowL[j];
        xR+=graph.link[k][j]*nowR[j];
      }
      delay[k].push(clampf(xL,-2.f,2.f),clampf(xR,-2.f,2.f));
      const float rawL=delay[k].tapL(tap[k]);
      const float rawR=delay[k].tapR(tap[k]);
      const float d=dmg*(k+1)*(1.f/3.f);
      const float alpha=1.f-.82f*d;
      lpL[k]=undenorm(lpL[k]+alpha*(rawL-lpL[k]));
      lpR[k]=undenorm(lpR[k]+alpha*(rawR-lpR[k]));
      const int bits=12-(int)(8.f*d+.5f);
      const float scale=(float)(1 << (bits-1));
      const float crushL=floorf(lpL[k]*scale+.5f)/scale;
      const float crushR=floorf(lpR[k]*scale+.5f)/scale;
      const int hold=1+(int)(9.f*d+.5f);
      if(heldCount[k]<=0) {
        heldL[k]=crushL; heldR[k]=crushR; heldCount[k]=hold-1;
      } else --heldCount[k];
      nowL[k]=heldL[k]; nowR[k]=heldR[k];
    }
    for(int k=0;k<3;k++){prevL[k]=undenorm(nowL[k]);prevR[k]=undenorm(nowR[k]);}
    const float a=1.f/(1.f+dec+dec*dec);
    wetL=(nowL[0]+dec*nowL[1]+dec*dec*nowL[2])*a;
    wetR=(nowR[0]+dec*nowR[1]+dec*dec*nowR[2])*a;
    displayFeedback=feedbackS.y;
  }
  void process(const float* inL,const float* inR,float* outL,float* outR,
               int n,const float* e,const Context&) override {
    const float t=map(0,e[0]), interval=map(1,e[1]);
    const float dec=map(2,e[2])*.01f, dmg=map(3,e[3])*.01f;
    const int algo=(int)clampf(floorf(map(4,e[4])+.5f),0.f,7.f);
    const float feedback=map(5,e[5])*.01f;
    const int ret=(int)clampf(floorf(map(6,e[6])+.5f),0.f,5.f);
    const float mix=map(7,e[7])*.01f;
    displayOrder=algo;
    for(int i=0;i<n;i++){
      const float l=inL[i],r=inR[i];
      accL+=l;accR+=r;accCount++;
      phase+=clockStep;
      if(phase>=1.f){
        phase-=1.f;
        slowTick(accL/accCount,accR/accCount,t,interval,dec,dmg,algo,ret,feedback);
        accL=accR=0.f;accCount=0;
      }
      const float m=mixS.next(mix);
      outL[i]=l*(1.f-m)+wetL*m;
      outR[i]=r*(1.f-m)+wetR*m;
    }
  }
  void meters(float* out) const override {out[0]=(float)displayOrder;out[1]=displayFeedback;}
};
} // namespace rig
