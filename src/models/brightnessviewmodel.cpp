#include "brightnessviewmodel.h"

BrightnessViewModel::BrightnessViewModel(QObject *parent)
    : QObject(parent)
    , m_visible(false)
    , m_showParameters(false)
    , m_brightness(70) // Default 70%
{
}

void BrightnessViewModel::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
    }
}

void BrightnessViewModel::setTitle(const QString& title)
{
    if (m_title != title) {
        m_title = title;
        emit titleChanged();
    }
}

void BrightnessViewModel::setInstruction(const QString& instruction)
{
    if (m_instruction != instruction) {
        m_instruction = instruction;
        emit instructionChanged();
    }
}

void BrightnessViewModel::setShowParameters(bool show)
{
    if (m_showParameters != show) {
        m_showParameters = show;
        emit showParametersChanged();
    }
}

void BrightnessViewModel::setBrightness(int brightness)
{
    if (m_brightness != brightness) {
        m_brightness = brightness;
        emit brightnessChanged();
    }
}

void BrightnessViewModel::setParameterLabel(const QString& label)
{
    if (m_parameterLabel != label) {
        m_parameterLabel = label;
        emit parameterLabelChanged();
    }
}

void BrightnessViewModel::setAccentColor(const QColor& color)
{
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
    }
}
