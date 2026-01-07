#ifndef SYSTEMCONTROLLER_H
#define SYSTEMCONTROLLER_H

#include <QObject>

// Forward declarations - Managers
class HardwareManager;
class ViewModelRegistry;
class ControllerRegistry;

// Forward declarations - Models & Services
class SystemStateModel;
class VideoImageProvider;

class QQmlApplicationEngine;

/**
 * @class SystemController
 * @brief Main system controller - coordinates all subsystems using manager pattern.
 *
 * Refactored to use specialized managers instead of managing everything directly.
 * Now follows Single Responsibility Principle.
 *
 * Responsibilities:
 * - Coordinate initialization phases
 * - Manage lifecycle of managers
 * - Provide API server
 * - Manage data logger
 */
class SystemController : public QObject {
    Q_OBJECT

public:
    explicit SystemController(QObject* parent = nullptr);
    ~SystemController();

    // ============================================================================
    // THREE-PHASE INITIALIZATION
    // ============================================================================

    /**
     * @brief Phase 1: Initialize hardware layer
     * Creates and starts all hardware devices, transports, and parsers
     */
    void initializeHardware();

    /**
     * @brief Phase 2: Initialize QML/UI layer
     * Creates ViewModels, Controllers, and registers with QML context
     * @param engine The QML application engine
     */
    void initializeQmlSystem(QQmlApplicationEngine* engine);

    /**
     * @brief Phase 3: Start system
     * Opens all transport connections and starts devices
     */
    void startSystem();

private:
    // Helper methods
    void createManagers();
    void connectVideoToProvider();

    // ============================================================================
    // CORE COMPONENTS
    // ============================================================================

    // Central data model
    SystemStateModel* m_systemStateModel = nullptr;

    // Managers (new architecture)
    HardwareManager* m_hardwareManager = nullptr;
    ViewModelRegistry* m_viewModelRegistry = nullptr;
    ControllerRegistry* m_controllerRegistry = nullptr;

    // Services
    VideoImageProvider* m_videoProvider = nullptr;
};

#endif  // SYSTEMCONTROLLER_H
