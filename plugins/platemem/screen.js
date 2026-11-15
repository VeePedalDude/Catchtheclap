// PLATE MEMORY: spatial interference fringes and separate delay/reverb energy.
// The geometry is illustrative. Audio comes from the compiled modal engine.
screen('platemem', (c, b, m, h) => {
  const limit = x => Math.max(0, Math.min(1, Number.isFinite(x) ? x : 0));
  const t = h.T || 0;
  const diff = limit(h.eff(3));
  const imprint = limit(h.eff(4));
  const angle = limit(h.eff(5));
  const energy = limit(h.meter('plate', 0));
  const echo = limit(h.meter('echo', 0));
  const accent = h.col.accent, ink = h.col.text;
  const w = b.w, ht = b.h;
  const cx = b.x + w * .50, cy = b.y + ht * .49;
  const plateW = w * .80, plateH = ht * .53;
  const fs = Math.max(7, Math.min(13, ht * .075));
  c.save();
  c.lineCap = 'round';
  c.textAlign = 'center'; c.textBaseline = 'middle';
  const font = (str, maxWidth) => {
    let size = fs;
    while (size > 7) {
      c.font = 'bold ' + size + 'px monospace';
      if (c.measureText(str).width <= maxWidth) break;
      size--;
    }
    return 'bold ' + size + 'px monospace';
  };
  const title = diff < .20 ? 'DELAY MEMORY' : diff > .80 ? 'RESONANT PLATE' : 'DELAY / PLATE';
  c.fillStyle = ink; c.globalAlpha = .93;
  c.font = font(title, w * .9);
  c.fillText(title, cx, b.y + ht * .11);
  // An angled, outlined plate: the line-interference mask changes with ANGLE.
  c.save();
  c.translate(cx, cy);
  c.rotate((angle - .5) * .38);
  c.strokeStyle = ink; c.globalAlpha = .60;
  c.lineWidth = Math.max(1, ht * .012);
  c.beginPath(); c.rect(-plateW*.5, -plateH*.5, plateW, plateH); c.stroke();
  c.beginPath(); c.rect(-plateW*.45, -plateH*.43, plateW*.90, plateH*.86); c.clip();
  // Crossed phase fronts create a spatial fringe field, not a literal optical hologram.
  const lines = 12;
  for (let i = 0; i < lines; i++) {
    const xx = (i / (lines - 1) - .5) * plateW * 1.24;
    const yWave = Math.sin(t * 1.1 + i * .92) * ht * .026 * (0.1 + energy);
    c.strokeStyle = (i % 2) ? accent : ink;
    c.globalAlpha = .11 + imprint * .42 + energy * .12;
    c.lineWidth = Math.max(.7, ht * .008);
    c.beginPath(); c.moveTo(xx - plateH*.35, -plateH*.58 + yWave);
    c.quadraticCurveTo(xx, 0 + yWave, xx + plateH*.30, plateH*.58 + yWave);
    c.stroke();
  }
  for (let i = 0; i < 9; i++) {
    const yy = (i / 8 - .5) * plateH * 1.45;
    const drift = Math.cos(t*.65 + i*.6 + angle*2*Math.PI) * plateW * .035;
    c.strokeStyle = accent; c.globalAlpha = .06 + imprint * .26 + energy * .1;
    c.beginPath();c.moveTo(-plateW*.58, yy-drift*.05);
    c.quadraticCurveTo(drift, yy + plateH*.10*Math.sin(i*.7 + t*.33), plateW*.58, yy + drift*.05);
    c.stroke();
  }
  c.restore();
  // Discrete repeats morph into a continuous reverb energy bar.
  const barY = b.y + ht * .83, left = b.x + w * .10, barWidth = w * .80;
  c.globalAlpha = .20; c.fillStyle = ink;
  c.fillRect(left, barY, barWidth, Math.max(2, ht*.014));
  for (let i = 0; i < 8; i++) {
    const x = left + (i+.5)/8*barWidth;
    const repetition = (1-diff)*Math.pow(.73,i);
    const haze = diff*(.20+.8*Math.exp(-i*.22));
    const strength = repetition*echo + haze*energy;
    c.globalAlpha = Math.min(.9,.18 + strength*.75);
    c.fillStyle = accent;
    c.fillRect(x - barWidth*.02, barY-ht*.05*(repetition + haze), barWidth*.04, ht*.05*(repetition + haze));
  }
  const label = 'ECHO ' + Math.round(echo * 100) + '   /   PLATE ' + Math.round(energy * 100);
  c.globalAlpha = .84; c.fillStyle = ink; c.font = font(label, w*.94);
  c.fillText(label, cx, b.y + ht * .94);
  c.restore();
});