#include "PluginEditor.h"
#include <map>

using namespace tsfx;

namespace
{
struct IdLabel { const char* id; const char* text; };

// short plain-language explanations; anything not listed falls back to the parameter name
juce::String describe (const juce::String& id, const juce::String& name)
{
    static const std::map<juce::String, juce::String> tips = {
        { "spSize", "SIZE: how far away the space is (short rooms to huge halls)" },
        { "spDecay", "DECAY: about 2 s up to a minute; 100% is infinite" },
        { "spDrift", "DRIFT: slow movement in the tail, from still to a wide drift" },
        { "spTone", "TONE: high cut of the tail, 1.2 to 20 kHz" },
        { "spShimmer", "SHIMMER: an octave-up halo blended into the space" },
        { "spFreeze", "FREEZE: hold the space forever as an endless pad" },
        { "spDuck", "DUCK: the space steps back under the signal and blooms in the gaps" },
        { "spSync", "SYNC: the space starts a 1/16 note after the sound, at your tempo" },
        { "spMix", "MIX: dry at full level to 50%, the space at full level from 50%" },
        { "spType", "Space character: Hall, Room, Plate, or Blackhole (a tail that swells)" },
        { "grnMix", "How much granular processing replaces the input before the filters" },
        { "grnMode", "Classic = steady random grains, Rhythmic = grains fired by the Euclid channel, Texture = dense wide clouds" },
        { "grnPos", "How far back in the live buffer grains are taken from" },
        { "grnQuant", "Snap grain pitches to the chosen scale and root" },
        { "grnCluster", "Layer extra pitched grains: octaves, fifths, triads or a tight cluster" },
        { "grnFreeze", "Stop recording; grains keep replaying what is in the buffer" },
        { "ufoMode", "Manual: you place the UFO. Auto: LFOs, random or a bouncing ball move it for you" },
        { "ufoCenter", "On: each slot swings both ways around the middle of the pad" },
        { "engage", "Click-free bypass: hear the before and after" },
        { "transSrc", "Off = the transition follows the input signal; or pick any modulation source to drive it" },
        { "eucMerge", "How layers A and B combine into one rhythm" },
        { "eucGrainCh", "Which Euclid channel fires Rhythmic grains" },
        { "lpEgMix", "Blend between the chosen mod source (0) and the filter envelope (1)" },
    };
    auto it = tips.find (id);
    return (it != tips.end() ? it->second : name) + "  (right-click: MIDI learn)";
}
} // namespace

//==============================================================================
ParamCell::ParamCell (TsfxProcessor& p, const juce::String& id, const juce::String& text) : proc (p), paramId (id)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (12.f)));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    auto& s = p.apvts;
    auto* prm = s.getParameter (id);
    const juce::String tip = prm != nullptr ? describe (id, prm->getName (100)) : text;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (prm))
    {
        kind = Choice;
        combo.addItemList (c->choices, 1);
        combo.setTooltip (tip);
        addAndMakeVisible (combo);
        cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, combo);
    }
    else if (dynamic_cast<juce::AudioParameterBool*> (prm) != nullptr)
    {
        kind = Switch;
        toggle.setButtonText ("On");
        toggle.setTooltip (tip);
        addAndMakeVisible (toggle);
        bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, toggle);
    }
    else
    {
        kind = Knob;
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 14);
        slider.setTooltip (tip);
        addAndMakeVisible (slider);
        sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    }
    addMouseListener (this, true); // so right-clicks on the child controls reach us
}

void ParamCell::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    switch (kind)
    {
        case Knob:   slider.setBounds (r); break;
        case Choice: combo.setBounds (r.withSizeKeepingCentre (r.getWidth() - 8, 24)); break;
        case Switch: toggle.setBounds (r.withSizeKeepingCentre (r.getWidth() - 20, 24)); break;
    }
}

void ParamCell::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) return;
    juce::PopupMenu m;
    m.addItem (1, proc.isLearning (paramId) ? "Waiting for a MIDI knob..." : "MIDI Learn (then move a knob)");
    m.addItem (2, "Clear MIDI mapping");
    juce::Component::SafePointer<ParamCell> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1) safe->proc.startLearn (safe->paramId);
        else if (r == 2) safe->proc.clearLearn (safe->paramId);
    });
}

