#include "trpscanmotionmode.h"
#include "controllers/gimbalcontroller.h" // For GimbalController and SystemStateData
#include <QDebug>
#include <QDateTime>  // For throttling model updates
#include <cmath> // For std::sqrt, std::hypot

/*TRPScanMotionMode::TRPScanMotionMode()
    : m_currentState(State::Idle)
    , m_currentTrpIndex(0)
{
    // Configure PID gains for responsive and smooth stopping at waypoints.
    // These might need tuning based on your gimbal\"s physical characteristics.
    // ✅ Load PID gains from runtime config (field-tunable without rebuild)
    const auto& cfg = MotionTuningConfig::instance();
    m_azPid.Kp = cfg.trpScanAz.kp;
    m_azPid.Ki = cfg.trpScanAz.ki;
    m_azPid.Kd = cfg.trpScanAz.kd;
    m_azPid.maxIntegral = cfg.trpScanAz.maxIntegral;

    m_elPid.Kp = cfg.trpScanEl.kp;
    m_elPid.Ki = cfg.trpScanEl.ki;
    m_elPid.Kd = cfg.trpScanEl.kd;
    m_elPid.maxIntegral = cfg.trpScanEl.maxIntegral;
}
 */

static constexpr double AZ_TOL = 1.0;
static constexpr double EL_TOL = 0.5;
static constexpr double DEFAULT_ACCEL = 20.0;
static constexpr double SMOOTH_TAU = 0.05;

double TRPScanMotionMode::norm360(double a) {
    double r = fmod(a, 360.0);
    return (r < 0) ? r + 360.0 : r;
}

double TRPScanMotionMode::shortestDiff(double t, double c) {
    double d = norm360(t) - norm360(c);
    if (d > 180) d -= 360;
    if (d <= -180) d += 360;
    return d;
}

TRPScanMotionMode::TRPScanMotionMode(QObject* parent)
    : GimbalMotionModeBase(parent)
{}

void TRPScanMotionMode::setTrpList(const QVector<TargetReferencePoint>& trps) {
    m_trps = trps;
}

void TRPScanMotionMode::setTrpList(const std::vector<TargetReferencePoint>& trps)
{
    QVector<TargetReferencePoint> qtrps;
    qtrps.reserve(static_cast<int>(trps.size()));
    for (const auto &t : trps) {
        qtrps.push_back(t);
    }
    setTrpList(qtrps);
} 

bool TRPScanMotionMode::selectPage(int locationPage) {
    m_pageOrder.clear();

    for (int i = 0; i < m_trps.size(); ++i) {
        if (m_trps[i].locationPage == locationPage)
            m_pageOrder.push_back(i);
    }

    std::sort(m_pageOrder.begin(), m_pageOrder.end(),
              [&](int a, int b){ return m_trps[a].trpInPage < m_trps[b].trpInPage; });

    if (m_pageOrder.isEmpty())
        return false;

    m_currentIndex = 0;
    return true;
}

void TRPScanMotionMode::enterMode(GimbalController* controller) {
    // If no TRPs at all -> abort
    if (m_trps.isEmpty()) {
        qWarning() << "[TRP] No TRPs selected!";
        if (controller) controller->setMotionMode(MotionMode::Idle);
        return;
    }

    // If pageOrder is empty, assume m_trps itself already contains only
    // the TRPs for the active page (controller-side filtering).
    // Populate pageOrder with sequential indices 0..N-1 so the rest of
    // the code (which relies on m_pageOrder) works unchanged.
    if (m_pageOrder.isEmpty()) {
        m_pageOrder.clear();
        for (int i = 0; i < m_trps.size(); ++i) m_pageOrder.push_back(i);
        qDebug() << "[TRP] pageOrder was empty; populated from m_trps with"
                 << m_pageOrder.size() << "entries.";
    }

    // If still empty (shouldn't happen), abort safely
    if (m_pageOrder.isEmpty()) {
        qWarning() << "[TRP] pageOrder empty after populate - aborting";
        if (controller) controller->setMotionMode(MotionMode::Idle);
        return;
    }

    m_prevAzVel = 0.0;
    m_state = SlewToPoint;

    // Ensure current index is valid
    if (m_currentIndex < 0 || m_currentIndex >= m_pageOrder.size()) m_currentIndex = 0;

    startCurrentTarget();

    qDebug() << "[TRP] ENTER – cycling" << m_pageOrder.size() << "points";
}


