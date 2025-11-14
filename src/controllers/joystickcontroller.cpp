#include "joystickcontroller.h"
#include <QDebug>
#include <cmath>
#include <QDateTime> // if   time-based debouncing

JoystickController::JoystickController(JoystickDataModel *joystickModel,
                                       SystemStateModel *stateModel,
                                       GimbalController *gimbalCtrl,
                                       CameraController *cameraCtrl,
                                       WeaponController *weaponCtrl,
                                       QObject *parent)
    : QObject(parent),
    m_joystickModel(joystickModel),
    m_stateModel(stateModel),
    m_gimbalController(gimbalCtrl),
    m_cameraController(cameraCtrl),
    m_weaponController(weaponCtrl),
    m_activeCameraIndex(0)
{
    // Option 1: connect directly to model’s signals
    connect(joystickModel, &JoystickDataModel::axisMoved,
            this, &JoystickController::onAxisChanged);
    connect(joystickModel, &JoystickDataModel::buttonPressed,
            this, &JoystickController::onButtonChanged);
    connect(joystickModel, &JoystickDataModel::hatMoved,
            this, &JoystickController::onHatChanged);

    // Option 2: or if you prefer raw device signals, connect to JoystickDevice instead
}

void JoystickController::onHatChanged(int hat, int value)
{
    if (!m_stateModel) return;

    // Check if we are in the acquisition phase to resize the tracking gate
    if (m_stateModel->data().currentTrackingPhase == TrackingPhase::Acquisition) {
        // We need a method in SystemStateModel to handle resizing
        // Let's assume: adjustAcquisitionBoxSize(float dW, float dH)
        // dW/dH are changes in width/height

        const float sizeStep = 4.0f; // Pixels to change size by per hat press

        if (hat == 0) { // Assuming Hat 0 is your D-pad
            if (value == SDL_HAT_UP) {
                m_stateModel->adjustAcquisitionBoxSize(0, -sizeStep); // Decrease height
            } else if (value == SDL_HAT_DOWN) {
                m_stateModel->adjustAcquisitionBoxSize(0, sizeStep);  // Increase height
            } else if (value == SDL_HAT_LEFT) {
                m_stateModel->adjustAcquisitionBoxSize(-sizeStep, 0); // Decrease width
            } else if (value == SDL_HAT_RIGHT) {
                m_stateModel->adjustAcquisitionBoxSize(sizeStep, 0);  // Increase width
            }
        }
        return; // Consume the hat event so it doesn't do anything else
    }

    // --- Your old gimbal control logic can be a fallback ---
    // This is probably not desired anymore, as gimbal is controlled by the analog stick.
    // If you want to keep it, ensure it doesn't conflict.
    // if (hat == 0) { ... your old debug messages ... }
}


void JoystickController::onAxisChanged(int axis, float value)
{
    // e.g. axis 0 => gimbal az, axis 1 => gimbal el
    if (!m_gimbalController)
        return;

    // Let’s assume we do velocity control or direct position offsets
    if (axis == 0) {
        // X-axis: az
        //float velocityAz = value * 10.0f; //  scale factor
        //qDebug() << "Joystick: Az Axis =>" << velocityAz;
    } else if (axis == 1) {
        // Y-axis: el
        //float velocityEl = -value * 10.0f; // maybe invert sign
        //qDebug() << "Joystick: El Axis =>" << velocityEl;
    }
        float velocityEl = -value * 10.0f; // maybe invert sign
        qDebug() << "Joystick: El Axis =>" << velocityEl;
}

