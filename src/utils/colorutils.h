#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <QColor>
#include <QString>

// ============================================================================
// ENUMS
// ============================================================================
// Enums (assuming these are already defined elsewhere, e.g., in SystemStateData.h)
enum class ColorStyle { Green, Red, White, COUNT };

// ============================================================================
// COLORUTILS NAMESPACE
// ============================================================================
namespace ColorUtils {
QColor toQColor(ColorStyle style);
ColorStyle fromQColor(const QColor& color);
QString toString(ColorStyle style);
ColorStyle fromString(const QString& str);
}  // namespace ColorUtils
