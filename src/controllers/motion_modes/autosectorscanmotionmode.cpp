#include "autosectorscanmotionmode.h"
#include "controllers/gimbalcontroller.h"
#include "hardware/devices/servodriverdevice.h"
#include <QDebug>
#include <cmath>
#include <algorithm>

// =========================================================
// TUNING PARAMETERS
// =========================================================
static const double ARRIVAL_TOLERANCE_DEG = 2.0; // Tolerance to consider "arrived"
static const double TURN_AROUND_DELAY_SEC = 0.5; // Wait 0.5s at edge (fixes short range)

// 🔥 CRITICAL FIX: Set this to TRUE because your log shows Positive Command = Negative Motion
static const bool   INVERT_MOTOR_DIRECTION = true; 

// Helper: Normalize angle to [0, 360)
static double norm360(double a) {
    double r = fmod(a, 360.0);
    return (r < 0.0) ? r + 360.0 : r;
}

// Helper: Get shortest signed difference [-180, 180]
// Positive Result = Target is CW (Right)
static double getShortestDiff(double target, double current) {
    double diff = norm360(target) - norm360(current);
    if (diff > 180.0) diff -= 360.0;
    if (diff <= -180.0) diff += 360.0;
    return diff;
}

AutoSectorScanMotionMode::AutoSectorScanMotionMode(QObject* parent)
    : GimbalMotionModeBase(parent), 
      m_scanZoneSet(false), 
      m_movingToPoint2(true),
      m_timeAtTarget(0.0)
{
    const auto& cfg = MotionTuningConfig::instance();
    m_azPid.Kp = cfg.autoScanAz.kp; 
    m_azPid.maxIntegral = cfg.autoScanAz.maxIntegral;
}

void AutoSectorScanMotionMode::enterMode(GimbalController* controller) {
    qDebug() << "[AutoSectorScan] ENTER";
    if (!m_scanZoneSet || !m_activeScanZone.isEnabled) {
        if (controller) controller->setMotionMode(MotionMode::Idle);
        return;
    }

    // Reset state
    m_previousDesiredAzVel = 0.0;
    m_timeAtTarget = 0.0;
    
    // --- SMART START: Pick closest endpoint ---
    SystemStateData cur = controller ? controller->systemStateModel()->data() : SystemStateData();
    
    // Calculate distance to both points
    double dist1 = std::abs(getShortestDiff(m_activeScanZone.az1, cur.gimbalAz));
    double dist2 = std::abs(getShortestDiff(m_activeScanZone.az2, cur.gimbalAz));

    // Determine initial target
    if (dist1 < dist2) {
        m_movingToPoint2 = false;
        m_targetAz = m_activeScanZone.az1;
    } else {
        m_movingToPoint2 = true;
        m_targetAz = m_activeScanZone.az2;
    }

    qDebug() << "[AutoSectorScan] Start. Heading to:" << m_targetAz;
    
    // Set acceleration for smooth turn-around
    if (controller) {
        if (auto azServo = controller->azimuthServo()) setAcceleration(azServo, 100000); 
    }
}

void AutoSectorScanMotionMode::exitMode(GimbalController* controller) {
    qDebug() << "[AutoSectorScan] EXIT";
    stopServos(controller);
}

void AutoSectorScanMotionMode::setActiveScanZone(const AutoSectorScanZone& scanZone) {
    m_activeScanZone = scanZone;
    m_scanZoneSet = true;
    
    // Normalize inputs immediately
    m_activeScanZone.az1 = norm360(scanZone.az1);
    m_activeScanZone.az2 = norm360(scanZone.az2);

    qDebug() << "[AutoSectorScan] Zone Set:" << m_activeScanZone.az1 << "to" << m_activeScanZone.az2;
}

void AutoSectorScanMotionMode::update(GimbalController* controller, double dt)
{
    if (!controller || !m_scanZoneSet || !m_activeScanZone.isEnabled) return;

    SystemStateData data = controller->systemStateModel()->data();
    
    // 1. Calculate Error (Shortest path)
    double errAz = getShortestDiff(m_targetAz, data.gimbalAz);
    double distAz = std::abs(errAz);

    // 2. Arrival Logic (With Hold Time)
    // This fixes "Short Range" by ensuring we actually STOP at the target before reversing
    if (distAz <= ARRIVAL_TOLERANCE_DEG) {
        m_timeAtTarget += dt;
        
        // Hold position (send 0 velocity)
        sendStabilizedServoCommands(controller, 0.0, 0.0, false, dt);

        if (m_timeAtTarget >= TURN_AROUND_DELAY_SEC) {
            // Switch target after delay
            m_movingToPoint2 = !m_movingToPoint2;
            m_targetAz = m_movingToPoint2 ? m_activeScanZone.az2 : m_activeScanZone.az1;
            m_timeAtTarget = 0.0; 
            qDebug() << "[AutoSectorScan] Reached Endpoint. Switching to:" << m_targetAz;
        }
        return; 
    } else {
        m_timeAtTarget = 0.0;
    }

    // 3. Motion Profile (Trapezoidal Velocity)
    double v_max = std::abs(m_activeScanZone.scanSpeed);
    if (v_max < 0.5) v_max = 5.0; // Safety floor

    double accel = 15.0; // deg/s^2 
    double decelDist = (v_max * v_max) / (2.0 * accel);

    double desiredVel = 0.0;
    // Direction: If error is positive, we go Right (1.0).
    double direction = (errAz > 0.0) ? 1.0 : -1.0;

    if (distAz <= decelDist) {
        double v_decel = std::sqrt(2.0 * accel * distAz);
        desiredVel = direction * std::min(v_max, v_decel);
    } else {
        desiredVel = direction * v_max;
    }

    // 4. Smoothing
    double alpha = 0.15; 
    double smoothedVel = (desiredVel * alpha) + (m_previousDesiredAzVel * (1.0 - alpha));
    m_previousDesiredAzVel = smoothedVel;

    // 5. MOTOR DIRECTION FIX
    // Logic: If I want +Vel, and hardware goes Left, I must send -Vel (which hardware interprets as Right).
    double finalCmd = INVERT_MOTOR_DIRECTION ? -smoothedVel : smoothedVel;

    // 6. Send Command
    // enableStabilization = false (Testing raw scan logic first)
    sendStabilizedServoCommands(controller, finalCmd, 0.0, false, dt);
}