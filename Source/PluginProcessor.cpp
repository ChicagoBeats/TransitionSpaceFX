#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace tsfx;

namespace
{
juce::StringArray toSA (const char* const* names, int n, int from = 0)
{
    juce::StringArray a;
    for (int i = from; i < n; ++i) a.add (names[i]);
    return a;
}

juce::StringArray groupIds (int g)
{
    juce::StringArray a;
    auto addAll = [&a] (std::initializer_list<const char*> l) { for (auto* s : l) a.add (s); };
    switch (g)
    {
        case 1:
            for (juce::String pre : { "lp", "hp" })
                for (auto* s : { "Cut", "Q", "Depth", "Src", "EgMix", "Inv", "A", "D", "S", "R" }) a.add (pre + s);
            a.add ("lpHpMix");
            break;
        case 2: addAll ({ "fdMix", "transOn", "transTime", "transRel", "transHold", "transSrc", "transInv" }); break;
        case 3: case 4:
            for (auto* s : { "Lo", "Sat", "DlyOn", "DlyLvl", "DlyFb", "DlyDiv", "Color", "Pan" }) a.add (juce::String (g == 3 ? "f" : "d") + s);
            break;
        case 5: addAll ({ "delayType", "tape", "fDlyDiv", "dDlyDiv", "fDlyFb", "dDlyFb", "fDlyLvl", "dDlyLvl", "fColor", "dColor" }); break;
        case 6: addAll ({ "fPan", "dPan", "oppPan", "splitOn", "splitFreq", "width", "haasMs", "haasSide" }); break;
        case 7:
            for (int k = 1; k <= kNumLfo; ++k)
                for (auto* s : { "Rate", "Shape", "Sync", "Ks", "Depth" }) a.add ("lfo" + juce::String (k) + s);
            addAll ({ "shRate", "shVar" });
            break;
        case 8:
            addAll ({ "stepRate", "stepSmooth", "stepMode", "stepKeyReset", "eucMerge", "eucDiv", "eucDecay", "eucGrainCh" });
            for (int s = 1; s <= kNumSteps; ++s) a.add ("step" + juce::String (s));
            for (juce::String l : { "A", "B" }) for (int c = 1; c <= kNumEuc; ++c)
                for (auto* s : { "Steps", "Pulses", "Rot" }) a.add ("euc" + l + juce::String (c) + s);
            break;
        case 9:
            for (int s = 1; s <= kNumSlots; ++s)
                for (auto* k : { "slotSrc", "slotDst", "slotAmt" }) a.add (juce::String (k) + juce::String (s));
            break;
        case 10:
            addAll ({ "ufoMode", "ufoXSrc", "ufoYSrc", "ufoXAuto", "ufoYAuto", "ufoSpeed", "ufoX", "ufoY" });
            for (int s = 1; s <= kUfoSlots; ++s)
                for (auto* k : { "Axis", "Dst", "Amt" }) a.add ("ufoS" + juce::String (s) + k);
            break;
        case 11:
            addAll ({ "grnMix", "grnMode", "grnSize", "grnDens", "grnPos", "grnPitch", "grnSpread", "grnJit", "grnRev", "grnQuant", "grnScale",
                      "grnRoot", "grnCluster", "spType", "spSize", "spDecay", "spDrift", "spTone", "spShimmer", "spDuck", "spMix", "spSync" });
            break;
        default: break;
    }
    return a;
}

// "musical" normalised ranges so random patches stay usable
std::pair<float, float> safeRange (const juce::String& id)
{
    if (id == "lpCut") return { 0.45f, 1.f };
    if (id == "hpCut") return { 0.f, 0.45f };
    if (id == "lpQ" || id == "hpQ") return { 0.f, 0.3f };
    if (id == "fdMix") return { 0.25f, 1.f };
    if (id.endsWith ("DlyFb") || id.endsWith ("Sat") || id.endsWith ("Lo")) return { 0.f, 0.6f };
    if (id.startsWith ("slotAmt")) return { 0.25f, 0.75f };
    if (id.startsWith ("ufoS") && id.endsWith ("Amt")) return { 0.3f, 1.f };
    if (id == "lpHpMix") return { 0.f, 0.5f };
    if (id == "grnMix") return { 0.3f, 1.f };
    if (id == "spMix") return { 0.2f, 0.8f };
    if (id == "spShimmer") return { 0.f, 0.6f };
    if (id == "spDecay") return { 0.2f, 0.9f };
    if (id.startsWith ("euc") && id.endsWith ("Pulses")) return { 0.f, 0.5f };
    return { 0.f, 1.f };
}

struct PV { const char* id; float v; };
struct FactoryPreset { const char* name; std::vector<PV> v; };

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> t = {
        { "Cosmic Drift",       { { "spMix", .6f }, { "spSize", .7f }, { "spDecay", .65f }, { "spDrift", .6f }, { "spShimmer", .2f }, { "lpCut", 6000 },
                                  { "slotSrc1", SrcLfo1 }, { "slotDst1", DstLpCut }, { "slotAmt1", .35f }, { "lfo1Rate", .12f }, { "ufoMode", 1 }, { "fdMix", .6f } } },
        { "Granular Nebula",    { { "grnMix", .85f }, { "grnMode", 2 }, { "grnSize", 140 }, { "grnDens", 18 }, { "grnPos", .4f }, { "grnSpread", .8f },
                                  { "spMix", .55f }, { "spDecay", .7f }, { "fdMix", 1.f } } },
        { "Crystal Fragments",  { { "grnMix", 1.f }, { "grnSize", 60 }, { "grnDens", 30 }, { "grnQuant", 1 }, { "grnScale", 4 }, { "grnCluster", 2 },
                                  { "spMix", .5f }, { "spShimmer", .5f }, { "fdMix", 1.f } } },
        { "Euclid Stutter",     { { "grnMix", 1.f }, { "grnMode", 1 }, { "eucA1Steps", 8 }, { "eucA1Pulses", 5 }, { "eucB1Steps", 7 }, { "eucB1Pulses", 3 }, { "eucMerge", 0 },
                                  { "grnSize", 50 }, { "grnPos", .15f }, { "fdMix", .9f }, { "spMix", .3f } } },
        { "Black Hole Wash",    { { "spType", 3 }, { "spMix", .8f }, { "spDecay", .6f }, { "spSize", .9f }, { "spDrift", .4f }, { "lpCut", 3000 }, { "fdMix", .7f } } },
        { "Infinite Space",     { { "spDecay", 1.f }, { "spMix", .7f }, { "spShimmer", .4f }, { "spSize", .8f }, { "spTone", .6f }, { "fdMix", .4f } } },
        { "Tape Echo Orbit",    { { "delayType", 2 }, { "tape", .6f }, { "dDlyLvl", .5f }, { "dDlyFb", .6f }, { "dDlyDiv", 2 }, { "fDlyLvl", .4f }, { "spMix", .3f } } },
        { "Ping-Pong Galaxy",   { { "delayType", 1 }, { "dDlyLvl", .5f }, { "dDlyFb", .55f }, { "dDlyDiv", 2 }, { "spMix", .4f }, { "width", 1.4f } } },
        { "Transition Sweep",   { { "transOn", 1 }, { "transTime", 3000 }, { "transHold", .8f }, { "lpCut", 1200 }, { "lpEgMix", 1.f }, { "lpDepth", .8f },
                                  { "lpA", 2.5f }, { "lpS", .6f }, { "spMix", .4f } } },
        { "Super Mod Playground", { { "ufoMode", 1 }, { "ufoSpeed", 2.f }, { "ufoS1Axis", 0 }, { "ufoS1Dst", DstLpCut }, { "ufoS2Axis", 2 }, { "ufoS2Dst", DstRevMix },
                                  { "ufoS3Axis", 1 }, { "ufoS3Dst", DstPanF }, { "ufoS3Amt", .6f }, { "ufoS4Axis", 3 }, { "ufoS4Dst", DstGrnPos },
                                  { "grnMix", .5f }, { "spMix", .5f } } },
    };
    return t;
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TsfxProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto addF = [&] (const juce::String& id, const juce::String& name, float lo, float hi, float def, float skew = 1.f, float interval = 0.f)
    { l.add (std::make_unique<F> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, interval, skew), def)); };
    auto addC = [&] (const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
    { l.add (std::make_unique<C> (juce::ParameterID { id, 1 }, name, items, def)); };
    auto addB = [&] (const juce::String& id, const juce::String& name, bool def)
    { l.add (std::make_unique<B> (juce::ParameterID { id, 1 }, name, def)); };

    const auto srcs = toSA (kSrcNames, SrcCount), dsts = toSA (kDstNames, DstCount);

    // filters
    for (juce::String pre : { "lp", "hp" })
    {
        const juce::String n = pre == "lp" ? "LP " : "HP ";
        addF (pre + "Cut", n + "Cutoff", 20.f, 20000.f, pre == "lp" ? 8000.f : 60.f, 0.3f);
        addF (pre + "Q", n + "Resonance", 0.5f, 12.f, 0.7071f, 0.5f);
        addF (pre + "Depth", n + "Mod Depth", 0.f, 1.f, 0.5f);
        addC (pre + "Src", n + "Mod Source", srcs, 0);
        addF (pre + "EgMix", n + "LFO:EG", 0.f, 1.f, 0.f);
        addB (pre + "Inv", n + "Invert", false);
        addF (pre + "A", n + "EG Attack", 0.001f, 5.f, 0.01f, 0.4f);
        addF (pre + "D", n + "EG Decay", 0.01f, 5.f, 0.3f, 0.4f);
        addF (pre + "S", n + "EG Sustain", 0.f, 1.f, 0.5f);
        addF (pre + "R", n + "EG Release", 0.01f, 8.f, 0.5f, 0.4f);
    }
    addF ("lpHpMix", "Filter LP:HP", 0.f, 1.f, 0.f);
    addF ("fdMix", "Mix D:F", 0.f, 1.f, 0.5f);

    for (juce::String pre : { "f", "d" })
    {
        const juce::String nm = pre == "f" ? "F " : "D ";
        addF (pre + "Lo", nm + "LoBoost", 0.f, 1.f, 0.f);
        addF (pre + "Sat", nm + "Saturate", 0.f, 1.f, 0.f);
        addB (pre + "DlyOn", nm + "Delay On", true);
        addF (pre + "DlyLvl", nm + "Delay Level", 0.f, 1.f, 0.3f);
        addF (pre + "DlyFb", nm + "Delay Feedback", 0.f, 0.95f, 0.4f);
        addC (pre + "DlyDiv", nm + "Delay Time", toSA (kDlyNames, 11), 1);
        addF (pre + "Color", nm + "Delay Color", 0.f, 1.f, 0.5f);
        addF (pre + "Pan", nm + "Pan", -1.f, 1.f, 0.f);
    }
    addB ("oppPan", "Delay Opposite Pan", true);
    addB ("splitOn", "Freq Split", false);
    addF ("splitFreq", "Split Freq", 40.f, 1000.f, 200.f, 0.5f);
    addC ("delayType", "Delay Type", toSA (kDelayTypes, 3), 0);
    addF ("tape", "Tape Wow/Flutter", 0.f, 1.f, 0.3f);
    addF ("wet", "Wet", 0.f, 1.f, 1.f);
    addF ("outGain", "Output dB", -24.f, 12.f, 0.f);
    addF ("width", "Stereo Width", 0.f, 2.f, 1.f);
    addF ("haasMs", "Haas Delay ms", 0.f, 30.f, 0.f);
    addC ("haasSide", "Haas Side", { "L", "R" }, 1);
    addB ("engage", "Engage", true);

    addB ("transOn", "Transition On", false);
    addF ("transTime", "Trans Time ms", 10.f, 8000.f, 1500.f, 0.4f);
    addF ("transRel", "Trans Release ms", 10.f, 8000.f, 1500.f, 0.4f);
    addF ("transHold", "Trans Hold", 0.f, 1.f, 0.5f);
    addC ("transSrc", "Trans Mod Source", srcs, 0);
    addB ("transInv", "Trans Invert", false);

    const auto shapes = toSA (kLfoShapeNames, 5), syncs = toSA (kSyncNames, 11);
    const float defRates[kNumLfo] = { 0.2f, 0.5f, 0.1f, 1.f, 4.5f };
    const int defShapes[kNumLfo] = { 0, 1, 0, 4, 0 };
    for (int k = 1; k <= kNumLfo; ++k)
    {
        const juce::String p = "lfo" + juce::String (k), n = (k == 5 ? juce::String ("PLFO ") : "LFO " + juce::String (k) + " ");
        addF (p + "Rate", n + "Rate", 0.01f, 20.f, defRates[k - 1], 0.4f);
        addC (p + "Shape", n + "Shape", shapes, defShapes[k - 1]);
        addC (p + "Sync", n + "Sync", syncs, 0);
        addB (p + "Ks", n + "Key Sync", false);
        addF (p + "Depth", n + "Depth", 0.f, 1.f, 1.f);
    }
    addF ("shRate", "S&H Rate", 0.1f, 30.f, 4.f, 0.4f);
    addF ("shVar", "S&H Variation", 0.f, 1.f, 0.3f);

    addC ("stepRate", "Step Rate", toSA (kSyncNames, 11, 1), 5);
    addF ("stepSmooth", "Step Smooth", 0.f, 1.f, 0.2f);
    addC ("stepMode", "Step Mode", toSA (kStepModes, 4), 0);
    addB ("stepKeyReset", "Step Key Reset", false);
    for (int s = 1; s <= kNumSteps; ++s) addF ("step" + juce::String (s), "Step " + juce::String (s), 0.f, 1.f, 0.5f);

    // granular
    addF ("grnMix", "Grain Mix", 0.f, 1.f, 0.f);
    addC ("grnMode", "Grain Mode", toSA (kGrainModes, 3), 0);
    addF ("grnSize", "Grain Size ms", 10.f, 500.f, 120.f, 0.5f);
    addF ("grnDens", "Grain Density", 1.f, 100.f, 12.f, 0.5f);
    addF ("grnPos", "Grain Position", 0.f, 1.f, 0.3f);
    addF ("grnPitch", "Grain Pitch", -24.f, 24.f, 0.f);
    addF ("grnSpread", "Grain Spread", 0.f, 1.f, 0.5f);
    addF ("grnJit", "Grain Jitter", 0.f, 1.f, 0.3f);
    addF ("grnRev", "Grain Reverse", 0.f, 1.f, 0.f);
    addB ("grnFreeze", "Grain Freeze", false);
    addB ("grnQuant", "Grain Quantize", false);
    addC ("grnScale", "Grain Scale", toSA (kScaleNames, 7), 0);
    addC ("grnRoot", "Grain Root", toSA (kNoteNames, 12), 0);
    addC ("grnCluster", "Grain Cluster", toSA (kClusterNames, 7), 0);

    // space
    addC ("spType", "Space Type", toSA (kRevTypes, 4), 0);
    addF ("spSize", "Space Size", 0.f, 1.f, 0.5f);
    addF ("spDecay", "Space Decay", 0.f, 1.f, 0.5f);
    addF ("spDrift", "Space Drift", 0.f, 1.f, 0.2f);
    addF ("spTone", "Space Tone", 0.f, 1.f, 0.7f);
    addF ("spShimmer", "Space Shimmer", 0.f, 1.f, 0.f);
    addF ("spDuck", "Space Duck", 0.f, 1.f, 0.f);
    addF ("spMix", "Space Mix", 0.f, 1.f, 0.3f);
    addB ("spFreeze", "Space Freeze", false);
    addB ("spSync", "Space Sync", false);

    // euclidean sequencer (two layers per channel)
    addC ("eucMerge", "Euclid Merge", toSA (kMergeNames, 5), 0);
    addC ("eucDiv", "Euclid Rate", toSA (kSyncNames, 11, 1), 5);
    addF ("eucDecay", "Euclid Pulse ms", 10.f, 1000.f, 150.f, 0.5f);
    addC ("eucGrainCh", "Euclid > Grains", { "1", "2", "3", "4", "5", "6", "7", "8" }, 0);
    for (juce::String lay : { "A", "B" })
        for (int c = 1; c <= kNumEuc; ++c)
        {
            const juce::String id = "euc" + lay + juce::String (c), n = "Euclid " + juce::String (c) + lay + " ";
            addF (id + "Steps", n + "Steps", 1.f, 16.f, lay == "A" ? 16.f : 12.f, 1.f, 1.f);
            addF (id + "Pulses", n + "Pulses", 0.f, 16.f, (lay == "A" && c == 1) ? 4.f : 0.f, 1.f, 1.f);
            addF (id + "Rot", n + "Rotate", 0.f, 15.f, 0.f, 1.f, 1.f);
        }

    for (int i = 1; i <= 4; ++i) addF ("macro" + juce::String (i), "Macro " + juce::String (i), 0.f, 1.f, 0.f);

    // Super Modulation UFO
    addF ("ufoX", "UFO X", 0.f, 1.f, 0.5f);
    addF ("ufoY", "UFO Y", 0.f, 1.f, 0.5f);
    addC ("ufoMode", "Super Mod Mode", { "Manual", "Auto" }, 0);
    addC ("ufoHide", "Super Mod UFO", { "Show", "Hide" }, 0);
    addC ("ufoCenter", "Super Mod Center", { "Off", "On" }, 1);
    addC ("ufoXSrc", "X Auto Source", toSA (kUfoAutoNames, 7), 0);
    addC ("ufoYSrc", "Y Auto Source", toSA (kUfoAutoNames, 7), 5);
    addF ("ufoXAuto", "X Auto Amount", 0.f, 1.f, 0.5f);
    addF ("ufoYAuto", "Y Auto Amount", 0.f, 1.f, 0.5f);
    addF ("ufoSpeed", "UFO Speed", 0.1f, 8.f, 1.f, 0.5f);
    const int defAxis[kUfoSlots] = { 0, 2, 4, 4, 4, 4, 4, 4 }, defDst[kUfoSlots] = { DstLpCut, DstRevMix, 0, 0, 0, 0, 0, 0 };
    for (int s = 1; s <= kUfoSlots; ++s)
    {
        const juce::String p = "ufoS" + juce::String (s), n = "Super Mod " + juce::String (s) + " ";
        addC (p + "Axis", n + "Axis", toSA (kUfoAxisNames, 5), defAxis[s - 1]);
        addC (p + "Dst", n + "Destination", dsts, defDst[s - 1]);
        addF (p + "Amt", n + "Depth", 0.f, 1.f, 1.f);
    }

    for (int s = 1; s <= kNumSlots; ++s)
    {
        const juce::String i (s);
        addC ("slotSrc" + i, "Slot " + i + " Source", srcs, 0);
        addC ("slotDst" + i, "Slot " + i + " Dest", dsts, 0);
        addF ("slotAmt" + i, "Slot " + i + " Amount", -1.f, 1.f, 0.f);
    }

    {
        juce::StringArray vn;
        for (int i = 0; i < kNumVisuals; ++i) vn.add (juce::String (kVisScenes[i / 5]) + " - " + kVisMoods[i % 5]);
        addC ("visScene", "Visual", vn, 1 * 5 + 1);
        addF ("visReact", "Visual Reactivity", 0.f, 2.f, 1.f);
        addB ("visAuto", "Visual Auto-Cycle", false);
    }
    return l;
}

