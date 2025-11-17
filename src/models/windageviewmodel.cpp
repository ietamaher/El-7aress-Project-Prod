#include "windageviewmodel.h"

WindageViewModel::WindageViewModel(QObject *parent)
    : QObject(parent)
    , m_visible(false)
    , m_showWindSpeed(false)
    , m_windSpeed(0.0f)
    , m_showWindDirection(false)
    , m_windDirection(0.0f)
{
}

void WindageViewModel::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
    }
}

void WindageViewModel::setTitle(const QString& title)
{
    if (m_title != title) {
        m_title = title;
        emit titleChanged();
    }
}

void WindageViewModel::setInstruction(const QString& instruction)
{
    if (m_instruction != instruction) {
        m_instruction = instruction;
        emit instructionChanged();
    }
}

void WindageViewModel::setShowWindSpeed(bool show)
{
    if (m_showWindSpeed != show) {
        m_showWindSpeed = show;
        emit showWindSpeedChanged();
    }
}

void WindageViewModel::setWindSpeed(float speed)
{
    if (m_windSpeed != speed) {
        m_windSpeed = speed;

        // Auto-generate label if not explicitly set
        if (m_windSpeedLabel.isEmpty() || !m_windSpeedLabel.contains("APPLIED")) {
            m_windSpeedLabel = QString("Headwind: %1 knots").arg(speed, 0, 'f', 0);
            emit windSpeedLabelChanged();
        }

        emit windSpeedChanged();
    }
}

void WindageViewModel::setWindSpeedLabel(const QString& label)
{
    if (m_windSpeedLabel != label) {
        m_windSpeedLabel = label;
        emit windSpeedLabelChanged();
    }
}

void WindageViewModel::setShowWindDirection(bool show)
{
    if (m_showWindDirection != show) {
        m_showWindDirection = show;
        emit showWindDirectionChanged();
    }
}

void WindageViewModel::setWindDirection(float direction)
{
    if (m_windDirection != direction) {
        m_windDirection = direction;
        emit windDirectionChanged();
    }
}

void WindageViewModel::setWindDirectionLabel(const QString& label)
{
    if (m_windDirectionLabel != label) {
        m_windDirectionLabel = label;
        emit windDirectionLabelChanged();
    }
}

void WindageViewModel::setAccentColor(const QColor& color) {
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
    }
}
