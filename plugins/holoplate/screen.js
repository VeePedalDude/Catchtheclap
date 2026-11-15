// HOLOPLATE v2: conceptual live spectral reconstruction panel. Spectrum bands
// represent the current MORPH/ANGLE mapping, not literal analyzed FFT values.
screen('holoplate', (c,b,m,h) => {
  const clamp=x=>Math.max(0,Math.min(1,Number.isFinite(x)?x:0));
  const morph=clamp(h.eff(4)),angle=clamp(h.eff(5));
  const diffuse=clamp(h.eff(3)),plate=clamp(h.meter('plate',0));
  const echo=clamp(h.meter('echo',0)),t=h.T||0;
  const w=b.w,ht=b.h,x0=b.x+w*.10,x1=b.x+w*.90;
  const cy=b.y+ht*.53,ac=h.col.accent,ink=h.col.text;
  const fs=Math.max(7,Math.min(13,ht*.075));
  c.save();c.lineCap='round';c.lineJoin='round';
  c.textAlign='center';c.textBaseline='middle';
  const fit=(s,width)=>{let px=fs;for(;px>7;px--){c.font='bold '+px+'px monospace';if(c.measureText(s).width<=width)break;}return 'bold '+px+'px monospace';};
  const title=morph<.15?'PLEASANT PLATE':morph>.83?'SPECTRAL ABERRATION':'HOLOGRAPHIC RECALL';
  c.fillStyle=ink;c.globalAlpha=.95;c.font=fit(title,w*.9);c.fillText(title,b.x+w*.5,b.y+ht*.10);
  // The lower half represents the real 1024-point reconstruction as spectral
  // lanes. Their drawn displacement tracks MORPH and ANGLE.
  const rows=17,spacing=(x1-x0)/(rows-1),maxH=ht*.25;
  for(let i=0;i<rows;i++){
    const pos=i/(rows-1),xi=x0+i*spacing;
    const ghostShift=morph*morph*(.08+.22*angle)*Math.sin(i*.98+angle*6.28+t*.48);
    const input=Math.pow(.84,i)*(.4+.6*Math.abs(Math.sin(i*.41+t*1.25)));
    const rise=maxH*input*(.42+echo*.58);
    c.strokeStyle=ink;c.globalAlpha=.13+.25*echo;c.lineWidth=Math.max(1,w*.005);
    c.beginPath();c.moveTo(xi,cy+maxH*.46);c.lineTo(xi,cy+maxH*.46-rise);c.stroke();
    const shifted=xi+ghostShift*w*.34;
    c.strokeStyle=ac;c.globalAlpha=.12+.66*morph*(.25+plate);
    c.lineWidth=Math.max(1.2,w*.01);
    c.beginPath();c.moveTo(shifted,cy+maxH*.46);
    c.lineTo(shifted,cy+maxH*.46-rise*(.5+.5*morph));c.stroke();
    if(morph>.20){
      c.strokeStyle=ac;c.globalAlpha=.09+.35*morph;
      c.lineWidth=Math.max(.7,w*.004);
      c.beginPath();c.moveTo(xi,cy+maxH*.46-rise*.3);
      c.lineTo(shifted,cy+maxH*.46-rise*(.5+.5*morph));c.stroke();
    }
  }
  // Optical-style curved phase fronts overlay the spectral map.
  c.strokeStyle=ac;c.lineWidth=Math.max(.7,ht*.008);
  for(let i=0;i<5;i++){
    c.globalAlpha=.03+.27*morph*(.35+.65*plate);
    const ripple=Math.sin(t*.65+i*.75+angle*6.28)*ht*.022*morph;
    c.beginPath();c.ellipse(b.x+w*.50,cy+ht*.015+ripple,w*(.12+i*.085),ht*(.025+i*.04),
                         (angle-.5)*.45,0,Math.PI*2);c.stroke();
  }
  const label='MORPH '+Math.round(morph*100)+'  /  '+(diffuse<.40?'ECHO':diffuse>.75?'PLATE':'HYBRID');
  c.globalAlpha=.83;c.fillStyle=ink;c.font=fit(label,w*.9);
  c.fillText(label,b.x+w*.5,b.y+ht*.84);
  const bar=b.y+ht*.92,bw=w*.76;
  c.globalAlpha=.23;c.fillStyle=ink;c.fillRect(b.x+w*.12,bar,bw,Math.max(2,ht*.012));
  c.globalAlpha=.95;c.fillStyle=ac;c.fillRect(b.x+w*.12,bar,bw*morph,Math.max(2,ht*.012));
  c.restore();
});