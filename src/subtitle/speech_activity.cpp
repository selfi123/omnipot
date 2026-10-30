#include "subtitle/speech_activity.h"
#include <algorithm>
#include <cmath>

namespace Omnipot {

// Quieter than this (about -46 dBFS) never counts as speech
static constexpr float kMinThreshold = 0.005f;
// Speech must be this many times louder than the chunk's background noise
static constexpr float kNoiseMultiplier = 4.0f;
// Frames in a row that must be loud, so clicks don't count as speech (3 x 20 ms)
static constexpr int kSustainFrames = 3;
// Don't move a start so late that almost nothing of the segment is left
static constexpr double kMinRemainingSeconds = 0.3;
// Quiet frames that must come before an onset (0.24 s), so the tail of the previous sentence isn't taken for this one
static constexpr int kPauseFrames = 12;

SpeechActivity::SpeechActivity(const float *samples, size_t count, int sampleRate) {
    const size_t frameSize = std::max<size_t>(1, static_cast<size_t>(sampleRate * kFrameSeconds));
    m_levels.reserve(count / frameSize);
    for (size_t offset = 0; offset + frameSize <= count; offset += frameSize) {
        double sum = 0.0;
        for (size_t i = 0; i < frameSize; ++i) {
            sum += static_cast<double>(samples[offset + i]) * samples[offset + i];
        }
        m_levels.push_back(static_cast<float>(std::sqrt(sum / frameSize)));
    }
    if (m_levels.empty()) return;

    // The quietest tenth of the chunk approximates its background noise
    std::vector<float> sorted = m_levels;
    const auto tenth = sorted.begin() + static_cast<std::ptrdiff_t>(sorted.size() / 10);
    std::nth_element(sorted.begin(), tenth, sorted.end());
    m_threshold = std::max(kMinThreshold, *tenth * kNoiseMultiplier);
}

double SpeechActivity::speechStart(double from, double to) const {
    const int first = std::max(0, static_cast<int>(std::floor(from / kFrameSeconds)));
    const int last = std::min(static_cast<int>(m_levels.size()), static_cast<int>(std::ceil(to / kFrameSeconds)));

    auto loud = [this](int i) { return m_levels[static_cast<size_t>(i)] > m_threshold; };
    // Quiet frames directly before `index`, looking back before `from` if needed
    auto pauseBefore = [&](int index) {
        int quiet = 0;
        for (int i = index - 1; i >= 0 && quiet < kPauseFrames && !loud(i); --i) ++quiet;
        return index == 0 ? kPauseFrames : quiet;
    };

    int run = 0;
    for (int i = first; i < last; ++i) {
        run = loud(i) ? run + 1 : 0;
        if (run != kSustainFrames) continue;
        const int onset = i - kSustainFrames + 1;
        // Sound that was already going on (e.g. the previous sentence) is not where this segment begins
        if (pauseBefore(onset) < kPauseFrames) continue;
        const double seconds = onset * kFrameSeconds;
        return seconds <= to - kMinRemainingSeconds ? std::max(from, seconds) : from;
    }
    return from;
}

} // namespace Omnipot
