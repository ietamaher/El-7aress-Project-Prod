#ifndef BALLISTICSPROCESSORLUT_H
#define BALLISTICSPROCESSORLUT_H

#include "ballisticslut.h"
#include "ballisticsprocessor.h"  // For LeadCalculationResult and LeadAngleStatus
#include "models/domain/systemstatemodel.h"

/**
 * @brief Lookup Table-based Ballistics Processor (Drop-in Replacement)
 *
 * This class adapts the BallisticsLUT to work with the existing
 * WeaponController interface, providing a drop-in replacement for
 * the equation-based BallisticsProcessor.
 *
 * MIGRATION PATH:
 * 1. Replace BallisticsProcessor with BallisticsProcessorLUT in WeaponController
 * 2. Load ammunition table at startup
 * 3. Environmental data (temp/altitude/wind) from sensors
 * 4. System automatically uses LUT instead of equations
 *
 * PERFORMANCE GAIN: 10-100x faster (0.05ms vs 0.5-10ms)
 */
class BallisticsProcessorLUT
{
public:
    BallisticsProcessorLUT();
    ~BallisticsProcessorLUT();

    /**
     * @brief Load ammunition table
     *
     * @param filepath Path to JSON table (Qt resource or filesystem)
     * @return true if loaded successfully
     *
     * Example:
     *   processor->loadAmmunitionTable(":/ballistic/tables/m2_ball.json");
     */
    bool loadAmmunitionTable(const QString& filepath);

    /**
     * @brief Calculate lead angle (drop-in replacement for old processor)
     *
     * This method maintains API compatibility with the old BallisticsProcessor
     * but uses LUT instead of equations internally.
     *
     * @param targetRangeMeters Target range from laser rangefinder
     * @param targetAngularRateAzDegS Target angular rate in azimuth (deg/s)
     * @param targetAngularRateElDegS Target angular rate in elevation (deg/s)
     * @param currentMuzzleVelocityMPS Muzzle velocity (m/s) - IGNORED for LUT
     * @param projectileTimeOfFlightGuessS Initial TOF guess - IGNORED for LUT
     * @param currentCameraFovHorizontalDegrees Current FOV for ZOOM_OUT check
     * @return LeadCalculationResult with lead angles and status
     */
    LeadCalculationResult calculateLeadAngle(
        float targetRangeMeters,
        float targetAngularRateAzDegS,
        float targetAngularRateElDegS,
        float currentMuzzleVelocityMPS,       // Ignored - table has MV baked in
        float projectileTimeOfFlightGuessS,   // Ignored - table provides exact TOF
        float currentCameraFovHorizontalDegrees
    );

    /**
     * @brief Set environmental conditions for corrections
     *
     * These should be updated from actual sensors in real-time:
     * - Temperature: From weather station or internal sensor
     * - Altitude: From GPS or barometric altimeter
     * - Crosswind: From weather station (perpendicular wind component)
     */
    void setEnvironmentalConditions(float temp_celsius,
                                     float altitude_m,
                                     float crosswind_ms);

    /**
     * @brief Get loaded ammunition name
     */
    QString getAmmunitionName() const;

    /**
     * @brief Check if table is loaded
     */
    bool isTableLoaded() const { return m_lut.isLoaded(); }

private:
    BallisticsLUT m_lut;                   ///< Lookup table engine

    // Environmental conditions (updated from sensors)
    float m_temperature_celsius = 15.0f;   ///< Air temperature (standard: 15°C)
    float m_altitude_m = 0.0f;             ///< Altitude above sea level (standard: 0m)
    float m_crosswind_ms = 0.0f;           ///< Crosswind speed (standard: 0 m/s)

    // Configuration
    const float MAX_LEAD_ANGLE_DEGREES = 10.0f;  ///< Maximum lead allowed
};

#endif // BALLISTICSPROCESSORLUT_H
