#pragma once

/**
 * @file ZoneEnforcementService.h
 * @brief Centralized zone enforcement service for safety zones
 *
 * This class provides a single authority for zone-related safety decisions:
 * - No-Fire Zone checking (firing prohibited)
 * - No-Traverse Zone checking (movement restricted)
 * - Zone boundary collision detection
 * - Zone status queries
 *
 * DESIGN PRINCIPLES:
 * - Pure computation functions where possible (for testability)
 * - Single source of truth for zone geometry logic
 * - Separated from SystemStateModel for reduced coupling
 * - Handles azimuth wrap-around (0-360°) correctly
 *
 * USAGE:
 * @code
 * ZoneEnforcementService zoneService;
 * zoneService.updateZones(stateData.areaZones);
 *
 * // Check if point is in restricted zone
 * if (zoneService.isInNoFireZone(gimbalAz, gimbalEl)) {
 *     // Block firing
 * }
 *
 * // Check if movement would cross into restricted zone
 * if (zoneService.wouldCrossIntoNTZ(currentAz, currentEl, deltaAz)) {
 *     // Block movement
 * }
 * @endcode
 *
 * @date 2025-12-31
 * @version 1.0
 */

// ============================================================================
// INCLUDES
// ============================================================================
#include <QObject>

#include <vector>

#include "models/domain/systemstatedata.h"

/**
 * @brief Result of a zone check operation
 */
struct ZoneCheckResult {
    bool isInZone = false;               ///< True if point is inside a zone
    int zoneId = -1;                     ///< ID of the zone (if in zone)
    QString zoneName;                    ///< Name of the zone (if in zone)
    ZoneType zoneType = ZoneType::None;  ///< Type of zone
    bool isOverridable = false;          ///< Whether the zone can be overridden
};

/**
 * @brief Result of a collision check operation
 */
struct CollisionCheckResult {
    bool wouldCollide = false;        ///< True if movement would cross into zone
    double allowedFraction = 1.0;     ///< Fraction of movement allowed (0.0-1.0)
    int zoneId = -1;                  ///< ID of the blocking zone
    QString zoneName;                 ///< Name of the blocking zone
    double collisionAzimuth = 0.0;    ///< Azimuth at collision point
    double collisionElevation = 0.0;  ///< Elevation at collision point
};

/**
 * @class ZoneEnforcementService
 * @brief Centralized service for all zone enforcement logic
 *
 * Extracted from SystemStateModel to:
 * - Improve testability (pure functions)
 * - Reduce SystemStateModel complexity
 * - Provide single authority for zone decisions
 * - Enable zone caching for performance
 */
class ZoneEnforcementService : public QObject {
    Q_OBJECT

public:
    // ============================================================================
    // CONSTANTS
    // ============================================================================

    /**
     * @brief Epsilon for zone boundary comparisons
     */
    static constexpr double ZONE_BOUNDARY_EPS = 0.01;

    // ============================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================================================

    explicit ZoneEnforcementService(QObject* parent = nullptr);
    ~ZoneEnforcementService() = default;

    // ============================================================================
    // ZONE CONFIGURATION
    // ============================================================================

    /**
     * @brief Update the zone list from system state
     * @param zones Vector of area zones to enforce
     */
    void updateZones(const std::vector<AreaZone>& zones);

    /**
     * @brief Get current zone list
     * @return Reference to current zone vector
     */
    const std::vector<AreaZone>& zones() const {
        return m_zones;
    }

    // ============================================================================
    // NO-FIRE ZONE CHECKING
    // ============================================================================

    /**
     * @brief Check if a point is in any No-Fire Zone
     * @param azimuth Azimuth angle in degrees (0-360)
     * @param elevation Elevation angle in degrees
     * @param range Optional range in meters (-1 to ignore)
     * @return ZoneCheckResult with zone details if in NFZ
     */
    ZoneCheckResult checkNoFireZone(float azimuth, float elevation, float range = -1.0f) const;

    /**
     * @brief Simple boolean check for No-Fire Zone
     * @param azimuth Azimuth angle in degrees
     * @param elevation Elevation angle in degrees
     * @param range Optional range in meters
     * @return true if point is in any enabled No-Fire Zone
     */
    bool isInNoFireZone(float azimuth, float elevation, float range = -1.0f) const;

    // ============================================================================
    // NO-TRAVERSE ZONE CHECKING
    // ============================================================================

    /**
     * @brief Check if a point is in any No-Traverse Zone
     * @param azimuth Azimuth angle in degrees (0-360)
     * @param elevation Elevation angle in degrees
     * @return ZoneCheckResult with zone details if in NTZ
     */
    ZoneCheckResult checkNoTraverseZone(float azimuth, float elevation) const;

