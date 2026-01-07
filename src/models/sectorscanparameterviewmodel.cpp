#include "sectorscanparameterviewmodel.h"

SectorScanParameterViewModel::SectorScanParameterViewModel(QObject* parent) : QObject(parent) {}

void SectorScanParameterViewModel::setIsEnabled(bool enabled) {
    if (m_isEnabled != enabled) {
        m_isEnabled = enabled;
        emit isEnabledChanged();
    }
}

void SectorScanParameterViewModel::setScanSpeed(int speed) {
    if (m_scanSpeed != speed) {
        m_scanSpeed = speed;
        emit scanSpeedChanged();
    }
}

void SectorScanParameterViewModel::setActiveField(int field) {
    if (m_activeField != field) {
        m_activeField = field;
        emit activeFieldChanged();
    }
}

void SectorScanParameterViewModel::setIsEditingValue(bool editing) {
    if (m_isEditingValue != editing) {
        m_isEditingValue = editing;
        emit isEditingValueChanged();
    }
}
