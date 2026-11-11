// CLOCKWITCH 8950 — dual-clock timing diagram for an HT8950-inspired 8-bit pitch machine.
screen('clockwitch', (c,b,m,h) => {
  const modes=['DN3 2/3','DN2 4/5','DN1 8/9','NORMAL','UP1 4/3','UP2 8/5','UP3 2','ROBOT'];
  const shapes=['SINE','TRI','SQUARE','RAMP','STEPS','RANDOM'];
  const mode=Math.max(0,Math.min(7,Math.round(h.eff(0)*7)));
  const shape=Math.max(0,Math.min(5,Math.round(h.eff(5)*5)));
  const adc=Math.max(0,h.meter('adc_khz',8));
  const dac=Math.max(0,h.meter('dac_khz',8));
  const depth=h.eff(4),unstable=h.eff(6),vibHz=.05*Math.pow(800,h.eff(3));
  const phase=h.T||0;
  const text=h.col.text,accent=h.col.accent;
  const x=b.x,y=b.y,w=b.w,z=b.h;
  const fs=Math.max(7,Math.min(12,z*.093));
  const clockDraw=(hz,yy,alpha)=>{
    const cycles=Math.max(2,Math.min(20,2+hz*.6));
    const x0=x+w*.17,span=w*.73,amp=z*.055;
    c.beginPath();
    for(let j=0;j<=112;j++){
      const p=j/112,edge=Math.floor((p*cycles+phase*.26)%1*2),val=edge?1:-1;
      const jitter=unstable>.02?unstable*Math.sin(j*7.13+phase*6.1)*.7:0;
      const xx=x0+span*p+jitter;
      const vy=yy+amp*val;
      if(j===0)c.moveTo(xx,vy);else c.lineTo(xx,vy);
    }
    c.strokeStyle=accent;c.globalAlpha=alpha;c.lineWidth=Math.max(1,z*.013);c.stroke();
  };
  c.save();c.lineCap='round';c.lineJoin='round';
  c.font='bold '+(fs+1)+'px monospace';c.fillStyle=text;c.textAlign='left';
  c.globalAlpha=.9;c.fillText('CLOCKWITCH / 8950',x+w*.04,y+z*.14);
  c.textAlign='right';c.fillStyle=accent;c.fillText(modes[mode],x+w*.96,y+z*.14);
  c.globalAlpha=.22;c.strokeStyle=text;c.lineWidth=1;
  c.strokeRect(x+w*.035,y+z*.21,w*.93,z*.58);
  c.beginPath();c.moveTo(x+w*.035,y+z*.50);c.lineTo(x+w*.965,y+z*.50);c.stroke();
  c.font='bold '+fs+'px monospace';c.fillStyle=text;c.globalAlpha=.82;c.textAlign='left';
  c.fillText('ADC',x+w*.055,y+z*.375);
  c.fillText('DAC',x+w*.055,y+z*.665);
  clockDraw(adc,y+z*.355,.85);
  clockDraw(dac,y+z*.640,.65);
  c.globalAlpha=.95;c.font=fs+'px monospace';
  c.fillStyle=text;c.textAlign='left';
  c.fillText('A '+adc.toFixed(2)+'k',x+w*.04,y+z*.89);
  c.textAlign='center';c.fillText('8 BIT / '+shapes[shape],x+w*.52,y+z*.89);
  c.textAlign='right';c.fillStyle=accent;c.fillText('D '+dac.toFixed(2)+'k',x+w*.96,y+z*.89);
  c.restore();
});