//==============================================================================
TsfxProcessor::TsfxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "STATE", createLayout())
{
    for (auto& c : ccMap) c = -1;
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p)) paramList.push_back (r);
}

bool TsfxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

void TsfxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    scratchR.assign ((size_t) std::max (samplesPerBlock, 512), 0.f);
}

Params TsfxProcessor::readParams() const
{
    auto f = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    auto i = [&] (const juce::String& id) { return (int) std::lround (f (id)); };
    auto b = [&] (const juce::String& id) { return f (id) > 0.5f; };

    Params p;
    for (int k = 0; k < 2; ++k)
    {
        const juce::String pre = k == 0 ? "lp" : "hp";
        FiltP& F = k == 0 ? p.lp : p.hp;
        F.cut = f (pre + "Cut"); F.q = f (pre + "Q"); F.depth = f (pre + "Depth"); F.src = i (pre + "Src"); F.egMix = f (pre + "EgMix");
        F.inv = b (pre + "Inv"); F.a = f (pre + "A"); F.d = f (pre + "D"); F.s = f (pre + "S"); F.r = f (pre + "R");
    }
    p.lpHpMix = f ("lpHpMix"); p.fdMix = f ("fdMix");
    for (int c = 0; c < 2; ++c)
    {
        const juce::String s = c == 0 ? "f" : "d";
        auto& cp = p.ch[c];
        cp.lo = f (s + "Lo"); cp.sat = f (s + "Sat"); cp.dlyOn = b (s + "DlyOn"); cp.dlyLvl = f (s + "DlyLvl"); cp.dlyFb = f (s + "DlyFb");
        cp.div = i (s + "DlyDiv"); cp.color = f (s + "Color"); cp.pan = f (s + "Pan");
    }
    p.oppositePan = b ("oppPan"); p.splitOn = b ("splitOn"); p.splitFreq = f ("splitFreq");
    p.delayType = i ("delayType"); p.tape = f ("tape");
    p.wet = f ("wet"); p.outGainDb = f ("outGain"); p.width = f ("width"); p.haasMs = f ("haasMs"); p.haasSide = i ("haasSide"); p.engage = b ("engage");
    p.transOn = b ("transOn"); p.transTimeMs = f ("transTime"); p.transRelMs = f ("transRel"); p.transHold = f ("transHold");
    p.transSrc = i ("transSrc"); p.transInv = b ("transInv");
    for (int k = 0; k < kNumLfo; ++k)
    {
        const juce::String n = "lfo" + juce::String (k + 1);
        auto& L = p.lfo[k];
        L.rate = f (n + "Rate"); L.shape = i (n + "Shape"); L.sync = i (n + "Sync"); L.ks = b (n + "Ks"); L.depth = f (n + "Depth");
    }
    p.shRate = f ("shRate"); p.shVar = f ("shVar");
    p.stepRate = i ("stepRate") + 1; p.stepSmooth = f ("stepSmooth"); p.stepMode = i ("stepMode"); p.stepKeyReset = b ("stepKeyReset");
    for (int s = 0; s < kNumSteps; ++s) p.step[s] = f ("step" + juce::String (s + 1));

    auto& g = p.grain;
    g.mix = f ("grnMix"); g.mode = i ("grnMode"); g.sizeMs = f ("grnSize"); g.density = f ("grnDens"); g.pos = f ("grnPos"); g.pitch = f ("grnPitch");
    g.spread = f ("grnSpread"); g.jitter = f ("grnJit"); g.rev = f ("grnRev"); g.freeze = b ("grnFreeze"); g.quant = b ("grnQuant");
    g.scale = i ("grnScale"); g.root = i ("grnRoot"); g.cluster = i ("grnCluster");
    auto& sp = p.space;
    sp.type = i ("spType"); sp.size = f ("spSize"); sp.decay = f ("spDecay"); sp.drift = f ("spDrift"); sp.tone = f ("spTone");
    sp.shimmer = f ("spShimmer"); sp.duck = f ("spDuck"); sp.mix = f ("spMix"); sp.freeze = b ("spFreeze"); sp.sync = b ("spSync");

    p.euc.merge = i ("eucMerge"); p.euc.div = i ("eucDiv") + 1; p.euc.decayMs = f ("eucDecay"); p.euc.grainCh = i ("eucGrainCh");
    for (int l = 0; l < 2; ++l)
        for (int c = 0; c < kNumEuc; ++c)
        {
            const juce::String id = juce::String ("euc") + (l == 0 ? "A" : "B") + juce::String (c + 1);
            p.euc.steps[l][c] = i (id + "Steps"); p.euc.pulses[l][c] = i (id + "Pulses"); p.euc.rot[l][c] = i (id + "Rot");
        }
    for (int m = 0; m < 4; ++m) p.macro[m] = f ("macro" + juce::String (m + 1));

    p.ufoX = f ("ufoX"); p.ufoY = f ("ufoY"); p.ufoAuto = i ("ufoMode") > 0; p.ufoCenter = i ("ufoCenter") > 0;
    p.ufoXSrc = i ("ufoXSrc"); p.ufoYSrc = i ("ufoYSrc"); p.ufoXAuto = f ("ufoXAuto"); p.ufoYAuto = f ("ufoYAuto"); p.ufoSpeed = f ("ufoSpeed");
    for (int s = 0; s < kUfoSlots; ++s)
    {
        const juce::String n = "ufoS" + juce::String (s + 1);
        p.ufoAxis[s] = i (n + "Axis"); p.ufoDst[s] = i (n + "Dst"); p.ufoAmt[s] = f (n + "Amt");
    }
    for (int s = 0; s < kNumSlots; ++s)
    {
        const juce::String n (s + 1);
        p.slotSrc[s] = i ("slotSrc" + n); p.slotDst[s] = i ("slotDst" + n); p.slotAmt[s] = f ("slotAmt" + n);
    }
    return p;
}

void TsfxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // MIDI learn + mapped controllers (applied directly; hosts accept this from the audio thread in practice)
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isController()) continue;
        const int cc = msg.getControllerNumber(), val = msg.getControllerValue();
        lastCC = cc; lastCCValue = val;
        const int li = learnIdx.load();
        if (li >= 0) { ccMap[cc] = li; learnIdx = -1; }
        const int pi = ccMap[cc].load();
        if (pi >= 0 && pi < (int) paramList.size()) paramList[(size_t) pi]->setValueNotifyingHost ((float) val / 127.f);
    }

    const int n = buffer.getNumSamples(), nch = buffer.getNumChannels();
    if (nch < 1 || n < 1) return;

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto hostBpm = pos->getBpm())
                bpm = *hostBpm;

    const auto p = readParams();
    uiIn.store (buffer.getMagnitude (0, n));

    if (nch >= 2)
        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, p, bpm);
    else
    {
        if ((int) scratchR.size() < n) scratchR.resize ((size_t) n);
        float* L = buffer.getWritePointer (0);
        std::copy (L, L + n, scratchR.begin());
        engine.process (L, scratchR.data(), n, p, bpm);
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + scratchR[(size_t) i]);
    }

    uiOut.store (buffer.getMagnitude (0, n));
    uiLevel.store (uiOut.load());
    uiX.store (engine.getX()); uiY.store (engine.getY());
    uiEnv.store (engine.getSource (SrcEnv)); uiTrans.store (engine.getTransition());
    uiLfoA.store (engine.getSource (SrcLfo1)); uiLfoB.store (engine.getSource (SrcLfo2)); uiCut.store (engine.getCutNorm());
}