void JoystickController::onButtonChanged(int button, bool pressed)
{
    qDebug() << "Joystick button" << button << " =>" << pressed;

    // For corner‐case: if you want to prevent rapid toggles:
    // static qint64 lastPressMs = 0;
    // qint64 now = QDateTime::currentMSecsSinceEpoch();
    // if (now - lastPressMs < 200) {
    //     // Debounce 200ms
    //     qDebug() << "Ignoring rapid button press (debounce)";
    //     return;
    // }
    // lastPressMs = now;

    SystemStateData curr = m_stateModel->data();

    if (button == 4 && pressed) {
        // to uncomment it in the future
        if (!m_stateModel->data().deadManSwitchActive) {
            qDebug() << "Joystick: TRACK button ignored, Deadman Switch not active.";
            return;
        }
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        bool isDoubleClick = (now - m_lastTrackButtonPressTime) < DOUBLE_CLICK_INTERVAL_MS;
        qDebug() << "Joystick: TRACK button pressed. Double-click detected:" << now - m_lastTrackButtonPressTime << "ms";
        m_lastTrackButtonPressTime = now;

        TrackingPhase currentPhase = curr.currentTrackingPhase;

        if (isDoubleClick) {
            qDebug() << "Joystick: TRACK button double-clicked. Aborting tracking.";
            m_stateModel->stopTracking(); // This should set phase to Off
            return;
        }

        // --- Single Press Logic ---
        switch (currentPhase) {
            case TrackingPhase::Off:
                // First press: Enter Acquisition mode
                qDebug() << "Joystick: TRACK button pressed. Entering Acquisition Phase.";
                m_stateModel->startTrackingAcquisition(); // This sets phase to Acquisition
                break;

            case TrackingPhase::Acquisition:
                // Second press: Request lock-on with the current acquisition gate
                qDebug() << "Joystick: TRACK button pressed. Requesting Tracker Lock-On.";
                m_stateModel->requestTrackerLockOn(); // This sets phase to Tracking_LockPending
                break;

            case TrackingPhase::Tracking_LockPending:
            case TrackingPhase::Tracking_ActiveLock:
            case TrackingPhase::Tracking_Coast:
            case TrackingPhase::Tracking_Firing:
                // A single press while in any active tracking phase might do nothing,
                // or it could be used to cycle targets if your tracker supports it.
                // For now, only a double-click will cancel.
                qDebug() << "Joystick: TRACK button pressed, but already in an active tracking phase. Double-click to cancel.";
                break;
        }
        return; // Consume this button press event
    }

    if (button == 11 || button == 13) {
        if (pressed) {
            if (!curr.stationEnabled) {
                qWarning() << "Cannot cycle modes, station is off.";
                return;
            }
            // *** CRUCIAL: Do not allow cycling into automated scans if tracking acquisition is in progress ***
            if (curr.currentTrackingPhase == TrackingPhase::Acquisition) {
                qDebug() << "Cannot cycle motion modes during Tracking Acquisition.";
                return; // Ignore button press
            }
            // If tracking is active (locked), cycling modes should probably stop it first.
            if (curr.currentTrackingPhase == TrackingPhase::Tracking_ActiveLock) {
                qDebug() << "Cycling motion modes. Stopping active track first.";
                m_stateModel->stopTracking();
                // After stopping, it will default to Manual. The next part will then cycle from Manual.
                // Re-fetch current state after stopping.
                curr = m_stateModel->data();
            }

            // Now, cycle surveillance modes
            if (curr.motionMode == MotionMode::Manual) {
                m_stateModel->setMotionMode(MotionMode::AutoSectorScan);
            } else if (curr.motionMode == MotionMode::AutoSectorScan) {
                m_stateModel->setMotionMode(MotionMode::TRPScan);
            } else if (curr.motionMode == MotionMode::TRPScan) {
                m_stateModel->setMotionMode(MotionMode::RadarSlew);
            } else if (curr.motionMode == MotionMode::RadarSlew) {
                m_stateModel->setMotionMode(MotionMode::Manual);
             }
            else {
                // If in an unexpected mode (like AutoTrack somehow), revert to Manual
                m_stateModel->setMotionMode(MotionMode::Manual);
            }
        }
        return; // Consume the event
    }

    switch (button) {

    case 0:
        if (pressed) {
            if (!curr.stationEnabled) {
                qDebug() << "Cannot toggle, station is off.";
                return;
            }
             // This acts like a momentary switch
             m_stateModel->commandEngagement(pressed);
        }
        break;

    // ========================================================================
    // BUTTON 1: LRF TRIGGER (Laser Range Finder)
    // ========================================================================
    // Trigger single-shot range measurement for ballistics calculation
    // LRF is physically mounted with camera, so CameraController manages it
    // ========================================================================
    case 1:
        if (pressed) {
            if (!curr.stationEnabled) {
                qDebug() << "[JoystickController] Cannot trigger LRF, station is off.";
                return;
            }

            qDebug() << "[JoystickController] Button 1 pressed - Triggering LRF";
            m_cameraController->triggerLRF();
        }
        break;

        // Fire Weapon
    case 5:
        if (!curr.stationEnabled) {
            qDebug() << "Cannot fire, station is off.";
            return; // Exit without firing
        }
        if (pressed) {
            m_weaponController->startFiring();
        } else {
            m_weaponController->stopFiring();
        }
        break;

        // Dead man switch
    case 3:
        m_stateModel->setDeadManSwitch(pressed);
        break;

        // Button 4 => Start Tracking (Manual or Auto)
    /*case 4:
        if (pressed) {
            emit trackSelectButtonPressed();

            qDebug() << "Joystick pressed: starting tracking.";
        }
        break;*/

        // Example up/down logic
    case 14:
        if (pressed) {
            if (curr.opMode == OperationalMode::Idle) {
                m_stateModel->setUpSw(pressed);
            } else if (curr.opMode == OperationalMode::Tracking) {
                m_stateModel->setUpTrack(pressed);
            } else if (curr.opMode == OperationalMode::Surveillance && curr.motionMode == MotionMode::TRPScan) {
                m_stateModel->selectNextTRPLocationPage();
                qDebug() << "Joystick: Next TRP Scan Zone selected via button 14. Now ID:" << m_stateModel->data().activeAutoSectorScanZoneId;
            } else if (curr.opMode == OperationalMode::Surveillance && curr.motionMode == MotionMode::AutoSectorScan) {
                m_stateModel->selectNextAutoSectorScanZone();
                qDebug() << "Joystick: Next Sector Scan Zone selected via button 14. Now ID:" << m_stateModel->data().activeAutoSectorScanZoneId;
            }
        }

        break;
    case 16:
        if (pressed) {
            if (curr.opMode == OperationalMode::Idle) {
                m_stateModel->setDownSw(pressed);
            } else if (curr.opMode == OperationalMode::Tracking) {
                m_stateModel->setDownTrack(pressed);
            }else if (curr.opMode == OperationalMode::Surveillance && curr.motionMode == MotionMode::TRPScan) {
                m_stateModel->selectPreviousTRPLocationPage();
                qDebug() << "Joystick: Previous TRP Scan Zone selected via button 16. Now ID:" << m_stateModel->data().activeAutoSectorScanZoneId;
            } else if (curr.opMode == OperationalMode::Surveillance && curr.motionMode == MotionMode::AutoSectorScan) {
                m_stateModel->selectPreviousAutoSectorScanZone();
                qDebug() << "Joystick: Previous Sector Scan Zone selected via button 16. Now ID:" << m_stateModel->data().activeAutoSectorScanZoneId;
            }
        }
        break;


        // Camera Zoom In / Out
    case 6:
        if (pressed) {
            // In day or night => same method for now
            m_cameraController->zoomIn();
        } else {
            // Only day camera has zoomStop ???
            // If the night camera also has stop, call it
            if (curr.activeCameraIsDay) {
                m_cameraController->zoomStop();
            } else {
                m_cameraController->zoomStop(); // if needed for night
            }
        }
        break;
    case 8:
        if (pressed) {
            m_cameraController->zoomOut();
        } else {
            if (curr.activeCameraIsDay) {
                m_cameraController->zoomStop();
            } else {
                m_cameraController->zoomStop(); // if needed
            }
        }
        break;

        // LUT toggles (thermal only)
    case 7:
        if (pressed && !curr.activeCameraIsDay) {
            videoLUT += 1;
            if (videoLUT > 12) {
                videoLUT = 12;
            }
            m_cameraController->nextVideoLUT();
        }
        break;
    case 9:
        if (pressed && !curr.activeCameraIsDay) {
            videoLUT -= 1;
            if (videoLUT < 0) {
                videoLUT = 0;
            }
            m_cameraController->prevVideoLUT();
        }
        break;

     case 2:
        // Toggle between cameras
        if (pressed) {
            // PDF: "Hold the Palm Switch (2) and press the LEAD button (1)"
            // For simulation, a single toggle might be easier to implement from UI.
            // Let's assume this toggles the LAC master state.
            bool isDeadManActive = m_stateModel->data().deadManSwitchActive;
            bool currentLACState = m_stateModel->data().leadAngleCompensationActive;
            
            if (!isDeadManActive) {
                qDebug() << "Cannot toggle Lead Angle Compensation ";
            } else {
                m_stateModel->setLeadAngleCompensationActive(!currentLACState);

                if (!currentLACState) { // Was off, now turning on
                    //m_stateModel->setLeadAngleCompensationActive(true);
                    // statusBar()->showMessage("Lead Angle Compensation ENABLED.", 2000);
                    // Trigger an initial calculation in WeaponController if it's not periodic
                    if (m_weaponController) m_weaponController->updateFireControlSolution();
                } else { // Was on, now turning off
                    //m_stateModel->setLeadAngleCompensationActive(false);
                    // statusBar()->showMessage("Lead Angle Compensation DISABLED.", 2000);
                    // WeaponController's updateFireControlSolution will see it's off and clear offsets.
                    // if (m_weaponCtrl) m_weaponCtrl->updateFireControlSolution(); // Ensure offsets are cleared
                }
            } 
        }  
        break;

    default:
        qDebug() << "Unhandled button" << button << " =>" << pressed;
        break; 
    }

}

