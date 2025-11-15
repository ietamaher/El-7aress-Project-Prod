#include "ballisticsprocessorlut.h"
#include <QDebug>
#include <cmath>

BallisticsProcessorLUT::BallisticsProcessorLUT()
{
}

BallisticsProcessorLUT::~BallisticsProcessorLUT()
{
}

bool BallisticsProcessorLUT::loadAmmunitionTable(const QString& filepath)
{
    bool success = m_lut.loadTable(filepath);

    if (success) {
        AmmunitionMetadata meta = m_lut.getAmmunitionMetadata();
        qInfo() << "[BallisticsProcessorLUT] Loaded ammunition:" << meta.name
                << "| MV:" << meta.muzzle_velocity_ms << "m/s"
                << "| BC:" << meta.bc_g1
                << "| Range:" << m_lut.getMinRange() << "-" << m_lut.getMaxRange() << "m"
                << "| Table size:" << m_lut.getTableSize() << "entries";
    } else {
        qCritical() << "[BallisticsProcessorLUT] Failed to load table:" << filepath;
    }

    return success;
}

void BallisticsProcessorLUT::setEnvironmentalConditions(float temp_celsius,
                                                         float altitude_m,
                                                         float crosswind_ms)
{
    m_temperature_celsius = temp_celsius;
    m_altitude_m = altitude_m;
    m_crosswind_ms = crosswind_ms;

    qDebug() << "[BallisticsProcessorLUT] Environmental conditions updated:"
             << "Temp:" << temp_celsius << "°C"
             << "Alt:" << altitude_m << "m"
             << "Wind:" << crosswind_ms << "m/s";
}

QString BallisticsProcessorLUT::getAmmunitionName() const
{
    if (!m_lut.isLoaded()) {
        return "No table loaded";
    }
    return m_lut.getAmmunitionMetadata().name;
}