VisFeat TsfxProcessor::getVisFeat() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    VisFeat v;
    v.level = uiLevel.load(); v.env = uiEnv.load(); v.x = uiX.load(); v.y = uiY.load();
    v.trans = uiTrans.load(); v.lfoA = uiLfoA.load(); v.lfoB = uiLfoB.load(); v.cut = uiCut.load();
    v.fb = juce::jmin (0.9f, 0.5f * (f ("fDlyFb") + f ("dDlyFb")) * 0.6f + 0.5f * f ("spDecay") * f ("spMix"));
    v.sat = 0.5f * (f ("fSat") + f ("dSat"));
    v.pan = 0.5f * (f ("fPan") + f ("dPan"));
    v.wet = f ("spMix");
    return v;
}

//==============================================================================
void TsfxProcessor::randomise (int group)
{
    juce::StringArray ids;
    if (group == 0) { for (int g = 1; g <= 11; ++g) ids.addArray (groupIds (g)); }
    else ids = groupIds (group);
    ids.removeDuplicates (false);
    for (auto& id : ids)
        if (auto* prm = apvts.getParameter (id))
        {
            const auto r = safeRange (id);
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (r.first + rng.nextFloat() * (r.second - r.first));
            prm->endChangeGesture();
        }
}

int TsfxProcessor::numFactoryPresets() { return (int) factoryPresets().size(); }
juce::String TsfxProcessor::factoryPresetName (int i) { return factoryPresets()[(size_t) juce::jlimit (0, numFactoryPresets() - 1, i)].name; }

