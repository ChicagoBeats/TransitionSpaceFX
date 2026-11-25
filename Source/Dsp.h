// Transition Space FX 2 - DSP core (no JUCE dependency, unit-testable).
// Original engine combining ideas from: wave-transition ambient synths (Super Modulation UFO, filter EGs, LFO bank,
// step modulator), granular multi-FX (classic / rhythmic / texture grains, scale quantise, chord clusters),
// "space" reverbs (size, decay to infinite, drift, tone, shimmer, freeze, duck, sync), and Euclidean polyrhythm
// sequencing (two merged layers per channel).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tsfx
{
constexpr int kNumLfo = 5, kNumSlots = 8, kNumSteps = 8, kNumEuc = 8, kUfoSlots = 8, kChunk = 32;
constexpr double kPi = 3.14159265358979323846;

enum Src { SrcOff = 0, SrcLfo1, SrcLfo2, SrcLfo3, SrcLfo4, SrcPlfo, SrcSlfo, SrcSH, SrcStep, SrcEnv, SrcEg, SrcUfoX, SrcUfoY, SrcTrans,
           SrcEuc1, SrcEuc2, SrcEuc3, SrcEuc4, SrcEuc5, SrcEuc6, SrcEuc7, SrcEuc8, SrcMacro1, SrcMacro2, SrcMacro3, SrcMacro4, SrcCount };
// NOTE: F/D pairs stay adjacent (index + channel)
enum Dst { DstNone = 0, DstLpCut, DstHpCut, DstLpQ, DstHpQ, DstFdMix, DstLpHpMix, DstDlyTimeF, DstDlyTimeD, DstDlyFbF, DstDlyFbD,
           DstPanF, DstPanD, DstSatF, DstSatD, DstWet, DstGrnMix, DstGrnPos, DstGrnSize, DstGrnDens, DstGrnPitch,
           DstRevMix, DstRevSize, DstRevDecay, DstShimmer, DstWidth, DstCount };

inline const char* const kSrcNames[SrcCount] = { "Off", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "PLFO", "S LFO", "S&H", "Step", "Env Follow", "Filter EG",
    "UFO X", "UFO Y", "Transition", "Euclid 1", "Euclid 2", "Euclid 3", "Euclid 4", "Euclid 5", "Euclid 6", "Euclid 7", "Euclid 8",
    "Macro 1", "Macro 2", "Macro 3", "Macro 4" };
inline const char* const kDstNames[DstCount] = { "None", "LP Cut", "HP Cut", "LP Res", "HP Res", "D:F Mix", "LP:HP Mix", "Dly Time F", "Dly Time D",
    "Dly Fdbk F", "Dly Fdbk D", "Pan F", "Pan D", "Sat F", "Sat D", "Wet", "Grain Mix", "Grain Pos", "Grain Size", "Grain Dens", "Grain Pitch",
    "Space Mix", "Space Size", "Space Decay", "Shimmer", "Width" };
inline const char* const kLfoShapeNames[5] = { "Sine", "Triangle", "Saw", "Square", "Smooth Rnd" };
inline const char* const kSyncNames[11] = { "Free", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T", "1/4.", "1/8." };
inline constexpr float kSyncBeats[11] = { 0.f, 8.f, 4.f, 2.f, 1.f, .5f, .25f, 2.f / 3.f, 1.f / 3.f, 1.5f, .75f };
inline const char* const kDlyNames[11] = { "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T", "1/4.", "1/8.", "Grv1", "Grv2", "Grv3" };
inline constexpr float kDlyBeats[11] = { 2.f, 1.f, .5f, .25f, 2.f / 3.f, 1.f / 3.f, 1.5f, .75f, .375f, .625f, .875f };
inline const char* const kUfoAxisNames[5] = { "X pos", "X inv", "Y pos", "Y inv", "Off" };
inline const char* const kUfoAutoNames[7] = { "LFO 1", "LFO 2", "LFO 3", "LFO 4", "S LFO", "S&H", "Bounce" };
inline const char* const kGrainModes[3] = { "Classic", "Rhythmic", "Texture" };
inline const char* const kScaleNames[7] = { "Chromatic", "Major", "Minor", "Dorian", "Pentatonic", "Whole Tone", "Lydian" };
inline const char* const kClusterNames[7] = { "Off", "Octaves", "Fifths", "Major", "Minor", "Sus", "Cluster" };
inline const char* const kRevTypes[4] = { "Hall", "Room", "Plate", "Blackhole" };
inline const char* const kDelayTypes[3] = { "Normal", "Ping-Pong", "Tape" };
inline const char* const kStepModes[4] = { "Forward", "Backward", "Ping-Pong", "Random" };
inline const char* const kMergeNames[5] = { "OR", "XOR", "AND", "A only", "B only" };
inline const char* const kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

inline bool srcIsUnipolar (int s) { return s == SrcEnv || s == SrcEg || s == SrcTrans || (s >= SrcEuc1 && s <= SrcMacro4); }

struct ChanParams { float lo = 0.f, sat = 0.f, dlyLvl = 0.3f, dlyFb = 0.4f, pan = 0.f, color = 0.5f; int div = 1; bool dlyOn = true; };
struct FiltP { float cut = 8000.f, q = 0.7071f, depth = 0.5f, egMix = 0.f, a = 0.01f, d = 0.3f, s = 0.5f, r = 0.5f; int src = 0; bool inv = false; };
struct LfoP { int shape = 0; float rate = 1.f, depth = 1.f; int sync = 0; bool ks = false; };
struct GrainP { float mix = 0.f, sizeMs = 120.f, density = 12.f, pos = 0.3f, pitch = 0.f, spread = 0.5f, jitter = 0.3f, rev = 0.f;
                int mode = 0, scale = 0, root = 0, cluster = 0; bool freeze = false, quant = false; };
struct SpaceP { float size = 0.5f, decay = 0.5f, drift = 0.2f, tone = 0.7f, shimmer = 0.f, duck = 0.f, mix = 0.f;
                bool freeze = false, sync = false; int type = 0; };
struct EucP
{
    int steps[2][kNumEuc], pulses[2][kNumEuc], rot[2][kNumEuc];
    int merge = 0, div = 6, grainCh = 0; float decayMs = 150.f;
    EucP() { for (int l = 0; l < 2; ++l) for (int c = 0; c < kNumEuc; ++c) { steps[l][c] = 8 + (c % 3) * 2 - (l ? 1 : 0); pulses[l][c] = 0; rot[l][c] = 0; } }
};

struct Params
{
    FiltP lp, hp;
    float lpHpMix = 0.f, fdMix = 0.5f;
    ChanParams ch[2];
    bool oppositePan = true, splitOn = false;
    float splitFreq = 200.f;
    int delayType = 0; float tape = 0.3f;
    float wet = 1.f, outGainDb = 0.f, width = 1.f, haasMs = 0.f; int haasSide = 1; bool engage = true;
    bool transOn = false; float transTimeMs = 1500.f, transRelMs = 1500.f, transHold = 0.5f; int transSrc = 0; bool transInv = false;
    LfoP lfo[kNumLfo];
    float shRate = 4.f, shVar = 0.3f;
    int stepRate = 6; float stepSmooth = 0.2f; int stepMode = 0; bool stepKeyReset = false; float step[kNumSteps] = {};
    GrainP grain; SpaceP space; EucP euc;
    float macro[4] = { 0, 0, 0, 0 };
    float ufoX = 0.5f, ufoY = 0.5f; bool ufoAuto = false, ufoCenter = true; int ufoXSrc = 0, ufoYSrc = 5;
    float ufoXAuto = 0.5f, ufoYAuto = 0.5f, ufoSpeed = 1.f;
    int ufoAxis[kUfoSlots] = { 0, 2, 4, 4, 4, 4, 4, 4 };
    int ufoDst[kUfoSlots] = { DstLpCut, DstRevMix, DstNone, DstNone, DstNone, DstNone, DstNone, DstNone };
    float ufoAmt[kUfoSlots] = { 1, 1, 1, 1, 1, 1, 1, 1 };
    int slotSrc[kNumSlots] = {}, slotDst[kNumSlots] = {}; float slotAmt[kNumSlots] = {};
    Params() { lp.cut = 8000.f; hp.cut = 60.f; }
};

//==============================================================================
struct Rng
{
    uint32_t s = 0x1234567u;
    float bip() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) s / 4294967295.f * 2.f - 1.f; }
    float uni() { return bip() * 0.5f + 0.5f; }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] = { 0, 0 }, z2[2] = { 0, 0 };
    static double clampF (double fs, double f) { return std::min (std::max (f, 10.0), fs * 0.45); }
    void set (double a0, double nb0, double nb1, double nb2, double na1, double na2)
    { b0 = (float) (nb0 / a0); b1 = (float) (nb1 / a0); b2 = (float) (nb2 / a0); a1 = (float) (na1 / a0); a2 = (float) (na2 / a0); }
    void setLP (double fs, double f, double q) { double w = 2 * kPi * clampF (fs, f) / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * q); set (1 + al, (1 - c) / 2, 1 - c, (1 - c) / 2, -2 * c, 1 - al); }
    void setHP (double fs, double f, double q) { double w = 2 * kPi * clampF (fs, f) / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * q); set (1 + al, (1 + c) / 2, -(1 + c), (1 + c) / 2, -2 * c, 1 - al); }
    void setLowShelf (double fs, double f, double gainDb)
    {
        double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * clampF (fs, f) / fs, c = std::cos (w), s = std::sin (w);
        double al = s * 0.70710678, sq = 2 * std::sqrt (A) * al;
        set ((A + 1) + (A - 1) * c + sq, A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    float process (float x, int ch) { float y = b0 * x + z1[ch]; z1[ch] = b1 * x - a1 * y + z2[ch]; z2[ch] = b2 * x - a2 * y; return y; }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
};

struct Adsr
{
    enum { Idle, Atk, Dec, Sus, Rel } st = Idle;
    float level = 0.f;
    void reset() { st = Idle; level = 0.f; }
    float step (bool gate, bool opened, float a, float d, float s, float r, float dt)
    {
        if (opened) st = Atk;
        else if (! gate && st != Idle && st != Rel) st = Rel;
        switch (st)
        {
            case Atk: level += dt / std::max (0.0005f, a); if (level >= 1.f) { level = 1.f; st = Dec; } break;
            case Dec: { level += (s - level) * (1.f - std::exp (-dt * 4.6f / std::max (0.005f, d))); if (std::fabs (level - s) < 0.0005f) { level = s; st = Sus; } } break;
            case Sus: level = s; break;
            case Rel: { level -= level * (1.f - std::exp (-dt * 4.6f / std::max (0.005f, r))); if (level < 0.0005f) { level = 0.f; st = Idle; } } break;
            default: level = 0.f; break;
        }
        return level;
    }
};

inline float softLimit (float x)
{
    const float a = std::fabs (x);
    if (a <= 0.6f) return x;
    const float y = 0.6f + 0.291f * std::tanh ((a - 0.6f) / 0.291f); // ceiling about -1 dBFS
    return x < 0.f ? -y : y;
}

//==============================================================================
// Granular engine (live input ring buffer). Modes: Classic (random clock), Rhythmic (Euclid-triggered), Texture (dense + wide).
inline const int* scaleTable (int s, int& n)
{
    static const int chr[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, maj[7] = { 0, 2, 4, 5, 7, 9, 11 }, mnr[7] = { 0, 2, 3, 5, 7, 8, 10 },
        dor[7] = { 0, 2, 3, 5, 7, 9, 10 }, pen[5] = { 0, 2, 4, 7, 9 }, wht[6] = { 0, 2, 4, 6, 8, 10 }, lyd[7] = { 0, 2, 4, 6, 7, 9, 11 };
    switch (s) { case 1: n = 7; return maj; case 2: n = 7; return mnr; case 3: n = 7; return dor; case 4: n = 5; return pen; case 5: n = 6; return wht; case 6: n = 7; return lyd; default: n = 12; return chr; }
}
inline int quantizeStep (int note, int scale, int root, int steps)
{
    int n; const int* sc = scaleTable (scale, n);
    const int rel = note - root, oct = (int) std::floor ((double) rel / 12.0), r = rel - 12 * oct;
    int best = 0, bd = 99;
    for (int i = 0; i < n; ++i) { const int d = std::abs (sc[i] - r); if (d < bd) { bd = d; best = i; } }
    const int idx = best + steps, os = (int) std::floor ((double) idx / (double) n), di = idx - os * n;
    return root + 12 * (oct + os) + sc[di];
}
inline int clusterVoices (int c) { static const int v[7] = { 1, 3, 3, 3, 3, 3, 4 }; return v[std::min (std::max (c, 0), 6)]; }
inline int clusterInterval (int c, int k)
{
    static const int t[7][4] = { { 0, 0, 0, 0 }, { 0, 12, -12, 0 }, { 0, 7, 12, 0 }, { 0, 4, 7, 0 }, { 0, 3, 7, 0 }, { 0, 5, 7, 0 }, { 0, 1, 2, 3 } };
    return t[std::min (std::max (c, 0), 6)][std::min (std::max (k, 0), 3)];
}

class Granular
{
public:
    struct Run { float mix = 0, sizeMs = 120, density = 12, pos = .3f, pitch = 0, spread = .5f, jitter = .3f, rev = 0;
                 int mode = 0, scale = 0, root = 0, cluster = 0; bool freeze = false, quant = false, trig = false; };
    void prepare (double sampleRate)
    {
        sr = sampleRate; size = (int) (6.0 * sr) + 8;
        for (auto& b : buf) b.assign ((size_t) size, 0.f);
        w = 0; countdown = 0;
        for (auto& g : grains) g.active = false;
        for (int i = 0; i <= 1024; ++i) win[i] = 0.5f - 0.5f * std::cos (2.f * (float) kPi * (float) i / 1024.f);
    }
    void process (float inL, float inR, float& oL, float& oR, const Run& r)
    {
        if (! r.freeze) { buf[0][(size_t) w] = inL; buf[1][(size_t) w] = inR; if (++w >= size) w = 0; }
        const float sizeMs = r.mode == 2 ? r.sizeMs * 2.f : r.sizeMs, dens = r.mode == 2 ? r.density * 2.f : r.density;
        bool fire = false;
        if (r.mode == 1) fire = r.trig;
        else if (--countdown <= 0) { fire = true; countdown = (int) std::max (1.0, sr / (double) std::max (0.5f, dens) * (double) (1.f - r.jitter * 0.5f + r.jitter * rng.uni())); }
        if (fire) spawn (r, sizeMs);
        float l = 0.f, rr = 0.f;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            const float wv = win[std::min (1024, (g.age * 1024) / g.len)];
            int i0 = (int) g.pos; const float fr = (float) (g.pos - (double) i0);
            if (i0 < 0) i0 += size;
            if (i0 >= size) i0 -= size;
            int i1 = i0 + 1; if (i1 >= size) i1 = 0;
            l += wv * g.l * (buf[0][(size_t) i0] + (buf[0][(size_t) i1] - buf[0][(size_t) i0]) * fr);
            rr += wv * g.r * (buf[1][(size_t) i0] + (buf[1][(size_t) i1] - buf[1][(size_t) i0]) * fr);
            g.pos += g.inc;
            if (g.pos >= (double) size) g.pos -= (double) size; else if (g.pos < 0.0) g.pos += (double) size;
            if (++g.age >= g.len) g.active = false;
        }
        const float ng = 1.f / std::sqrt (std::max (1.f, dens * sizeMs / 1000.f * (float) clusterVoices (r.cluster)));
        oL = l * ng; oR = rr * ng;
    }

private:
    struct Grain { bool active = false; double pos = 0, inc = 1; int age = 0, len = 1; float l = 1, r = 1; };
    void spawn (const Run& r, float sizeMs)
    {
        const int nv = clusterVoices (r.cluster), len = std::max (64, (int) (sizeMs * 0.001f * (float) sr));
        for (int k = 0; k < nv; ++k)
        {
            Grain* g = nullptr;
            for (auto& x : grains) if (! x.active) { g = &x; break; }
            if (! g) return;
            float semis = r.pitch + (float) clusterInterval (r.cluster, k);
            if (r.quant) semis = (float) quantizeStep ((int) std::lround (semis), r.scale, r.root, (int) (rng.uni() * 5.f) - 2);
            const double ratio = std::exp2 ((double) semis / 12.0);
            double back = (double) r.pos * 4.0 * sr + (double) len * std::max (1.0, ratio) + (double) (rng.bip() * r.jitter * 0.1f) * sr;
            back = std::min (std::max (back, (double) len + 64.0), (double) size - 64.0);
            const bool rev = rng.uni() < r.rev;
            double start = (double) w - back + (rev ? (double) len * ratio : 0.0);
            while (start < 0.0) start += (double) size;
            while (start >= (double) size) start -= (double) size;
            g->active = true; g->pos = start; g->inc = rev ? -ratio : ratio; g->len = len; g->age = 0;
            const float pn = rng.bip() * r.spread;
            g->l = std::cos ((pn + 1.f) * (float) kPi * 0.25f) * 1.4142f; g->r = std::sin ((pn + 1.f) * (float) kPi * 0.25f) * 1.4142f;
        }
    }
    double sr = 44100.0;
    std::vector<float> buf[2];
    int size = 1, w = 0, countdown = 0;
    Grain grains[48];
    float win[1025] = {};
    Rng rng;
};

//==============================================================================
// Octave-up pitch shifter for the shimmer feedback path
class PitchUp
{
public:
    void prepare() { buf.assign (N, 0.f); w = 0; ph = 0.f; }
    float process (float x)
    {
        buf[(size_t) w] = x;
        float out = 0.f;
        for (int t = 0; t < 2; ++t)
        {
            float p = ph + 0.5f * (float) t; if (p >= 1.f) p -= 1.f;
            const float d = (1.f - p) * (float) D + 2.f;
            float rp = (float) w - d; if (rp < 0.f) rp += (float) N;
            const int i0 = (int) rp; const float fr = rp - (float) i0;
            const int i1 = (i0 + 1) % N;
            const float s = buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * fr, sn = std::sin ((float) kPi * p);
            out += s * sn * sn;
        }
        ph += 1.f / (float) D; if (ph >= 1.f) ph -= 1.f;
        w = (w + 1) % N;
        return out;
    }
private:
    static constexpr int N = 8192, D = 2048;
    std::vector<float> buf; int w = 0; float ph = 0.f;
};

// "Space" reverb: 8-line feedback delay network (Householder), drift modulation, tone, decay up to infinite, freeze, shimmer.
class SpaceVerb
{
public:
    struct Run { float size = .5f, decay = .5f, drift = .2f, tone = .7f, shimmer = 0.f, wetGain = 1.f; int type = 0; bool freeze = false; double preSamples = 0; };
    void prepare (double sampleRate)
    {
        sr = sampleRate; maxLen = (int) (0.6 * sr);
        for (int i = 0; i < N; ++i) { line[i].assign ((size_t) maxLen + 4, 0.f); wp[i] = 0; lp[i] = 0.f; ph[i] = (float) i * 0.7f; }
        preSize = (int) (1.5 * sr) + 4; pre[0].assign ((size_t) preSize, 0.f); pre[1].assign ((size_t) preSize, 0.f); pw = 0; inGain = 1.f;
        shim.prepare();
    }
    void process (float inL, float inR, float& oL, float& oR, const Run& r)
    {
        pre[0][(size_t) pw] = inL; pre[1][(size_t) pw] = inR;
        const double pd = std::min (std::max (r.preSamples, 0.0), (double) preSize - 4.0);
        double prp = (double) pw - pd; if (prp < 0) prp += (double) preSize;
        const int p0 = (int) prp, p1 = (p0 + 1) % preSize; const float pf = (float) (prp - (double) p0);
        const float pL = pre[0][(size_t) p0] + (pre[0][(size_t) p1] - pre[0][(size_t) p0]) * pf;
        const float pR = pre[1][(size_t) p0] + (pre[1][(size_t) p1] - pre[1][(size_t) p0]) * pf;
        if (++pw >= preSize) pw = 0;

        static const float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 79.3f };
        const float typeScale = r.type == 1 ? 0.4f : r.type == 2 ? 0.6f : r.type == 3 ? 1.2f : 1.f;
        const float scale = (0.4f + 2.6f * r.size) * typeScale;
        const float t60 = 2.f * std::pow (30.f, r.decay);
        const bool inf = r.freeze || r.decay >= 0.995f;
        const float fc = 1200.f * std::pow (20000.f / 1200.f, r.tone);
        const float a = r.freeze ? 1.f : 1.f - std::exp (-2.f * (float) kPi * fc / (float) sr);
        inGain += ((r.freeze ? 0.f : 1.f) - inGain) * 0.002f;

        float y[N], sum = 0.f, eL = 0.f, eR = 0.f;
        float d[N];
        for (int i = 0; i < N; ++i)
        {
            ph[i] += 2.f * (float) kPi * (0.07f + 0.031f * (float) i) / (float) sr; if (ph[i] > 6.2831853f) ph[i] -= 6.2831853f;
            d[i] = baseMs[i] * 0.001f * (float) sr * scale + r.drift * std::sin (ph[i]) * 0.0006f * (float) sr;
            d[i] = std::min (std::max (d[i], 8.f), (float) maxLen - 4.f);
            float rp = (float) wp[i] - d[i]; if (rp < 0.f) rp += (float) (maxLen + 4);
            const int i0 = (int) rp, i1 = (i0 + 1) % (maxLen + 4); const float fr = rp - (float) i0;
            float v = line[i][(size_t) i0] + (line[i][(size_t) i1] - line[i][(size_t) i0]) * fr;
            lp[i] += a * (v - lp[i]); v = lp[i];
            y[i] = v; sum += v;
            if (i & 1) eR += v; else eL += v;
        }
        const float hh = 2.f / (float) N * sum;
        const float sh = r.shimmer > 0.001f ? shim.process ((eL + eR) * 0.25f) * r.shimmer * 0.7f : 0.f;
        for (int i = 0; i < N; ++i)
        {
            float g = inf ? 1.f : std::pow (10.f, -3.f * d[i] / (float) sr / t60);
            if (r.type == 3 && ! r.freeze) g = 0.985f + r.decay * 0.035f; // blackhole: above ~43% decay the tail swells instead of fading (soft-clipped)
            float v = (y[i] - hh) * g + ((i & 1) ? pR : pL) * inGain * 0.35f + sh * 0.35f;
            v = r.type == 3 ? std::tanh (v) : std::min (std::max (v, -8.f), 8.f);
            line[i][(size_t) wp[i]] = v;
            if (++wp[i] >= maxLen + 4) wp[i] = 0;
        }
        oL = eL * 0.8f * r.wetGain; oR = eR * 0.8f * r.wetGain;
    }
private:
    static constexpr int N = 8;
    double sr = 44100.0;
    std::vector<float> line[N], pre[2];
    int wp[N] = {}, maxLen = 1, preSize = 1, pw = 0;
    float lp[N] = {}, ph[N] = {}, inGain = 1.f;
    PitchUp shim;
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int size = (int) (4.5 * sr) + 8;
        for (auto& c : ch)
        {
            c.size = size; c.w = 0; c.buf[0].assign ((size_t) size, 0.f); c.buf[1].assign ((size_t) size, 0.f);
            c.lp[0] = c.lp[1] = 0; c.dSmp = c.dTarget = 0.25f * (float) sr;
            c.shelf.reset(); c.xo1.reset(); c.xo2.reset(); c.satS = 0; c.panS = 0;
        }
        lp1.reset(); lp2.reset(); hp.reset(); lpEg.reset(); hpEg.reset();
        gran.prepare (sr); space.prepare (sr);
        haasSize = (int) (0.04 * sr) + 4; haasBuf[0].assign ((size_t) haasSize, 0.f); haasBuf[1].assign ((size_t) haasSize, 0.f); haasW = 0;
        env = 0; gate = false; attacking = false; transPos = 0; fAmtS = 0.5f; engS = 1.f; duckS = 1.f;
        sh = shTarget = 0; stepVal = 0; stepIdx = 0; stepPh = 0; stepDir = 1;
        for (int k = 0; k < kNumLfo; ++k) { lfoPhase[k] = 0; lfoR0[k] = 0; lfoR1[k] = rng.bip(); }
        for (int c = 0; c < kNumEuc; ++c) eucEnv[c] = 0.f;
        eucTick = 0; eucPh = 0; std::fill (lastSv, lastSv + SrcCount, 0.f);
    }

    float getX() const { return ufoX; }
    float getY() const { return ufoY; }
    float getTransition() const { return transPos; }
    float getSource (int i) const { return lastSv[i]; }
    float getCutNorm() const { return cutNorm; }
    int getStepIndex() const { return stepIdx; }
    int getEucTick() const { return eucTick; }

    void process (float* L, float* R, int n, const Params& p, double bpm)
    {
        bpm = std::min (std::max (bpm, 40.0), 300.0);
        const double spb = 60.0 / bpm;
        for (int pos = 0; pos < n; pos += kChunk)
        {
            const int m = std::min (kChunk, n - pos);
            control (L + pos, R + pos, m, p, spb);
            audio (L + pos, R + pos, m, p);
        }
    }

