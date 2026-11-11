// CASTLE ALMOST WIZARD — simulated Kastle-style cables visualizing DSP modes.
// The RIG plugin screen contract exposes effective knob values, meters and time,
// but NOT the actual modulation-rack source-to-target patch graph. Cables here
// show conceptual/virtual routings for the selected mode, not user-made patches.
screen('castlewiz', (c, b, m, h) => {
  const labels=['DELAY','FLANGER','FREEZER','PANNER','CRUSHER','SLICER','PITCHER','REPLAYER','SHIFTER'];
  const sources=['LFO','SEQ','ENV','TRIG'];
  const dest=['TIME','FB','AMT','MODE'];
  const cableSets=[
    [[0,0],[2,1]],                // delay time sweeps, envelope regeneration
    [[0,0],[2,2],[1,1]],          // flanger modulation and intensity
    [[3,2],[1,0]],                // freezer trigger and duration
    [[0,2],[1,0]],                // panner speed and stereo depth
    [[0,0],[2,2],[1,1]],          // crusher clock, amount and feedback
    [[1,0],[3,2]],                // slicer clock/gate
    [[0,0],[2,2],[3,3]],          // pitcher rate and articulation
    [[3,2],[1,0],[2,1]],          // replayer capture, speed and return
    [[0,0],[2,2],[1,1]]           // shifter sweep, envelope, feedback
  ];
  const mode=Math.max(0,Math.min(8,Math.round(h.eff(0)*8)));
  const act=Math.max(0,Math.min(1,h.meter('activity',0)));
  const held=Math.max(0,Math.min(1,h.meter('held',0)));
  const t=h.T||0;
  const col=h.col.text,accent=h.col.accent;
  const xL=b.x+b.w*.17,xR=b.x+b.w*.82;
  const top=b.y+b.h*.24,dy=b.h*.155;
  const radius=Math.max(2,Math.min(5,b.h*.045));
  const fs=Math.max(7,Math.min(12,b.h*.095));
  c.save();
  c.textBaseline='middle';c.lineCap='round';c.lineJoin='round';
  c.fillStyle=col;c.globalAlpha=.95;c.font='bold '+(fs+1)+'px monospace';
  c.textAlign='center';c.fillText(labels[mode],b.x+b.w*.5,b.y+b.h*.105);
  c.font=Math.max(6,fs*.72)+'px monospace';
  c.globalAlpha=.6;c.fillText('VIRTUAL PATCH / MODE ROUTING',b.x+b.w*.5,b.y+b.h*.96);
  c.font=fs+'px monospace';
  for(let i=0;i<4;i++){
    const y=top+dy*i;
    c.globalAlpha=.65;c.fillStyle=col;
    c.textAlign='left';c.fillText(sources[i],b.x+b.w*.025,y);
    c.textAlign='right';c.fillText(dest[i],b.x+b.w*.98,y);
    c.beginPath();c.arc(xL,y,radius,0,Math.PI*2);
    c.fillStyle=col;c.globalAlpha=.2;c.fill();c.globalAlpha=.6;c.strokeStyle=col;c.lineWidth=1;c.stroke();
    c.beginPath();c.arc(xR,y,radius,0,Math.PI*2);
    c.fillStyle=col;c.globalAlpha=.2;c.fill();c.globalAlpha=.6;c.stroke();
  }
  const lines=cableSets[mode];
  for(let j=0;j<lines.length;j++){
    const [src,dst]=lines[j];
    const a=top+dy*src,z=top+dy*dst;
    const offset=(j-(lines.length-1)*.5)*b.h*.09;
    const cx=(xL+xR)*.5,cy=Math.max(a,z)+b.h*.13+offset;
    const energy=(Math.sin(t*3.5+j*2.1)+1)*.5;
    const alpha=.4+act*.3+energy*.2;
    c.beginPath();c.moveTo(xL+radius,a);
    c.bezierCurveTo(cx,b.y+cy-b.y,cx,cy,xR-radius,z);
    c.strokeStyle=j%2===0?accent:col;c.globalAlpha=alpha;
    c.lineWidth=Math.max(1.5,b.h*.022);
    c.stroke();
    const pulse=(t*.22+j*.29)%1;
    // A traveling pulse suggests control signals through the patch wire.
    const u=1-pulse,x1=xL+radius,x2=cx,x3=cx,x4=xR-radius;
    const px=u*u*u*x1+3*u*u*pulse*x2+3*u*pulse*pulse*x3+pulse*pulse*pulse*x4;
    const py=u*u*u*a+3*u*u*pulse*cy+3*u*pulse*pulse*cy+pulse*pulse*pulse*z;
    c.beginPath();c.arc(px,py,Math.max(1.5,radius*.45),0,Math.PI*2);
    c.fillStyle=accent;c.globalAlpha=.55+act*.4;c.fill();
    c.beginPath();c.arc(xL+radius,a,radius*.65,0,Math.PI*2);
    c.arc(xR-radius,z,radius*.65,0,Math.PI*2);
    c.fillStyle=accent;c.globalAlpha=.85;c.fill();
  }
  c.font=fs+'px monospace';c.textAlign='center';c.fillStyle=col;c.globalAlpha=.75;
  const status=held>.5?'CAPTURE HELD':('ACT '+Math.round(act*100)+'%');
  c.fillText(status,b.x+b.w*.5,b.y+b.h*.87);
  c.restore();
});