void TsfxProcessor::resetToDefault()
{
    for (auto* p : paramList)
    {
        if (p->paramID.startsWith ("vis") || p->paramID == "engage") continue; // keep the screen + bypass as they are
        p->setValueNotifyingHost (p->getDefaultValue());
    }
    currentPresetName = "Init";
}

void TsfxProcessor::loadFactoryPreset (int i)
{
    if (i < 0 || i >= numFactoryPresets()) return;
    resetToDefault();
    for (auto& pv : factoryPresets()[(size_t) i].v)
        if (auto* prm = apvts.getParameter (pv.id)) prm->setValueNotifyingHost (prm->convertTo0to1 (pv.v));
    currentPresetName = factoryPresets()[(size_t) i].name;
}

juce::File TsfxProcessor::presetsFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("TransitionSpaceFX").getChildFile ("Presets");
}

bool TsfxProcessor::savePreset (const juce::File& f)
{
    presetsFolder().createDirectory();
    if (auto xml = apvts.copyState().createXml())
    {
        const auto target = f.withFileExtension ("tsfxpreset");
        if (xml->writeTo (target)) { currentPresetName = target.getFileNameWithoutExtension(); return true; }
    }
    return false;
}

bool TsfxProcessor::loadPresetFile (const juce::File& f)
{
    if (auto xml = juce::XmlDocument::parse (f))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPresetName = f.getFileNameWithoutExtension();
            return true;
        }
    return false;
}

