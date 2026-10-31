#pragma once
#include <QImage>
#include <QStringList>
#include <QVariantMap>
#include <QVector4D>
#include <array>

namespace PhotoCrap::Filters
{
const QStringList &gpuTonePreviewTypes();
const QList<int>  &gpuToneBlendModes();
const QList<int>  &gpuToneHslBlendModes();
const QList<int>  &gpuToneRatioBlendModes();
const QList<int>  &gpuToneContrastBlendModes();
const QList<int>  &gpuToneColorSelectionBlendModes();
const QList<int>  &gpuToneConditionalBlendModes();
bool               gpuToneBlendSupported(const QString &_type, int _blend, bool _fullCoverage);
// The low four bits identify the filter; bit 4 selects the mask texture.
// Higher bits encode the blend mode in the shader uniform.
enum class ToneMappingMode : qint32 {
    Channels               = 0,
    Luminance              = 1,
    Color                  = 2,
    GradientMap            = 3,
    BlackWhite             = 4,
    Matrix                 = 5,
    Threshold              = 6,
    ColorLookup            = 7,
    SelectiveColorRelative = 8,
    SelectiveColorAbsolute = 9,
    Vibrance               = 10,
    HueSaturation          = 11
};
struct ToneMappingSettings {
    ToneMappingMode          mode = ToneMappingMode::Channels;
    QVector4D                color{};
    QVector4D                families0{}, families1{};
    std::array<QVector4D, 3> matrix{};
    std::array<QVector4D, 9> selective{};
    qint32                   blendMode                                     = 0;
    float                    effectOpacity                                 = 1.f;
    float                    maskCoverage                                  = 255.f;
    bool                     operator==(const ToneMappingSettings &) const = default;
};
ToneMappingSettings toneMappingSettings(const QString &_type, const QVariantMap &_parameters);
// CPU reference lookup: RGB channels for Curves/Levels/Balance, luminance for Light,
// palettes for BlackWhite/GradientMap, and an identity table for analytic shaders.
QImage toneMappingTable(const QString &_type, const QVariantMap &_parameters);
} // namespace PhotoCrap::Filters
