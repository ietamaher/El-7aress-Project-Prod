#pragma once

/**
 * @file reticleaimpointcalculator.h
 * @brief Reticle aimpoint position calculator for ballistic corrections
 */

// ============================================================================
// INCLUDES
// ============================================================================
#include <QPointF>

#include "models/domain/systemstatemodel.h"

// ============================================================================
// CLASS DECLARATION
// ============================================================================
class ReticleAimpointCalculator {
public:
    static QPointF calculateReticleImagePositionPx(float zeroingAzDeg, float zeroingElDeg,
                                                   bool zeroingActive, float leadAzDeg,
                                                   float leadElDeg, bool leadActive,
                                                   LeadAngleStatus leadStatus, float cameraHfovDeg,
                                                   int imageWidthPx, int imageHeightPx);

private:
    static QPointF convertSingleAngularToPixelShift(float angularOffsetAzDeg,
                                                    float angularOffsetElDeg, float cameraHfovDeg,
                                                    int imageWidthPx, int imageHeightPx);
};
