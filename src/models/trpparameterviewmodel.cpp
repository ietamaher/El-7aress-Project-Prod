#include "trpparameterviewmodel.h"

TRPParameterViewModel::TRPParameterViewModel(QObject* parent) : QObject(parent) {}

void TRPParameterViewModel::setLocationPage(int page) {
    if (m_locationPage != page) {
        m_locationPage = page;
        emit locationPageChanged();
    }
}

void TRPParameterViewModel::setTrpInPage(int trp) {
    if (m_trpInPage != trp) {
        m_trpInPage = trp;
        emit trpInPageChanged();
    }
}

void TRPParameterViewModel::setHaltTime(float time) {
    if (!qFuzzyCompare(m_haltTime, time)) {
        m_haltTime = time;
        emit haltTimeChanged();
    }
}

void TRPParameterViewModel::setActiveField(int field) {
    if (m_activeField != field) {
        m_activeField = field;
        emit activeFieldChanged();
    }
}

void TRPParameterViewModel::setIsEditingValue(bool editing) {
    if (m_isEditingValue != editing) {
        m_isEditingValue = editing;
        emit isEditingValueChanged();
    }
}