LeadCalculationResult BallisticsProcessorLUT::calculateLeadAngle(
    float targetRangeMeters,
    float targetAngularRateAzDegS,
    float targetAngularRateElDegS,
    float currentMuzzleVelocityMPS,
    float projectileTimeOfFlightGuessS,
    float currentCameraFovHorizontalDegrees)
{
    LeadCalculationResult result;
    result.status = LeadAngleStatus::Off;
    result.leadAzimuthDegrees = 0.0f;
    result.leadElevationDegrees = 0.0f;

    // Validate table is loaded
    if (!m_lut.isLoaded()) {
        qWarning() << "[BallisticsProcessorLUT] No ammunition table loaded!";
        return result;
    }

    // Validate range
    if (targetRangeMeters <= 0.1f) {
        result.status = LeadAngleStatus::Off;
        return result;
    }

    // ========================================================================
    // STEP 1: Get ballistic solution from LUT with environmental corrections
    // ========================================================================

    BallisticSolution sol = m_lut.getSolution(
        targetRangeMeters,
        m_temperature_celsius,
        m_altitude_m,
        m_crosswind_ms
    );

    if (!sol.valid) {
        qDebug() << "[BallisticsProcessorLUT] Invalid solution for range:" << targetRangeMeters << "m";
        result.status = LeadAngleStatus::Off;
        return result;
    }

    // ========================================================================
    // STEP 2: Calculate MOVING TARGET lead angle
    // ========================================================================
    // The LUT gives us elevation for a STATIONARY target.
    // For moving targets, we need to add additional lead based on:
    //   - Target angular rate (from tracker)
    //   - Time of flight (from LUT)
    //
    // Lead_angle = angular_rate × TOF
    // ========================================================================

    float tof_s = sol.tof_s;

    // Convert angular rates from deg/s to radians/s
    float targetAngularRateAzRadS = targetAngularRateAzDegS * (M_PI / 180.0f);
    float targetAngularRateElRadS = targetAngularRateElDegS * (M_PI / 180.0f);

    // Calculate motion lead (target movement during bullet flight)
    float motionLeadAzRad = targetAngularRateAzRadS * tof_s;
    float motionLeadElRad = targetAngularRateElRadS * tof_s;

    // Convert back to degrees
    float motionLeadAzDeg = motionLeadAzRad * (180.0f / M_PI);
    float motionLeadElDeg = motionLeadElRad * (180.0f / M_PI);

    // ========================================================================
    // STEP 3: Combine ballistic drop + motion lead + wind
    // ========================================================================

    // ELEVATION = Ballistic drop compensation (from LUT) + Target motion lead
    result.leadElevationDegrees = sol.elevation_deg + motionLeadElDeg;

    // AZIMUTH = Target motion lead + Wind correction (from LUT)
    // Wind correction is already in mils, convert to degrees
    float windCorrectionDeg = sol.azimuth_correction_mils * 0.05625f;
    result.leadAzimuthDegrees = motionLeadAzDeg + windCorrectionDeg;

    // ========================================================================
    // STEP 4: Apply limits and determine status
    // ========================================================================

    result.status = LeadAngleStatus::On;  // Default to On

    // Check if lead angles exceed maximum
    bool lag = false;
    if (std::abs(result.leadAzimuthDegrees) > MAX_LEAD_ANGLE_DEGREES) {
        result.leadAzimuthDegrees = std::copysign(MAX_LEAD_ANGLE_DEGREES, result.leadAzimuthDegrees);
        lag = true;
    }
    if (std::abs(result.leadElevationDegrees) > MAX_LEAD_ANGLE_DEGREES) {
        result.leadElevationDegrees = std::copysign(MAX_LEAD_ANGLE_DEGREES, result.leadElevationDegrees);
        lag = true;
    }

    // ========================================================================
    // STATUS DETERMINATION - Priority order matters!
    // ========================================================================
    // 1. ZOOM OUT (highest priority) - Lead exceeds FOV, CCIP off-screen
    // 2. LAG (medium priority) - Lead at max limit but still on-screen
    // 3. ON (default) - Normal operation
    // ========================================================================

    // PRIORITY 1: Check ZOOM OUT condition first (lead exceeds FOV)
    float vfov_approx = currentCameraFovHorizontalDegrees;  // Simplified (assume VFOV ≈ HFOV)
    if (currentCameraFovHorizontalDegrees > 0 && vfov_approx > 0) {
        if (std::abs(result.leadAzimuthDegrees) > (currentCameraFovHorizontalDegrees / 2.0f) ||
            std::abs(result.leadElevationDegrees) > (vfov_approx / 2.0f)) {
            result.status = LeadAngleStatus::ZoomOut;
        }
    }

    // PRIORITY 2: Check LAG condition (only if not already ZoomOut)
    if (result.status != LeadAngleStatus::ZoomOut && lag) {
        result.status = LeadAngleStatus::Lag;
    }

    qDebug() << "[BallisticsProcessorLUT] Range:" << targetRangeMeters << "m"
             << "| TOF:" << tof_s << "s"
             << "| Ballistic Elev:" << sol.elevation_deg << "°"
             << "| Motion Lead Az:" << motionLeadAzDeg << "° El:" << motionLeadElDeg << "°"
             << "| Wind:" << windCorrectionDeg << "°"
             << "| Total Lead Az:" << result.leadAzimuthDegrees << "° El:" << result.leadElevationDegrees << "°"
             << "| FOV:" << currentCameraFovHorizontalDegrees << "° (limit:" << (currentCameraFovHorizontalDegrees / 2.0f) << "°)"
             << "| Status:" << static_cast<int>(result.status)
             << (result.status == LeadAngleStatus::On ? "(On)" :
                 result.status == LeadAngleStatus::Lag ? "(Lag)" :
                 result.status == LeadAngleStatus::ZoomOut ? "(ZoomOut)" : "(Unknown)");

    return result;
}
