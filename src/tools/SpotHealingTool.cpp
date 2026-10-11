#include "Document.h"

#include <QtMath>
#include <array>
#include <cmath>
#include <limits>

void Document::setSpotHealingSampleAllLayers(bool _value)
{
    endStroke();
    m_spotHealingSampleAllLayers = _value;
    emit settingsChanged();
}

void Document::setSpotHealingMode(int _value)
{
    endStroke();
    m_spotHealingMode = qBound(0, _value, 7);
    emit settingsChanged();
}

void Document::prepareSpotHealingStroke()
{
    // One immutable source per gesture; overlapping dabs never resample repairs.
    m_spotHealingSample   = sampleHealingLayers(m_spotHealingSampleAllLayers ? 2 : 0, false);
    m_spotHealingTip      = makeHealingTip();
    m_spotHealingExcluded = {};
}

void Document::spotHealingDab(const QPointF &_point)
{
    if (m_spotHealingSample.isNull() || m_brushOpacity <= 0 || m_brushFlow <= 0)
        return;
    const QPoint centre(qFloor(_point.x()), qFloor(_point.y()));
    const int    side = m_spotHealingTip.width();
    const QRect  dab(centre - QPoint(side / 2, side / 2), QSize(side, side));
    if (!dab.intersects(m_spotHealingSample.rect()) ||
        (hasSelection() && !m_selectionRegion.intersects(dab)))
        return;
    m_spotHealingExcluded |= dab;

    // Proximity match: bounded local patch search, comparing the healthy rim
    // rather than the defect at the centre. No document-wide scan or mask.
    std::array<QPoint, 32> rim;
    const qreal            angle  = qDegreesToRadians(m_brushAngle);
    const qreal            cosine = std::cos(angle), sine = std::sin(angle);
    for (int i = 0; i < int(rim.size()); ++i) {
        const qreal theta = 2 * M_PI * i / rim.size();
        const qreal x     = std::cos(theta) * m_brushSize * .48;
        const qreal y     = std::sin(theta) * m_brushSize * .48 * m_brushRoundness;
        rim[i]            = QPoint(qRound(x * cosine - y * sine), qRound(x * sine + y * cosine));
    }
    QPoint bestOffset;
    qreal  bestScore = std::numeric_limits<qreal>::max();
    bool   found     = false;
    for (int shell = 1; shell <= 3; ++shell) {
        const qreal distance = side * (1.1 + (shell - 1) * .75);
        for (int direction = 0; direction < 32; ++direction) {
            const qreal  theta = 2 * M_PI * direction / 32;
            const QPoint offset(qRound(distance * std::cos(theta)),
                                qRound(distance * std::sin(theta)));
            const QRect  sourceRect = dab.translated(offset);
            if (!m_spotHealingSample.rect().contains(sourceRect) ||
                m_spotHealingExcluded.intersects(sourceRect))
                continue;

            // Don't copy transparent holes into a repair on an empty retouch
            // layer. Check the patch interior as well as its matching rim.
            bool                 opaque = true;
            std::array<qreal, 3> innerSum{};
            qreal                innerEnergy = 0;
            for (int y = -2; y <= 2 && opaque; ++y)
                for (int x = -2; x <= 2; ++x) {
                    const QPoint probe = centre + offset + QPoint(x * side / 6, y * side / 6);
                    const QRgb   pixel = qUnpremultiply(m_spotHealingSample.pixel(probe));
                    if (qAlpha(pixel) != 255) {
                        opaque = false;
                        break;
                    }
                    innerSum[0] += qRed(pixel);
                    innerSum[1] += qGreen(pixel);
                    innerSum[2] += qBlue(pixel);
                    innerEnergy += qRed(pixel) * qRed(pixel) + qGreen(pixel) * qGreen(pixel) +
                                   qBlue(pixel) * qBlue(pixel);
                }
            if (!opaque)
                continue;

            std::array<qreal, 3> edgeSum{};
            qreal                edgeEnergy = 0;
            qreal                score      = 0;
            int                  count      = 0;
            for (const QPoint &delta : rim) {
                const QPoint reference = centre + delta;
                if (!m_spotHealingSample.rect().contains(reference))
                    continue;
                const QRgb before = qUnpremultiply(m_spotHealingSample.pixel(reference));
                const QRgb source = qUnpremultiply(m_spotHealingSample.pixel(reference + offset));
                if (!qAlpha(before) || qAlpha(source) != 255)
                    continue;
                const int red   = qRed(source) - qRed(before);
                const int green = qGreen(source) - qGreen(before);
                const int blue  = qBlue(source) - qBlue(before);
                score += red * red + green * green + blue * blue;
                edgeSum[0] += qRed(source);
                edgeSum[1] += qGreen(source);
                edgeSum[2] += qBlue(source);
                edgeEnergy += qRed(source) * qRed(source) + qGreen(source) * qGreen(source) +
                              qBlue(source) * qBlue(source);
                ++count;
            }
            if (count < 4)
                continue;
            // Avoid another isolated blemish hiding inside an otherwise good
            // matching patch. Normal texture variation already on its rim is
            // allowed; only excess interior variance is penalized.
            qreal innerVariance = innerEnergy / 25.;
            qreal edgeVariance  = edgeEnergy / count;
            for (int c = 0; c < 3; ++c) {
                innerVariance -= innerSum[c] * innerSum[c] / 625.;
                edgeVariance -= edgeSum[c] * edgeSum[c] / (count * count);
            }
            score = score / count + .5 * qMax(0., innerVariance - edgeVariance) +
                    .0001 * distance * distance;
            if (score < bestScore) {
                bestScore  = score;
                bestOffset = offset;
                found      = true;
            }
        }
    }
    if (found)
        applyHealingDab(_point, m_spotHealingSample, m_spotHealingTip, bestOffset,
                        m_spotHealingMode, 5, false);
}

void Document::finishSpotHealingStroke()
{
    m_spotHealingSample   = {};
    m_spotHealingTip      = {};
    m_spotHealingExcluded = {};
}
