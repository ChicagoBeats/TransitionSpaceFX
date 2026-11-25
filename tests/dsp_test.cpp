#include "../Source/Dsp.h"
#include <cstdio>
#include <cstdlib>
using namespace tsfx;
static const double SR = 48000;
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } } while (0)
static void run(Engine& e, std::vector<float>& L, std::vector<float>& R, const Params& p, double bpm = 120.0)
{ for (size_t i = 0; i < L.size(); i += 512) { int n = (int) std::min<size_t>(512, L.size() - i); e.process(&L[i], &R[i], n, p, bpm); } }
static float rms(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += v[i] * v[i]; return (float) std::sqrt(s / (double)(b - a)); }
int main()
{
    // 1) delay echo lands on the 1/8 note (24000 samples @120bpm would be 1/4; 1/8 = 12000)
    { Engine e; e.prepare(SR); Params p; p.fdMix = 0; p.ch[1].div = 2; p.ch[1].dlyLvl = 1; p.ch[1].dlyFb = 0; p.ch[1].color = 1;
      std::vector<float> L(48000, 0.f), R(48000, 0.f); L[0] = R[0] = 1.f; run(e, L, R, p);
      int best = 0; float bv = 0; for (int i = 100; i < 48000; i++) if (std::fabs(L[i]) > bv) { bv = std::fabs(L[i]); best = i; }
      printf("echo peak at %d (expect ~12000)\n", best); CHECK(std::abs(best - 12000) < 40 && bv > 0.05f, "echo timing"); }
    // 2) ping-pong: echo appears on right line first-order later than left
    { Engine e; e.prepare(SR); Params p; p.delayType = 1; p.fdMix = 0; p.ch[1].div = 2; p.ch[1].dlyLvl = 1; p.ch[1].dlyFb = 0.8f; p.ch[1].color = 1; p.oppositePan = false;
      std::vector<float> L(48000, 0.f), R(48000, 0.f); L[0] = R[0] = 1.f; run(e, L, R, p);
      float l1 = 0, r1 = 0, l2 = 0, r2 = 0; for (int i = 11900; i < 12100; i++) { l1 = std::max(l1, std::fabs(L[i])); r1 = std::max(r1, std::fabs(R[i])); }
      for (int i = 23900; i < 24100; i++) { l2 = std::max(l2, std::fabs(L[i])); r2 = std::max(r2, std::fabs(R[i])); }
      printf("pingpong 1st echo L %.2f R %.2f | 2nd echo L %.2f R %.2f\n", l1, r1, l2, r2);
      CHECK(l1 > 0.05f && r2 > 0.05f, "ping-pong alternates"); }
    // 3) stress: everything on, random-ish extreme settings -> finite and bounded
    { Engine e; e.prepare(SR); Params p; p.transOn = true; p.splitOn = true; p.lp.q = 15; p.hp.q = 10; p.lp.src = SrcLfo1; p.lp.egMix = 0.5f; p.hp.src = SrcUfoX;
      p.ch[0].sat = 1; p.ch[1].sat = 1; p.ch[0].lo = 1; p.ch[0].dlyFb = .95f; p.ch[1].dlyFb = .95f; p.delayType = 2;
      p.grain.mix = 0.8f; p.grain.cluster = 6; p.grain.quant = true; p.grain.density = 80; p.grain.rev = 0.5f; p.grain.pitch = 12;
      p.space.mix = 0.8f; p.space.shimmer = 1; p.space.decay = 1; p.space.type = 3; p.space.drift = 1; p.space.duck = 1;
      p.ufoAuto = true; p.ufoXSrc = 6; p.ufoYSrc = 5; for (int s = 0; s < kUfoSlots; ++s) { p.ufoAxis[s] = s % 4; p.ufoDst[s] = 1 + s * 3; }
      for (int s = 0; s < kNumSlots; ++s) { p.slotSrc[s] = 1 + s * 3; p.slotDst[s] = 1 + s * 3; p.slotAmt[s] = (s & 1) ? 1.f : -1.f; }
      for (int c = 0; c < kNumEuc; ++c) { p.euc.pulses[0][c] = 3 + c % 3; p.euc.pulses[1][c] = 2; }
      p.width = 2; p.haasMs = 20;
      float mx = 0; bool bad = false; std::vector<float> L(512), R(512);
      for (int b = 0; b < (int)(SR * 12 / 512); b++) { for (int i = 0; i < 512; i++) { L[i] = (float) rand() / RAND_MAX * 2 - 1; R[i] = (float) rand() / RAND_MAX * 2 - 1; }
        e.process(L.data(), R.data(), 512, p, 97.0);
        for (int i = 0; i < 512; i++) { if (!std::isfinite(L[i]) || !std::isfinite(R[i])) bad = true; mx = std::max(mx, std::max(std::fabs(L[i]), std::fabs(R[i]))); } }
      printf("stress max %.2f bad=%d\n", mx, (int) bad); CHECK(!bad && mx < 20.f, "stress stable"); }
    // 4) transition rises with signal
    { Engine e; e.prepare(SR); Params p; p.transOn = true; p.transTimeMs = 500; p.transHold = 1.f; std::vector<float> L(512), R(512);
      for (int b = 0; b < 60; b++) { for (int i = 0; i < 512; i++) L[i] = R[i] = 0.5f * std::sin(0.05f * (float)(i + b * 512)); e.process(L.data(), R.data(), 512, p, 120.0); }
      printf("transition = %.2f\n", e.getTransition()); CHECK(e.getTransition() > 0.9f, "transition"); }
    // 5) granular: noise burst, then the grain buffer is frozen; grains keep replaying it after the input is silent
    { auto burst = [] (std::vector<float>& L, std::vector<float>& R) { for (size_t i = 0; i < L.size(); i++) { float n = i < SR * 0.5 ? ((float) rand() / RAND_MAX * 2 - 1) * 0.5f : 0.f; L[i] = R[i] = n; } };
      Params p; p.fdMix = 1; p.grain.mix = 1; p.grain.density = 30; p.grain.pos = 0.05f; p.grain.sizeMs = 100; p.ch[0].dlyOn = false; p.lp.cut = 20000; p.wet = 1;
      { Engine e; e.prepare(SR); std::vector<float> L((size_t)(SR * 2)), R(L.size()); burst(L, R);
        for (size_t i = 0; i < L.size(); i += 512) { Params q = p; q.grain.freeze = i >= (size_t)(SR * 0.6); e.process(&L[i], &R[i], (int) std::min<size_t>(512, L.size() - i), q, 120.0); }
        float tail = rms(L, (size_t)(SR * 1.0), (size_t)(SR * 1.8)); printf("granular classic tail rms %.4f (silent input)\n", tail); CHECK(tail > 0.005f, "granular replays frozen buffer"); }
      { Engine e; e.prepare(SR); std::vector<float> L((size_t)(SR * 3)), R(L.size()); burst(L, R);
        Params r = p; r.grain.mode = 1; r.euc.pulses[0][0] = 4; r.euc.steps[0][0] = 8; r.euc.div = 6;
        for (size_t i = 0; i < L.size(); i += 512) { Params q = r; q.grain.freeze = i >= (size_t)(SR * 0.6); e.process(&L[i], &R[i], (int) std::min<size_t>(512, L.size() - i), q, 120.0); }
        float t2 = rms(L, (size_t)(SR * 1.5), (size_t)(SR * 3.0)); printf("granular rhythmic tail rms %.4f\n", t2); CHECK(t2 > 0.002f, "rhythmic grains triggered by euclid"); } }
    // 6) space reverb: tail present; short decay dies away; freeze holds steadily
    { Params p; p.space.mix = 1.f; p.fdMix = 0; p.ch[1].dlyOn = false; p.wet = 1;
      auto burst = [] (std::vector<float>& L, std::vector<float>& R) { for (size_t i = 0; i < L.size(); i++) { float n = i < SR * 0.5 ? ((float) rand() / RAND_MAX * 2 - 1) * 0.3f : 0.f; L[i] = R[i] = n; } };
      { Engine e; e.prepare(SR); Params q = p; q.space.decay = 0.1f; std::vector<float> L((size_t)(SR * 12)), R(L.size()); burst(L, R); run(e, L, R, q);
        float during = rms(L, (size_t)(SR * 0.2), (size_t)(SR * 0.5)), late = rms(L, (size_t)(SR * 10), (size_t)(SR * 12)); printf("reverb during %.4f late %.6f\n", during, late); CHECK(during > 0.02f && late < during * 0.05f, "reverb decays"); }
      { Engine e; e.prepare(SR); Params g0 = p; g0.space.decay = 0.5f; Params g = g0; g.space.freeze = true; std::vector<float> L((size_t)(SR * 3)), R(L.size()); burst(L, R);
        for (size_t i = 0; i < L.size(); i += 512) { const Params& use = (i >= (size_t)(SR * 0.6)) ? g : g0; e.process(&L[i], &R[i], (int) std::min<size_t>(512, L.size() - i), use, 120.0); }
        float a = rms(L, (size_t)(SR * 1.0), (size_t)(SR * 1.3)), b = rms(L, (size_t)(SR * 2.6), (size_t)(SR * 2.9)); printf("freeze rms %.4f -> %.4f\n", a, b); CHECK(a > 0.001f && b > a * 0.5f && b < a * 2.f, "freeze sustains steadily"); } }
    // 7) euclid: channel 1 steps 8 pulses 3 -> 3 hits per 8 ticks (1/16 @120 = 6000 samples/tick)
    { Engine e; e.prepare(SR); Params p; p.euc.div = 6; p.euc.steps[0][0] = 8; p.euc.pulses[0][0] = 3; p.euc.merge = 3; p.euc.decayMs = 20;
      int hits = 0; float prev = 0; std::vector<float> L(32), R(32);
      for (int b = 0; b < (int)(SR * 4 / 32); b++) { e.process(L.data(), R.data(), 32, p, 120.0); float v = e.getSource(SrcEuc1); if (v > 0.9f && prev <= 0.9f) hits++; prev = v; }
      int ticks = e.getEucTick(); printf("euclid hits %d over %d ticks (expect ~%d)\n", hits, ticks, ticks * 3 / 8); CHECK(std::abs(hits - ticks * 3 / 8) <= 1, "euclid pattern density"); }
    // 8) UFO auto movement
    { Engine e; e.prepare(SR); Params p; p.ufoAuto = true; p.ufoXSrc = 6; p.ufoYSrc = 5; p.ufoXAuto = 1; p.ufoYAuto = 1; p.ufoSpeed = 4;
      float mnx = 1, mxx = 0; std::vector<float> L(512, 0.f), R(512, 0.f);
      for (int b = 0; b < (int)(SR * 6 / 512); b++) { e.process(L.data(), R.data(), 512, p, 120.0); mnx = std::min(mnx, e.getX()); mxx = std::max(mxx, e.getX()); }
      printf("UFO X range %.2f..%.2f\n", mnx, mxx); CHECK(mxx - mnx > 0.2f, "UFO moves in auto"); }
    // 9) quantiser sanity: C major, note 1 (C#) snaps to C or D
    { int q = quantizeStep(1, 1, 0, 0); printf("quantize C# in C major -> %d\n", q); CHECK(q == 0 || q == 2, "quantize snaps to scale"); }
    puts(fails ? "SOME TESTS FAILED" : "ALL OK");
    return fails;
}
