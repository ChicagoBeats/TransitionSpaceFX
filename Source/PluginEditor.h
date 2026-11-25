#pragma once
#include "PluginProcessor.h"
#include "Visuals.h"

// One labelled control (knob / combo / toggle) bound to a parameter. Right-click = MIDI learn. Tooltip on hover.
class ParamCell : public juce::Component
{
public:
    ParamCell (TsfxProcessor& p, const juce::String& id, const juce::String& text);
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    TsfxProcessor& proc;
    juce::String paramId;
    juce::Label label;
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::ComboBox combo;
    juce::ToggleButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;
    enum Kind { Knob, Choice, Switch } kind = Knob;
};

class Section : public juce::Component
{
public:
    explicit Section (const juce::String& t) : title (t) {}
    void add (TsfxProcessor& p, const juce::String& id, const juce::String& text);
    int heightFor (int width) const;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int cellW = 104, cellH = 96;
    juce::String title;
    std::vector<std::unique_ptr<ParamCell>> cells;
};

class Meter : public juce::Component, private juce::Timer
{
public:
    Meter (std::atomic<float>& src, const juce::String& t) : source (src), text (t) { startTimerHz (30); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { const float x = source.load(); lvl = x > lvl ? x : lvl * 0.88f; repaint(); }
    std::atomic<float>& source;
    juce::String text;
    float lvl = 0.f;
};

class Content : public juce::Component
{
public:
    explicit Content (TsfxProcessor& p);
    int layout (int width);

private:
    juce::OwnedArray<Section> sections;
};

class TsfxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TsfxEditor (TsfxProcessor&);
    ~TsfxEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void showBrowseMenu();
    void doSave();

    TsfxProcessor& proc;
    juce::TextButton prevP { "<" }, nextP { ">" }, browse { "Browse" }, save { "Save" }, folder { "Folder" }, reset { "Reset" };
    juce::TextButton abA { "A" }, abB { "B" }, abCopy { "A>B" }, undoB { "Undo" }, redoB { "Redo" };
    juce::ToggleButton engage { "Engage" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> engageAtt;
    juce::Label presetLabel, ccLabel;
    Meter inMeter, outMeter;
    Visualizer visual;
    bool fullVisual = false;
    std::array<juce::TextButton, TsfxProcessor::kNumLazy> lazy;
    Content content;
    juce::Viewport viewport;
    juce::TooltipWindow tooltip { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser;
    int presetIdx = -1;
};
