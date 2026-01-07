#ifndef FIRECONTROLCOMPUTATION_H
#define FIRECONTROLCOMPUTATION_H

/**
 * @file firecontrolcomputation.h
 * @brief Extracted fire control computation logic for unit-testability
 *
 * This class provides pure computation functions for fire control solution,
 * separating the calculation logic from state management in WeaponController.
 *
 * DESIGN PRINCIPLES:
 * 1. Pure Functions: All computations are stateless and side-effect free
 * 2. Dependency Injection: BallisticsProcessorLUT is passed as parameter
 * 3. Testability: All inputs are provided via input struct, outputs via result struct
 * 4. Separation of Concerns: Computation only - no state updates
 *
 * @date 2025-12-30
 * @version 1.0
 */

#include "ballisticsprocessorlut.h"
#include <cmath>

// Forward declaration
class BallisticsProcessorLUT;

/**
 * @brief Input data for fire control computation
 *
 * Contains all parameters needed to compute fire control solution.
 * Extracted from SystemStateData to allow unit testing without full state model.
 */
struct FireControlInput {
    // ============================================================================
    // RANGE DATA
    // ============================================================================
    float currentTargetRange = 0.0f;        ///< LRF range in meters (0 = no lock)

    // ============================================================================
    // ANGULAR RATE DATA (for motion lead)
    // ============================================================================
    float currentTargetAngularRateAz = 0.0f;  ///< Target angular rate azimuth (deg/s)
    float currentTargetAngularRateEl = 0.0f;  ///< Target angular rate elevation (deg/s)

    // ============================================================================
    // CAMERA DATA (for FOV checks)
    // ============================================================================
    bool activeCameraIsDay = true;          ///< True if day camera active
    float dayCurrentHFOV = 20.0f;           ///< Day camera horizontal FOV (deg)
    float dayCurrentVFOV = 15.0f;           ///< Day camera vertical FOV (deg)
    float nightCurrentHFOV = 20.0f;         ///< Night camera horizontal FOV (deg)
    float nightCurrentVFOV = 15.0f;         ///< Night camera vertical FOV (deg)

    // ============================================================================
    // WINDAGE DATA
    // ============================================================================
    bool windageAppliedToBallistics = false;  ///< Whether windage should be applied
    float windageSpeedKnots = 0.0f;           ///< Wind speed in knots
    float windageDirectionDegrees = 0.0f;     ///< Wind direction (from, deg true)

    // ============================================================================
    // GIMBAL / VEHICLE DATA (for crosswind calculation)
    // ============================================================================
    float imuYawDeg = 0.0f;                 ///< Vehicle heading (deg true)
    float azimuthDirection = 0.0f;          ///< Gimbal azimuth relative to vehicle (deg)

    // ============================================================================
    // ENVIRONMENTAL DATA
    // ============================================================================
    bool environmentalAppliedToBallistics = false;  ///< Whether env corrections applied
    float environmentalTemperatureCelsius = 15.0f;  ///< Air temperature (Celsius)
    float environmentalAltitudeMeters = 0.0f;       ///< Altitude above sea level (m)

    // ============================================================================
    // LAC (Lead Angle Compensation) STATE
    // ============================================================================
    bool leadAngleCompensationActive = false;  ///< Whether LAC toggle is active
};

/**
 * @brief Result of fire control computation
 *
 * Contains all computed offsets and status values.
 * WeaponController applies these to SystemStateData.
 */
struct FireControlResult {
    // ============================================================================
    // CROSSWIND
    // ============================================================================
    float calculatedCrosswindMS = 0.0f;     ///< Computed crosswind component (m/s)
    bool crosswindChanged = false;          ///< True if crosswind value changed

    // ============================================================================
    // BALLISTIC DROP (auto-applied when range valid)
    // ============================================================================
    float ballisticDropOffsetAz = 0.0f;     ///< Drop offset azimuth (deg, wind component)
    float ballisticDropOffsetEl = 0.0f;     ///< Drop offset elevation (deg, gravity)
    bool ballisticDropActive = false;       ///< True if drop is being applied
    bool dropChanged = false;               ///< True if drop values changed

    // ============================================================================
    // MOTION LEAD (only when LAC active)
    // ============================================================================
    float motionLeadOffsetAz = 0.0f;        ///< Motion lead azimuth (deg)
    float motionLeadOffsetEl = 0.0f;        ///< Motion lead elevation (deg)
    LeadAngleStatus currentLeadAngleStatus = LeadAngleStatus::Off;  ///< LAC status
    bool leadChanged = false;               ///< True if lead values changed

    // ============================================================================
    // BACKWARD COMPATIBILITY (deprecated fields)
    // ============================================================================
    float leadAngleOffsetAz = 0.0f;         ///< Deprecated: use motionLeadOffsetAz
    float leadAngleOffsetEl = 0.0f;         ///< Deprecated: use motionLeadOffsetEl