//==============================================================================
void Section::add (TsfxProcessor& p, const juce::String& id, const juce::String& text)
{
    cells.push_back (std::make_unique<ParamCell> (p, id, text));
    addAndMakeVisible (*cells.back());
}

int Section::heightFor (int width) const
{
    const int per = std::max (1, (width - 16) / cellW);
    return 30 + (((int) cells.size() + per - 1) / per) * cellH + 8;
}

void Section::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff1d2433));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.f), 8.f);
    g.setColour (juce::Colour (0xff8fb8ff));
    g.setFont (juce::Font (juce::FontOptions (15.f)));
    g.drawText (title, 14, 6, getWidth() - 28, 20, juce::Justification::left);
}

void Section::resized()
{
    const int per = std::max (1, (getWidth() - 16) / cellW);
    for (size_t i = 0; i < cells.size(); ++i)
        cells[i]->setBounds (8 + ((int) i % per) * cellW, 30 + ((int) i / per) * cellH, cellW, cellH);
}

void Meter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.f);
    g.setColour (juce::Colour (0xff0b0f18));
    g.fillRoundedRectangle (b, 4.f);
    const float db = juce::Decibels::gainToDecibels (lvl, -60.f);
    const float w = juce::jmap (db, -60.f, 0.f, 0.f, b.getWidth());
    g.setColour (db > -3.f ? juce::Colour (0xffff5c5c) : db > -12.f ? juce::Colour (0xffffd24c) : juce::Colour (0xff4cc9ff));
    g.fillRoundedRectangle (b.withWidth (w), 4.f);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (11.f)));
    g.drawText (text, b.toNearestInt().reduced (4, 0), juce::Justification::centredLeft);
}

