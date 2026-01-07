#include "zeroingviewmodel.h"

ZeroingViewModel::ZeroingViewModel(QObject* parent)
    : QObject(parent), m_visible(false), m_showOffsets(false), m_azimuthOffset(0.0f),
      m_elevationOffset(0.0f) {}

void ZeroingViewModel::setVisible(bool visible) {
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
    }
}

void ZeroingViewModel::setTitle(const QString& title) {
    if (m_title != title) {
        m_title = title;
        emit titleChanged();
    }
}

void ZeroingViewModel::setInstruction(const QString& instruction) {
    if (m_instruction != instruction) {
        m_instruction = instruction;
        emit instructionChanged();
    }
}

void ZeroingViewModel::setStatus(const QString& status) {
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

void ZeroingViewModel::setShowOffsets(bool show) {
    if (m_showOffsets != show) {
        m_showOffsets = show;
        emit showOffsetsChanged();
    }
}

void ZeroingViewModel::setAzimuthOffset(float offset) {
    if (m_azimuthOffset != offset) {
        m_azimuthOffset = offset;
        emit azimuthOffsetChanged();
    }
}

void ZeroingViewModel::setElevationOffset(float offset) {
    if (m_elevationOffset != offset) {
        m_elevationOffset = offset;
        emit elevationOffsetChanged();
    }
}

void ZeroingViewModel::setAccentColor(const QColor& color) {
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
    }
}
