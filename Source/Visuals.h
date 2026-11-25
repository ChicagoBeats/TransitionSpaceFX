#pragma once
#include "PluginProcessor.h"
#include "VisualsCommon.h"

// 50 space visuals (10 scene engines x 5 colour moods). Live mappings:
//  output level  -> pulse / speed / glow      LP cutoff   -> brightness
//  XY pad X / Y  -> camera offset, twist, tilt  LFO 1 / 2   -> hue drift, wobble
//  transition    -> zoom / link range           delay fdbk  -> trail length
//  saturation    -> contrast / sparkle          pan         -> sideways drift
class Visualizer : public juce::Component, private juce::Timer
{
public:
    explicit Visualizer (TsfxProcessor&);
    ~Visualizer() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent& e) override { setXY (e); }
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onFullToggle;

private:
    struct Ripple { float birth, str; };

    void timerCallback() override;
    void renderFrame();
    void setXY (const juce::MouseEvent&);
    int currentScene() const;
    void setSceneIndex (int idx);
    juce::Colour pc (float f, float v, float a) const;

    void sceneWarp (juce::Graphics&);   void sceneSpiral (juce::Graphics&);   void sceneNebula (juce::Graphics&);
    void sceneAurora (juce::Graphics&); void sceneBlackHole (juce::Graphics&); void scenePulsar (juce::Graphics&);
    void sceneConstel (juce::Graphics&); void sceneComets (juce::Graphics&);  void scenePlanet (juce::Graphics&);
    void sceneHorizon (juce::Graphics&);

    TsfxProcessor& proc;
    juce::Image canvas;
    juce::TextButton prev { "<" }, next { ">" }, fullBtn { "Full" };
    juce::ComboBox sceneBox;
    juce::ToggleButton autoBtn { "Auto" };
    juce::Slider react { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label reactLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> boxAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reactAtt;

    juce::Random rng;
    std::vector<Ripple> ripples;
    juce::String caption;
    int lastScene = -1;
    bool fresh = true;

    // time + smoothed features
    float t = 0, changeT = 0, lastSpawn = 0, prevL = 0, W = 1, H = 1, cx = 0, cy = 0, R = 1;
    double accA = 0, accB = 0, accC = 0;
    float lvl = 0, env = 0, fx = .5f, fy = .5f, tr = 0, la = 0, lb = 0, cut = 1, fb = 0, sat = 0, pan = 0, wet = 1;
    float eL = 0, react_ = 1, hA = .6f, hB = .5f, pSat = .7f, hueShift = 0, bright = 1;
};
