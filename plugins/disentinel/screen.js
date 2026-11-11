// DI SENTINEL — live floor/reduction instrumentation, not a spectrum analyzer.
screen('disentinel', (c, b, m, h) => {
  const clamp = v => Math.max(0, Math.min(1, v));
  const floor = clamp(h.meter('floor', 0));
  const reduction = clamp(h.meter('reduction', 0));
  const noiseDb = Math.round(-100 + 80 * floor);
  const cutDb = -20 * Math.log10(Math.max(0.001, 1 - reduction));
  const sub = 12 * Math.pow(140 / 12, clamp(h.eff(5)));
  const top = 5000 * Math.pow(20000 / 5000, clamp(h.eff(6)));
  const learning = h.eff(7) >= .5;
  const accent = h.col.accent, ink = h.col.text;
  function fit(s, x, y, maxW, base, align='left') {
    let size = base;
    c.textAlign = align;
    do { c.font = `bold ${size.toFixed(1)}px monospace`; if (c.measureText(s).width <= maxW) break; size -= .5; }
    while (size > 6);
    if (c.measureText(s).width > maxW) {
      let short = s;
      while (short.length > 2 && c.measureText(short + '…').width > maxW)
        short = short.slice(0, -1);
      s = short + '…';
    }
    c.fillText(s, x, y);
  }
  const x = b.x + b.w * .055, right = b.x + b.w * .945;
  const y = b.y + b.h * .08, bh = b.h;
  c.save(); c.lineCap = 'round'; c.lineJoin = 'round';
  c.fillStyle = ink; c.globalAlpha = .95;
  fit('DI SENTINEL', x, y + bh * .08, b.w * .7, Math.max(8, bh * .12));
  c.globalAlpha = .7;
  fit(learning ? 'LEARNING' : 'AUTO FLOOR', right, y + bh * .08, b.w * .28, Math.max(6, bh * .075), 'right');
  c.globalAlpha = 1;
  const ty = y + bh * .30;
  fit(`NOISE ${noiseDb} dBFS`, x, ty, b.w*.45, Math.max(7, bh * .095));
  fit(`RED ${cutDb.toFixed(0)} dB`, right, ty, b.w*.43, Math.max(7, bh * .095), 'right');
  // Measured values shown as two readable, independent instrumentation bars.
  const bw = b.w * .89, y1 = y + bh * .41, y2 = y + bh * .53, barH = Math.max(4, bh * .045);
  c.globalAlpha = .18; c.fillStyle = ink;
  c.fillRect(x, y1, bw, barH); c.fillRect(x, y2, bw, barH);
  c.globalAlpha = .85; c.fillStyle = accent;
  c.fillRect(x, y1, bw * floor, barH); c.fillRect(x, y2, bw * reduction, barH);
  c.globalAlpha = .58; c.fillStyle = ink;
  const subTxt = `HP ${Math.round(sub)} Hz`, topTxt = `LP ${(top/1000).toFixed(1)} kHz`;
  fit(subTxt, x, y + bh * .77, b.w*.44, Math.max(7, bh*.085));
  fit(topTxt, right, y + bh * .77, b.w*.44, Math.max(7, bh*.085), 'right');
  c.restore();
});