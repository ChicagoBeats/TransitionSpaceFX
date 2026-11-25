#pragma once
// Shared between processor and visuals: names + the live "feature" snapshot the visuals react to.
struct VisFeat
{
    float level = 0, env = 0, x = .5f, y = .5f, trans = 0, lfoA = 0, lfoB = 0, cut = 1, fb = 0, sat = 0, pan = 0, wet = 1;
};

inline const char* const kVisScenes[10] = { "Warp Starfield", "Spiral Galaxy", "Nebula Clouds", "Aurora Curtains", "Black Hole",
                                            "Pulsar Ripples", "Constellations", "Comet Shower", "Ringed Planet", "Cosmic Horizon" };
inline const char* const kVisMoods[5] = { "Deep Space", "Violet Nebula", "Polar Green", "Crimson Dust", "Solar Gold" };
constexpr int kNumVisuals = 50; // 10 scenes x 5 moods; index = scene * 5 + mood
