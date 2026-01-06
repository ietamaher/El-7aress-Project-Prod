#include "joystickcontroller.h"

#include <QDebug>
#include <QDateTime>
#include <cmath>

// ============================================================================
// Constructor
// ============================================================================

JoystickController::JoystickController(JoystickDataModel *joystickModel,
                                       SystemStateModel *stateModel,
                                       GimbalController *gimbalCtrl,
                                       CameraController *cameraCtrl,
                                       WeaponController *weaponCtrl,
                                       QObject *parent)
    : QObject(parent)
    , m_joystickModel(joystickModel)
    , m_stateModel(stateModel)
    , m_gimbalController(gimbalCtrl)
    , m_cameraController(cameraCtrl)
    , m_weaponController(weaponCtrl)
{
    connect(joystickModel, &JoystickDataModel::axisMoved,
            this, &JoystickController::onAxisChanged);
    connect(joystickModel, &JoystickDataModel::buttonPressed,
            this, &JoystickController::onButtonChanged);
    connect(joystickModel, &JoystickDataModel::hatMoved,
            this, &JoystickController::onHatChanged);
}

// ============================================================================
// Axis Handler
// ============================================================================

void JoystickController::onAxisChanged(int axis, float value)
{
    if (!m_gimbalController)
        return;

    // axis 0 => gimbal azimuth, axis 1 => gimbal elevation
    if (axis == 0) {
        // X-axis: azimuth
        // float velocityAz = value * 10.0f;
    } else if (axis == 1) {
        // Y-axis: elevation
        // float velocityEl = -value * 10.0f; // inverted
    }
}

// ============================================================================
// Hat Switch Handler
// ============================================================================

void JoystickController::onHatChanged(int hat, int value)
{
    if (!m_stateModel)
        return;

    // Resize tracking gate during Acquisition phase
    if (m_stateModel->data().currentTrackingPhase == TrackingPhase::Acquisition) {
        const float sizeStep = 4.0f; // Pixels to change size per hat press

        if (hat == 0) { // Hat 0 = D-pad
            if (value == SDL_HAT_UP) {
                m_stateModel->adjustAcquisitionBoxSize(0, -sizeStep);
            } else if (value == SDL_HAT_DOWN) {
                m_stateModel->adjustAcquisitionBoxSize(0, sizeStep);
            } else if (value == SDL_HAT_LEFT) {
                m_stateModel->adjustAcquisitionBoxSize(-sizeStep, 0);
            } else if (value == SDL_HAT_RIGHT) {
                m_stateModel->adjustAcquisitionBoxSize(sizeStep, 0);
            }
        }
    }
}

// ============================================================================
// Button Handler
// ============================================================================

