#include "reticleaimpointcalculator.h"
#include <cmath>   // For M_PI, std::atan, std::tan
#include <QDebug> // For logging

// ============================================================================
// CROWS-GRADE TAN-PROJECTION MATH FOR ANGULAR-TO-PIXEL CONVERSION
// ============================================================================
// This implementation uses proper perspective projection (tan-based) instead
// of linear approximation (deg × PPD). This is critical for:
//
// 1. High zoom levels (HFOV far from native sensor FOV)
// 2. Large angular offsets (zeroing, lead angles > 1°)
// 3. Accurate elevation projection (non-linear at steep angles)
//
// References:
// - CROWS M153 Fire Control System math
// - ASELSAN SARP EO/RCWS projection equations
// - Standard camera pinhole model: x = fx * tan(θ)
//
// Key insight: In a perspective camera, angular displacement maps to pixels
// via TAN, not linear scaling. The difference becomes significant at:
// - ±5° offset: ~0.5% error with linear approximation
// - ±15° offset: ~4% error with linear approximation
// ============================================================================

/**
 * @brief Converts angular offset to pixel displacement using tan-projection
 *
 * This is the CROWS-grade replacement for linear deg×PPD approximation.
 * Uses proper pinhole camera model: pixel_offset = focal_length × tan(angle)
 *
 * @param angularOffsetAzDeg Azimuth offset in degrees (positive = right)
 * @param angularOffsetElDeg Elevation offset in degrees (positive = up)
 * @param cameraHfovDeg Camera horizontal field of view in degrees
 * @param imageWidthPx Image width in pixels
 * @param imageHeightPx Image height in pixels
 * @return QPointF Pixel displacement from image center (X right, Y down in Qt coords)
 */
QPointF ReticleAimpointCalculator::convertSingleAngularToPixelShift(
    float angularOffsetAzDeg, float angularOffsetElDeg,
    float cameraHfovDeg, int imageWidthPx, int imageHeightPx)
{
    // Validate inputs
    if (cameraHfovDeg <= 0.001f || imageWidthPx <= 0 || imageHeightPx <= 0) {
        qWarning() << "ReticleAimpointCalculator::convertSingleAngularToPixelShift: Invalid params"
                   << "HFOV=" << cameraHfovDeg << "W=" << imageWidthPx << "H=" << imageHeightPx;
        return QPointF(0, 0);
    }

    // ========================================================================
    // STEP 1: Calculate camera intrinsics (focal lengths in pixels)
    // ========================================================================
    // For a pinhole camera model:
    //   fx = (imageWidth / 2) / tan(HFOV / 2)
    //   fy = (imageHeight / 2) / tan(VFOV / 2)
    //
    // VFOV is derived from HFOV using the aspect ratio:
    //   tan(VFOV/2) = tan(HFOV/2) / aspectRatio
    // ========================================================================

    double hfov_rad = cameraHfovDeg * M_PI / 180.0;
    double aspectRatio = static_cast<double>(imageWidthPx) / static_cast<double>(imageHeightPx);

    // Calculate VFOV from HFOV (correct perspective relationship)
    double vfov_rad = 2.0 * std::atan(std::tan(hfov_rad / 2.0) / aspectRatio);

    // Focal lengths in pixels (pinhole camera intrinsics)
    double fx = (imageWidthPx / 2.0) / std::tan(hfov_rad / 2.0);
    double fy = (imageHeightPx / 2.0) / std::tan(vfov_rad / 2.0);

    // ========================================================================
    // STEP 2: Convert angular offset to pixel displacement using TAN projection
    // ========================================================================
    // Pinhole model: pixel_offset = focal_length × tan(angle)
    //
    // Sign convention (consistent with Qt coordinate system):
    //   - Positive azimuth (right) → positive X pixel shift (right on screen)
    //   - Positive elevation (up in world) → negative Y pixel shift (up on screen)
    //     BUT: We want reticle to show where bullets GO, so if gun shoots HIGH,
    //     we need reticle to move UP (negative Y in Qt).
    //
    // For ZEROING offset interpretation:
    //   - zeroingAz > 0 means impact was RIGHT of aim point
    //   - We want reticle to shift RIGHT to show where bullets hit
    //   - So: shiftX = +fx × tan(az)
    //
    //   - zeroingEl > 0 means impact was ABOVE aim point
    //   - We want reticle to shift UP to show where bullets hit
    //   - In Qt coords, UP is negative Y
    //   - So: shiftY = -fy × tan(el)
    // ========================================================================

    double az_rad = angularOffsetAzDeg * M_PI / 180.0;
    double el_rad = angularOffsetElDeg * M_PI / 180.0;

    // Pixel displacement using proper perspective projection
    // Positive az → shift right (positive X)
    // Positive el → shift up (negative Y in Qt coords)
    qreal shiftX_px = static_cast<qreal>(fx * std::tan(az_rad));
    qreal shiftY_px = static_cast<qreal>(-fy * std::tan(el_rad));

    return QPointF(shiftX_px, shiftY_px);
}

