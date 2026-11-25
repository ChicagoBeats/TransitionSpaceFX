#include "Visuals.h"

namespace
{
constexpr float TAU = 6.2831853f;
const float kHueA[5] = { 0.60f, 0.78f, 0.42f, 0.99f, 0.11f };
const float kHueB[5] = { 0.52f, 0.90f, 0.30f, 0.08f, 0.55f };
const float kPalSat[5] = { 0.75f, 0.70f, 0.65f, 0.80f, 0.75f };

// deterministic hash -> 0..1 (PCG style), lets scenes be fully procedural
float h01 (int n)
{
    uint32_t x = (uint32_t) n * 747796405u + 2891336453u;
    x = ((x >> ((x >> 28) + 4)) ^ x) * 277803737u;
    x = (x >> 22) ^ x;
    return (float) x / 4294967295.f;
}
} // namespace

Visualizer::Visualizer (TsfxProcessor& p) : proc (p)
{
    juce::StringArray names;
    for (int i = 0; i < kNumVisuals; ++i)
        names.add (juce::String (kVisScenes[i / 5]) + " - " + kVisMoods[i % 5]);
    sceneBox.addItemList (names, 1);
    boxAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "visScene", sceneBox);
    autoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "visAuto", autoBtn);
    reactAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "visReact", react);
    reactLabel.setText ("React", juce::dontSendNotification);
    reactLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));

    prev.onClick = [this] { setSceneIndex (currentScene() - 1); };
    next.onClick = [this] { setSceneIndex (currentScene() + 1); };
    fullBtn.onClick = [this] { if (onFullToggle) onFullToggle(); };
    for (juce::Component* c : { (juce::Component*) &prev, (juce::Component*) &next, (juce::Component*) &sceneBox,
                                (juce::Component*) &autoBtn, (juce::Component*) &react, (juce::Component*) &reactLabel,
                                (juce::Component*) &fullBtn })
        addAndMakeVisible (c);

    startTimerHz (30);
}

int Visualizer::currentScene() const
{
    return juce::jlimit (0, kNumVisuals - 1, (int) proc.apvts.getRawParameterValue ("visScene")->load());
}

void Visualizer::setSceneIndex (int idx)
{
    idx = (idx % kNumVisuals + kNumVisuals) % kNumVisuals;
    if (auto* prm = proc.apvts.getParameter ("visScene"))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) idx));
        prm->endChangeGesture();
    }
}

void Visualizer::resized()
{
    canvas = juce::Image (juce::Image::ARGB, std::max (1, getWidth()), std::max (1, getHeight()), true);
    fresh = true;
    auto r = getLocalBounds().removeFromTop (28).reduced (4, 2);
    prev.setBounds (r.removeFromLeft (28));
    next.setBounds (r.removeFromLeft (28));
    r.removeFromLeft (4);
    fullBtn.setBounds (r.removeFromRight (52));
    reactLabel.setBounds (r.removeFromRight (44));
    react.setBounds (r.removeFromRight (90));
    autoBtn.setBounds (r.removeFromRight (64));
    sceneBox.setBounds (r.reduced (2, 0));
}

void Visualizer::setXY (const juce::MouseEvent& e)
{
    const float x = juce::jlimit (0.f, 1.f, (float) e.x / (float) std::max (1, getWidth()));
    const float y = juce::jlimit (0.f, 1.f, 1.f - (float) e.y / (float) std::max (1, getHeight()));
    if (auto* px = proc.apvts.getParameter ("ufoX")) px->setValueNotifyingHost (x);
    if (auto* py = proc.apvts.getParameter ("ufoY")) py->setValueNotifyingHost (y);
}

void Visualizer::mouseDown (const juce::MouseEvent& e)
{
    if (auto* px = proc.apvts.getParameter ("ufoX")) px->beginChangeGesture();
    if (auto* py = proc.apvts.getParameter ("ufoY")) py->beginChangeGesture();
    setXY (e);
}

void Visualizer::mouseUp (const juce::MouseEvent&)
{
    if (auto* px = proc.apvts.getParameter ("ufoX")) px->endChangeGesture();
    if (auto* py = proc.apvts.getParameter ("ufoY")) py->endChangeGesture();
}

