// SELF FM MATRIX — three audio-rate phase paths driven only by incoming guitar.
screen('selfm', (c, b, m, h) => {
  const t = h.T || 0;
  const env = Math.max(0, Math.min(1, h.meter('env', 0)));
  const index = Math.max(0, Math.min(1, h.meter('index', 0)));
  const route = Math.max(0, Math.min(3, Math.round(3*h.eff(5))));
  const labels = ['SELF','SERIAL','FAN-IN','PARALLEL'];
  const ink = h.col.text, accent = h.col.accent;
  const cx = b.x+b.w*.5, cy = b.y+b.h*.52;
  const fs = Math.max(7, Math.min(13,b.h*.085));
  c.save();c.lineCap='round';c.textBaseline='middle';c.textAlign='center';
  const fit=(s,w)=>{let z=fs;for(;z>7;z--){c.font='bold '+z+'px monospace';if(c.measureText(s).width<=w)break;}return 'bold '+z+'px monospace';};
  c.fillStyle=ink;c.globalAlpha=.9;c.font=fit('SELF FM / '+labels[route],b.w*.92);
  c.fillText('SELF FM / '+labels[route],cx,b.y+b.h*.12);
  const nodes=[{x:cx-b.w*.29,y:cy-b.h*.04,s:'A'},{x:cx+b.w*.29,y:cy-b.h*.04,s:'B'},{x:cx,y:cy+b.h*.22,s:'G'}];
  const edge=(a,z,energy,shift)=>{
    const A=nodes[a],Z=nodes[z];c.beginPath();c.moveTo(A.x,A.y);
    c.quadraticCurveTo(cx+shift,cy-b.h*.18,Z.x,Z.y);
    c.strokeStyle=accent;c.globalAlpha=.18+energy*.6;c.lineWidth=1+energy*3;c.stroke();
  };
  edge(0,1,index,0);
  edge(route===0?0:1,2,index,route===2?-b.w*.2:b.w*.2);
  if(route>=2)edge(0,2,index,-b.w*.2);
  for(const o of nodes){c.beginPath();c.arc(o.x,o.y,Math.min(b.w,b.h)*.11,0,2*Math.PI);
    c.fillStyle=accent;c.globalAlpha=.12+index*.20;c.fill();
    c.strokeStyle=ink;c.globalAlpha=.78;c.lineWidth=1.5;c.stroke();
    c.fillStyle=ink;c.font=fit(o.s,20);c.globalAlpha=.95;c.fillText(o.s,o.x,o.y);
  }
  const bar=b.w*.66,x=cx-bar*.5,y=b.y+b.h*.87;
  c.fillStyle=ink;c.globalAlpha=.25;c.fillRect(x,y,bar,3);
  c.fillStyle=accent;c.globalAlpha=.85;c.fillRect(x,y,bar*env,3);
  c.font=fit('INPUT ENVELOPE',b.w*.85);c.fillStyle=ink;c.globalAlpha=.7;
  c.fillText('INPUT ENVELOPE',cx,b.y+b.h*.96);
  c.restore();
});