//==============================================================================
Content::Content (TsfxProcessor& p)
{
    auto make = [&] (const juce::String& title) { auto* s = sections.add (new Section (title)); addAndMakeVisible (s); return s; };

    auto* sup = make ("Super Modulation  (drag on the picture above to move the UFO)");
    for (auto* id : { "ufoMode", "ufoHide", "ufoCenter", "ufoXSrc", "ufoYSrc", "ufoXAuto", "ufoYAuto", "ufoSpeed" }) sup->add (p, id, p.apvts.getParameter (id)->getName (14));
    for (int s = 1; s <= kUfoSlots; ++s)
    {
        const juce::String n = "ufoS" + juce::String (s), i (s);
        sup->add (p, n + "Axis", "Axis " + i); sup->add (p, n + "Dst", "Dest " + i); sup->add (p, n + "Amt", "Depth " + i);
    }

    auto* sp = make ("Space");
    for (const auto& e : std::initializer_list<IdLabel> { { "spType", "Type" }, { "spSize", "Size" }, { "spDecay", "Decay" }, { "spDrift", "Drift" },
                          { "spTone", "Tone" }, { "spShimmer", "Shimmer" }, { "spFreeze", "Freeze" }, { "spDuck", "Duck" }, { "spSync", "Sync" }, { "spMix", "Mix" } })
        sp->add (p, e.id, e.text);

    auto* gr = make ("Granular");
    for (const auto& e : std::initializer_list<IdLabel> { { "grnMode", "Mode" }, { "grnMix", "Mix" }, { "grnSize", "Size ms" }, { "grnDens", "Density" },
                          { "grnPos", "Position" }, { "grnPitch", "Pitch" }, { "grnSpread", "Spread" }, { "grnJit", "Jitter" }, { "grnRev", "Reverse" },
                          { "grnFreeze", "Freeze" }, { "grnQuant", "Quantize" }, { "grnScale", "Scale" }, { "grnRoot", "Root" }, { "grnCluster", "Cluster" } })
        gr->add (p, e.id, e.text);

    for (juce::String pre : { "lp", "hp" })
    {
        auto* f = make (pre == "lp" ? "Low-pass 24 dB" : "High-pass 12 dB");
        f->add (p, pre + "Cut", "Cutoff"); f->add (p, pre + "Q", "Reso"); f->add (p, pre + "Src", "Mod Src"); f->add (p, pre + "Depth", "Mod Depth");
        f->add (p, pre + "EgMix", "LFO : EG"); f->add (p, pre + "Inv", "Invert");
        f->add (p, pre + "A", "EG Atk"); f->add (p, pre + "D", "EG Dcy"); f->add (p, pre + "S", "EG Sus"); f->add (p, pre + "R", "EG Rls");
        if (pre == "lp") f->add (p, "lpHpMix", "LP : HP");
    }

    auto* tr = make ("Transition  (input starts a D to F morph, or pick a mod source)");
    for (const auto& e : std::initializer_list<IdLabel> { { "fdMix", "D : F Mix" }, { "transOn", "Transition" }, { "transTime", "Time" }, { "transHold", "Hold" },
                          { "transRel", "Release" }, { "transSrc", "Mod Src" }, { "transInv", "Invert" } })
        tr->add (p, e.id, e.text);

    for (juce::String pre : { "f", "d" })
    {
        auto* c = make (pre == "f" ? "Channel F (processed)" : "Channel D (direct)");
        c->add (p, pre + "Lo", "LoBoost"); c->add (p, pre + "Sat", "Saturate"); c->add (p, pre + "Pan", "Pan"); c->add (p, pre + "DlyOn", "Delay");
        c->add (p, pre + "DlyLvl", "Dly Level"); c->add (p, pre + "DlyFb", "Dly Fdbk"); c->add (p, pre + "DlyDiv", "Dly Time"); c->add (p, pre + "Color", "Dly Color");
    }

    auto* out = make ("Output");
    for (const auto& e : std::initializer_list<IdLabel> { { "delayType", "Delay Type" }, { "tape", "Tape W/F" }, { "oppPan", "Opp. Dly Pan" },
                          { "splitOn", "Freq Split" }, { "splitFreq", "Split Hz" }, { "width", "Width" }, { "haasMs", "Haas ms" }, { "haasSide", "Haas Side" },
                          { "wet", "Wet" }, { "outGain", "Out dB" } })
        out->add (p, e.id, e.text);

    auto* lfo = make ("LFOs (5th = PLFO) + Sample & Hold");
    for (int k = 1; k <= kNumLfo; ++k)
    {
        const juce::String id = "lfo" + juce::String (k), n = (k == 5 ? juce::String ("PLFO") : "LFO" + juce::String (k)) + " ";
        lfo->add (p, id + "Rate", n + "Rate"); lfo->add (p, id + "Shape", n + "Shape"); lfo->add (p, id + "Sync", n + "Sync");
        lfo->add (p, id + "Ks", n + "KeySync"); lfo->add (p, id + "Depth", n + "Depth");
    }
    lfo->add (p, "shRate", "S&H Rate"); lfo->add (p, "shVar", "S&H Var");

    auto* st = make ("Step Modulator");
    st->add (p, "stepRate", "Rate"); st->add (p, "stepSmooth", "Smooth"); st->add (p, "stepMode", "Mode"); st->add (p, "stepKeyReset", "Key Reset");
    for (int i = 1; i <= kNumSteps; ++i) st->add (p, "step" + juce::String (i), "Step " + juce::String (i));

    auto* eu = make ("Euclid Sequencer  (8 channels, 2 merged layers; drives mod sources and Rhythmic grains)");
    eu->add (p, "eucMerge", "Merge"); eu->add (p, "eucDiv", "Rate"); eu->add (p, "eucDecay", "Pulse ms"); eu->add (p, "eucGrainCh", "> Grains");
    for (int c = 1; c <= kNumEuc; ++c)
        for (juce::String lay : { "A", "B" })
        {
            const juce::String id = "euc" + lay + juce::String (c), n = juce::String (c) + lay + " ";
            eu->add (p, id + "Steps", n + "Steps"); eu->add (p, id + "Pulses", n + "Pulses"); eu->add (p, id + "Rot", n + "Rotate");
        }

    auto* mc = make ("Macros");
    for (int i = 1; i <= 4; ++i) mc->add (p, "macro" + juce::String (i), "Macro " + juce::String (i));

    auto* mat = make ("Mod Matrix");
    for (int i = 1; i <= kNumSlots; ++i)
    {
        const juce::String id (i);
        mat->add (p, "slotSrc" + id, "Src " + id); mat->add (p, "slotDst" + id, "Dest " + id); mat->add (p, "slotAmt" + id, "Amt " + id);
    }
}

int Content::layout (int width)
{
    int y = 0;
    for (auto* sec : sections)
    {
        const int h = sec->heightFor (width);
        sec->setBounds (0, y, width, h);
        y += h;
    }
    setSize (width, y);
    return y;
}