void TRPScanMotionMode::exitMode(GimbalController* controller) {
    stopServos(controller);
}

void TRPScanMotionMode::startCurrentTarget() {
    if (m_pageOrder.isEmpty()) {
        qWarning() << "[TRP] startCurrentTarget called but pageOrder empty";
        return;
    }
    if (m_currentIndex < 0 || m_currentIndex >= m_pageOrder.size()) {
        qWarning() << "[TRP] startCurrentTarget: currentIndex out of range, resetting to 0";
        m_currentIndex = 0;
    }

    int trpIndex = m_pageOrder[m_currentIndex];
    if (trpIndex < 0 || trpIndex >= m_trps.size()) {
        qWarning() << "[TRP] startCurrentTarget: trpIndex out-of-range:" << trpIndex;
        return;
    }

    const auto &T = m_trps[trpIndex];
    m_targetAz = norm360(T.azimuth);
    m_targetEl = T.elevation;
    m_holdRemaining = T.haltTime;

    qDebug() << "[TRP] Go to point:" << T.id
             << " Az=" << m_targetAz
             << " El=" << m_targetEl
             << " Hold=" << m_holdRemaining;
}


void TRPScanMotionMode::advanceToNextPoint() {
    if (m_pageOrder.isEmpty()) {
        qWarning() << "[TRP] advanceToNextPoint called but pageOrder empty";
        return;
    }
    m_currentIndex = (m_currentIndex + 1) % m_pageOrder.size();
    startCurrentTarget();
    m_state = SlewToPoint;
    m_prevAzVel = 0.0;
}


void TRPScanMotionMode::update(GimbalController* controller, double dt) {
    // If controller missing or no TRPs/page -> do nothing safely
    if (!controller) return;
    if (m_pageOrder.isEmpty() || m_trps.isEmpty()) {
        // nothing to do, keep servos stable
        sendStabilizedServoCommands(controller, 0.0, 0.0, true, dt > 0 ? dt : 1e-3);
        return;
    }

    if (dt <= 0) dt = 1e-3;

    auto data = controller->systemStateModel()->data();
    const auto &T = m_trps[m_pageOrder[m_currentIndex]];

    //------------------------------------------------------------
    // 1) HOLD PHASE
    //------------------------------------------------------------
    if (m_state == HoldPoint) {

        sendStabilizedServoCommands(controller, 0, 0, true, dt);

        m_holdRemaining -= dt;

        if (m_holdRemaining <= 0) {
            advanceToNextPoint();
        }
        return;
    }

    //------------------------------------------------------------
    // 2) SLEW-TO-POINT PHASE
    //------------------------------------------------------------
    double errAz = shortestDiff(m_targetAz, data.gimbalAz);
    double errEl = - (m_targetEl - data.gimbalEl);

    if (std::abs(errAz) <= AZ_TOL && std::abs(errEl) <= EL_TOL) {
        m_state = HoldPoint;
        m_prevAzVel = 0.0;
        sendStabilizedServoCommands(controller, 0, 0, true, dt);
        return;
    }

    //------------------------------------------------------------
    // Smooth + accel limited az motion
    //------------------------------------------------------------
    const double accel = DEFAULT_ACCEL;
    const double v_max = 20.0;

    double decelDist = (v_max * v_max) / (2 * accel);
    double distAz = std::abs(errAz);
    double direction = (errAz > 0 ? 1.0 : -1.0);

    double desiredAz;

    if (distAz <= decelDist) {
        double v_req = std::sqrt(2 * accel * distAz);
        desiredAz = direction * std::min(v_max, v_req);
    } else {
        desiredAz = direction * v_max;
    }

    // Rate-limit
    double maxDelta = accel * dt;
    desiredAz = std::clamp(desiredAz, m_prevAzVel - maxDelta, m_prevAzVel + maxDelta);

    // Smooth
    double alpha = dt / (SMOOTH_TAU + dt);
    double smoothedAz = alpha * desiredAz + (1 - alpha) * m_prevAzVel;
    m_prevAzVel = smoothedAz;

    //------------------------------------------------------------
    // Elevation — simple proportional control
    //------------------------------------------------------------
    double kpEl = 2.2;
    double elVel = std::clamp(kpEl * errEl, -15.0, 15.0);

    //------------------------------------------------------------
    // Send with stabilization enabled
    //------------------------------------------------------------
    sendStabilizedServoCommands(controller, smoothedAz, elVel, true, dt);
}
