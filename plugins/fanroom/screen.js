// FAN ROOM — animated fan, moving head, rotor and gain / pan indicators.
screen('fanroom', (c, b, m, h) => {
  const t = h.T || 0;
  const rotor = .8 * Math.pow(20, Math.max(0, Math.min(1, h.eff(0))));
  const blades = Math.max(2, Math.min(6, Math.round(2 + h.eff(1) * 4)));
  const swing = .08 * Math.pow(25, Math.max(0, Math.min(1, h.eff(4))));
  const arc = Math.max(0, Math.min(1, h.eff(5)));
  const gain = Math.max(0, Math.min(1, h.meter('gain', 1)));
  const pan = Math.max(-1, Math.min(1, h.meter('pan', 0)));
  const a = Math.sin(t * swing * Math.PI * 2) * arc;
  const accent = h.col.accent, ink = h.col.text;
  const x = b.x + b.w * .50, y = b.y + b.h * .43;
  const radius = Math.max(7, Math.min(b.w * .30, b.h * .32));
  const fs = Math.max(7, Math.min(13, b.h * .09));
  c.save(); c.lineCap = 'round'; c.lineJoin = 'round';
  c.textBaseline = 'middle'; c.textAlign = 'center';
  // Stand, foot and swivel.
  c.strokeStyle = ink; c.globalAlpha = .5; c.lineWidth = Math.max(1, radius * .075);
  c.beginPath(); c.moveTo(x, y + radius * .70); c.lineTo(x, y + radius * 1.39); c.stroke();
  c.beginPath(); c.moveTo(x - radius * .57, y + radius * 1.42);
  c.quadraticCurveTo(x, y + radius * 1.27, x + radius * .57, y + radius * 1.42); c.stroke();
  c.beginPath(); c.arc(x, y + radius * .7, radius * .12, 0, Math.PI * 2);
  c.fillStyle = accent; c.globalAlpha = .72; c.fill();
  c.save();
  c.translate(x + radius * .20 * a, y);
  // Sideways head shows a narrower projected face at full left/right.
  c.scale(1 - .32 * Math.abs(a), 1);
  c.strokeStyle = ink; c.fillStyle = ink;
  c.lineWidth = Math.max(1, radius * .06); c.globalAlpha = .77;
  c.beginPath(); c.arc(0, 0, radius, 0, Math.PI * 2); c.stroke();
  c.beginPath(); c.arc(0, 0, radius * .86, 0, Math.PI * 2); c.stroke();
  // Rotating airfoils.
  c.save(); c.rotate(2 * Math.PI * t * rotor);
  c.fillStyle = accent; c.globalAlpha = .69 + .22 * (1 - gain);
  for (let i = 0; i < blades; i++) {
    c.save(); c.rotate(i * 2 * Math.PI / blades);
    c.beginPath(); c.moveTo(0, -radius * .13);
    c.bezierCurveTo(radius * .13, -radius * .60, radius * .66, -radius * .80, radius * .74, -radius * .20);
    c.bezierCurveTo(radius * .71, -radius * .01, radius * .24, -radius * .02, 0, -radius * .13);
    c.closePath(); c.fill(); c.restore();
  }
  c.restore();
  // Cage spokes and central hub.
  c.strokeStyle = ink; c.globalAlpha = .25; c.lineWidth = Math.max(.6, radius * .022);
  for (let i = 0; i < 10; i++) {
    const theta = 2 * Math.PI * i / 10;
    c.beginPath(); c.moveTo(0, 0);
    c.lineTo(Math.cos(theta) * radius * .91, Math.sin(theta) * radius * .91); c.stroke();
  }
  c.beginPath(); c.arc(0, 0, radius * .16, 0, Math.PI * 2);
  c.fillStyle = ink; c.globalAlpha = .97; c.fill();
  c.restore();
  // Adaptive text sizing as specified in AGENTS.md.
  const fontFor = (s, width) => {
    let size = fs;
    while (size > 7) {
      c.font = 'bold ' + size + 'px monospace';
      if (c.measureText(s).width <= width) break;
      size -= 1;
    }
    return 'bold ' + size + 'px monospace';
  };
  const label = 'ROTOR ' + rotor.toFixed(1) + ' Hz  /  ' + blades + ' BLADES';
  c.fillStyle = ink; c.globalAlpha = .85;
  c.font = fontFor(label, b.w * .94);
  c.fillText(label, b.x + b.w * .5, b.y + b.h * .10);
  c.font = fontFor('OSCILLATING HEAD  /  FAN ROOM', b.w * .94);
  c.fillText('OSCILLATING HEAD  /  FAN ROOM', b.x + b.w * .5, b.y + b.h * .94);
  const yBar = b.y + b.h * .83, barWidth = b.w * .25;
  const lx = b.x + b.w * .09, rx = b.x + b.w * .66;
  c.globalAlpha = .40; c.fillStyle = ink;
  c.fillRect(lx, yBar, barWidth, 3);
  c.fillRect(rx, yBar, barWidth, 3);
  c.globalAlpha = .98; c.fillStyle = accent;
  c.fillRect(lx, yBar, barWidth * gain, 3);
  c.fillRect(rx + barWidth * .5, yBar, barWidth * .5 * pan, 3);
  c.restore();
});