void TsfxProcessor::switchAB (int slot)
{
    if (slot == abSlot) return;
    abState[abSlot] = apvts.copyState();
    abSlot = slot;
    if (abState[slot].isValid()) apvts.replaceState (abState[slot].createCopy());
}

void TsfxProcessor::copyToOther() { abState[1 - abSlot] = apvts.copyState(); }

int TsfxProcessor::indexOfParam (const juce::String& id) const
{
    for (size_t i = 0; i < paramList.size(); ++i) if (paramList[i]->paramID == id) return (int) i;
    return -1;
}
void TsfxProcessor::startLearn (const juce::String& id) { learnIdx = indexOfParam (id); }
void TsfxProcessor::clearLearn (const juce::String& id)
{
    const int idx = indexOfParam (id);
    for (auto& c : ccMap) if (c.load() == idx) c = -1;
    if (learnIdx.load() == idx) learnIdx = -1;
}
bool TsfxProcessor::isLearning (const juce::String& id) const { return learnIdx.load() >= 0 && learnIdx.load() == indexOfParam (id); }

void TsfxProcessor::restoreMidiMap (const juce::String& s)
{
    for (auto& c : ccMap) c = -1;
    for (auto& tok : juce::StringArray::fromTokens (s, ";", ""))
    {
        const int eq = tok.indexOfChar ('=');
        if (eq <= 0) continue;
        const int cc = tok.substring (0, eq).getIntValue(), idx = indexOfParam (tok.substring (eq + 1));
        if (cc >= 0 && cc < 128 && idx >= 0) ccMap[cc] = idx;
    }
}

void TsfxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto st = apvts.copyState();
    juce::String map;
    for (int cc = 0; cc < 128; ++cc)
    {
        const int pi = ccMap[cc].load();
        if (pi >= 0 && pi < (int) paramList.size()) map << cc << "=" << paramList[(size_t) pi]->paramID << ";";
    }
    st.setProperty ("midiMap", map, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, destData);
}

void TsfxProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (st);
            restoreMidiMap (st.getProperty ("midiMap").toString());
        }
}

juce::AudioProcessorEditor* TsfxProcessor::createEditor() { return new TsfxEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TsfxProcessor(); }
