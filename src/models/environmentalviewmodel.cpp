#include "environmentalviewmodel.h"

EnvironmentalViewModel::EnvironmentalViewModel(QObject *parent)
    : QObject(parent)
    , m_visible(false)
    , m_showParameters(false)
    , m_temperature(15.0f)
    , m_altitude(0.0f)
    , m_crosswind(0.0f)
{
}

void EnvironmentalViewModel::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
    }
}

void EnvironmentalViewModel::setTitle(const QString& title)
{
    if (m_title != title) {
        m_title = title;
        emit titleChanged();
    }
}

void EnvironmentalViewModel::setInstruction(const QString& instruction)
{
    if (m_instruction != instruction) {
        m_instruction = instruction;
        emit instructionChanged();
    }
}

void EnvironmentalViewModel::setShowParameters(bool show)
{
    if (m_showParameters != show) {
        m_showParameters = show;
        emit showParametersChanged();
    }
}

void EnvironmentalViewModel::setTemperature(float temp)
{
    if (m_temperature != temp) {
        m_temperature = temp;
        emit temperatureChanged();
    }
}

void EnvironmentalViewModel::setAltitude(float alt)
{
    if (m_altitude != alt) {
        m_altitude = alt;
        emit altitudeChanged();
    }
}

void EnvironmentalViewModel::setCrosswind(float wind)
{
    if (m_crosswind != wind) {
        m_crosswind = wind;
        emit crosswindChanged();
    }
}

void EnvironmentalViewModel::setParameterLabel(const QString& label)
{
    if (m_parameterLabel != label) {
        m_parameterLabel = label;
        emit parameterLabelChanged();
    }
}

void EnvironmentalViewModel::setAccentColor(const QColor& color)
{
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
    }
}
