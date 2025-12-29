#ifndef VIEWMODELREGISTRY_H
#define VIEWMODELREGISTRY_H

#include <QObject>

// Forward declarations - ViewModels
class OsdViewModel;
class ZoneDefinitionViewModel;
class ZoneMapViewModel;
class AreaZoneParameterViewModel;
class SectorScanParameterViewModel;
class TRPParameterViewModel;
class MenuViewModel;
class ZeroingViewModel;
class WindageViewModel;
class EnvironmentalViewModel;
class BrightnessViewModel;
class HomeCalibrationViewModel;
class PresetHomePositionViewModel;
// class SystemStatusViewModel;  // DISABLED
class AboutViewModel;

class QQmlContext;

/**
 * @class ViewModelRegistry
 * @brief Central registry for creating and managing all ViewModels.
 *
 * This class acts as a factory for ViewModels, providing centralized
 * creation and lifecycle management. It also handles registration
 * of ViewModels with the QML context.
 */
class ViewModelRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ViewModelRegistry(QObject* parent = nullptr);
    ~ViewModelRegistry();

    // ========================================================================
    // VIEWMODEL CREATION
    // ========================================================================

    /**
     * @brief Creates all ViewModels
     * @return true if successful
     */
    bool createViewModels();

    /**
     * @brief Registers all ViewModels with QML context
     * @param context The QML root context
     * @return true if successful
     */
    bool registerWithQml(QQmlContext* context);

    // ========================================================================
    // VIEWMODEL ACCESSORS
    // ========================================================================

    // Core UI ViewModels
    OsdViewModel* osdViewModel() const { return m_osdViewModel; }

    // Menu ViewModels (separate instances for each menu)
    MenuViewModel* mainMenuViewModel() const { return m_mainMenuViewModel; }
    MenuViewModel* reticleMenuViewModel() const { return m_reticleMenuViewModel; }
    MenuViewModel* colorMenuViewModel() const { return m_colorMenuViewModel; }

    // Zone Management ViewModels
    ZoneDefinitionViewModel* zoneDefinitionViewModel() const { return m_zoneDefinitionViewModel; }
    ZoneMapViewModel* zoneMapViewModel() const { return m_zoneMapViewModel; }
    AreaZoneParameterViewModel* areaZoneParameterViewModel() const { return m_areaZoneParameterViewModel; }
    SectorScanParameterViewModel* sectorScanParameterViewModel() const { return m_sectorScanParameterViewModel; }
    TRPParameterViewModel* trpParameterViewModel() const { return m_trpParameterViewModel; }

    // Ballistics ViewModels
    ZeroingViewModel* zeroingViewModel() const { return m_zeroingViewModel; }
    WindageViewModel* windageViewModel() const { return m_windageViewModel; }
    EnvironmentalViewModel* environmentalViewModel() const { return m_environmentalViewModel; }
    BrightnessViewModel* brightnessViewModel() const { return m_brightnessViewModel; }

    // Calibration ViewModels
    HomeCalibrationViewModel* homeCalibrationViewModel() const { return m_homeCalibrationViewModel; }
    PresetHomePositionViewModel* presetHomePositionViewModel() const { return m_presetHomePositionViewModel; }

    // System Info ViewModels
    // SystemStatusViewModel* systemStatusViewModel() const { return m_systemStatusViewModel; }  // DISABLED
    AboutViewModel* aboutViewModel() const { return m_aboutViewModel; }

signals:
    void viewModelsCreated();
    void viewModelsRegistered();

private:
    // ========================================================================
    // VIEWMODELS
    // ========================================================================

    // Core UI
    OsdViewModel* m_osdViewModel = nullptr;

    // Separate MenuViewModels for each menu
    MenuViewModel* m_mainMenuViewModel = nullptr;
    MenuViewModel* m_reticleMenuViewModel = nullptr;
    MenuViewModel* m_colorMenuViewModel = nullptr;

    // Zone Management
    ZoneDefinitionViewModel* m_zoneDefinitionViewModel = nullptr;
    ZoneMapViewModel* m_zoneMapViewModel = nullptr;
    AreaZoneParameterViewModel* m_areaZoneParameterViewModel = nullptr;
    SectorScanParameterViewModel* m_sectorScanParameterViewModel = nullptr;
    TRPParameterViewModel* m_trpParameterViewModel = nullptr;

    // Ballistics
    ZeroingViewModel* m_zeroingViewModel = nullptr;
    WindageViewModel* m_windageViewModel = nullptr;
    EnvironmentalViewModel* m_environmentalViewModel = nullptr;
    BrightnessViewModel* m_brightnessViewModel = nullptr;

    // Calibration
    HomeCalibrationViewModel* m_homeCalibrationViewModel = nullptr;
    PresetHomePositionViewModel* m_presetHomePositionViewModel = nullptr;

    // System Info
    // SystemStatusViewModel* m_systemStatusViewModel = nullptr;  // DISABLED
    AboutViewModel* m_aboutViewModel = nullptr;
};

#endif // VIEWMODELREGISTRY_H
