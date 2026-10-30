#pragma once

#include <cstddef>
#include <vector>

namespace Omnipot {

// Loudness analysis of a mono audio chunk, used to correct whisper's segment start
// times, which often begin in the silence before the words are spoken
class SpeechActivity {
public:
    SpeechActivity(const float *samples, size_t count, int sampleRate);

    // Where sustained sound begins after a pause within [from, to), in seconds from the chunk start.
    // Returns `from` when no clear onset is found (e.g. speech over loud music or nonstop talking).
    double speechStart(double from, double to) const;

private:
    static constexpr double kFrameSeconds = 0.02;

    std::vector<float> m_levels; // RMS per 20 ms frame
    float m_threshold = 0.0f;
};

} // namespace Omnipot
