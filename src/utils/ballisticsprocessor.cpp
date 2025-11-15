#include "ballisticsprocessor.h"

#include <cmath>
#include <QDebug>


const float GRAVITY_MPS2 = 9.80665f; // Standard gravity

BallisticsProcessor::BallisticsProcessor() {}

void BallisticsProcessor::setEnvironmentalConditions(float temp_celsius, float altitude_m, float crosswind_ms)
{
    m_temperature_celsius = temp_celsius;
    m_altitude_m = altitude_m;
    m_crosswind_ms = crosswind_ms;

    qDebug() << "[BallisticsProcessor] Environmental conditions updated:"
             << "Temp:" << temp_celsius << "°C"
             << "Alt:" << altitude_m << "m"
             << "Wind:" << crosswind_ms << "m/s";
}

LeadCalculationResult BallisticsProcessor::calculateLeadAngle(
    float targetRangeMeters,
    float targetAngularRateAzDegS,
    float targetAngularRateElDegS,
    float currentMuzzleVelocityMPS, // Now we might use this for a better TOF
    float projectileTimeOfFlightGuessS, // Still useful as an initial guess or if accurately provided
    float currentCameraFovHorizontalDegrees,
    float currentCameraFovVerticalDegrees)
{
    LeadCalculationResult result;
    result.status = LeadAngleStatus::On; // Default to On if calculation proceeds

    if (targetRangeMeters <= 0.1f) { // Use a small epsilon for range
        result.status = LeadAngleStatus::Off;
        return result;
    }

    // --- Time of Flight (TOF) ---
    // If projectileTimeOfFlightGuessS is not provided accurately, calculate a basic one.
    // This is still very simplified as it ignores drag.
    float tofS = projectileTimeOfFlightGuessS;
    if (tofS <= 0.0f && currentMuzzleVelocityMPS > 0.0f) {
        tofS = targetRangeMeters / currentMuzzleVelocityMPS;
    }
    if (tofS <= 0.0f) { // Still no valid TOF
        result.status = LeadAngleStatus::Off;
        return result;
    }

    // --- Lead for Target Motion ---
    float targetAngularRateAzRadS = targetAngularRateAzDegS * (M_PI / 180.0);
    float targetAngularRateElRadS = targetAngularRateElDegS * (M_PI / 180.0);

    float motionLeadAzRad = targetAngularRateAzRadS * tofS;
    float motionLeadElRad = targetAngularRateElRadS * tofS;

    // --- Lead for Projectile Drop (Gravity) ---
    // Simple calculation ignoring air resistance. Weapon bore must be elevated.
    // Drop (meters) = 0.5 * g * TOF^2
    // Elevation angle (radians) to compensate for drop = atan(drop / range_horizontal)
    // Assuming targetRangeMeters is slant range, for very flat trajectories horizontal_range ~ slant_range.
    float projectileDropMeters = 0.5f * GRAVITY_MPS2 * tofS * tofS;
    float dropCompensationElRad = 0.0f;
    if (targetRangeMeters > 0.1f) { // Avoid division by zero
        dropCompensationElRad = std::atan(projectileDropMeters / targetRangeMeters);
    }

    // --- Total Lead ---
    // Azimuth lead is only from target motion (in this simplified model)
    float totalLeadAzRad = motionLeadAzRad;
    // Elevation lead is motion lead + compensation for projectile drop
    // If target is moving upwards (positive motionLeadElRad), we need to lead even higher.
    // Drop compensation always means aiming higher.
    float totalLeadElRad = motionLeadElRad + dropCompensationElRad;

    // --- Convert to degrees ---
    result.leadAzimuthDegrees = static_cast<float>(totalLeadAzRad * (180.0 / M_PI));
    result.leadElevationDegrees = static_cast<float>(totalLeadElRad * (180.0 / M_PI));

    // ========================================================================
    // ENVIRONMENTAL CORRECTIONS
    // ========================================================================
    // Apply real-time corrections for temperature, altitude, and wind
    // ========================================================================

    // 1. TEMPERATURE CORRECTION (affects air density, thus drag)
    // Hot air = less dense → less drag → aim lower
    // Cold air = more dense → more drag → aim higher
    float temp_kelvin = m_temperature_celsius + 273.15f;
    float temp_correction = std::sqrt(288.15f / temp_kelvin);  // Standard: 15°C = 288.15K
    result.leadElevationDegrees *= temp_correction;

    // 2. ALTITUDE CORRECTION (affects air density)
    // Higher altitude = less dense → less drag → aim lower
    float rho_altitude = std::exp(-m_altitude_m / 8500.0f);  // Scale height: 8500m
    float altitude_correction = 1.0f / rho_altitude;
    result.leadElevationDegrees *= altitude_correction;

    // 3. WIND CORRECTION (crosswind deflection)
    // ========================================================================
    // CONVENTION (Right-Hand Rule from shooter's perspective):
    //   crosswind_ms > 0: Wind from LEFT  → Bullet drifts RIGHT → Aim LEFT  (negative correction)
    //   crosswind_ms < 0: Wind from RIGHT → Bullet drifts LEFT  → Aim RIGHT (positive correction)
    //
    // Example: crosswind_ms = +5.0 m/s (wind from left)
    //   - Bullet deflects RIGHT by (5 × TOF) meters
    //   - To hit target, aim LEFT by angle = atan(deflection/range)
    //   - Apply NEGATIVE correction to azimuth
    // ========================================================================
    if (m_crosswind_ms != 0.0f && targetRangeMeters > 0.1f) {
        float wind_deflection_m = m_crosswind_ms * tofS;  // Lateral displacement (+ = right, - = left)
        float wind_correction_rad = std::atan(wind_deflection_m / targetRangeMeters);
        float wind_correction_deg = wind_correction_rad * (180.0f / M_PI);

        // CRITICAL: SUBTRACT wind correction to compensate for drift
        // Wind from left (+) pushes bullet right, so aim left (-)
        result.leadAzimuthDegrees -= wind_correction_deg;

        qDebug() << "[Ballistics] Wind correction: crosswind=" << m_crosswind_ms << "m/s"
                 << "deflection=" << wind_deflection_m << "m"
                 << "correction=" << wind_correction_deg << "deg (applied as" << -wind_correction_deg << ")";
    }

    // --- Apply Limits and Status ---
    bool lag = false;
    if (std::abs(result.leadAzimuthDegrees) > MAX_LEAD_ANGLE_DEGREES) {
        result.leadAzimuthDegrees = std::copysign(MAX_LEAD_ANGLE_DEGREES, result.leadAzimuthDegrees);
        lag = true;
    }
    // Note: Max lead for elevation might be different due to physical gun limits
    if (std::abs(result.leadElevationDegrees) > MAX_LEAD_ANGLE_DEGREES) { // Using same MAX_LEAD for El for now
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
    // Use actual VFOV - cameras are NOT square (day: 1280×720→1024×768, night: 640×512)
    if (currentCameraFovHorizontalDegrees > 0 && currentCameraFovVerticalDegrees > 0) {
        if (std::abs(result.leadAzimuthDegrees) > (currentCameraFovHorizontalDegrees / 2.0f) ||
            std::abs(result.leadElevationDegrees) > (currentCameraFovVerticalDegrees / 2.0f)) {
            result.status = LeadAngleStatus::ZoomOut;
        }
    }

    // PRIORITY 2: Check LAG condition (only if not already ZoomOut)
    if (result.status != LeadAngleStatus::ZoomOut && lag) {
        result.status = LeadAngleStatus::Lag;
    }

    qDebug() << "Ballistics: R:" << targetRangeMeters << "TOF:" << tofS
             << "Rates Az:" << targetAngularRateAzDegS << "El:" << targetAngularRateElDegS
             << "DropCompElRad:" << dropCompensationElRad
             << "=> Lead Az:" << result.leadAzimuthDegrees << "El:" << result.leadElevationDegrees
             << "Status:" << static_cast<int>(result.status);

    return result;
}
