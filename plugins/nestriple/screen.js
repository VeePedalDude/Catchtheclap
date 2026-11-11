// BITROT MATRIX: FM-synth algorithm cards with real audio links and selectable regenerative loops.
screen('nestriple', (c,b,m,h) => {
  const names=['SERIES','PARALLEL','FORK','MERGE','REVERSE','LADDER','CENTER','BRIDGE'];
  const returns=['WRAP','MIDDLE','SPLIT','LOCAL','CHAIN','CROSS'];
  const order=Math.max(0,Math.min(7,Math.round(h.eff(4)*7)));
  const ret=Math.max(0,Math.min(5,Math.round(h.eff(6)*5)));
  const grids=[
    [[.23,.43],[.50,.43],[.77,.43]], // A-B-C
    [[.49,.19],[.49,.43],[.49,.67]], // independent A/B/C
    [[.24,.43],[.74,.23],[.74,.63]], // A forks
    [[.24,.23],[.24,.63],[.74,.43]], // merge
    [[.77,.43],[.50,.43],[.23,.43]], // reverse C-B-A
    [[.24,.43],[.50,.23],[.77,.43]], // ladder A -> B and C
    [[.73,.23],[.24,.43],[.73,.63]], // center B forks
    [[.24,.23],[.50,.23],[.73,.63]]  // bridge A->B->C, input at C
  ];
  const dry=[[0],[0,1,2],[0],[0,1],[2],[0],[1],[0,2]];
  const links=[
    [[0,1],[1,2]],[],[[0,1],[0,2]],[[0,2],[1,2]],
    [[2,1],[1,0]],[[0,1],[0,2],[1,2]],[[1,0],[1,2]]
  ];
  const feedback=[
    [[2,0]],[[2,1]],[[2,0],[2,1]],[[0,0],[1,1],[2,2]],
    [[1,0],[2,1]],[[2,0],[0,2]]
  ];
  const w=b.w,hh=b.h,sz=Math.max(11,Math.min(w*.13,hh*.22));
  const pts=grids[order].map(p=>({x:b.x+p[0]*w,y:b.y+p[1]*hh}));
  const line=(x1,y1,x2,y2,color,opacity)=>{
    c.strokeStyle=color;c.globalAlpha=opacity;c.beginPath();c.moveTo(x1,y1);c.lineTo(x2,y2);c.stroke();
    const angle=Math.atan2(y2-y1,x2-x1);
    const ar=Math.max(3,sz*.14);
    c.beginPath();c.moveTo(x2,y2);
    c.lineTo(x2-ar*Math.cos(angle-.55),y2-ar*Math.sin(angle-.55));
    c.moveTo(x2,y2);c.lineTo(x2-ar*Math.cos(angle+.55),y2-ar*Math.sin(angle+.55));c.stroke();
  };
  c.save();
  c.lineWidth=Math.max(1,hh*.014);c.lineJoin='round';c.lineCap='round';
  for(const [src,dst] of links[order]){
    const p=pts[src],q=pts[dst];
    const dx=q.x-p.x,dy=q.y-p.y,mag=Math.max(1,Math.hypot(dx,dy));
    const off=sz*.62;
    line(p.x+dx/mag*off,p.y+dy/mag*off,q.x-dx/mag*off,q.y-dy/mag*off,h.col.text,.70);
  }
  // Small input arrows show the parallel and bridge injection paths.
  for(const k of dry[order]){
    const p=pts[k];
    line(p.x-sz*.91,p.y,p.x-sz*.58,p.y,h.col.text,.48);
  }
  // Regenerative arrows use the skin accent and return to the selected operators.
  feedback[ret].forEach(([src,dst],i)=>{
    const p=pts[src],q=pts[dst];
    const sx=p.x,sy=p.y+sz*.57,ex=q.x,ey=q.y+sz*.57;
    const curveY=b.y+hh*(.76+i*.035);
    c.strokeStyle=h.col.accent;c.globalAlpha=.85;c.setLineDash([Math.max(2,sz*.18),Math.max(2,sz*.1)]);
    c.beginPath();c.moveTo(sx,sy);
    if(src===dst){
      c.bezierCurveTo(sx+sz*.7,curveY,sx-sz*.7,curveY,ex,ey);
    }else{
      c.bezierCurveTo(sx,curveY,ex,curveY,ex,ey);
    }
    c.stroke();c.setLineDash([]);
    const pointUp=sz*.24;
    c.beginPath();c.moveTo(ex-pointUp*.65,ey+pointUp);
    c.lineTo(ex,ey);c.lineTo(ex+pointUp*.65,ey+pointUp);c.stroke();
  });
  c.font='bold '+Math.max(9,Math.min(16,sz*.6))+'px monospace';
  c.textBaseline='middle';c.textAlign='center';
  pts.forEach((p,i)=>{
    c.globalAlpha=.96;c.fillStyle=h.col.text;
    c.strokeStyle=h.col.text;
    c.fillRect(p.x-sz*.5,p.y-sz*.5,sz,sz);
    c.fillStyle=h.col.accent;c.fillText('ABC'[i],p.x,p.y+sz*.035);
  });
  c.globalAlpha=.92;c.fillStyle=h.col.text;
  c.font='bold '+Math.max(9,Math.min(13,hh*.115))+'px monospace';
  c.textAlign='left';c.fillText(names[order],b.x+7,b.y+hh*.88);
  c.textAlign='right';c.fillStyle=h.col.accent;
  c.fillText(returns[ret]+' '+Math.round(h.eff(5)*80)+'%',b.x+w-7,b.y+hh*.88);
  c.restore();
});
