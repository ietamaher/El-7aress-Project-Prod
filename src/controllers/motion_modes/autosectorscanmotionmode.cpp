#include "autosectorscanmotionmode.h"
#include "controllers/gimbalcontroller.h"
#include "hardware/devices/servodriverdevice.h"
#include <QDebug>
#include <cmath>
#include <algorithm>

// =========================================================
// CONSTANTS
// =========================================================
static const double ARRIVAL_TOLERANCE_DEG     = 2.0;
static const double ELEVATION_TOLERANCE_DEG   = 0.5;
static const double TURN_AROUND_DELAY_SEC     = 0.5;
static const double DEFAULT_ACCEL_DEG_S2       = 15.0;
static const double SMOOTHING_TAU_S            = 0.06;

// =========================================================
// ANGLE HELPERS
// =========================================================
static double norm360(double a) {
    double r = fmod(a, 360.0);
    return (r < 0.0) ? r + 360.0 : r;
}

static double shortestDiff(double target, double current) {
    double diff = norm360(target) - norm360(current);
    if (diff > 180.0) diff -= 360.0;
    if (diff <= -180.0) diff += 360.0;
    return diff;
}

// =========================================================
// CONSTRUCTOR
// =========================================================
AutoSectorScanMotionMode::AutoSectorScanMotionMode(QObject* parent)
    : GimbalMotionModeBase(parent)
{
    const auto& cfg = MotionTuningConfig::instance();
    m_azPid.Kp = cfg.autoScanAz.kp;
    m_azPid.maxIntegral = cfg.autoScanAz.maxIntegral;

    // You can optionally read hardware polarity here:
    // m_azHardwareSign = cfg.hardwareSettings.azSign;
}

// =========================================================
// ENTER MODE
// =========================================================
void AutoSectorScanMotionMode::enterMode(GimbalController* controller)
{
    qDebug() << "[AutoScan] ENTER";

    if (!m_scanZoneSet || !m_activeScanZone.isEnabled) {
        if (controller) controller->setMotionMode(MotionMode::Idle);
        return;
    }

    SystemStateData cur = controller->systemStateModel()->data();

    // --- Choose elevation midpoint (CROWS style) ---
    m_targetEl = 0.5 * (m_activeScanZone.el1 + m_activeScanZone.el2);

    // --- Determine closest az endpoint ---
    double d1 = std::abs(shortestDiff(m_activeScanZone.az1, cur.gimbalAz));
    double d2 = std::abs(shortestDiff(m_activeScanZone.az2, cur.gimbalAz));

    if (d1 < d2) {
        m_movingToPoint2 = false;
        m_targetAz = m_activeScanZone.az1;
    } else {
        m_movingToPoint2 = true;
        m_targetAz = m_activeScanZone.az2;
    }

    // Reset motion memory
    m_previousDesiredAzVel = 0.0;
    m_timeAtTarget = 0.0;

    // Always align elevation first
    m_state = AlignElevation;

    qDebug() << "[AutoScan] Will align elevation to:" << m_targetEl
             << " then scan azimuth to:" << m_targetAz;
}

// =========================================================
// EXIT
// =========================================================
void AutoSectorScanMotionMode::exitMode(GimbalController* controller)
{
    qDebug() << "[AutoScan] EXIT";
    stopServos(controller);
    m_scanZoneSet = false;
}

// =========================================================
// CONFIGURE SCAN ZONE
// =========================================================
void AutoSectorScanMotionMode::setActiveScanZone(const AutoSectorScanZone& scanZone)
{
    m_activeScanZone = scanZone;
    m_activeScanZone.az1 = norm360(scanZone.az1);
    m_activeScanZone.az2 = norm360(scanZone.az2);
    m_scanZoneSet = true;

    qDebug() << "[AutoScan] Zone Set:" 
             << m_activeScanZone.az1 << "to" << m_activeScanZone.az2
             << "EL:" << m_activeScanZone.el1 << m_activeScanZone.el2;
}

