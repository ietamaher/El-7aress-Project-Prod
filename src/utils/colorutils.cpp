#include "colorutils.h"

namespace ColorUtils {
QColor toQColor(ColorStyle style) {
    switch (style) {
    case ColorStyle::Green:
        return QColor(70, 226, 165);
    case ColorStyle::Red:
        return QColor("#910000");
    case ColorStyle::White:
        return Qt::white;
    case ColorStyle::COUNT:
        return QColor();  // Should not happen
    }
    return QColor();  // Default fallback
}

ColorStyle fromQColor(const QColor& color) {
    if (color == QColor(70, 226, 165))
        return ColorStyle::Green;
    if (color == QColor("#910000"))
        return ColorStyle::Red;
    if (color == Qt::white)
        return ColorStyle::White;
    return ColorStyle::Green;  // Default fallback
}

QString toString(ColorStyle style) {
    switch (style) {
    case ColorStyle::Green:
        return "Green";
    case ColorStyle::Red:
        return "Red";
    case ColorStyle::White:
        return "White";
    case ColorStyle::COUNT:
        return "Unknown";  // Should not happen
    }
    return "Unknown";  // Default fallback
}

ColorStyle fromString(const QString& str) {
    if (str == "Green")
        return ColorStyle::Green;
    if (str == "Red")
        return ColorStyle::Red;
    if (str == "White")
        return ColorStyle::White;
    return ColorStyle::Green;  // Default fallback
}
}  // namespace ColorUtils