juce::Colour Visualizer::pc (float f, float v, float a) const
{
    const float h = std::fmod (hA + (hB - hA) * f + hueShift + 10.f, 1.f);
    return juce::Colour::fromHSV (h, juce::jlimit (0.f, 1.f, pSat), juce::jlimit (0.f, 1.f, v * bright), juce::jlimit (0.f, 1.f, a));
}

void Visualizer::timerCallback()
{
    const float dt = 1.f / 30.f;
    t += dt;

    // auto-cycle mood every 25 s
    if (proc.apvts.getRawParameterValue ("visAuto")->load() > 0.5f && t - changeT > 25.f)
        setSceneIndex (rng.nextInt (kNumVisuals));

    const int sc = currentScene();
    if (sc != lastScene)
    {
        lastScene = sc;
        changeT = t;
        fresh = true;
        ripples.clear();
        caption = juce::String (kVisScenes[sc / 5]) + " - " + kVisMoods[sc % 5];
        hA = kHueA[sc % 5]; hB = kHueB[sc % 5]; pSat = kPalSat[sc % 5];
    }

    const VisFeat F = proc.getVisFeat();
    auto sm = [] (float& s, float v, float up, float down) { s += (v - s) * (v > s ? up : down); };
    sm (lvl, std::pow (juce::jmin (1.f, F.level * 1.4f), 0.6f), 0.5f, 0.08f);
    sm (env, F.env, 0.3f, 0.05f);
    sm (fx, F.x, 0.2f, 0.2f);   sm (fy, F.y, 0.2f, 0.2f);
    sm (tr, F.trans, 0.2f, 0.2f);
    la = F.lfoA; lb = F.lfoB;
    sm (cut, F.cut, 0.1f, 0.1f);
    sm (fb, F.fb, 0.1f, 0.1f);
    sm (sat, F.sat, 0.1f, 0.1f);
    sm (pan, F.pan, 0.1f, 0.1f);
    sm (wet, F.wet, 0.1f, 0.1f);

    react_ = proc.apvts.getRawParameterValue ("visReact")->load();
    eL = juce::jlimit (0.f, 2.f, (lvl + 0.15f * env) * react_);
    hueShift = 0.06f * la * react_;
    bright = 0.55f + 0.45f * cut;

    accA += dt * (0.10 + 0.55 * eL + 0.5 * (fy - 0.5f) + 0.12 * tr);
    accB += dt * (0.10 + 0.25 * eL + 0.25 * lb + 0.35 * (fx - 0.5f));
    accC += dt * (0.2 + 0.8 * eL);

    // pulsar ripples: spawn on level jumps
    if (eL - prevL > 0.12f && t - lastSpawn > 0.25f) { ripples.push_back ({ t, eL }); lastSpawn = t; }
    else if (t - lastSpawn > 1.8f) { ripples.push_back ({ t, 0.3f }); lastSpawn = t; }
    prevL = eL;

    renderFrame();
    repaint();
}

void Visualizer::renderFrame()
{
    if (canvas.isNull()) return;
    W = (float) canvas.getWidth(); H = (float) canvas.getHeight();
    cx = W * 0.5f + pan * W * 0.08f; cy = H * 0.5f; R = std::min (W, H) * 0.5f;

    juce::Graphics g (canvas);
    if (fresh) { g.fillAll (juce::Colour::fromHSV (hA, 0.7f, 0.03f, 1.f)); fresh = false; }
    // trail length follows delay feedback: more feedback = longer smears
    const float fade = 0.32f - 0.27f * juce::jlimit (0.f, 1.f, fb / 0.9f);
    g.setColour (juce::Colour::fromHSV (hA, 0.7f, 0.035f, fade));
    g.fillAll();

    switch (currentScene() / 5)
    {
        case 0: sceneWarp (g); break;     case 1: sceneSpiral (g); break;   case 2: sceneNebula (g); break;
        case 3: sceneAurora (g); break;   case 4: sceneBlackHole (g); break; case 5: scenePulsar (g); break;
        case 6: sceneConstel (g); break;  case 7: sceneComets (g); break;   case 8: scenePlanet (g); break;
        default: sceneHorizon (g); break;
    }

    // saturation = sparkle: random glints
    const int glints = (int) (sat * 40.f * react_);
    for (int i = 0; i < glints; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (0.6f * rng.nextFloat()));
        g.fillRect (rng.nextFloat() * W, rng.nextFloat() * H, 1.5f, 1.5f);
    }
}