// =========================================================
// UPDATE IMPLEMENTATION (Main State Machine)
// =========================================================
// NOTE: Safety checks are handled by base class updateWithSafety()
void AutoSectorScanMotionMode::updateImpl(GimbalController* controller, double dt)
{
    if (!controller || !m_scanZoneSet || !m_activeScanZone.isEnabled)
        return;

    if (dt <= 0.0) dt = 1e-3;

    SystemStateData data = controller->systemStateModel()->data();
    const auto& cfg = MotionTuningConfig::instance();

    // =========================================================
    // STATE 1 — Align Elevation
    // =========================================================
    if (m_state == AlignElevation)
    {
        double elErr = -(m_targetEl - data.gimbalEl);

        if (std::abs(elErr) <= ELEVATION_TOLERANCE_DEG) {
            qDebug() << "[AutoScan] Elevation aligned → switching to azimuth scan";
            m_state = ScanAzimuth;
            m_previousDesiredAzVel = 0.0;
            return;
        }

        // Move ONLY elevation
        double kp_el = 1.0; // you may place in config
        double elVel = kp_el * elErr;

        sendStabilizedServoCommands(controller, 
                                    0.0,           // no azimuth motion
                                    elVel, 
                                    true, dt);
        return;
    }

    // =========================================================
    // STATE 2 — Azimuth scanning
    // =========================================================
    if (m_state == ScanAzimuth)
    {
        double errAz  = shortestDiff(m_targetAz, data.gimbalAz);
        double distAz = std::abs(errAz);

        // Arrival logic
        if (distAz <= ARRIVAL_TOLERANCE_DEG) {

            m_timeAtTarget += dt;
            m_previousDesiredAzVel = 0.0;

            sendStabilizedServoCommands(controller, 0.0, 0.0, true, dt);

            if (m_timeAtTarget >= TURN_AROUND_DELAY_SEC) {
                m_movingToPoint2 = !m_movingToPoint2;
                m_targetAz = m_movingToPoint2 ? m_activeScanZone.az2
                                              : m_activeScanZone.az1;

                m_timeAtTarget = 0.0;
                m_previousDesiredAzVel = 0.0;

                qDebug() << "[AutoScan] Switch direction → New target:" << m_targetAz;
            }
            return;
        }

        m_timeAtTarget = 0.0;

        // Motion profile
        double v_max = std::abs(m_activeScanZone.scanSpeed);
        if (v_max < 0.5) v_max = 5.0;

        double accel = (cfg.motion.scanMaxAccelDegS2 > 0.0)
                     ? cfg.motion.scanMaxAccelDegS2
                     : DEFAULT_ACCEL_DEG_S2;

        double decelDist = (v_max * v_max) / (2.0 * accel);
        double direction = (errAz > 0 ? 1.0 : -1.0);

        double desiredVel;
        if (distAz <= decelDist) {
            double v_dec = std::sqrt(2.0 * accel * distAz);
            desiredVel = direction * std::min(v_max, v_dec);
        } else {
            desiredVel = direction * v_max;
        }

        // Rate-limit
        double maxDelta = accel * dt;
        desiredVel = std::clamp(desiredVel,
                                m_previousDesiredAzVel - maxDelta,
                                m_previousDesiredAzVel + maxDelta);

        // Smoothing (adaptive alpha)
        double alpha = dt / (SMOOTHING_TAU_S + dt);
        double smoothedVel = alpha * desiredVel + (1.0 - alpha) * m_previousDesiredAzVel;
        m_previousDesiredAzVel = smoothedVel;

        // Hardware polarity
        double finalCmd = m_azHardwareSign * smoothedVel;

        // Clamp to global limit
        double globalMax = (cfg.motion.maxVelocityDegS > 0.0)
                         ? cfg.motion.maxVelocityDegS
                         : v_max * 1.5;
        finalCmd = std::clamp(finalCmd, -globalMax, globalMax);

        sendStabilizedServoCommands(controller, finalCmd, 0.0, true, dt);
    }
}
