// FM-operator style routing map; shows the six ORDER permutations and the active feedback return.
screen('nestriple', (c, b, m, h) => {
  const routes = [[0,1,2],[0,2,1],[1,0,2],[1,2,0],[2,0,1],[2,1,0]];
  const names = ['A', 'B', 'C'];
  const order = Math.max(0, Math.min(5, Math.round(h.eff(4) * 5)));
  const back = h.eff(6) >= 0.5 ? 1 : 0;
  const fb = Math.round(h.eff(5) * 80);
  const w = b.w / 6.5, bh = b.h * .29;
  const cy = b.y + b.h * .23, gap = (b.w - 3*w) / 4;
  const xs = [b.x + gap, b.x + 2*gap+w, b.x+3*gap+2*w];
  const yBottom = cy + bh;
  c.save();
  c.lineWidth = Math.max(1, b.h * .015);
  c.font = 'bold ' + Math.max(9, Math.min(16, b.h*.19)) + 'px monospace';
  c.textAlign = 'center'; c.textBaseline = 'middle';
  c.strokeStyle = h.col.text; c.fillStyle = h.col.text;
  for (let p=0;p<3;p++) {
    c.globalAlpha=.9;
    c.strokeRect(xs[p], cy, w, bh);
    c.fillText(names[routes[order][p]], xs[p]+w/2,cy+bh/2);
    if(p<2){
      const x=xs[p]+w, nx=xs[p+1], midY=cy+bh/2;
      c.beginPath();c.moveTo(x,midY);c.lineTo(nx,midY);c.stroke();
      c.beginPath();c.moveTo(nx-5,midY-4);c.lineTo(nx,midY);c.lineTo(nx-5,midY+4);c.stroke();
    }
  }
  c.strokeStyle=h.col.accent; c.fillStyle=h.col.accent;
  c.globalAlpha=.9;
  const bx=xs[2]+w/2, tx=xs[back]+w/2, by=b.y+b.h*.78;
  c.beginPath();c.moveTo(bx,yBottom);c.lineTo(bx,by);c.lineTo(tx,by);c.lineTo(tx,yBottom+2);c.stroke();
  c.beginPath();c.moveTo(tx-4,yBottom+7);c.lineTo(tx,yBottom+2);c.lineTo(tx+4,yBottom+7);c.stroke();
  c.font=Math.max(8,Math.min(12,b.h*.12))+'px monospace';
  c.textAlign='center'; c.fillText('FB '+fb+'%',(bx+tx)/2,by+Math.min(12,b.h*.12));
  c.restore();
});