void Visualizer::paint (juce::Graphics& g)
{
    g.drawImageAt (canvas, 0, 0);
    juce::ColourGradient vg (juce::Colours::transparentBlack, (float) getWidth() * 0.5f, (float) getHeight() * 0.5f,
                             juce::Colours::black.withAlpha (0.55f), 0.f, 0.f, true);
    g.setGradientFill (vg);
    g.fillAll();
    {   // Super Modulation UFO (same idea as the classic pad): crosshair at centre, saucer at the modulated position
        const VisFeat F = proc.getVisFeat();
        const float px = F.x * (float) getWidth(), py = (1.f - F.y) * (float) getHeight();
        if (proc.apvts.getRawParameterValue ("ufoCenter")->load() > 0.5f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.drawVerticalLine (getWidth() / 2, 30.f, (float) getHeight());
            g.drawHorizontalLine (getHeight() / 2, 0.f, (float) getWidth());
        }
        if (proc.apvts.getRawParameterValue ("ufoHide")->load() < 0.5f)
        {
            g.saveState();
            g.addTransform (juce::AffineTransform::rotation (-0.3f, px, py));
            g.setColour (juce::Colour (0xffcfe8ff).withAlpha (0.22f));
            g.fillEllipse (px - 26.f, py - 11.f, 52.f, 22.f);
            g.setColour (juce::Colour (0xffcfe8ff));
            g.drawEllipse (px - 20.f, py - 8.f, 40.f, 16.f, 2.f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (px - 6.f, py - 3.f, 12.f, 6.f);
            g.restoreState();
        }
    }
    const float age = t - changeT;
    if (age < 3.f)
    {
        g.setColour (juce::Colours::white.withAlpha (1.f - age / 3.f));
        g.setFont (juce::Font (juce::FontOptions (18.f)));
        g.drawText (caption, 12, getHeight() - 32, getWidth() - 24, 24, juce::Justification::left);
    }
}

//==============================================================================
void Visualizer::sceneWarp (juce::Graphics& g)
{
    const float ox = cx + (fx - 0.5f) * W * 0.5f, oy = cy + (0.5f - fy) * H * 0.5f;
    const float maxR = std::sqrt (W * W + H * H) * 0.6f;
    for (int i = 0; i < 280; ++i)
    {
        const float ang = h01 (i * 3) * TAU, spd = 0.4f + h01 (i * 3 + 1) * 0.9f;
        const float z = std::fmod ((float) accA * spd + h01 (i * 3 + 2), 1.f);
        const float z0 = std::max (0.f, z - 0.04f - 0.05f * eL);
        const float r1 = z * z * maxR, r0 = z0 * z0 * maxR, c = std::cos (ang), s = std::sin (ang);
        g.setColour (pc (h01 (i * 7), 1.f, 0.2f + 0.8f * z));
        g.drawLine (ox + c * r0, oy + s * r0, ox + c * r1, oy + s * r1, 0.6f + z * 2.2f * (1.f + eL));
    }
    juce::ColourGradient gr (pc (0.f, 1.f, 0.35f + 0.3f * eL), ox, oy, pc (0.f, 1.f, 0.f), ox + R * 0.2f * (1.f + eL), oy, true);
    g.setGradientFill (gr);
    g.fillEllipse (ox - R * 0.3f, oy - R * 0.3f, R * 0.6f, R * 0.6f);
}

void Visualizer::sceneSpiral (juce::Graphics& g)
{
    const int arms = 4;
    const float twist = 3.f + 4.f * fx, tilt = 0.35f + 0.6f * fy, gal = 0.5f + 0.3f * lb;
    const float cg = std::cos (gal), sg = std::sin (gal), rot = (float) accB;
    for (int i = 0; i < 900; ++i)
    {
        const float d = std::pow (h01 (i * 2), 1.6f);
        const float a = (float) (i % arms) * TAU / (float) arms + d * twist + rot * (1.f - 0.5f * d)
                        + (h01 (i * 5 + 3) - 0.5f) * 0.35f * (0.3f + d);
        const float rad = d * R * 0.95f;
        const float x0 = std::cos (a) * rad, y0 = std::sin (a) * rad * tilt;
        const float x = cx + x0 * cg - y0 * sg, y = cy + x0 * sg + y0 * cg;
        const float s = 0.8f + 2.2f * (1.f - d) * (1.f + eL * 1.5f);
        g.setColour (pc (d, 0.7f + 0.3f * (1.f - d), 0.35f + 0.6f * (1.f - d)));
        g.fillEllipse (x - s * 0.5f, y - s * 0.5f, s, s);
    }
    juce::ColourGradient core (pc (0.f, 1.f, 0.9f), cx, cy, pc (0.f, 1.f, 0.f), cx + R * 0.25f * (1.f + 1.2f * eL), cy, true);
    g.setGradientFill (core);
    const float cr = R * 0.25f * (1.f + 1.2f * eL);
    g.fillEllipse (cx - cr, cy - cr, cr * 2.f, cr * 2.f);
}

void Visualizer::sceneNebula (juce::Graphics& g)
{
    for (int k = 0; k < 14; ++k)
    {
        const float sp = 0.07f * (0.5f + h01 (k));
        const float bx = cx + std::sin (t * sp + (float) k * 1.7f) * W * 0.35f + (fx - 0.5f) * W * 0.2f * (float) (k % 3);
        const float by = cy + std::cos (t * sp * 1.3f + (float) k * 2.3f) * H * 0.3f;
        const float rad = R * (0.35f + 0.5f * h01 (k + 30)) * (1.f + 0.4f * eL + 0.2f * tr);
        const float f = h01 (k + 60);
        juce::ColourGradient gr (pc (f, 0.9f, 0.10f + 0.22f * eL + 0.05f), bx, by, pc (f, 0.9f, 0.f), bx + rad, by, true);
        g.setGradientFill (gr);
        g.fillEllipse (bx - rad, by - rad, rad * 2.f, rad * 2.f);
    }
    for (int i = 0; i < 90; ++i)
    {
        const float tw = 0.5f + 0.5f * std::sin (t * (1.f + 3.f * h01 (i * 3)) + h01 (i * 3 + 1) * 20.f);
        g.setColour (juce::Colours::white.withAlpha (tw * 0.7f));
        g.fillEllipse (h01 (i * 3 + 2) * W, h01 (i * 3 + 5) * H, 1.8f, 1.8f);
    }
}

void Visualizer::sceneAurora (juce::Graphics& g)
{
    for (int k = 0; k < 5; ++k)
    {
        const float base = H * (0.25f + 0.12f * (float) k) - (fy - 0.5f) * H * 0.2f;
        const float amp = H * (0.06f + 0.06f * h01 (k)) * (1.f + eL * 1.5f);
        const float freq = 0.006f + 0.004f * h01 (k + 9), speed = 0.4f + 0.12f * (float) k;
        juce::Path edge;
        for (float x = 0.f; x < W; x += 5.f)
        {
            const float ph = x * freq + t * speed + (float) k * 2.f + la * 1.5f;
            const float y = base + std::sin (ph) * amp + std::sin (ph * 2.3f + t * 0.6f) * amp * 0.4f;
            const float len = H * (0.12f + 0.2f * (0.5f + 0.5f * std::sin (x * 0.013f + t * 0.5f + (float) k))) * (0.7f + eL);
            const float seg = len * 0.25f;
            for (int j = 0; j < 4; ++j)
            {
                g.setColour (pc ((float) k / 5.f, 1.f, 0.22f * (1.f - (float) j / 4.f) * (0.6f + 0.4f * eL)));
                g.fillRect (x, y + (float) j * seg, 5.f, seg + 1.f);
            }
            if (x == 0.f) edge.startNewSubPath (x, y); else edge.lineTo (x, y);
        }
        g.setColour (pc ((float) k / 5.f, 1.f, 0.55f));
        g.strokePath (edge, juce::PathStrokeType (1.6f));
    }
}

void Visualizer::sceneBlackHole (juce::Graphics& g)
{
    const float ox = cx + (fx - 0.5f) * W * 0.2f, oy = cy + (0.5f - fy) * H * 0.2f, tilt = 0.28f + 0.15f * lb;
    const int rings = 26;
    for (int j = 0; j < rings; ++j)
    {
        const float u = (float) j / (float) (rings - 1), r = R * (0.16f + 0.78f * u);
        const int n = 24 + j * 2;
        const float a0 = (float) accB * (0.8f / (0.2f + u)) + (float) j;
        const float sz = 1.2f + (1.f - u) * 2.f;
        g.setColour (pc (1.f - u, 0.6f + 0.4f * (1.f - u), 0.25f + 0.6f * (1.f - u) * (0.5f + eL)));
        for (int k = 0; k < n; ++k)
        {
            const float a = a0 + (float) k * TAU / (float) n;
            g.fillRect (ox + std::cos (a) * r, oy + std::sin (a) * r * tilt, sz, sz);
        }
    }
    const float hr = R * 0.15f;
    g.setColour (juce::Colours::black);
    g.fillEllipse (ox - hr, oy - hr * tilt * 2.f, hr * 2.f, hr * tilt * 4.f);
    g.setColour (pc (0.f, 1.f, 0.6f + 0.4f * juce::jmin (1.f, eL)));
    g.drawEllipse (ox - hr * 1.05f, oy - hr * 1.05f, hr * 2.1f, hr * 2.1f, 2.f + 4.f * eL);
}

void Visualizer::scenePulsar (juce::Graphics& g)
{
    ripples.erase (std::remove_if (ripples.begin(), ripples.end(), [this] (const Ripple& r) { return t - r.birth > 4.f; }), ripples.end());
    const float ox = cx + (fx - 0.5f) * W * 0.3f, oy = cy + (0.5f - fy) * H * 0.3f;
    for (auto& r : ripples)
    {
        const float age = t - r.birth, rad = age * R * 0.55f * (1.f + 0.5f * fy);
        g.setColour (pc (std::fmod (age * 0.2f, 1.f), 1.f, std::max (0.f, 1.f - age / 3.5f) * (0.3f + 0.7f * r.str)));
        g.drawEllipse (ox - rad, oy - rad, rad * 2.f, rad * 2.f, 1.5f + r.str * 5.f);
    }
    const float ang = (float) accB * 3.f;
    for (int s = -1; s <= 1; s += 2)
    {
        const float ex = ox + std::cos (ang) * R * 1.4f * (float) s, ey = oy + std::sin (ang) * R * 1.4f * (float) s;
        g.setColour (pc (0.f, 1.f, 0.05f)); g.drawLine (ox, oy, ex, ey, 12.f);
        g.setColour (pc (0.f, 1.f, 0.12f)); g.drawLine (ox, oy, ex, ey, 5.f);
        g.setColour (pc (0.3f, 1.f, 0.5f * (0.4f + eL))); g.drawLine (ox, oy, ex, ey, 1.5f);
    }
    const float cr = R * 0.1f * (1.f + eL * 1.5f);
    juce::ColourGradient gr (juce::Colours::white, ox, oy, pc (0.f, 1.f, 0.f), ox + cr * 2.f, oy, true);
    g.setGradientFill (gr);
    g.fillEllipse (ox - cr * 2.f, oy - cr * 2.f, cr * 4.f, cr * 4.f);
}

void Visualizer::sceneConstel (juce::Graphics& g)
{
    const int N = 64;
    std::vector<juce::Point<float>> pts ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const float x = W * (h01 (i * 2) + 0.06f * std::sin (t * 0.3f * (0.4f + h01 (i * 5)) + (float) i * 1.3f)) + (fx - 0.5f) * W * 0.1f;
        const float y = H * (h01 (i * 2 + 1) + 0.06f * std::cos (t * 0.25f * (0.4f + h01 (i * 7)) + (float) i * 0.9f));
        pts[(size_t) i] = { x, y };
    }
    const float thr = R * (0.55f + 0.8f * eL + 0.5f * tr);
    for (int i = 0; i < N; ++i)
        for (int j = i + 1; j < N; ++j)
        {
            const float d = pts[(size_t) i].getDistanceFrom (pts[(size_t) j]);
            if (d < thr)
            {
                g.setColour (pc (h01 (i), 1.f, (1.f - d / thr) * 0.5f));
                g.drawLine (pts[(size_t) i].x, pts[(size_t) i].y, pts[(size_t) j].x, pts[(size_t) j].y, 0.8f);
            }
        }
    for (int i = 0; i < N; ++i)
    {
        const float s = (2.5f + 3.f * h01 (i + 99)) * (1.f + eL);
        g.setColour (pc (h01 (i), 1.f, 0.15f));
        g.fillEllipse (pts[(size_t) i].x - s * 1.5f, pts[(size_t) i].y - s * 1.5f, s * 3.f, s * 3.f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (pts[(size_t) i].x - s * 0.35f, pts[(size_t) i].y - s * 0.35f, s * 0.7f, s * 0.7f);
    }
}

void Visualizer::sceneComets (juce::Graphics& g)
{
    const float ang = 2.4f + (fx - 0.5f) * 0.8f, dx = std::cos (ang), dy = std::sin (ang);
    for (int i = 0; i < 70; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f + 0.2f * std::sin (t + (float) i)));
        g.fillRect (h01 (i * 2 + 300) * W, h01 (i * 2 + 301) * H, 1.4f, 1.4f);
    }
    const float pathLen = (W + H) * 1.1f;
    for (int k = 0; k < 20; ++k)
    {
        const float p = std::fmod ((float) accA * (0.15f + 0.2f * h01 (k * 3)) + h01 (k * 3 + 1), 1.f);
        const float sx = W * (0.2f + h01 (k * 3 + 2)), sy = -H * 0.1f;
        const float hx = sx + dx * p * pathLen, hy = sy + dy * p * pathLen;
        const float tail = (40.f + 120.f * h01 (k)) * (1.f + eL);
        for (int s = 0; s < 14; ++s)
        {
            const float f0 = (float) s / 14.f, f1 = (float) (s + 1) / 14.f;
            g.setColour (pc (h01 (k + 50), 0.5f + 0.5f * (1.f - f0), (1.f - f0) * 0.8f));
            g.drawLine (hx - dx * tail * f0, hy - dy * tail * f0, hx - dx * tail * f1, hy - dy * tail * f1, (1.f - f0) * 3.f);
        }
        g.setColour (juce::Colours::white);
        g.fillEllipse (hx - 2.f, hy - 2.f, 4.f, 4.f);
    }
}

