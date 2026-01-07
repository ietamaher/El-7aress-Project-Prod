/**
 * @file firecontrolcomputation.cpp
 * @brief Implementation of fire control computation logic
 */

#include "firecontrolcomputation.h"
#include <QDebug>
#include <cmath>

// ============================================================================
// MAIN COMPUTATION ENTRY POINT
// ============================================================================

FireControlResult FireControlComputation::compute(const FireControlInput& input,
                                                  BallisticsProcessorLUT* ballisticsProcessor,
                                                  const FireControlResult& previousResult) const {
    FireControlResult result;

    // Store previous values for change detection
    result.previousCrosswindMS = previousResult.calculatedCrosswindMS;
    result.previousDropActive = previousResult.ballisticDropActive;
    result.previousLeadStatus = previousResult.currentLeadAngleStatus;

    // ============================================================================
    // STEP 1: COMPUTE CROSSWIND FROM WINDAGE
    // ============================================================================
    // This runs INDEPENDENTLY of LAC status.
    // Crosswind component varies with absolute gimbal bearing.
    // ============================================================================
    computeCrosswind(input, previousResult.calculatedCrosswindMS, result.calculatedCrosswindMS,
                     result.crosswindChanged);

    // ============================================================================
    // STEP 2: NULL CHECK FOR BALLISTICS PROCESSOR
    // ============================================================================
    if (!ballisticsProcessor) {
        qCritical() << "[FireControlComputation] BallisticsProcessor pointer is NULL!";
        return result;
    }

    // ============================================================================
    // STEP 3: UPDATE ENVIRONMENTAL CONDITIONS
    // ============================================================================
    // Environmental conditions affect both drop and lead calculations.
    // ============================================================================
    if (input.environmentalAppliedToBallistics) {
        ballisticsProcessor->setEnvironmentalConditions(input.environmentalTemperatureCelsius,
                                                        input.environmentalAltitudeMeters,
                                                        result.calculatedCrosswindMS);
    } else {
        // Standard conditions + current crosswind
        ballisticsProcessor->setEnvironmentalConditions(15.0f, 0.0f, result.calculatedCrosswindMS);
    }

    // ============================================================================
    // STEP 4: COMPUTE BALLISTIC DROP (auto-applied when range valid)
    // ============================================================================
    computeBallisticDrop(input, ballisticsProcessor, result.calculatedCrosswindMS,
                         previousResult.ballisticDropActive, result);

    // ============================================================================
    // STEP 5: COMPUTE MOTION LEAD (only when LAC active)
    // ============================================================================
    computeMotionLead(input, ballisticsProcessor, previousResult.currentLeadAngleStatus, result);

    return result;
}

// ============================================================================
// CROSSWIND COMPUTATION
// ============================================================================

void FireControlComputation::computeCrosswind(const FireControlInput& input,
                                              float previousCrosswind, float& outCrosswind,
                                              bool& outChanged) const {
    outCrosswind = 0.0f;
    outChanged = false;

    if (input.windageAppliedToBallistics && input.windageSpeedKnots > 0.001f) {
        // Convert wind speed from knots to m/s
        float windSpeedMS = input.windageSpeedKnots * KNOTS_TO_MS;

        // Calculate absolute gimbal bearing
        float absoluteGimbalBearing =
            calculateAbsoluteGimbalBearing(input.imuYawDeg, input.azimuthDirection);

        // Calculate crosswind component
        outCrosswind = calculateCrosswindComponent(windSpeedMS, input.windageDirectionDegrees,
                                                   absoluteGimbalBearing);

        // Check for significant change
        if (std::abs(previousCrosswind - outCrosswind) > CROSSWIND_CHANGE_THRESHOLD) {
            outChanged = true;
        }
    } else {
        // Windage not applied - clear crosswind
        if (std::abs(previousCrosswind) > CROSSWIND_CHANGE_THRESHOLD) {
            outChanged = true;
        }
    }
}

float FireControlComputation::calculateCrosswindComponent(float windSpeedMS, float windDirectionDeg,
                                                          float gimbalAzimuthDeg) {
    // ============================================================================
    // BALLISTICS PHYSICS: Crosswind Component Calculation
    // Wind direction: Direction wind is coming FROM (meteorological convention)
    // Gimbal azimuth: Direction weapon is pointing TO
    //
    // Relative angle = wind_direction - firing_direction
    // Crosswind = wind_speed x sin(relative_angle)
    //
    // Examples:
    //   Wind from 0deg (North), firing at 0deg (North):
    //     relative = 0deg -> sin(0deg) = 0 -> no crosswind (headwind)
    //   Wind from 0deg (North), firing at 90deg (East):
    //     relative = -90deg -> sin(-90deg) = -1 -> full crosswind (left)
    //   Wind from 0deg (North), firing at 180deg (South):
    //     relative = -180deg -> sin(-180deg) = 0 -> no crosswind (tailwind)
    //   Wind from 0deg (North), firing at 270deg (West):
    //     relative = -270deg = +90deg -> sin(90deg) = +1 -> full crosswind (right)
    // ============================================================================

    float relativeAngle = windDirectionDeg - gimbalAzimuthDeg;

    // Normalize to [-180, +180] for correct trigonometry
    while (relativeAngle > 180.0f) {
        relativeAngle -= 360.0f;
    }
    while (relativeAngle < -180.0f) {
        relativeAngle += 360.0f;
    }

    // Calculate crosswind component
    // sin(90deg) = 1.0 -> full crosswind (wind perpendicular to fire direction)
    // sin(0deg) = 0.0 -> no crosswind (headwind or tailwind)
    float crosswindMS = windSpeedMS * std::sin(relativeAngle * DEG_TO_RAD);

    return crosswindMS;
}

