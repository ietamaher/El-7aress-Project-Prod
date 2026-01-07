#include "zonedefinitionviewmodel.h"

ZoneDefinitionViewModel::ZoneDefinitionViewModel(QObject* parent) : QObject(parent) {}

void ZoneDefinitionViewModel::setVisible(bool visible) {
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
    }
}

void ZoneDefinitionViewModel::setShowMainMenu(bool show) {
    if (m_showMainMenu != show) {
        m_showMainMenu = show;
        emit showMainMenuChanged();
    }
}

void ZoneDefinitionViewModel::setShowZoneSelectionList(bool show) {
    if (m_showZoneSelectionList != show) {
        m_showZoneSelectionList = show;
        emit showZoneSelectionListChanged();
    }
}

void ZoneDefinitionViewModel::setShowParameterPanel(bool show) {
    if (m_showParameterPanel != show) {
        m_showParameterPanel = show;
        emit showParameterPanelChanged();
    }
}

void ZoneDefinitionViewModel::setShowMap(bool show) {
    if (m_showMap != show) {
        m_showMap = show;
        emit showMapChanged();
    }
}

void ZoneDefinitionViewModel::setShowConfirmDialog(bool show) {
    if (m_showConfirmDialog != show) {
        m_showConfirmDialog = show;
        emit showConfirmDialogChanged();
    }
}

void ZoneDefinitionViewModel::setActivePanelType(int type) {
    if (m_activePanelType != type) {
        m_activePanelType = type;
        emit activePanelTypeChanged();
    }
}

void ZoneDefinitionViewModel::setTitle(const QString& title) {
    if (m_title != title) {
        m_title = title;
        emit titleChanged();
    }
}

void ZoneDefinitionViewModel::setInstruction(const QString& instruction) {
    if (m_instruction != instruction) {
        m_instruction = instruction;
        emit instructionChanged();
    }
}

void ZoneDefinitionViewModel::setMenuOptions(const QStringList& options) {
    if (m_menuOptions != options) {
        m_menuOptions = options;
        emit menuOptionsChanged();
    }
}

void ZoneDefinitionViewModel::setCurrentIndex(int index) {
    if (m_currentIndex != index) {
        m_currentIndex = index;
        emit currentIndexChanged();
    }
}

void ZoneDefinitionViewModel::setGimbalPosition(float az, float el) {
    bool changed = false;
    if (!qFuzzyCompare(m_gimbalAz, az)) {
        m_gimbalAz = az;
        changed = true;
        emit gimbalAzChanged();
    }
    if (!qFuzzyCompare(m_gimbalEl, el)) {
        m_gimbalEl = el;
        changed = true;
        emit gimbalElChanged();
    }
}

void ZoneDefinitionViewModel::setAccentColor(const QColor& color) {
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
    }
}