//==============================================================================
TsfxEditor::TsfxEditor (TsfxProcessor& p)
    : AudioProcessorEditor (&p), proc (p), inMeter (p.uiIn, "IN"), outMeter (p.uiOut, "OUT"), visual (p), content (p)
{
    for (juce::Component* c : { (juce::Component*) &prevP, (juce::Component*) &nextP, (juce::Component*) &browse, (juce::Component*) &save,
                                (juce::Component*) &folder, (juce::Component*) &reset, (juce::Component*) &abA, (juce::Component*) &abB,
                                (juce::Component*) &abCopy, (juce::Component*) &undoB, (juce::Component*) &redoB, (juce::Component*) &engage,
                                (juce::Component*) &presetLabel, (juce::Component*) &ccLabel, (juce::Component*) &inMeter, (juce::Component*) &outMeter,
                                (juce::Component*) &visual, (juce::Component*) &viewport })
        addAndMakeVisible (c);

    presetLabel.setJustificationType (juce::Justification::centred);
    presetLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff0b0f18));
    ccLabel.setJustificationType (juce::Justification::centredRight);
    ccLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    engageAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "engage", engage);
    engage.setTooltip (describe ("engage", "Engage"));

    prevP.setTooltip ("Previous preset"); nextP.setTooltip ("Next preset"); browse.setTooltip ("Pick a factory or saved preset");
    save.setTooltip ("Save your settings as a .tsfxpreset file"); folder.setTooltip ("Open the presets folder to back up or share");
    reset.setTooltip ("Back to a clean start"); abA.setTooltip ("Setting A"); abB.setTooltip ("Setting B"); abCopy.setTooltip ("Copy this setting to the other slot");
    undoB.setTooltip ("Undo (Ctrl+Z)"); redoB.setTooltip ("Redo (Ctrl+Y)");

    prevP.onClick = [this] { presetIdx = (presetIdx - 1 + TsfxProcessor::numFactoryPresets()) % TsfxProcessor::numFactoryPresets(); proc.loadFactoryPreset (presetIdx); };
    nextP.onClick = [this] { presetIdx = (presetIdx + 1) % TsfxProcessor::numFactoryPresets(); proc.loadFactoryPreset (presetIdx); };
    browse.onClick = [this] { showBrowseMenu(); };
    save.onClick = [this] { doSave(); };
    folder.onClick = [this] { proc.presetsFolder().createDirectory(); proc.presetsFolder().revealToUser(); };
    reset.onClick = [this] { presetIdx = -1; proc.resetToDefault(); };
    abA.onClick = [this] { proc.switchAB (0); };
    abB.onClick = [this] { proc.switchAB (1); };
    abCopy.onClick = [this] { proc.copyToOther(); };
    undoB.onClick = [this] { proc.undoManager.undo(); };
    redoB.onClick = [this] { proc.undoManager.redo(); };

    const char* lazyNames[TsfxProcessor::kNumLazy] = { "Lazy All", "Filters", "Mix/Trans", "Chan F", "Chan D", "Delays", "Pan/Split", "LFOs", "Step/Euc", "Matrix", "SupM", "Grain/Space" };
    for (int i = 0; i < TsfxProcessor::kNumLazy; ++i)
    {
        lazy[(size_t) i].setButtonText (lazyNames[i]);
        lazy[(size_t) i].setTooltip ("Randomise this section with musical limits");
        lazy[(size_t) i].onClick = [this, i] { proc.randomise (i); };
        addAndMakeVisible (lazy[(size_t) i]);
    }
    visual.onFullToggle = [this] { fullVisual = ! fullVisual; resized(); };
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);

    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (750, 560, 2000, 1600);
    const int w = (int) proc.apvts.state.getProperty ("uiW", 1000), h = (int) proc.apvts.state.getProperty ("uiH", 900);
    setSize (juce::jlimit (750, 2000, w), juce::jlimit (560, 1600, h));
    startTimerHz (10);
}

TsfxEditor::~TsfxEditor() { stopTimer(); }

