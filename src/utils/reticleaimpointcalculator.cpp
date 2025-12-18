#include "reticleaimpointcalculator.h"
#include <cmath> // For M_PI, std::atan, std::tan
#include <QDebug> // For logging

// Static helper method, defined first or forward-declared if used before definition in the same file
QPointF ReticleAimpointCalculator::convertSingleAngularToPixelShift(
    float angularOffsetAzDeg, float angularOffsetElDeg,
    float cameraHfovDeg, int imageWidthPx, int imageHeightPx)
{
    if (cameraHfovDeg <= 0.001f || imageWidthPx <= 0 || imageHeightPx <= 0) {
        qWarning() << "ReticleAimpointCalculator::convertSingleAngularToPixelShift: Invalid params HFOV="
                   << cameraHfovDeg << "W=" << imageWidthPx << "H=" << imageHeightPx;
        return QPointF(0,0);
    }

    double ppdAz = static_cast<double>(imageWidthPx) / cameraHfovDeg;

    // Calculate VFOV from HFOV and aspect ratio
    double aspectRatio = static_cast<double>(imageWidthPx) / static_cast<double>(imageHeightPx);
    double vfov_rad = 2.0 * std::atan(std::tan((cameraHfovDeg * M_PI / 180.0) / 2.0) / aspectRatio);
    double vfov_deg = vfov_rad * 180.0 / M_PI;

    double ppdEl = (vfov_deg > 0.001f) ? (static_cast<double>(imageHeightPx) / vfov_deg) : ppdAz; // Fallback if vfov is 0

    // Reticle shifts in opposite direction to required gun offset
    qreal shiftX_px = static_cast<qreal>(-angularOffsetAzDeg * ppdAz);
    // Positive gun El offset (aim up) = reticle moves down on screen (positive Y in Qt)
    qreal shiftY_px = static_cast<qreal>(angularOffsetElDeg * ppdEl);

    // qDebug() << "ReticleAimpointCalculator::convertSingleAngularToPixelShift: GunOffsetAzD" << angularOffsetAzDeg
    //          << "GunOffsetElD" << angularOffsetElDeg << "HFOV" << cameraHfovDeg
    //          << "-> ReticleShiftPxX" << shiftX_px << "ReticleShiftPxY" << shiftY_px;

    return QPointF(shiftX_px, shiftY_px);
}

QPointF ReticleAimpointCalculator::calculateReticleImagePositionPx(
    float zeroingAzDeg, float zeroingElDeg, bool zeroingActive,
    float leadAzDeg, float leadElDeg, bool leadActive, LeadAngleStatus leadStatus,
    float cameraHfovDeg, int imageWidthPx, int imageHeightPx)
{
    QPointF totalPixelShift(0.0, 0.0);

    if (zeroingActive) {
        // Qualify with class name when calling another static member
        totalPixelShift += ReticleAimpointCalculator::convertSingleAngularToPixelShift(
                                zeroingAzDeg, - zeroingElDeg,
                                cameraHfovDeg, imageWidthPx, imageHeightPx);
    }

    // ========================================================================
    // BUG FIX: Apply CCIP offset for drop-only case (Issue #1)
    // ========================================================================
    // The offset should be applied when:
    // 1. LAC active with valid status (On or Lag) - motion lead compensation
    // 2. OR drop-only mode (leadActive=true but leadStatus=Off)
    //
    // The offset should NOT be applied when:
    // - leadStatus == ZoomOut (can't calculate lead at wide FOV per CROWS doctrine)
    // ========================================================================
    bool applyLeadOffset = leadActive && (leadStatus != LeadAngleStatus::ZoomOut);
    if (applyLeadOffset) {
        // Qualify with class name
        totalPixelShift += ReticleAimpointCalculator::convertSingleAngularToPixelShift(
                                leadAzDeg, leadElDeg,
                                cameraHfovDeg, imageWidthPx, imageHeightPx);
    }

    qreal screenCenterX_px = static_cast<qreal>(imageWidthPx) / 2.0f;
    qreal screenCenterY_px = static_cast<qreal>(imageHeightPx) / 2.0f;

    qDebug() << "🎯 ReticleAimpointCalculator::calculateReticleImagePositionPx: Center(" << screenCenterX_px << "," << screenCenterY_px
             << ") TotalShift(" << totalPixelShift.x() << "," << totalPixelShift.y() << ")"
             << "FinalPos(" << (screenCenterX_px + totalPixelShift.x()) << "," << (screenCenterY_px + totalPixelShift.y()) << ")";

    return QPointF(screenCenterX_px + totalPixelShift.x(),
                   screenCenterY_px + totalPixelShift.y());
}