void Visualizer::scenePlanet (juce::Graphics& g)
{
    const float px = cx + (fx - 0.5f) * W * 0.15f, py = cy + (0.5f - fy) * H * 0.1f;
    const float pr = R * 0.38f * (1.f + 0.05f * eL + 0.06f * tr);
    for (int i = 0; i < 120; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (0.3f + 0.3f * std::sin (t * 0.7f + (float) i)));
        g.fillRect (h01 (i * 2 + 500) * W, h01 (i * 2 + 501) * H, 1.4f, 1.4f);
    }
    // atmosphere glow
    juce::ColourGradient atm (pc (0.3f, 1.f, 0.25f * (0.3f + eL)), px, py, pc (0.3f, 1.f, 0.f), px + pr * 1.5f, py, true);
    g.setGradientFill (atm);
    g.fillEllipse (px - pr * 1.5f, py - pr * 1.5f, pr * 3.f, pr * 3.f);

    const float tiltA = -0.35f + 0.25f * lb;
    auto drawRings = [&] (bool frontOnly)
    {
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (tiltA, px, py));
        for (int k = 0; k < 4; ++k)
        {
            const float rx = pr * (1.5f + 0.18f * (float) k), ry = rx * 0.22f;
            juce::Path ring;
            if (frontOnly) ring.addCentredArc (px, py, rx, ry, 0.f, juce::MathConstants<float>::halfPi, juce::MathConstants<float>::pi * 1.5f, true);
            else ring.addEllipse (px - rx, py - ry, rx * 2.f, ry * 2.f);
            g.setColour (pc ((float) k / 4.f, 0.9f, 0.35f + 0.3f * eL));
            g.strokePath (ring, juce::PathStrokeType (2.f + (float) (k % 2) * 2.f));
        }
        g.restoreState();
    };
    drawRings (false);

    // planet body, lit from the left
    juce::ColourGradient body (pc (0.2f, 1.f, 1.f), px - pr * 0.4f, py - pr * 0.4f, pc (0.9f, 0.12f, 1.f), px - pr * 0.4f + pr * 1.8f, py - pr * 0.4f, true);
    g.setGradientFill (body);
    g.fillEllipse (px - pr, py - pr, pr * 2.f, pr * 2.f);
    g.saveState();
    juce::Path clip; clip.addEllipse (px - pr, py - pr, pr * 2.f, pr * 2.f);
    g.reduceClipRegion (clip, juce::AffineTransform());
    for (int b = 0; b < 6; ++b)
    {
        g.setColour (pc (h01 (b + 700), 0.6f, 0.14f));
        g.fillRect (px - pr, py - pr + ((float) b + 0.5f) * pr / 3.f + std::sin (t * 0.2f + (float) b) * 4.f, pr * 2.f, pr * 0.12f);
    }
    g.restoreState();
    drawRings (true);

    for (int m = 0; m < 3; ++m)
    {
        const float a = (float) accB * (0.6f / (1.f + (float) m * 0.5f)) + (float) m * 2.1f;
        const float rx = pr * (1.7f + 0.5f * (float) m), mx = px + std::cos (a) * rx, my = py + std::sin (a) * rx * 0.3f;
        if (std::sin (a) < 0.f && std::fabs (mx - px) < pr) continue; // hidden behind planet
        const float ms = pr * 0.08f * (1.f + 0.3f * (float) m);
        g.setColour (pc (0.6f + 0.1f * (float) m, 0.9f, 1.f));
        g.fillEllipse (mx - ms, my - ms, ms * 2.f, ms * 2.f);
    }
}

