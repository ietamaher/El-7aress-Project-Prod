#ifndef COLORUTILS_H
#define COLORUTILS_H

#include <QColor>
#include <QString>

// Enums (assuming these are already defined elsewhere, e.g., in SystemStateData.h)
enum class ColorStyle { Green, Red, White, COUNT };

namespace ColorUtils {
QColor toQColor(ColorStyle style);
ColorStyle fromQColor(const QColor& color);
QString toString(ColorStyle style);
ColorStyle fromString(const QString& str);
}  // namespace ColorUtils

#endif  // COLORUTILS_H