private:
    struct Chan
    {
        Biquad shelf, xo1, xo2;
        std::vector<float> buf[2];
        int size = 0, w = 0;
        float lp[2] = { 0, 0 }, dSmp = 0, dTarget = 0, satS = 0, panS = 0, fb = 0, lvl = 0, panT = 0, satT = 0, cc = 0.5f, tapeSat = 1.f;
    };
    static float lerp (float a, float b, float t) { return a + (b - a) * t; }
    static float clampf (float v, float lo, float hi) { return std::min (std::max (v, lo), hi); }
    static float lfoWave (int shape, double ph, float r0, float r1)
    {
        switch (shape)
        {
            case 0: return (float) std::sin (2 * kPi * ph);
            case 1: return ph < 0.5 ? (float) (4 * ph - 1) : (float) (3 - 4 * ph);
            case 2: return (float) (2 * ph - 1);
            case 3: return ph < 0.5 ? 1.f : -1.f;
            default: { float t = (float) ph; t = t * t * (3 - 2 * t); return r0 + (r1 - r0) * t; }
        }
    }
    static bool eucHit (int steps, int pulses, int rot, int tick)
    {
        if (steps < 1) return false;
        pulses = std::min (pulses, steps);
        if (pulses <= 0) return false;
        const int i = (((tick - rot) % steps) + steps) % steps;
        return ((i * pulses) % steps) < pulses;
    }

    void control (const float* l, const float* r, int m, const Params& p, double spb)
    {
        const double dt = (double) m / sr;
        const float fdt = (float) dt;

        float peak = 0;
        for (int i = 0; i < m; ++i) peak = std::max (peak, std::max (std::fabs (l[i]), std::fabs (r[i])));
        const float cA = 1.f - (float) std::exp (-dt / 0.005), cR = 1.f - (float) std::exp (-dt / 0.15);
        env += (peak - env) * (peak > env ? cA : cR);
        const float envDb = 20.f * std::log10 (env + 1e-9f);
        const bool wasGate = gate;
        if (! gate && envDb > -42.f) { gate = true; attacking = true; }
        else if (gate && envDb < -54.f) { gate = false; attacking = false; }
        const bool opened = gate && ! wasGate;

        // transition: gate driven, or follows any modulation source
        if (p.transSrc > 0)
        {
            const float s = lastSv[p.transSrc];
            float v = srcIsUnipolar (p.transSrc) ? s : 0.5f + 0.5f * s;
            if (p.transInv) v = 1.f - v;
            transPos += (clampf (v, 0.f, 1.f) - transPos) * 0.2f;
        }
        else
        {
            const float tAtk = std::max (0.01f, p.transTimeMs * 0.001f), tRel = std::max (0.01f, p.transRelMs * 0.001f);
            if (gate)
            {
                if (attacking) { transPos += fdt / tAtk; if (transPos >= 1.f) { transPos = 1.f; attacking = false; } }
                else transPos += (p.transHold - transPos) * (1.f - (float) std::exp (-dt / (0.25 * tAtk + 0.01)));
            }
            else { transPos -= fdt / tRel; if (transPos < 0.f) transPos = 0.f; }
        }

        float sv[SrcCount] = {};
        for (int k = 0; k < kNumLfo; ++k)
        {
            const LfoP& lf = p.lfo[k];
            if (opened && lf.ks) lfoPhase[k] = 0.0;
            const double inc = lf.sync > 0 ? 1.0 / ((double) kSyncBeats[std::min (lf.sync, 10)] * spb) : (double) lf.rate;
            lfoPhase[k] += inc * dt;
            if (lfoPhase[k] >= 1.0) { lfoPhase[k] -= std::floor (lfoPhase[k]); lfoR0[k] = lfoR1[k]; lfoR1[k] = rng.bip(); }
            sv[SrcLfo1 + k] = lfoWave (lf.shape, lfoPhase[k], lfoR0[k], lfoR1[k]) * lf.depth;
        }
        // super LFO (smooth random, two axes) + S&H + bounce are shared by the UFO and the matrix
        slfoT += fdt * p.ufoSpeed * 0.35f;
        if (slfoT >= 1.f) { slfoT -= 1.f; for (int a = 0; a < 2; ++a) { slfoA[a] = slfoB[a]; slfoB[a] = rng.bip(); } }
        const float smt = slfoT * slfoT * (3.f - 2.f * slfoT);
        sv[SrcSlfo] = slfoA[0] + (slfoB[0] - slfoA[0]) * smt;
        shPhase += (double) p.shRate * dt;
        if (shPhase >= 1.0) { shPhase -= std::floor (shPhase); shTarget = rng.bip(); }
        sh += (shTarget - sh) * (1.f - (float) std::exp (-dt / (0.001 + (double) p.shVar * 0.8 / std::max (0.1f, p.shRate))));
        sv[SrcSH] = sh;
        shT2 += fdt * p.ufoSpeed * 1.5f;
        if (shT2 >= 1.f) { shT2 -= 1.f; for (int a = 0; a < 2; ++a) ufoShV[a] = rng.bip(); }
        bounceX += bvx * fdt * p.ufoSpeed * 0.5f; bounceY += bvy * fdt * p.ufoSpeed * 0.5f;
        if (bounceX < 0.f) { bounceX = -bounceX; bvx = std::fabs (bvx); } else if (bounceX > 1.f) { bounceX = 2.f - bounceX; bvx = -std::fabs (bvx); }
        if (bounceY < 0.f) { bounceY = -bounceY; bvy = std::fabs (bvy); } else if (bounceY > 1.f) { bounceY = 2.f - bounceY; bvy = -std::fabs (bvy); }

        // step modulator
        if (opened && p.stepKeyReset) { stepIdx = 0; stepPh = 0; stepDir = 1; }
        const double stepSec = (double) kSyncBeats[std::min (std::max (p.stepRate, 1), 10)] * spb;
        stepPh += dt / stepSec;
        if (stepPh >= 1.0)
        {
            stepPh -= std::floor (stepPh);
            switch (p.stepMode)
            {
                case 1: stepIdx = (stepIdx + kNumSteps - 1) % kNumSteps; break;
                case 2: { int n = stepIdx + stepDir; if (n >= kNumSteps || n < 0) { stepDir = -stepDir; n = stepIdx + stepDir; } stepIdx = std::min (std::max (n, 0), kNumSteps - 1); } break;
                case 3: stepIdx = std::min (kNumSteps - 1, (int) (rng.uni() * (float) kNumSteps)); break;
                default: stepIdx = (stepIdx + 1) % kNumSteps; break;
            }
        }
        stepVal += (p.step[stepIdx] * 2.f - 1.f - stepVal) * (1.f - (float) std::exp (-dt / (0.001 + (double) (p.stepSmooth * p.stepSmooth) * 0.5)));
        sv[SrcStep] = stepVal;

        // Euclidean sequencer (two layers per channel, merged)
        const double eucSec = (double) kSyncBeats[std::min (std::max (p.euc.div, 1), 10)] * spb;
        eucPh += dt / eucSec;
        grainTrig = false;
        if (eucPh >= 1.0)
        {
            eucPh -= std::floor (eucPh); ++eucTick;
            for (int c = 0; c < kNumEuc; ++c)
            {
                const bool a = eucHit (p.euc.steps[0][c], p.euc.pulses[0][c], p.euc.rot[0][c], eucTick);
                const bool b = eucHit (p.euc.steps[1][c], p.euc.pulses[1][c], p.euc.rot[1][c], eucTick);
                bool hit = false;
                switch (p.euc.merge) { case 0: hit = a || b; break; case 1: hit = a != b; break; case 2: hit = a && b; break; case 3: hit = a; break; default: hit = b; break; }
                if (hit) { eucEnv[c] = 1.f; if (c == p.euc.grainCh) grainTrig = true; }
            }
        }
        const float eDec = std::exp (-fdt / std::max (0.005f, p.euc.decayMs * 0.001f));
        for (int c = 0; c < kNumEuc; ++c) { eucEnv[c] *= eDec; sv[SrcEuc1 + c] = eucEnv[c]; }
        for (int i = 0; i < 4; ++i) sv[SrcMacro1 + i] = p.macro[i];

        // filter envelopes (triggered when audio starts)
        const float lpE = lpEg.step (gate, opened, p.lp.a, p.lp.d, p.lp.s, p.lp.r, fdt);
        const float hpE = hpEg.step (gate, opened, p.hp.a, p.hp.d, p.hp.s, p.hp.r, fdt);
        sv[SrcEg] = lpE;
        sv[SrcEnv] = clampf ((envDb + 60.f) / 60.f, 0.f, 1.f);
        sv[SrcTrans] = transPos;

        // Super Modulation UFO: manual, or moved by LFOs / smooth random / S&H / bouncing ball (Auto)
        auto axisVal = [&] (int srcIdx, int axis) -> float
        {
            switch (std::min (std::max (srcIdx, 0), 6))
            {
                case 0: case 1: case 2: case 3: return sv[SrcLfo1 + srcIdx];
                case 4: return slfoA[axis] + (slfoB[axis] - slfoA[axis]) * smt;
                case 5: return ufoShV[axis];
                default: return (axis == 0 ? bounceX : bounceY) * 2.f - 1.f;
            }
        };
        float x = p.ufoX, y = p.ufoY;
        if (p.ufoAuto)
        {
            x = clampf (p.ufoX + axisVal (p.ufoXSrc, 0) * p.ufoXAuto * 0.5f, 0.f, 1.f);
            y = clampf (p.ufoY + axisVal (p.ufoYSrc, 1) * p.ufoYAuto * 0.5f, 0.f, 1.f);
        }
        ufoX = x; ufoY = y;
        sv[SrcUfoX] = x * 2.f - 1.f; sv[SrcUfoY] = y * 2.f - 1.f;
        std::copy (sv, sv + SrcCount, lastSv);

        // mod matrix + Super Modulation slots
        float dm[DstCount] = {};
        for (int s = 0; s < kNumSlots; ++s) if (p.slotSrc[s] > 0 && p.slotDst[s] > 0) dm[p.slotDst[s]] += sv[p.slotSrc[s]] * p.slotAmt[s];
        for (int s = 0; s < kUfoSlots; ++s)
        {
            const int axis = p.ufoAxis[s];
            if (axis >= 4 || p.ufoDst[s] <= 0) continue;
            float v = axis < 2 ? x : y;
            if (axis & 1) v = 1.f - v;
            dm[p.ufoDst[s]] += (p.ufoCenter ? (v - 0.5f) * 2.f : v) * p.ufoAmt[s];
        }

        // filters (each: own modulation source, LFO:EG blend, invert, depth)
        auto filtMod = [&] (const FiltP& f, float eg)
        {
            const float srcV = f.src > 0 ? sv[f.src] : 0.f;
            return f.depth * ((1.f - f.egMix) * srcV + f.egMix * eg) * (f.inv ? -1.f : 1.f);
        };
        const double nyq = sr * 0.45;
        const double lpC = std::min (nyq, (double) p.lp.cut * std::exp2 (4.0 * (double) (dm[DstLpCut] + filtMod (p.lp, lpE))));
        const double hpC = std::min (nyq, (double) p.hp.cut * std::exp2 (4.0 * (double) (dm[DstHpCut] + filtMod (p.hp, hpE))));
        cutNorm = clampf ((float) (std::log (lpC / 20.0) / std::log (1000.0)), 0.f, 1.f);
        const float q = clampf (p.lp.q + 6.f * dm[DstLpQ], 0.5f, 20.f);
        lp1.setLP (sr, lpC, 0.5412);
        lp2.setLP (sr, lpC, std::min (25.0, 1.3066 * (double) q / 0.7071));
        hp.setHP (sr, hpC, clampf (p.hp.q + 6.f * dm[DstHpQ], 0.5f, 20.f));
        lpHp = clampf (p.lpHpMix + dm[DstLpHpMix], 0.f, 1.f);
        const float fT = clampf ((p.transOn ? transPos : p.fdMix) + dm[DstFdMix], 0.f, 1.f);
        fAmtS += (fT - fAmtS) * 0.3f;
        wetEff = clampf (p.wet + dm[DstWet], 0.f, 1.f);
        outGain = std::pow (10.f, p.outGainDb / 20.f);
        widthEff = clampf (p.width + dm[DstWidth], 0.f, 2.f);
        engS += ((p.engage ? 1.f : 0.f) - engS) * 0.02f;

        // granular
        gr.mix = clampf (p.grain.mix + dm[DstGrnMix], 0.f, 1.f);
        gr.sizeMs = clampf (p.grain.sizeMs * std::exp2 (dm[DstGrnSize] * 1.5f), 8.f, 800.f);
        gr.density = clampf (p.grain.density * std::exp2 (dm[DstGrnDens] * 1.5f), 0.5f, 120.f);
        gr.pos = clampf (p.grain.pos + dm[DstGrnPos], 0.f, 1.f);
        gr.pitch = p.grain.pitch + 12.f * dm[DstGrnPitch];
        gr.spread = p.grain.spread; gr.jitter = p.grain.jitter; gr.rev = p.grain.rev;
        gr.mode = p.grain.mode; gr.scale = p.grain.scale; gr.root = p.grain.root; gr.cluster = p.grain.cluster;
        gr.freeze = p.grain.freeze; gr.quant = p.grain.quant;

        // space reverb
        sr_.size = clampf (p.space.size + 0.5f * dm[DstRevSize], 0.f, 1.f);
        sr_.decay = clampf (p.space.decay + 0.5f * dm[DstRevDecay], 0.f, 1.f);
        sr_.drift = p.space.drift; sr_.tone = p.space.tone; sr_.type = p.space.type; sr_.freeze = p.space.freeze;
        sr_.shimmer = clampf (p.space.shimmer + 0.5f * dm[DstShimmer], 0.f, 1.f);
        sr_.preSamples = p.space.sync ? (double) spb * 0.25 * sr : 0.0;
        const float duckT = 1.f - p.space.duck * clampf ((envDb + 50.f) / 50.f, 0.f, 1.f);
        duckS += (duckT - duckS) * (duckT < duckS ? 0.4f : 0.02f); // steps back fast, blooms back slowly
        sr_.wetGain = duckS;
        const float mix = clampf (p.space.mix + 0.5f * dm[DstRevMix], 0.f, 1.f);
        revDry = std::min (1.f, 2.f * (1.f - mix)); revWet = std::min (1.f, 2.f * mix);

        haasSamples = (int) (p.haasMs * 0.001f * (float) sr); haasSide = p.haasSide;
        wowPh += fdt * 0.6f * 6.2831853f; flutPh += fdt * 6.3f * 6.2831853f;
        if (wowPh > 6.2831853f) wowPh -= 6.2831853f;
        if (flutPh > 6.2831853f) flutPh -= 6.2831853f;
        const float wob = p.delayType == 2 ? 1.f + p.tape * (0.004f * std::sin (wowPh) + 0.0015f * std::sin (flutPh)) : 1.f;

        for (int c = 0; c < 2; ++c)
        {
            Chan& h = ch[c];
            const ChanParams& cp = p.ch[c];
            h.shelf.setLowShelf (sr, 120.0, 12.0 * (double) cp.lo);
            h.xo1.setLP (sr, p.splitFreq, 0.7071); h.xo2.setLP (sr, p.splitFreq, 0.7071);
            const int div = std::min (std::max (cp.div, 0), 10);
            const double dS = (double) kDlyBeats[div] * spb * sr * std::exp2 ((double) dm[DstDlyTimeF + c]) * (double) wob;
            h.dTarget = (float) std::min (std::max (dS, 16.0), (double) (h.size - 4));
            h.fb = cp.dlyOn ? clampf (cp.dlyFb + 0.5f * dm[DstDlyFbF + c], 0.f, 0.95f) : 0.f;
            h.lvl = cp.dlyOn ? cp.dlyLvl : 0.f;
            h.panT = clampf (cp.pan + dm[DstPanF + c], -1.f, 1.f);
            h.satT = clampf (cp.sat + dm[DstSatF + c], 0.f, 1.f);
            h.panS += (h.panT - h.panS) * 0.3f; h.satS += (h.satT - h.satS) * 0.3f;
            const double fcDly = 800.0 * std::pow (15.0, (double) cp.color) * (p.delayType == 2 ? 0.6 : 1.0);
            h.cc = 1.f - (float) std::exp (-2 * kPi * fcDly / sr);
            h.tapeSat = p.delayType == 2 ? 1.4f : 1.f;
        }
        delayType = p.delayType;
    }

    void chanProc (int c, const Params& p, float inL, float inR, float& oL, float& oR)
    {
        Chan& h = ch[c];
        if (h.satS > 0.0005f)
        {
            const float d = 1.f + 6.f * h.satS, nrm = 1.f / std::tanh (d);
            inL = lerp (inL, std::tanh (d * inL) * nrm, h.satS); inR = lerp (inR, std::tanh (d * inR) * nrm, h.satS);
        }
        inL = h.shelf.process (inL, 0); inR = h.shelf.process (inR, 1);
        float lowL = 0, lowR = 0, hiL = inL, hiR = inR;
        if (p.splitOn)
        {
            lowL = h.xo2.process (h.xo1.process (inL, 0), 0); lowR = h.xo2.process (h.xo1.process (inR, 1), 1);
            hiL = inL - lowL; hiR = inR - lowR; lowL = lowR = 0.5f * (lowL + lowR);
        }
        h.dSmp += (h.dTarget - h.dSmp) * dCoef;
        float rp = (float) h.w - h.dSmp; if (rp < 0) rp += (float) h.size;
        int i0 = (int) rp; const float fr = rp - (float) i0; if (i0 >= h.size) i0 -= h.size;
        int i1 = i0 + 1; if (i1 >= h.size) i1 = 0;
        float wet[2];
        for (int k = 0; k < 2; ++k)
        {
            const float rd = lerp (h.buf[k][(size_t) i0], h.buf[k][(size_t) i1], fr);
            h.lp[k] += h.cc * (rd - h.lp[k]);
            if (std::fabs (h.lp[k]) < 1e-20f) h.lp[k] = 0.f;
        }
        if (delayType == 1) // ping-pong: left line fed by the mono input, right fed by the left echo
        {
            h.buf[0][(size_t) h.w] = 0.5f * (hiL + hiR) + std::tanh (h.lp[1] * h.fb);
            h.buf[1][(size_t) h.w] = std::tanh (h.lp[0] * h.fb);
        }
        else
        {
            h.buf[0][(size_t) h.w] = hiL + std::tanh (h.lp[0] * h.fb * h.tapeSat);
            h.buf[1][(size_t) h.w] = hiR + std::tanh (h.lp[1] * h.fb * h.tapeSat);
        }
        wet[0] = h.lp[0] * h.lvl; wet[1] = h.lp[1] * h.lvl;
        if (++h.w >= h.size) h.w = 0;
        const float dp = h.panS, wp = p.oppositePan ? -h.panS : h.panS;
        auto gl = [] (float x) { return x > 0.f ? 1.f - x : 1.f; };
        auto gr2 = [] (float x) { return x < 0.f ? 1.f + x : 1.f; };
        oL = hiL * gl (dp) + wet[0] * gl (wp) + lowL;
        oR = hiR * gr2 (dp) + wet[1] * gr2 (wp) + lowR;
    }

    void audio (float* l, float* r, int m, const Params& p)
    {
        dCoef = 1.f - (float) std::exp (-1.0 / (0.05 * sr));
        for (int i = 0; i < m; ++i)
        {
            const float xl = l[i], xr = r[i];
            gr.trig = (i == 0) && grainTrig;
            float gL = 0.f, gR = 0.f;
            gran.process (xl, xr, gL, gR, gr);
            const float fiL = lerp (xl, gL, gr.mix), fiR = lerp (xr, gR, gr.mix);
            const float aL = lp2.process (lp1.process (fiL, 0), 0), aR = lp2.process (lp1.process (fiR, 1), 1);
            const float hL = hp.process (fiL, 0), hR = hp.process (fiR, 1);
            float fOutL, fOutR, dOutL, dOutR;
            chanProc (0, p, lerp (aL, hL, lpHp), lerp (aR, hR, lpHp), fOutL, fOutR);
            chanProc (1, p, xl, xr, dOutL, dOutR);
            const float mixL = fOutL * fAmtS + dOutL * (1.f - fAmtS), mixR = fOutR * fAmtS + dOutR * (1.f - fAmtS);
            float rvL, rvR;
            space.process (mixL, mixR, rvL, rvR, sr_);
            float sL = mixL * revDry + rvL * revWet, sR = mixR * revDry + rvR * revWet;
            // width (mid/side) + Haas micro-delay on one side
            const float mid = 0.5f * (sL + sR), side = 0.5f * (sL - sR) * widthEff;
            sL = mid + side; sR = mid - side;
            haasBuf[0][(size_t) haasW] = sL; haasBuf[1][(size_t) haasW] = sR;
            if (haasSamples > 0)
            {
                int rd = haasW - std::min (haasSamples, haasSize - 2); if (rd < 0) rd += haasSize;
                if (haasSide == 0) sL = haasBuf[0][(size_t) rd]; else sR = haasBuf[1][(size_t) rd];
            }
            if (++haasW >= haasSize) haasW = 0;
            sL = softLimit (sL); sR = softLimit (sR); // output safety on the wet path only
            const float yL = (xl * (1.f - wetEff) + sL * wetEff) * outGain, yR = (xr * (1.f - wetEff) + sR * wetEff) * outGain;
            l[i] = lerp (xl, yL, engS); r[i] = lerp (xr, yR, engS);
        }
    }

    double sr = 44100.0;
    Biquad lp1, lp2, hp;
    Chan ch[2];
    Adsr lpEg, hpEg;
    Granular gran; Granular::Run gr;
    SpaceVerb space; SpaceVerb::Run sr_;
    Rng rng;
    std::vector<float> haasBuf[2]; int haasSize = 1, haasW = 0, haasSamples = 0, haasSide = 1;
    float env = 0, transPos = 0, fAmtS = 0.5f, lpHp = 0, wetEff = 1, outGain = 1, dCoef = 0.001f, widthEff = 1, engS = 1, duckS = 1, revDry = 1, revWet = 0, cutNorm = 1;
    bool gate = false, attacking = false, grainTrig = false;
    int delayType = 0;
    double lfoPhase[kNumLfo] = {}, shPhase = 0, stepPh = 0, eucPh = 0;
    float lfoR0[kNumLfo] = {}, lfoR1[kNumLfo] = {}, sh = 0, shTarget = 0, stepVal = 0, eucEnv[kNumEuc] = {}, lastSv[SrcCount] = {};
    float ufoX = 0.5f, ufoY = 0.5f, slfoT = 0.f, shT2 = 0.f, slfoA[2] = { 0, 0 }, slfoB[2] = { 0, 0 }, ufoShV[2] = { 0, 0 };
    float bounceX = 0.3f, bounceY = 0.6f, bvx = 0.63f, bvy = 0.41f, wowPh = 0.f, flutPh = 0.f;
    int stepIdx = 0, stepDir = 1, eucTick = 0;
};
} // namespace tsfx