void Visualizer::sceneHorizon (juce::Graphics& g)
{
    const float hy = H * 0.58f + (0.5f - fy) * H * 0.1f;
    for (int i = 0; i < 100; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (0.3f + 0.3f * std::sin (t + (float) i)));
        g.fillRect (h01 (i * 2 + 900) * W, h01 (i * 2 + 901) * hy, 1.4f, 1.4f);
    }
    const float sr = R * 0.42f * (1.f + 0.12f * eL), sx = cx, sy = hy - sr * 0.35f;
    juce::ColourGradient sun (pc (0.f, 1.f, 1.f), sx, sy - sr, pc (1.f, 1.f, 1.f), sx, sy + sr, false);
    g.setGradientFill (sun);
    g.fillEllipse (sx - sr, sy - sr, sr * 2.f, sr * 2.f);
    g.saveState();
    juce::Path sunClip; sunClip.addEllipse (sx - sr, sy - sr, sr * 2.f, sr * 2.f);
    g.reduceClipRegion (sunClip, juce::AffineTransform());
    g.setColour (juce::Colours::black);
    for (int b = 0; b < 6; ++b)
        g.fillRect (sx - sr, sy + sr * (0.05f + 0.15f * (float) b), sr * 2.f, 1.f + (float) b * 1.3f);
    g.restoreState();

    juce::Path hills;
    hills.startNewSubPath (0.f, hy);
    for (float x = 0.f; x <= W; x += 6.f)
    {
        const float y = hy - (std::sin (x * 0.011f + 1.3f) + 0.6f * std::sin (x * 0.027f + t * 0.1f) + 0.4f * std::sin (x * 0.05f)) * H * 0.04f * (1.f + eL * 1.2f);
        hills.lineTo (x, y);
    }
    hills.lineTo (W, hy); hills.closeSubPath();
    g.setColour (pc (0.5f, 0.15f, 1.f)); g.fillPath (hills);
    g.setColour (pc (0.5f, 1.f, 0.6f)); g.strokePath (hills, juce::PathStrokeType (1.4f));

    g.setColour (pc (0.5f, 0.1f, 1.f));
    g.fillRect (0.f, hy, W, H - hy);
    for (int j = 0; j < 14; ++j)
    {
        const float u = std::fmod ((float) j / 14.f + (float) accA * 0.2f, 1.f), y = hy + std::pow (u, 2.2f) * (H - hy);
        g.setColour (pc (0.8f, 1.f, 0.15f + 0.6f * u));
        g.drawLine (0.f, y, W, y, 0.6f + 2.f * u);
    }
    for (int i = -12; i <= 12; ++i)
    {
        g.setColour (pc (0.2f, 1.f, 0.45f));
        g.drawLine (cx + (float) i * W * 0.012f, hy, cx + (float) i * W * 0.14f + (fx - 0.5f) * W * 0.4f, H, 1.f);
    }
}
