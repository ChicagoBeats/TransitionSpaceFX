#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Dsp.h"
#include "VisualsCommon.h"

class TsfxProcessor : public juce::AudioProcessor
{
public:
    TsfxProcessor();
    ~TsfxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Transition Space FX"; }
    bool acceptsMidi() const override { return true; }   // MIDI learn
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 60.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // "Lazy" randomise buttons: 0 = everything, 1..11 = individual sections
    static constexpr int kNumLazy = 12;
    void randomise (int group);

    // presets
    static int numFactoryPresets();
    static juce::String factoryPresetName (int i);
    void loadFactoryPreset (int i);
    void resetToDefault();
    juce::File presetsFolder() const;
    bool savePreset (const juce::File& f);
    bool loadPresetFile (const juce::File& f);
    juce::String currentPresetName { "Init" };

    // A/B compare
    int abSlot = 0;
    void switchAB (int slot);
    void copyToOther();

    // MIDI learn
    void startLearn (const juce::String& paramId);
    void clearLearn (const juce::String& paramId);
    bool isLearning (const juce::String& paramId) const;
    std::atomic<int> lastCC { -1 }, lastCCValue { 0 };

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    // live values for the screen
    std::atomic<float> uiX { 0.5f }, uiY { 0.5f }, uiLevel { 0.f }, uiEnv { 0.f }, uiTrans { 0.f }, uiLfoA { 0.f }, uiLfoB { 0.f },
                       uiCut { 1.f }, uiIn { 0.f }, uiOut { 0.f };
    VisFeat getVisFeat() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    tsfx::Params readParams() const;
    int indexOfParam (const juce::String& id) const;
    void restoreMidiMap (const juce::String& s);

    tsfx::Engine engine;
    juce::Random rng;
    std::vector<juce::RangedAudioParameter*> paramList;
    std::atomic<int> ccMap[128];
    std::atomic<int> learnIdx { -1 };
    juce::ValueTree abState[2];
    std::vector<float> scratchR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsfxProcessor)
};
