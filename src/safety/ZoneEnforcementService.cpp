/**
 * @file ZoneEnforcementService.cpp
 * @brief Implementation of centralized zone enforcement service
 */

#include "ZoneEnforcementService.h"
#include <QDebug>
#include <cmath>
#include <algorithm>

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ZoneEnforcementService::ZoneEnforcementService(QObject* parent)
    : QObject(parent)
{
    qDebug() << "[ZoneEnforcementService] Initialized";
}

// ============================================================================
// ZONE CONFIGURATION
// ============================================================================

void ZoneEnforcementService::updateZones(const std::vector<AreaZone>& zones)
{
    m_zones = zones;

    // Count enabled zones by type for logging
    int nfzCount = 0, ntzCount = 0;
    for (const auto& zone : m_zones) {
        if (zone.isEnabled) {
            if (zone.type == ZoneType::NoFire) nfzCount++;
            if (zone.type == ZoneType::NoTraverse) ntzCount++;
        }
    }

    qDebug() << "[ZoneEnforcementService] Zones updated:"
             << m_zones.size() << "total,"
             << nfzCount << "NFZ enabled,"
             << ntzCount << "NTZ enabled";

    emit zonesUpdated();
}

// ============================================================================
// NO-FIRE ZONE CHECKING
// ============================================================================

ZoneCheckResult ZoneEnforcementService::checkNoFireZone(float azimuth, float elevation,
                                                          float range) const
{
    ZoneCheckResult result;

    for (const auto& zone : m_zones) {
        if (!zone.isEnabled || zone.type != ZoneType::NoFire) {
            continue;
        }

        if (isPointInZone(zone, azimuth, elevation, range)) {
            result.isInZone = true;
            result.zoneId = zone.id;
            result.zoneName = zone.name;
            result.zoneType = ZoneType::NoFire;
            result.isOverridable = zone.isOverridable;

            // Emit signal if this is a new zone entry
            if (m_lastNFZId != zone.id) {
                m_lastNFZId = zone.id;
                // Note: Can't emit from const method, would need mutable signal or separate check
            }

            return result;
        }
    }

    // Not in any NFZ - check if we exited
    if (m_lastNFZId != -1) {
        m_lastNFZId = -1;
    }

    return result;
}

bool ZoneEnforcementService::isInNoFireZone(float azimuth, float elevation, float range) const
{
    return checkNoFireZone(azimuth, elevation, range).isInZone;
}

// ============================================================================
// NO-TRAVERSE ZONE CHECKING
// ============================================================================

ZoneCheckResult ZoneEnforcementService::checkNoTraverseZone(float azimuth, float elevation) const
{
    ZoneCheckResult result;

    for (const auto& zone : m_zones) {
        if (!zone.isEnabled || zone.type != ZoneType::NoTraverse) {
            continue;
        }

        if (isPointInZone(zone, azimuth, elevation)) {
            result.isInZone = true;
            result.zoneId = zone.id;
            result.zoneName = zone.name;
            result.zoneType = ZoneType::NoTraverse;
            result.isOverridable = zone.isOverridable;

            if (m_lastNTZId != zone.id) {
                m_lastNTZId = zone.id;
            }

            return result;
        }
    }

    if (m_lastNTZId != -1) {
        m_lastNTZId = -1;
    }

    return result;
}

bool ZoneEnforcementService::isInNoTraverseZone(float azimuth, float elevation) const
{
    return checkNoTraverseZone(azimuth, elevation).isInZone;
}

CollisionCheckResult ZoneEnforcementService::checkMovementCollision(float currentAz, float currentEl,
                                                                      float deltaAz, float deltaEl) const
{
    CollisionCheckResult result;
    result.allowedFraction = 1.0;

    double curAz = normalizeAzimuth(currentAz);
    double nextAz = normalizeAzimuth(curAz + deltaAz);
    double nextEl = currentEl + deltaEl;

    // If there's no movement, there's no collision
    if (std::abs(deltaAz) < 1e-6 && std::abs(deltaEl) < 1e-6) {
        return result;
    }

    for (const auto& zone : m_zones) {
        if (!zone.isEnabled || zone.type != ZoneType::NoTraverse) {
            continue;
        }

        // Check elevation bounds
        if (currentEl < zone.minElevation || currentEl > zone.maxElevation) {
            // If we're not in the elevation band, check if movement would take us there
            if (deltaEl != 0.0f) {
                bool wouldEnterElBand = (nextEl >= zone.minElevation && nextEl <= zone.maxElevation);
                if (!wouldEnterElBand) continue;
            } else {
                continue;
            }
        }

        // Check azimuth crossing
        bool curInside = isAzimuthInRange(static_cast<float>(curAz),
                                           zone.startAzimuth, zone.endAzimuth);
        bool nextInside = isAzimuthInRange(static_cast<float>(nextAz),
                                            zone.startAzimuth, zone.endAzimuth);

        // Collision: moving from outside to inside
        if (!curInside && nextInside) {
            result.wouldCollide = true;
            result.zoneId = zone.id;
            result.zoneName = zone.name;

            // Calculate fraction allowed
            double fracStart = getCollisionFraction(curAz, zone.startAzimuth, deltaAz, true);
            double fracEnd = getCollisionFraction(curAz, zone.endAzimuth, deltaAz, true);

            // Take the minimum positive fraction
            double minFrac = 1.0;
            if (fracStart >= 0 && fracStart < minFrac) minFrac = fracStart;
            if (fracEnd >= 0 && fracEnd < minFrac) minFrac = fracEnd;

            if (minFrac < result.allowedFraction) {
                result.allowedFraction = std::max(0.0, minFrac - ZONE_BOUNDARY_EPS);
                result.collisionAzimuth = normalizeAzimuth(curAz + deltaAz * result.allowedFraction);
                result.collisionElevation = currentEl + deltaEl * result.allowedFraction;
            }
        }
    }

    return result;
}

