#pragma once

#include <QList>
#include <QString>
#include <optional>

namespace Omnipot {

struct SubtitleSegment {
    double start = 0.0;
    double end = 0.0;
    QString text;
};

// Remembers which parts of the media have already been transcribed and
// decides which stretch of audio Realtime subtitle should work on next
class SubtitleTimeline {
public:
    struct Window {
        double start = 0.0;
        double end = 0.0;
    };

    void clear();

    // The next stretch to transcribe for the given playback position, or nullopt when
    // everything up to position + lookahead is done. duration <= 0 means unknown.
    std::optional<Window> nextWindow(double position, double duration, double lookahead, double maxLength) const;

    // Marks [start, end) as transcribed and stores the segments found in it
    void addResult(double start, double end, const QList<SubtitleSegment> &segments);

    bool isCovered(double seconds) const;
    const QList<SubtitleSegment>& segments() const { return m_segments; }

    QString toSrt() const;
    static QString formatSrtTime(double seconds);

private:
    struct Range {
        double start = 0.0;
        double end = 0.0;
    };

    QList<Range> m_covered;            // sorted, non-overlapping
    QList<SubtitleSegment> m_segments; // sorted by start time
};

} // namespace Omnipot
