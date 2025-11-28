#include "ViewModelRegistry.h"

// ViewModels
#include "models/osdviewmodel.h"
#include "models/menuviewmodel.h"
#include "models/zonedefinitionviewmodel.h"
#include "models/zonemapviewmodel.h"
#include "models/areazoneparameterviewmodel.h"
#include "models/sectorscanparameterviewmodel.h"
#include "models/trpparameterviewmodel.h"
#include "models/zeroingviewmodel.h"
#include "models/windageviewmodel.h"
#include "models/environmentalviewmodel.h"
// #include "models/systemstatusviewmodel.h"  // DISABLED
#include "models/aboutviewmodel.h"

#include <QQmlContext>
#include <QDebug>

ViewModelRegistry::ViewModelRegistry(QObject* parent)
    : QObject(parent)
{
}

ViewModelRegistry::~ViewModelRegistry()
{
    qInfo() << "ViewModelRegistry: Destroyed";
}

bool ViewModelRegistry::createViewModels()
{
    qInfo() << "=== ViewModelRegistry: Creating ViewModels ===";

    try {
        // Core UI ViewModel
        m_osdViewModel = new OsdViewModel(this);

        // Separate MenuViewModels for each menu
        m_mainMenuViewModel = new MenuViewModel(this);
        m_reticleMenuViewModel = new MenuViewModel(this);
        m_colorMenuViewModel = new MenuViewModel(this);

        // Zone Management ViewModels
        m_zoneDefinitionViewModel = new ZoneDefinitionViewModel(this);
        m_zoneMapViewModel = new ZoneMapViewModel(this);
        m_areaZoneParameterViewModel = new AreaZoneParameterViewModel(this);
        m_sectorScanParameterViewModel = new SectorScanParameterViewModel(this);
        m_trpParameterViewModel = new TRPParameterViewModel(this);

        // Ballistics ViewModels
        m_zeroingViewModel = new ZeroingViewModel(this);
        m_windageViewModel = new WindageViewModel(this);
        m_environmentalViewModel = new EnvironmentalViewModel(this);

        // System Info ViewModels
        // m_systemStatusViewModel = new SystemStatusViewModel(this);  // DISABLED
        m_aboutViewModel = new AboutViewModel(this);

        qInfo() << "  ✓ All ViewModels created";
        emit viewModelsCreated();
        return true;

    } catch (const std::exception& e) {
        qCritical() << "Failed to create ViewModels:" << e.what();
        return false;
    }
}

bool ViewModelRegistry::registerWithQml(QQmlContext* context)
{
    if (!context) {
        qCritical() << "ViewModelRegistry: QML context is null!";
        return false;
    }

    qInfo() << "=== ViewModelRegistry: Registering ViewModels with QML ===";

    // Core UI
    context->setContextProperty("osdViewModel", m_osdViewModel);

    // Menus
    context->setContextProperty("mainMenuViewModel", m_mainMenuViewModel);
    context->setContextProperty("reticleMenuViewModel", m_reticleMenuViewModel);
    context->setContextProperty("colorMenuViewModel", m_colorMenuViewModel);

    // Zone Management
    context->setContextProperty("zoneDefinitionViewModel", m_zoneDefinitionViewModel);
    context->setContextProperty("zoneMapViewModel", m_zoneMapViewModel);
    context->setContextProperty("areaZoneParameterViewModel", m_areaZoneParameterViewModel);
    context->setContextProperty("sectorScanParameterViewModel", m_sectorScanParameterViewModel);
    context->setContextProperty("trpParameterViewModel", m_trpParameterViewModel);

    // Ballistics
    context->setContextProperty("zeroingViewModel", m_zeroingViewModel);
    context->setContextProperty("windageViewModel", m_windageViewModel);
    context->setContextProperty("environmentalViewModel", m_environmentalViewModel);

    // System Info
    // context->setContextProperty("systemStatusViewModel", m_systemStatusViewModel);  // DISABLED
    context->setContextProperty("aboutViewModel", m_aboutViewModel);

    qInfo() << "  ✓ All ViewModels registered with QML context";
    emit viewModelsRegistered();
    return true;
}