bool ZoneEnforcementService::wouldCrossIntoNTZ(float currentAz, float currentEl, float deltaAz) const
{
    return checkMovementCollision(currentAz, currentEl, deltaAz, 0.0f).wouldCollide;
}

// ============================================================================
// COMBINED ZONE CHECKING
// ============================================================================

ZoneCheckResult ZoneEnforcementService::checkAllZones(float azimuth, float elevation,
                                                        float range) const
{
    // Check No-Fire first (more restrictive for firing)
    ZoneCheckResult nfzResult = checkNoFireZone(azimuth, elevation, range);
    if (nfzResult.isInZone) {
        return nfzResult;
    }

    // Then check No-Traverse
    ZoneCheckResult ntzResult = checkNoTraverseZone(azimuth, elevation);
    return ntzResult;
}

std::vector<int> ZoneEnforcementService::getZonesContainingPoint(float azimuth, float elevation) const
{
    std::vector<int> result;

    for (const auto& zone : m_zones) {
        if (!zone.isEnabled) continue;

        if (isPointInZone(zone, azimuth, elevation)) {
            result.push_back(zone.id);
        }
    }

    return result;
}

// ============================================================================
// UTILITY FUNCTIONS (static)
// ============================================================================

double ZoneEnforcementService::normalizeAzimuth(double azimuth)
{
    double result = std::fmod(azimuth, 360.0);
    if (result < 0) {
        result += 360.0;
    }
    return result;
}

bool ZoneEnforcementService::isAzimuthInRange(float azimuth, float startAz, float endAz)
{
    // Normalize all values to 0-360
    float targetAz = static_cast<float>(normalizeAzimuth(azimuth));
    float start = static_cast<float>(normalizeAzimuth(startAz));
    float end = static_cast<float>(normalizeAzimuth(endAz));

    if (start <= end) {
        // Normal case, e.g., 30 to 60
        return targetAz >= start && targetAz <= end;
    } else {
        // Wraps around 360, e.g., 350 to 10
        return targetAz >= start || targetAz <= end;
    }
}

double ZoneEnforcementService::shortestSignedDelta(double from, double to)
{
    double fromNorm = normalizeAzimuth(from);
    double toNorm = normalizeAzimuth(to);

    double delta = toNorm - fromNorm;

    if (delta > 180.0) {
        delta -= 360.0;
    }
    if (delta <= -180.0) {
        delta += 360.0;
    }

    return delta;
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

bool ZoneEnforcementService::isPointInZone(const AreaZone& zone, float azimuth,
                                            float elevation, float range) const
{
    // Check azimuth
    bool azMatch = isAzimuthInRange(azimuth, zone.startAzimuth, zone.endAzimuth);
    if (!azMatch) return false;

    // Check elevation
    bool elMatch = (elevation >= zone.minElevation && elevation <= zone.maxElevation);
    if (!elMatch) return false;

    // Check range (if specified)
    if (range >= 0.0f && (zone.minRange > 0 || zone.maxRange > 0)) {
        bool rangeMatch = (range >= zone.minRange &&
                          (zone.maxRange == 0 || range <= zone.maxRange));
        if (!rangeMatch) return false;
    }

    return true;
}

double ZoneEnforcementService::getCollisionFraction(double current, double boundary,
                                                     double delta, bool isAzimuth)
{
    if (std::abs(delta) < 1e-6) {
        return 2.0;  // No movement, no collision
    }

    double distToWall;
    if (isAzimuth) {
        distToWall = shortestSignedDelta(current, boundary);
    } else {
        distToWall = boundary - current;
    }

    double t = distToWall / delta;

    // If t < 0, we're moving away from the wall
    if (t < 0.0) {
        return 2.0;
    }

    // Apply epsilon to prevent exactly touching the boundary
    double effectiveDist = std::abs(distToWall) - ZONE_BOUNDARY_EPS;
    if (effectiveDist < 0) {
        effectiveDist = 0;
    }

    return effectiveDist / std::abs(delta);
}