void JoystickController::onButtonChanged(int button, bool pressed)
{
    qDebug() << "Joystick button" << button << "=>" << pressed;

    SystemStateData curr = m_stateModel->data();

    // ==========================================================================
    // BUTTON 4: TRACK (single press = cycle phase, double press = abort)
    // ==========================================================================
    if (button == 4 && pressed) {
        if (!curr.deadManSwitchActive) {
            qDebug() << "Joystick: TRACK button ignored, Deadman Switch not active.";
            return;
        }

        qint64 now = QDateTime::currentMSecsSinceEpoch();
        bool isDoubleClick = (now - m_lastTrackButtonPressTime) < DOUBLE_CLICK_INTERVAL_MS;
        qDebug() << "Joystick: TRACK button pressed. Double-click detected:"
                 << now - m_lastTrackButtonPressTime << "ms";
        m_lastTrackButtonPressTime = now;

        TrackingPhase currentPhase = curr.currentTrackingPhase;

        // Double press: abort tracking
        if (isDoubleClick) {
            qDebug() << "Joystick: TRACK button double-clicked. Aborting tracking.";
            m_stateModel->stopTracking();
            return;
        }

        // Single press: cycle tracking phase
        switch (currentPhase) {
        case TrackingPhase::Off:
            qDebug() << "Joystick: TRACK button pressed. Entering Acquisition Phase.";
            m_stateModel->startTrackingAcquisition();
            break;

        case TrackingPhase::Acquisition:
            qDebug() << "Joystick: TRACK button pressed. Requesting Tracker Lock-On.";
            m_stateModel->requestTrackerLockOn();
            break;

        case TrackingPhase::Tracking_LockPending:
        case TrackingPhase::Tracking_ActiveLock:
        case TrackingPhase::Tracking_Coast:
        case TrackingPhase::Tracking_Firing:
            qDebug() << "Joystick: TRACK button pressed, already in active tracking phase. "
                        "Double-click to cancel.";
            break;
        }
        return;
    }

    // ==========================================================================
    // BUTTONS 11/13: MOTION MODE CYCLING
    // ==========================================================================
    if (button == 11 || button == 13) {
        if (pressed) {
            if (!curr.stationEnabled) {
                qWarning() << "Cannot cycle modes, station is off.";
                return;
            }

            // BUG FIX: Block motion mode cycling during ANY tracking phase
            // Military requirement: Operator must not accidentally change modes during tracking
            if (curr.currentTrackingPhase != TrackingPhase::Off) {
                qWarning() << "[BUG FIX] Cannot cycle motion modes during tracking (phase:"
                           << static_cast<int>(curr.currentTrackingPhase) << ")";
                qWarning() << "[BUG FIX] Operator must stop tracking first (double-press Track button)";
                return;
            }

            // Cycle surveillance modes (only when NOT tracking)
            if (curr.motionMode == MotionMode::Manual) {
                m_stateModel->setMotionMode(MotionMode::AutoSectorScan);
            } else if (curr.motionMode == MotionMode::AutoSectorScan) {
                m_stateModel->setMotionMode(MotionMode::TRPScan);
            } else if (curr.motionMode == MotionMode::TRPScan) {
                m_stateModel->setMotionMode(MotionMode::RadarSlew);
            } else if (curr.motionMode == MotionMode::RadarSlew) {
                m_stateModel->setMotionMode(MotionMode::Manual);
            } else {
                // Unexpected mode - revert to Manual
                m_stateModel->setMotionMode(MotionMode::Manual);
            }
        }
        return;
    }

    // ==========================================================================
    // Individual Button Handlers
    // ==========================================================================
    switch (button) {

    // ==========================================================================
    // BUTTON 0: ENGAGEMENT MODE (Momentary Switch - CROWS Style)
    // PDF: Press = Enter Engagement mode (stores previous mode)
    //      Release = Return to previous mode (Surveillance/Tracking/etc)
    // ==========================================================================
    /*case 0:
        if (pressed) {
            if (!curr.stationEnabled) {
                qDebug() << "Cannot enter engagement, station is off.";
                return;
            }
            m_stateModel->commandEngagement(true);

        } else {
            // BUG FIX: Exit engagement on button release
            m_stateModel->commandEngagement(false);
        }
        break;*/

    // BUTTON 0 — FIRE INTENT (half-press)
    case 0:
        if (pressed) {
            if (curr.lacArmed && !curr.leadAngleCompensationActive) {
                m_stateModel->engageLAC();     // ✅ engage lead early
            }

            if (curr.currentTrackingPhase == TrackingPhase::Tracking_ActiveLock ||
                curr.currentTrackingPhase == TrackingPhase::Tracking_Coast) {
                m_stateModel->enterDeadReckoning(
                    curr.currentTargetAngularRateAz,
                    curr.currentTargetAngularRateEl
                );
            }
            m_stateModel->commandEngagement(true);

        } else {
            // Release half-press → remove lead, keep LAC armed
        // =========================================================================
        // CROWS/SARP LAC WORKFLOW - DISENGAGE LAC ON TRIGGER RELEASE
        // =========================================================================
        // Per CROWS doctrine: Lead is only applied during firing.
        // When fire trigger is released, disengage LAC (but keep it armed).
        // =========================================================================
        if (curr.lacArmed && curr.leadAngleCompensationActive) {
            qInfo() << "[CROWS] TRIGGER RELEASED - Disengaging LAC (lead removed, LAC still armed)";
            m_stateModel->disengageLAC();
        }

        // =========================================================================
        // CROWS: Exit dead reckoning when firing stops
        // =========================================================================
        // Per CROWS doctrine: Firing terminates tracking completely.
        // After firing, operator must re-acquire target to resume tracking.
        // =========================================================================
        if (curr.deadReckoningActive) {
            m_stateModel->exitDeadReckoning();
        }            
            m_stateModel->commandEngagement(false);
        }
        break;
    // ==========================================================================
    // BUTTON 1: LRF TRIGGER (Laser Range Finder)
    // PDF: Single press = single LRF measurement
    //      Double press (within 1 second) = toggle continuous LRF mode
    //      Continuous modes: 1Hz, 5Hz, or 10Hz (configurable)
    // ==========================================================================
    case 1:
        if (pressed) {
            if (!curr.stationEnabled) {
                qDebug() << "[JoystickController] Cannot trigger LRF, station is off.";
                return;
            }

            qint64 now = QDateTime::currentMSecsSinceEpoch();
            qint64 timeSinceLastPress = now - m_lastLrfButtonPressTime;
            bool isDoubleClick = (timeSinceLastPress < DOUBLE_CLICK_INTERVAL_MS);

            qDebug() << "[LRF] Button 1 pressed. Time since last:" << timeSinceLastPress
                     << "ms | Double-click:" << isDoubleClick
                     << "| Continuous LRF active:" << m_continuousLrfActive;

            m_lastLrfButtonPressTime = now;

            if (isDoubleClick) {
                // Double press: Toggle continuous LRF mode
                m_continuousLrfActive = !m_continuousLrfActive;

                if (m_continuousLrfActive) {
                    qInfo() << "[LRF] CONTINUOUS LRF ENABLED (5Hz mode)";
                    qInfo() << "[LRF] Operator: System will continuously range at 5Hz";
                    qInfo() << "[LRF] Operator: Double-press Button 1 again to disable";
                    m_cameraController->startContinuousLRF();
                } else {
                    qInfo() << "[LRF] CONTINUOUS LRF DISABLED";
                    qInfo() << "[LRF] Operator: Returning to single-shot mode";
                    m_cameraController->stopContinuousLRF();
                }
            } else {
                // Single press: Single LRF measurement (if not in continuous mode)
                if (m_continuousLrfActive) {
                    qDebug() << "[LRF] Single press ignored - Continuous LRF already active";
                    qDebug() << "[LRF] Operator: Double-press Button 1 to disable continuous mode";
                } else {
                    qDebug() << "[LRF] Triggering single LRF measurement";
                    m_cameraController->triggerLRF();
                }
            }
        }
        break;

    // ==========================================================================
    // BUTTON 2: LEAD ANGLE COMPENSATION (CROWS-Style Latching)
    // ==========================================================================
    // Per TM 9-1090-225-10-2:
    // "Hold the Palm Switch (2) and press the LEAD button (1)"
    // "A minimum of 2 seconds must be waited before reuse of lead angle
    //  compensation feature."
    // ==========================================================================
    case 2:
        if (pressed) {
            bool isDeadManActive = curr.deadManSwitchActive;

            if (!isDeadManActive) {
                qDebug() << "[CROWS] LAC toggle ignored - Palm Switch not active";
                return;
            }

            bool currentLACState = curr.lacArmed;

            if (!currentLACState) {
                // === ARMING LAC (CROWS-compliant latching) ===
                // Check 2-second minimum interval
                if (!m_stateModel->canRearmLAC()) {
                    qWarning() << "[CROWS] LAC arm BLOCKED - minimum 2 seconds between LAC toggles";
                    return;
                }

                float azRate_dps = curr.azRpm * 6.0f;
                float elRate_dps = curr.elRpm * 6.0f;

 
                // Latch current tracking rate
                m_stateModel->armLAC(-azRate_dps, -elRate_dps);
 

                // Trigger initial fire control calculation with new LAC state
                if (m_weaponController) {
                    m_weaponController->updateFireControlSolution();
                }
            } else {
                // === DISARMING LAC ===
                // Per CROWS: "Cancel Lead Angle Compensation by pressing the
                //            Palm Switch with the CG in the neutral position."
                bool disarmed = m_stateModel->disarmLAC();
                if (!disarmed) {
                    qWarning() << "[CROWS] LAC disarm failed (too soon after arm)";
                }
            }
        }
        break;

    // ==========================================================================
    // BUTTON 3: DEAD MAN SWITCH
    // ==========================================================================
    case 3:
        m_stateModel->setDeadManSwitch(pressed);
        break;

    // ==========================================================================
    // BUTTON 5: FIRE WEAPON
    // ==========================================================================
    case 5:
        if (!curr.stationEnabled) {
            qDebug() << "Cannot fire, station is off.";
            return;
        }
        if (pressed) {
            m_weaponController->startFiring();
        } else {
            m_weaponController->stopFiring();
        }
        break;

    // ==========================================================================
    // BUTTON 6: CAMERA ZOOM IN
    // ==========================================================================
    case 6:
        if (pressed) {
            m_cameraController->zoomIn();
        } else {
            m_cameraController->zoomStop();
        }
        break;

    // ==========================================================================
    // BUTTON 7: VIDEO LUT NEXT (Thermal camera only)
    // ==========================================================================
    case 7:
        if (pressed && !curr.activeCameraIsDay) {
            m_videoLUT = qMin(m_videoLUT + 1, 12);
            m_cameraController->nextVideoLUT();
        }
        break;

    // ==========================================================================
    // BUTTON 8: CAMERA ZOOM OUT
    // ==========================================================================
    case 8:
        if (pressed) {
            m_cameraController->zoomOut();
        } else {
            m_cameraController->zoomStop();
        }
        break;

    // ==========================================================================
    // BUTTON 9: VIDEO LUT PREVIOUS (Thermal camera only)
    // ==========================================================================
    case 9:
        if (pressed && !curr.activeCameraIsDay) {
            m_videoLUT = qMax(m_videoLUT - 1, 0);
            m_cameraController->prevVideoLUT();
        }
        break;

    // ==========================================================================
    // BUTTON 10: LRF CLEAR
    // NOTE: Does NOT affect zeroing (separate clear function for that)
    // ==========================================================================
    case 10:
        if (pressed) {
            qInfo() << "[Joystick] Button 10 (LRF CLEAR) pressed";
            m_stateModel->clearLRF();
            qInfo() << "[Joystick] LRF cleared";
        }
        break;

    // ==========================================================================
    // BUTTON 12: Exit to manual Mode from any motion mode
    // ==========================================================================
    case 12:
        if (pressed) {
            if (!curr.stationEnabled) {
                qDebug() << "Cannot enter manual mode, station is off.";
                return;
            }

            // BUG FIX: Block motion mode changes during ANY tracking phase
            // Military requirement: Operator must not accidentally change modes during tracking
            if (curr.currentTrackingPhase != TrackingPhase::Off) {
                qWarning() << "[BUG FIX] Cannot change to manual mode during tracking (phase:"
                           << static_cast<int>(curr.currentTrackingPhase) << ")";
                qWarning() << "[BUG FIX] Operator must stop tracking first (double-press Track button)";
                return;
            }

            m_stateModel->setMotionMode(MotionMode::Manual);
        }
        break;

    // ==========================================================================
    // BUTTON 14: UP / NEXT ZONE
    // ==========================================================================
    case 14:
        if (pressed) {
            if (curr.opMode == OperationalMode::Idle) {
                m_stateModel->setUpSw(pressed);
            } else if (curr.opMode == OperationalMode::Tracking) {
                m_stateModel->setUpTrack(pressed);
            } else if (curr.opMode == OperationalMode::Surveillance) {
                if (curr.motionMode == MotionMode::TRPScan) {
                    m_stateModel->selectNextTRPLocationPage();
                    qDebug() << "Joystick: Next TRP Scan Zone selected via button 14. Now ID:"
                             << m_stateModel->data().activeAutoSectorScanZoneId;
                } else if (curr.motionMode == MotionMode::AutoSectorScan) {
                    m_stateModel->selectNextAutoSectorScanZone();
                    qDebug() << "Joystick: Next Sector Scan Zone selected via button 14. Now ID:"
                             << m_stateModel->data().activeAutoSectorScanZoneId;
                }
            }
        }
        break;

    // ==========================================================================
    // BUTTON 16: DOWN / PREVIOUS ZONE
    // ==========================================================================
    case 16:
        if (pressed) {
            if (curr.opMode == OperationalMode::Idle) {
                m_stateModel->setDownSw(pressed);
            } else if (curr.opMode == OperationalMode::Tracking) {
                m_stateModel->setDownTrack(pressed);
            } else if (curr.opMode == OperationalMode::Surveillance) {
                if (curr.motionMode == MotionMode::TRPScan) {
                    m_stateModel->selectPreviousTRPLocationPage();
                    qDebug() << "Joystick: Previous TRP Scan Zone selected via button 16. Now ID:"
                             << m_stateModel->data().activeAutoSectorScanZoneId;
                } else if (curr.motionMode == MotionMode::AutoSectorScan) {
                    m_stateModel->selectPreviousAutoSectorScanZone();
                    qDebug() << "Joystick: Previous Sector Scan Zone selected via button 16. Now ID:"
                             << m_stateModel->data().activeAutoSectorScanZoneId;
                }
            }
        }
        break;

    default:
        qDebug() << "Unhandled button" << button << "=>" << pressed;
        break;
    }
}