    // ============================================================================
    // PREVIOUS VALUES (for change detection)
    // ============================================================================
    float previousCrosswindMS = 0.0f;       ///< Previous crosswind for change detection
    bool previousDropActive = false;        ///< Previous drop state for change detection
    LeadAngleStatus previousLeadStatus = LeadAngleStatus::Off;  ///< Previous lead status
};

/**
 * @class FireControlComputation
 * @brief Pure computation class for fire control solution
 *
 * Extracts fire control logic from WeaponController for unit-testability.
 * All methods are stateless - they take input and return output without side effects.
 *
 * USAGE:
 * @code
 * FireControlComputation fcc;
 * FireControlInput input = buildInputFromState(systemState);
 * FireControlResult result = fcc.compute(input, &ballisticsProcessor, previousResult);
 * applyResultToState(result, systemState);
 * @endcode
 */
class FireControlComputation
{
public:
    // ============================================================================
    // CONFIGURATION CONSTANTS
    // ============================================================================

    /**
     * @brief Default range for motion lead when LRF is cleared
     *
     * Professional FCS uses 500-1000m default range for TOF calculation
     * when LRF is not locked. This allows CCIP to function for close-range
     * moving targets even without LRF.
     */
    static constexpr float DEFAULT_LAC_RANGE = 500.0f;  // meters

    /**
     * @brief Threshold for valid LRF range
     */
    static constexpr float VALID_RANGE_THRESHOLD = 0.1f;  // meters

    /**
     * @brief Threshold for crosswind change detection
     */
    static constexpr float CROSSWIND_CHANGE_THRESHOLD = 0.01f;  // m/s

    // ============================================================================
    // CONVERSION CONSTANTS
    // ============================================================================

    /**
     * @brief Knots to meters per second conversion factor
     */
    static constexpr float KNOTS_TO_MS = 0.514444f;

    /**
     * @brief Degrees to radians conversion
     */
    static constexpr float DEG_TO_RAD = static_cast<float>(M_PI) / 180.0f;

    // ============================================================================
    // PUBLIC METHODS
    // ============================================================================

    /**
     * @brief Compute complete fire control solution
     *
     * This is the main entry point. It computes crosswind, drop, and lead
     * based on the input parameters.
     *
     * @param input Input parameters from system state
     * @param ballisticsProcessor Pointer to ballistics processor (must not be null)
     * @param previousResult Previous result for change detection
     * @return Computed fire control result
     */
    FireControlResult compute(
        const FireControlInput& input,
        BallisticsProcessorLUT* ballisticsProcessor,
        const FireControlResult& previousResult) const;

    /**
     * @brief Calculate crosswind component from wind and bearing
     *
     * Pure function that calculates the crosswind component perpendicular
     * to the firing direction.
     *
     * @param windSpeedMS Wind speed in m/s
     * @param windDirectionDeg Wind direction (where FROM, degrees true)
     * @param gimbalAzimuthDeg Gimbal azimuth (where TO, degrees true)
     * @return Crosswind component in m/s (positive = right, negative = left)
     */
    static float calculateCrosswindComponent(
        float windSpeedMS,
        float windDirectionDeg,
        float gimbalAzimuthDeg);

    /**
     * @brief Calculate absolute gimbal bearing (world frame)
     *
     * @param imuYawDeg Vehicle heading (degrees true)
     * @param gimbalAzimuth Gimbal azimuth relative to vehicle (degrees)
     * @return Absolute bearing (degrees true, normalized 0-360)
     */
    static float calculateAbsoluteGimbalBearing(
        float imuYawDeg,
        float gimbalAzimuth);

    /**
     * @brief Get active camera FOV values
     *
     * @param input Input with camera state
     * @param outHFOV Output horizontal FOV
     * @param outVFOV Output vertical FOV
     */
    static void getActiveCameraFOV(
        const FireControlInput& input,
        float& outHFOV,
        float& outVFOV);

private:
    /**
     * @brief Compute crosswind from windage parameters
     *
     * @param input Input parameters
     * @param previousCrosswind Previous crosswind value
     * @param outCrosswind Output crosswind value
     * @param outChanged Output whether value changed
     */
    void computeCrosswind(
        const FireControlInput& input,
        float previousCrosswind,
        float& outCrosswind,
        bool& outChanged) const;

    /**
     * @brief Compute ballistic drop compensation
     *
     * @param input Input parameters
     * @param ballisticsProcessor Ballistics calculator
     * @param crosswindMS Current crosswind
     * @param previousDropActive Previous drop state
     * @param result Output result to populate
     */
    void computeBallisticDrop(
        const FireControlInput& input,
        BallisticsProcessorLUT* ballisticsProcessor,
        float crosswindMS,
        bool previousDropActive,
        FireControlResult& result) const;

    /**
     * @brief Compute motion lead compensation
     *
     * @param input Input parameters
     * @param ballisticsProcessor Ballistics calculator
     * @param previousStatus Previous lead status
     * @param result Output result to populate
     */
    void computeMotionLead(
        const FireControlInput& input,
        BallisticsProcessorLUT* ballisticsProcessor,
        LeadAngleStatus previousStatus,
        FireControlResult& result) const;
};

#endif // FIRECONTROLCOMPUTATION_H
