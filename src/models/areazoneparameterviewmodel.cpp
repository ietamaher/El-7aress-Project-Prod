#include "areazoneparameterviewmodel.h"

AreaZoneParameterViewModel::AreaZoneParameterViewModel(QObject* parent) : QObject(parent) {}

void AreaZoneParameterViewModel::setIsEnabled(bool enabled) {
    if (m_isEnabled != enabled) {
        m_isEnabled = enabled;
        emit isEnabledChanged();
    }
}

void AreaZoneParameterViewModel::setIsOverridable(bool overridable) {
    if (m_isOverridable != overridable) {
        m_isOverridable = overridable;
        emit isOverridableChanged();
    }
}

void AreaZoneParameterViewModel::setActiveField(int field) {
    if (m_activeField != field) {
        m_activeField = field;
        emit activeFieldChanged();
    }
}
