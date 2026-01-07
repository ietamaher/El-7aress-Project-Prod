#include "presethomepositionviewmodel.h"

/**
 * @file PresetHomePositionViewModel.cpp
 * @brief Implementation of PresetHomePositionViewModel
 *
 * @author Claude Code
 * @date December 2025
 */

PresetHomePositionViewModel::PresetHomePositionViewModel(QObject* parent) : QObject(parent) {}

void PresetHomePositionViewModel::setVisible(bool visible) {
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
    }
}

void PresetHomePositionViewModel::setTitle(const QString& title) {
    if (m_title != title) {
        m_title = title;
        emit titleChanged();
    }
}

void PresetHomePositionViewModel::setInstruction(const QString& instruction) {
    if (m_instruction != instruction) {
        m_instruction = instruction;
        emit instructionChanged();
    }
}

void PresetHomePositionViewModel::setStatus(const QString& status) {
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

void PresetHomePositionViewModel::setCurrentAzimuthDeg(double degrees) {
    if (!qFuzzyCompare(m_currentAzimuthDeg, degrees)) {
        m_currentAzimuthDeg = degrees;
        emit currentAzimuthDegChanged();
    }
}

void PresetHomePositionViewModel::setCurrentElevationDeg(double degrees) {
    if (!qFuzzyCompare(m_currentElevationDeg, degrees)) {
        m_currentElevationDeg = degrees;
        emit currentElevationDegChanged();
    }
}

void PresetHomePositionViewModel::setAccentColor(const QColor& color) {
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
    }
}
