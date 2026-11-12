// DI FINISHER: visual approximation of filter/tone response and live output dynamics.
// RIG/01 screen.js is draw-only; this is a readout, not an interactive EQ editor.
screen('difinish',(c,b,m,h)=>{
  const low=15*Math.pow(180/15,h.eff(0));
  const high=3000*Math.pow(20000/3000,h.eff(1));
  const tilt=-6+12*h.eff(4),body=-6+12*h.eff(5);
  const gr=Math.max(0,Math.min(60,h.meter('reduction',0)));
  const peak=Math.max(-100,Math.min(3,h.meter('peak',-100)));
  const ink=h.col.text,accent=h.col.accent;
  const x0=b.x+b.w*.07,x1=b.x+b.w*.94;
  const top=b.y+b.h*.24,bottom=b.y+b.h*.74;
  const text=(s,x,y,nominal,maxW,align='left')=>{
    let fs=nominal;
    while(fs>7){c.font=`${fs}px monospace`;if(c.measureText(s).width<=maxW)break;fs-=.5;}
    c.font=`${fs}px monospace`;c.textAlign=align;c.fillText(s,x,y);
  };
  c.save();c.lineJoin='round';c.lineCap='round';c.textBaseline='middle';
  c.fillStyle=ink;c.globalAlpha=.86;
  text('OUTPUT RESPONSE',b.x+b.w*.5,b.y+b.h*.1,Math.min(14,b.h*.12),b.w*.82,'center');
  c.strokeStyle=ink;c.lineWidth=1;c.globalAlpha=.25;
  for(let j=0;j<3;j++){
    const y=top+(bottom-top)*j/2;c.beginPath();c.moveTo(x0,y);c.lineTo(x1,y);c.stroke();
  }
  c.strokeStyle=accent;c.globalAlpha=.95;c.lineWidth=Math.max(1.4,b.h*.024);
  for(let k=0;k<=80;k++){
    const x=k/80, f=20*Math.pow(1000,x);
    const hp=f*f/(f*f+low*low);
    const lp=high*high/(f*f+high*high);
    const tiltDb=tilt*(Math.log2(f/700)/6);
    const bodyDb=body*.62*Math.exp(-Math.pow(Math.log2(f/190)/1.7,2));
    const db=20*Math.log10(Math.max(0.00001,hp*lp))+tiltDb+bodyDb;
    const xx=x0+(x1-x0)*x,yy=bottom-(Math.max(-30,Math.min(10,db))+30)/40*(bottom-top);
    if(k===0)c.beginPath(),c.moveTo(xx,yy);else c.lineTo(xx,yy);
  }
  c.stroke();
  c.globalAlpha=.82;c.fillStyle=ink;
  const ff=(f)=>f>=1000?(f/1000).toFixed(1)+'k':Math.round(f)+'Hz';
  text('HP '+ff(low),x0,b.y+b.h*.82,Math.min(11,b.h*.105),b.w*.42);
  text('LP '+ff(high),x1,b.y+b.h*.82,Math.min(11,b.h*.105),b.w*.42,'right');
  text('GR '+gr.toFixed(1)+' dB',x0,b.y+b.h*.94,Math.min(11,b.h*.10),b.w*.42);
  text('PEAK '+peak.toFixed(1)+' dBFS',x1,b.y+b.h*.94,Math.min(11,b.h*.10),b.w*.48,'right');
  c.restore();
});