    /**
     * @brief Simple boolean check for No-Traverse Zone
     * @param azimuth Azimuth angle in degrees
     * @param elevation Elevation angle in degrees
     * @return true if point is in any enabled No-Traverse Zone
     */
    bool isInNoTraverseZone(float azimuth, float elevation) const;

    /**
     * @brief Check if movement would cross into a No-Traverse Zone
     * @param currentAz Current azimuth in degrees
     * @param currentEl Current elevation in degrees
     * @param deltaAz Intended azimuth movement in degrees
     * @param deltaEl Intended elevation movement in degrees
     * @return CollisionCheckResult with collision details
     */
    CollisionCheckResult checkMovementCollision(float currentAz, float currentEl, float deltaAz,
                                                float deltaEl = 0.0f) const;

    /**
     * @brief Check if intended movement would enter a No-Traverse Zone
     * @param currentAz Current azimuth in degrees
     * @param currentEl Current elevation in degrees
     * @param deltaAz Intended azimuth movement in degrees
     * @return true if movement would cross into NTZ
     */
    bool wouldCrossIntoNTZ(float currentAz, float currentEl, float deltaAz) const;

    // ============================================================================
    // COMBINED ZONE CHECKING
    // ============================================================================

    /**
     * @brief Check if a point is in any restricted zone
     * @param azimuth Azimuth angle in degrees
     * @param elevation Elevation angle in degrees
     * @param range Optional range in meters
     * @return ZoneCheckResult for the most restrictive zone found
     */
    ZoneCheckResult checkAllZones(float azimuth, float elevation, float range = -1.0f) const;

    /**
     * @brief Get all zones containing a given point
     * @param azimuth Azimuth angle in degrees
     * @param elevation Elevation angle in degrees
     * @return Vector of zone IDs containing the point
     */
    std::vector<int> getZonesContainingPoint(float azimuth, float elevation) const;

    // ============================================================================
    // UTILITY FUNCTIONS (static for testability)
    // ============================================================================

    /**
     * @brief Normalize azimuth to 0-360 range
     * @param azimuth Input azimuth in degrees
     * @return Normalized azimuth (0-360)
     */
    static double normalizeAzimuth(double azimuth);

    /**
     * @brief Check if azimuth is within a range (handles wrap-around)
     * @param azimuth Target azimuth in degrees
     * @param startAz Start of range in degrees
     * @param endAz End of range in degrees
     * @return true if azimuth is within the range
     */
    static bool isAzimuthInRange(float azimuth, float startAz, float endAz);

    /**
     * @brief Calculate shortest signed angular delta
     * @param from Starting angle in degrees
     * @param to Ending angle in degrees
     * @return Shortest signed delta (-180 to 180)
     */
    static double shortestSignedDelta(double from, double to);

signals:
    /**
     * @brief Emitted when zone configuration changes
     */
    void zonesUpdated();

    /**
     * @brief Emitted when entering a restricted zone
     * @param zoneId ID of the zone entered
     * @param zoneType Type of zone
     */
    void enteredRestrictedZone(int zoneId, ZoneType zoneType);

    /**
     * @brief Emitted when exiting a restricted zone
     * @param zoneId ID of the zone exited
     */
    void exitedRestrictedZone(int zoneId);

private:
    // ============================================================================
    // INTERNAL HELPERS
    // ============================================================================

    /**
     * @brief Check if point is inside a specific zone
     * @param zone Zone to check
     * @param azimuth Azimuth in degrees
     * @param elevation Elevation in degrees
     * @param range Range in meters (-1 to ignore)
     * @return true if point is inside zone
     */
    bool isPointInZone(const AreaZone& zone, float azimuth, float elevation,
                       float range = -1.0f) const;

    /**
     * @brief Calculate fraction of movement allowed before hitting zone boundary
     * @param current Current position
     * @param boundary Zone boundary position
     * @param delta Movement delta
     * @param isAzimuth True if checking azimuth (handles wrap-around)
     * @return Fraction of movement allowed (0.0-1.0), >1.0 if no collision
     */
    static double getCollisionFraction(double current, double boundary, double delta,
                                       bool isAzimuth);

    // ============================================================================
    // ZONE STORAGE
    // ============================================================================
    std::vector<AreaZone> m_zones;

    // ============================================================================
    // CACHED STATE (for edge detection)
    // ============================================================================
    mutable int m_lastNFZId = -1;  ///< Last No-Fire Zone ID (for exit detection)
    mutable int m_lastNTZId = -1;  ///< Last No-Traverse Zone ID (for exit detection)
};