float FireControlComputation::calculateAbsoluteGimbalBearing(float imuYawDeg, float gimbalAzimuth) {
    // Calculate absolute gimbal bearing (true bearing where weapon is pointing)
    float absoluteBearing = imuYawDeg + gimbalAzimuth;

    // Normalize to 0-360 range
    while (absoluteBearing >= 360.0f)
        absoluteBearing -= 360.0f;
    while (absoluteBearing < 0.0f)
        absoluteBearing += 360.0f;

    return absoluteBearing;
}

void FireControlComputation::getActiveCameraFOV(const FireControlInput& input, float& outHFOV,
                                                float& outVFOV) {
    if (input.activeCameraIsDay) {
        outHFOV = input.dayCurrentHFOV;
        outVFOV = input.dayCurrentVFOV;
    } else {
        outHFOV = input.nightCurrentHFOV;
        outVFOV = input.nightCurrentVFOV;
    }
}

// ============================================================================
// BALLISTIC DROP COMPUTATION
// ============================================================================

void FireControlComputation::computeBallisticDrop(const FireControlInput& input,
                                                  BallisticsProcessorLUT* ballisticsProcessor,
                                                  float crosswindMS, bool previousDropActive,
                                                  FireControlResult& result) const {
    float targetRange = input.currentTargetRange;
    bool applyDrop = (targetRange > VALID_RANGE_THRESHOLD);

    if (applyDrop) {
        // Calculate drop from ballistics processor
        LeadCalculationResult drop = ballisticsProcessor->calculateBallisticDrop(targetRange);

        result.ballisticDropOffsetAz = drop.leadAzimuthDegrees;
        result.ballisticDropOffsetEl = drop.leadElevationDegrees;
        result.ballisticDropActive = true;

        // Check for change (from inactive to active, or values changed significantly)
        if (!previousDropActive) {
            result.dropChanged = true;
        }
    } else {
        // No valid range - clear drop
        result.ballisticDropOffsetAz = 0.0f;
        result.ballisticDropOffsetEl = 0.0f;
        result.ballisticDropActive = false;

        // Check for change (from active to inactive)
        if (previousDropActive) {
            result.dropChanged = true;
            qDebug() << "[FireControlComputation] DROP CLEARED (no valid range)";
        }
    }
}

// ============================================================================
// MOTION LEAD COMPUTATION
// ============================================================================

void FireControlComputation::computeMotionLead(const FireControlInput& input,
                                               BallisticsProcessorLUT* ballisticsProcessor,
                                               LeadAngleStatus previousStatus,
                                               FireControlResult& result) const {
    if (!input.leadAngleCompensationActive) {
        // LAC toggle is OFF - clear motion lead
        result.motionLeadOffsetAz = 0.0f;
        result.motionLeadOffsetEl = 0.0f;
        result.currentLeadAngleStatus = LeadAngleStatus::Off;
        result.leadAngleOffsetAz = 0.0f;
        result.leadAngleOffsetEl = 0.0f;

        // Check for change (from active to inactive)
        if (previousStatus != LeadAngleStatus::Off) {
            result.leadChanged = true;
            qDebug() << "[FireControlComputation] MOTION LEAD CLEARED (LAC off)";
        }
        return;
    }

    // LAC is ACTIVE - calculate motion lead for moving target
    float targetRange = input.currentTargetRange;
    float targetAngRateAz = input.currentTargetAngularRateAz;
    float targetAngRateEl = input.currentTargetAngularRateEl;

    // Get active camera FOV
    float currentHFOV, currentVFOV;
    getActiveCameraFOV(input, currentHFOV, currentVFOV);

    // ============================================================================
    // USE DEFAULT RANGE FOR MOTION LEAD WHEN LRF IS CLEARED
    // ============================================================================
    // Motion lead requires TOF which depends on range. When LRF is cleared:
    // - Use DEFAULT_LAC_RANGE (500m) for TOF calculation
    // - This allows CCIP to work for close-range moving targets
    // - Status will show "Lag" to indicate estimated range is used
    // ============================================================================
    float leadCalculationRange =
        (targetRange > VALID_RANGE_THRESHOLD) ? targetRange : DEFAULT_LAC_RANGE;

    LeadCalculationResult lead = ballisticsProcessor->calculateMotionLead(
        leadCalculationRange, targetAngRateAz, targetAngRateEl, currentHFOV, currentVFOV);

    // If using default range (no LRF), force Lag status to indicate estimation
    if (targetRange <= VALID_RANGE_THRESHOLD && lead.status == LeadAngleStatus::On) {
        lead.status = LeadAngleStatus::Lag;
    }

    result.motionLeadOffsetAz = lead.leadAzimuthDegrees;
    result.motionLeadOffsetEl = lead.leadElevationDegrees;
    result.currentLeadAngleStatus = lead.status;

    // Backward compatibility
    result.leadAngleOffsetAz = lead.leadAzimuthDegrees;
    result.leadAngleOffsetEl = lead.leadElevationDegrees;

    // Check for status change
    if (lead.status != previousStatus) {
        result.leadChanged = true;
    }

    qDebug() << "[FireControlComputation] MOTION LEAD:" << "Az:" << lead.leadAzimuthDegrees << "deg"
             << "| El:" << lead.leadElevationDegrees << "deg" << "| Range:" << leadCalculationRange
             << "m" << (targetRange <= VALID_RANGE_THRESHOLD ? "(DEFAULT)" : "(LRF)")
             << "| Status:" << static_cast<int>(lead.status)
             << (lead.status == LeadAngleStatus::On        ? "(On)"
                 : lead.status == LeadAngleStatus::Lag     ? "(Lag)"
                 : lead.status == LeadAngleStatus::ZoomOut ? "(ZoomOut)"
                                                           : "(Unknown)");
}
