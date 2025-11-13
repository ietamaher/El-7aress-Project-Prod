#ifndef BALLISTICSPROCESSOR_H
#define BALLISTICSPROCESSOR_H

#include "models/domain/systemstatemodel.h"

// Forward declare if SystemStateData is complex or to reduce includes
// struct SystemStateData;
// enum class AmmunitionType;

 
struct LeadCalculationResult {
    float leadAzimuthDegrees   = 0.0f; // The calculated lead offset in Azimuth (degrees)
    float leadElevationDegrees = 0.0f; // The calculated lead offset in Elevation (degrees)
                                       // (This might include bullet drop + moving target lead)
    LeadAngleStatus status     = LeadAngleStatus::Off; // Status of the calculation
};
class BallisticsProcessor
{
public:
    BallisticsProcessor();

    // Calculates lead based on current target and system parameters
    LeadCalculationResult calculateLeadAngle(
        float targetRangeMeters,
        float targetAngularRateAzDegS, // Relative angular rate of target AZ
        float targetAngularRateElDegS, // Relative angular rate of target EL
        // AmmunitionType ammoType, // Requires ammo parameters (e.g., muzzle velocity, BC)
        float currentMuzzleVelocityMPS, // Example input
        float projectileTimeOfFlightGuessS, // An initial guess, can be iterative
        float currentCameraFovHorizontalDegrees // Needed for ZOOM_OUT check
    );

    /**
     * @brief Set environmental conditions for ballistic corrections
     *
     * @param temp_celsius Air temperature in Celsius (standard: 15°C)
     * @param altitude_m Altitude above sea level in meters (standard: 0m)
     * @param crosswind_ms Crosswind speed in m/s (standard: 0)
     */
    void setEnvironmentalConditions(float temp_celsius, float altitude_m, float crosswind_ms);

private:
    // Constants for calculation (simplified)
    const float MAX_LEAD_ANGLE_DEGREES = 10.0f; // Example maximum lead allowed

    // Environmental conditions (for corrections)
    float m_temperature_celsius = 15.0f;   ///< Air temperature
    float m_altitude_m = 0.0f;             ///< Altitude above sea level
    float m_crosswind_ms = 0.0f;           ///< Crosswind speed

    // Helper methods for more complex ballistics if needed
    // float calculateTOF(float range, float muzzleVelocity, float projectileDragCoeff);
    // Point2D predictTargetPosition(float currentRange, float angularRateAz, float angularRateEl, float tof);
};

#endif // BALLISTICSPROCESSOR_H
