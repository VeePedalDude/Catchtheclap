// plugins/{{ID}}/test.cpp - does {{NAME}} do what it says? `python3 tools/check.py plugins/{{ID}} --fast`
// runs it after the rig's own checks. Test each claim with a number; replace these with your effect's.
#include "rigtest.h"
#include "{{ID}}.h"
using namespace rigtest;

TEST("LEVEL sets the output level") {
  for (float level : {-12.f, 0.f}) {
    Fx<rig::{{STRUCT}}> fx; fx.set("drive", 0); fx.set("level", level);
    const Signal in = sine(48000, 375, 1, 0.01f);                // quiet, so DRIVE 0 stays clean
    const auto o = fx.run(in);
    const float got = dB(rms(o.L, 4800)) - dB(rms(in, 4800));
    CHECK(fabsf(got - level) < 0.2f, "LEVEL %+.0f dB: the output is %+.2f dB from the input", level, got);
  }
}

TEST("DRIVE adds harmonics") {
  Fx<rig::{{STRUCT}}> clean, driven; clean.set("drive", 0); driven.set("drive", 24);
  const Signal in = sine(48000, 375, 1, 0.25f);
  const auto a = clean.run(in), b = driven.run(in);
  const float h3a = toneLevel(a.L, 48000, 1125, 4800) / toneLevel(a.L, 48000, 375, 4800);
  const float h3b = toneLevel(b.L, 48000, 1125, 4800) / toneLevel(b.L, 48000, 375, 4800);
  CHECK(dB(h3b) > dB(h3a) + 20, "3rd harmonic: %.0f dB below the note at DRIVE 0, %.0f dB at DRIVE 24", -dB(h3a), -dB(h3b));
}

RIGTEST_MAIN
