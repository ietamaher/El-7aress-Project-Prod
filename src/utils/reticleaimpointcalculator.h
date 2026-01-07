// reticleaimpointcalculator.h
#ifndef RETICLEAIMPOINTCALCULATOR_H
#define RETICLEAIMPOINTCALCULATOR_H

#include <QPointF>
#include "models/domain/systemstatemodel.h"  // Or wherever LeadAngleStatus is defined

// Forward declare if needed, or include the header where LeadAngleStatus is defined
// enum class LeadAngleStatus;

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

#endif  // RETICLEAIMPOINTCALCULATOR_H
