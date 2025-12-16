#include "homecalibrationviewmodel.h"

/**
 * @file HomeCalibrationViewModel.cpp
 * @brief Implementation of HomeCalibrationViewModel
 *
 * @author Claude Code
 * @date December 2025
 */

HomeCalibrationViewModel::HomeCalibrationViewModel(QObject *parent)
    : QObject(parent)
{
}

void HomeCalibrationViewModel::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
    }
}

void HomeCalibrationViewModel::setTitle(const QString& title)
{
    if (m_title != title) {
        m_title = title;
        emit titleChanged();
    }
}

void HomeCalibrationViewModel::setInstruction(const QString& instruction)
{
    if (m_instruction != instruction) {
        m_instruction = instruction;
        emit instructionChanged();
    }
}

void HomeCalibrationViewModel::setStatus(const QString& status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

void HomeCalibrationViewModel::setCurrentEncoderSteps(double steps)
{
    if (!qFuzzyCompare(m_currentEncoderSteps, steps)) {
        m_currentEncoderSteps = steps;
        emit currentEncoderStepsChanged();
    }
}

void HomeCalibrationViewModel::setCurrentOffsetSteps(double steps)
{
    if (!qFuzzyCompare(m_currentOffsetSteps, steps)) {
        m_currentOffsetSteps = steps;
        emit currentOffsetStepsChanged();
    }
}

void HomeCalibrationViewModel::setCurrentAzimuthDeg(double degrees)
{
    if (!qFuzzyCompare(m_currentAzimuthDeg, degrees)) {
        m_currentAzimuthDeg = degrees;
        emit currentAzimuthDegChanged();
    }
}

void HomeCalibrationViewModel::setOffsetApplied(bool applied)
{
    if (m_offsetApplied != applied) {
        m_offsetApplied = applied;
        emit offsetAppliedChanged();
    }
}

void HomeCalibrationViewModel::setAccentColor(const QColor& color)
{
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
    }
}