void TsfxEditor::timerCallback()
{
    presetLabel.setText (proc.currentPresetName, juce::dontSendNotification);
    const int cc = proc.lastCC.load();
    ccLabel.setText (cc < 0 ? "CC# --  Value --" : "CC# " + juce::String (cc) + "  Value " + juce::String (proc.lastCCValue.load()), juce::dontSendNotification);
    abA.setToggleState (proc.abSlot == 0, juce::dontSendNotification);
    abB.setToggleState (proc.abSlot == 1, juce::dontSendNotification);
    undoB.setEnabled (proc.undoManager.canUndo());
    redoB.setEnabled (proc.undoManager.canRedo());
}

bool TsfxEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { proc.undoManager.undo(); return true; }
    if (k == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0)
        || k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { proc.undoManager.redo(); return true; }
    return false;
}

void TsfxEditor::showBrowseMenu()
{
    juce::PopupMenu m;
    juce::PopupMenu fac, usr;
    for (int i = 0; i < TsfxProcessor::numFactoryPresets(); ++i) fac.addItem (1 + i, TsfxProcessor::factoryPresetName (i));
    m.addSubMenu ("Factory presets", fac);
    auto files = proc.presetsFolder().findChildFiles (juce::File::findFiles, false, "*.tsfxpreset");
    files.sort();
    for (int i = 0; i < files.size(); ++i) usr.addItem (1000 + i, files[i].getFileNameWithoutExtension());
    if (files.isEmpty()) usr.addItem (-1, "(none saved yet)", false);
    m.addSubMenu ("My presets", usr);
    juce::Component::SafePointer<TsfxEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&browse), [safe, files] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r >= 1000) { if (r - 1000 < files.size()) safe->proc.loadPresetFile (files[r - 1000]); }
        else { safe->presetIdx = r - 1; safe->proc.loadFactoryPreset (r - 1); }
    });
}

void TsfxEditor::doSave()
{
    proc.presetsFolder().createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save preset", proc.presetsFolder().getChildFile (proc.currentPresetName + ".tsfxpreset"), "*.tsfxpreset");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc) { const auto f = fc.getResult(); if (f != juce::File()) proc.savePreset (f); });
}

void TsfxEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121722));
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (20.f)));
    g.drawText ("Transition Space FX", 16, 6, 260, 28, juce::Justification::left);
}

void TsfxEditor::resized()
{
    proc.apvts.state.setProperty ("uiW", getWidth(), nullptr);
    proc.apvts.state.setProperty ("uiH", getHeight(), nullptr);

    auto r = getLocalBounds();
    auto row1 = r.removeFromTop (34).reduced (8, 3);
    row1.removeFromLeft (250);
    prevP.setBounds (row1.removeFromLeft (28));
    presetLabel.setBounds (row1.removeFromLeft (std::min (240, std::max (120, row1.getWidth() - 330))));
    nextP.setBounds (row1.removeFromLeft (28));
    row1.removeFromLeft (8);
    browse.setBounds (row1.removeFromLeft (70)); save.setBounds (row1.removeFromLeft (56)); folder.setBounds (row1.removeFromLeft (60)); reset.setBounds (row1.removeFromLeft (56));

    auto row2 = r.removeFromTop (30).reduced (8, 2);
    abA.setBounds (row2.removeFromLeft (34)); abB.setBounds (row2.removeFromLeft (34)); abCopy.setBounds (row2.removeFromLeft (48));
    row2.removeFromLeft (10);
    undoB.setBounds (row2.removeFromLeft (54)); redoB.setBounds (row2.removeFromLeft (54));
    row2.removeFromLeft (10);
    engage.setBounds (row2.removeFromLeft (80));
    inMeter.setBounds (row2.removeFromLeft (110).reduced (0, 3)); row2.removeFromLeft (6);
    outMeter.setBounds (row2.removeFromLeft (110).reduced (0, 3));
    ccLabel.setBounds (row2);

    const int vh = fullVisual ? std::max (60, r.getHeight() - 30) : 300;
    visual.setBounds (r.removeFromTop (vh));
    auto bar = r.removeFromTop (30).reduced (8, 0);
    const int bw = bar.getWidth() / TsfxProcessor::kNumLazy;
    for (auto& b : lazy) b.setBounds (bar.removeFromLeft (bw).reduced (2, 0));
    viewport.setVisible (! fullVisual);
    if (! fullVisual)
    {
        viewport.setBounds (r.reduced (4));
        content.layout (viewport.getMaximumVisibleWidth());
    }
}