/**
 * @brief Calculates reticle/CCIP position on image using CROWS-grade projection
 *
 * This function computes the final reticle or CCIP position by combining:
 * 1. Zeroing offset (camera-to-gun alignment)
 * 2. Lead angle offset (motion compensation for moving targets)
 *
 * All offsets are converted using proper tan-projection for FCS-grade accuracy.
 *
 * @param zeroingAzDeg Zeroing azimuth offset in degrees
 * @param zeroingElDeg Zeroing elevation offset in degrees
 * @param zeroingActive Whether zeroing is applied
 * @param leadAzDeg Lead angle azimuth in degrees
 * @param leadElDeg Lead angle elevation in degrees
 * @param leadActive Whether lead angle is active
 * @param leadStatus Current lead angle status (On, Lag, ZoomOut)
 * @param cameraHfovDeg Camera horizontal FOV in degrees
 * @param imageWidthPx Image width in pixels
 * @param imageHeightPx Image height in pixels
 * @return QPointF Final position in image pixel coordinates
 */
QPointF ReticleAimpointCalculator::calculateReticleImagePositionPx(
    float zeroingAzDeg, float zeroingElDeg, bool zeroingActive,
    float leadAzDeg, float leadElDeg, bool leadActive, LeadAngleStatus leadStatus,
    float cameraHfovDeg, int imageWidthPx, int imageHeightPx)
{
    // ========================================================================
    // ACCUMULATE TOTAL ANGULAR OFFSET
    // ========================================================================
    // We sum all angular offsets FIRST, then project to pixels ONCE.
    // This is more accurate than projecting each offset separately and summing
    // pixels, because tan(a+b) ≠ tan(a) + tan(b).
    //
    // However, for small angles (<5°), the difference is negligible.
    // For simplicity and to maintain separate control over zeroing vs lead,
    // we project each component separately and sum the pixel shifts.
    // ========================================================================

    QPointF totalPixelShift(0.0, 0.0);

    // ========================================================================
    // ZEROING OFFSET
    // ========================================================================
    // Zeroing offset represents the angular difference between camera LOS
    // and gun bore. Positive offset means impact is RIGHT/UP of aim point.
    //
    // Sign convention: Input zeroingAzDeg/zeroingElDeg are stored as
    // (impact_position - aim_position), so positive = impact to the right/up.
    // We want reticle to show where bullets HIT, so we apply the offset directly.
    // ========================================================================
    if (zeroingActive) {
        totalPixelShift += ReticleAimpointCalculator::convertSingleAngularToPixelShift(
            zeroingAzDeg,   // Positive = impact right → reticle shifts right
            zeroingElDeg,   // Positive = impact up → reticle shifts up
            cameraHfovDeg, imageWidthPx, imageHeightPx);
    }

    // ========================================================================
    // BALLISTIC/LEAD OFFSET (for CCIP)
    // ========================================================================
    // This offset combines:
    // 1. Ballistic drop (gravity + wind) - auto-applied when LRF valid
    // 2. Motion lead (target velocity compensation) - when LAC active
    //
    // CROWS/SARP DOCTRINE FIX:
    // - Always apply offsets when leadActive is true
    // - The caller (SystemStateModel) has already combined drop + lead
    // - leadStatus is for DISPLAY purposes (status text, color) not filtering
    // - ZoomOut detection is handled by SystemStateModel after position calc
    //
    // Previous bug: leadStatus == Off blocked ballistic-only CCIP display
    // ========================================================================
    if (leadActive) {
        totalPixelShift += ReticleAimpointCalculator::convertSingleAngularToPixelShift(
            leadAzDeg,      // Combined ballistic + lead in azimuth direction
            leadElDeg,      // Combined ballistic + lead in elevation direction
            cameraHfovDeg, imageWidthPx, imageHeightPx);
    }

    // ========================================================================
    // FINAL POSITION = IMAGE CENTER + TOTAL SHIFT
    // ========================================================================
    qreal screenCenterX_px = static_cast<qreal>(imageWidthPx) / 2.0;
    qreal screenCenterY_px = static_cast<qreal>(imageHeightPx) / 2.0;

    QPointF finalPos(screenCenterX_px + totalPixelShift.x(),
                     screenCenterY_px + totalPixelShift.y());

    // Debug output (throttled in production)
    /*qDebug() << "🎯 ReticleAimpointCalculator [TAN-PROJECTION]:"
             << "HFOV=" << cameraHfovDeg << "°"
             << "| Zeroing(" << zeroingAzDeg << "," << zeroingElDeg << ")°"
             << "| Lead(" << leadAzDeg << "," << leadElDeg << ")°"
             << "| Shift(" << totalPixelShift.x() << "," << totalPixelShift.y() << ")px"
             << "| Final(" << finalPos.x() << "," << finalPos.y() << ")px";*/

    return finalPos;
}
