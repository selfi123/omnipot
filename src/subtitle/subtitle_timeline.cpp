#include "subtitle/subtitle_timeline.h"
#include <algorithm>
#include <cmath>

namespace Omnipot {

// Ranges closer than this are merged, so tiny gaps never turn into tiny windows
static constexpr double kMergeGap = 0.5;

void SubtitleTimeline::clear() {
    m_covered.clear();
    m_segments.clear();
}

std::optional<SubtitleTimeline::Window> SubtitleTimeline::nextWindow(double position, double duration, double lookahead, double maxLength) const {
    position = std::max(0.0, position);

    // Continue from the end of the transcribed range the playhead is in (or about to enter)
    double next = position;
    for (const Range &r : m_covered) {
        if (r.start <= next + kMergeGap && next < r.end) {
            next = r.end;
        }
    }

    if (duration > 0.0 && next >= duration - 0.05) return std::nullopt;
    if (next > position + lookahead) return std::nullopt;

    double end = next + maxLength;
    if (duration > 0.0) end = std::min(end, duration);
    // Stop where an already transcribed range begins (after a backward seek)
    for (const Range &r : m_covered) {
        if (r.start > next && r.start < end) {
            end = r.start;
            break;
        }
    }
    return Window{next, end};
}

void SubtitleTimeline::addResult(double start, double end, const QList<SubtitleSegment> &segments) {
    if (end > start) {
        m_covered.append(Range{start, end});
        std::sort(m_covered.begin(), m_covered.end(), [](const Range &a, const Range &b) { return a.start < b.start; });

        QList<Range> merged;
        for (const Range &r : std::as_const(m_covered)) {
            if (!merged.isEmpty() && r.start <= merged.last().end + kMergeGap) {
                merged.last().end = std::max(merged.last().end, r.end);
            } else {
                merged.append(r);
            }
        }
        m_covered = merged;
    }

    for (const SubtitleSegment &seg : segments) {
        if (seg.text.trimmed().isEmpty() || seg.end <= seg.start) continue;
        m_segments.append(seg);
    }
    std::stable_sort(m_segments.begin(), m_segments.end(), [](const SubtitleSegment &a, const SubtitleSegment &b) { return a.start < b.start; });
}

bool SubtitleTimeline::isCovered(double seconds) const {
    return std::any_of(m_covered.cbegin(), m_covered.cend(), [seconds](const Range &r) {
        return r.start <= seconds && seconds < r.end;
    });
}

QString SubtitleTimeline::formatSrtTime(double seconds) {
    const qint64 totalMs = std::llround(std::max(0.0, seconds) * 1000.0);
    const qint64 ms = totalMs % 1000;
    const qint64 s = (totalMs / 1000) % 60;
    const qint64 m = (totalMs / 60000) % 60;
    const qint64 h = totalMs / 3600000;
    return QStringLiteral("%1:%2:%3,%4")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'))
        .arg(ms, 3, 10, QLatin1Char('0'));
}

QString SubtitleTimeline::toSrt() const {
    QString out;
    for (qsizetype i = 0; i < m_segments.size(); ++i) {
        const SubtitleSegment &seg = m_segments.at(i);
        // Whisper's end times can run into the next line; overlapping lines would be shown stacked
        double end = seg.end;
        if (i + 1 < m_segments.size()) end = std::min(end, m_segments.at(i + 1).start);
        if (end <= seg.start) end = seg.end;
        out += QString::number(i + 1) + QLatin1Char('\n');
        out += formatSrtTime(seg.start) + QStringLiteral(" --> ") + formatSrtTime(end) + QLatin1Char('\n');
        out += seg.text.trimmed() + QStringLiteral("\n\n");
    }
    return out;
}

} // namespace Omnipot
