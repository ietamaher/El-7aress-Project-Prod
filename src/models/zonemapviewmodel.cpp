#include "zonemapviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include <QtMath>

ZoneMapViewModel::ZoneMapViewModel(QObject* parent) : QObject(parent) {}

void ZoneMapViewModel::setGimbalPosition(float az, float el) {
    bool changed = false;
    float normalizedAz = normalizeAzimuth(az);

    if (!qFuzzyCompare(m_gimbalAz, normalizedAz)) {
        m_gimbalAz = normalizedAz;
        changed = true;
        emit gimbalAzChanged();
    }
    if (!qFuzzyCompare(m_gimbalEl, el)) {
        m_gimbalEl = el;
        changed = true;
        emit gimbalElChanged();
    }
}

void ZoneMapViewModel::updateZones(SystemStateModel* model) {
    if (!model)
        return;

    QVariantList newAreaZones = convertAreaZonesToVariant(model);
    QVariantList newSectorScans = convertSectorScansToVariant(model);
    QVariantList newTRPs = convertTRPsToVariant(model);

    if (m_areaZones != newAreaZones) {
        m_areaZones = newAreaZones;
        emit areaZonesChanged();
    }
    if (m_sectorScans != newSectorScans) {
        m_sectorScans = newSectorScans;
        emit sectorScansChanged();
    }
    if (m_trps != newTRPs) {
        m_trps = newTRPs;
        emit trpsChanged();
    }
}

void ZoneMapViewModel::setWipZone(const QVariantMap& zone, int type, bool definingStart,
                                  bool definingEnd) {
    m_wipZone = zone;
    m_wipZoneType = type;
    m_isDefiningStart = definingStart;
    m_isDefiningEnd = definingEnd;
    m_hasWipZone = true;

    emit wipZoneChanged();
    emit wipZoneTypeChanged();
    emit isDefiningStartChanged();
    emit isDefiningEndChanged();
    emit hasWipZoneChanged();
}

void ZoneMapViewModel::clearWipZone() {
    if (m_hasWipZone) {
        m_hasWipZone = false;
        m_wipZone.clear();
        m_wipZoneType = 0;
        m_isDefiningStart = false;
        m_isDefiningEnd = false;
        emit hasWipZoneChanged();
    }
}

void ZoneMapViewModel::setHighlightedZone(int id) {
    if (m_highlightedZoneId != id) {
        m_highlightedZoneId = id;
        emit highlightedZoneIdChanged();
    }
}

QPointF ZoneMapViewModel::azElToPixel(float az, float el, float width, float height) const {
    float normalizedAz = normalizeAzimuth(az);

    float azRange = AZ_MAX - AZ_MIN;
    float elRange = EL_MAX - EL_MIN;

    float x = (normalizedAz - AZ_MIN) / azRange * width;
    float y = height - ((el - EL_MIN) / elRange * height);

    return QPointF(x, y);
}

float ZoneMapViewModel::normalizeAzimuth(float az) const {
    float normalized = fmod(az, 360.0f);
    if (normalized < 0) {
        normalized += 360.0f;
    }
    return normalized;
}

QVariantList ZoneMapViewModel::convertAreaZonesToVariant(SystemStateModel* model) {
    QVariantList result;
    const auto& zones = model->getAreaZones();

    for (const auto& zone : zones) {
        QVariantMap zoneMap;
        zoneMap["id"] = zone.id;
        zoneMap["type"] = static_cast<int>(zone.type);
        zoneMap["isEnabled"] = zone.isEnabled;
        zoneMap["isOverridable"] = zone.isOverridable;
        zoneMap["startAzimuth"] = zone.startAzimuth;
        zoneMap["endAzimuth"] = zone.endAzimuth;
        zoneMap["minElevation"] = zone.minElevation;
        zoneMap["maxElevation"] = zone.maxElevation;
        result.append(zoneMap);
    }

    return result;
}

QVariantList ZoneMapViewModel::convertSectorScansToVariant(SystemStateModel* model) {
    QVariantList result;
    const auto& zones = model->getSectorScanZones();

    for (const auto& zone : zones) {
        QVariantMap zoneMap;
        zoneMap["id"] = zone.id;
        zoneMap["isEnabled"] = zone.isEnabled;
        zoneMap["az1"] = zone.az1;
        zoneMap["el1"] = zone.el1;
        zoneMap["az2"] = zone.az2;
        zoneMap["el2"] = zone.el2;
        result.append(zoneMap);
    }

    return result;
}

QVariantList ZoneMapViewModel::convertTRPsToVariant(SystemStateModel* model) {
    QVariantList result;
    const auto& zones = model->getTargetReferencePoints();

    for (const auto& zone : zones) {
        QVariantMap zoneMap;
        zoneMap["id"] = zone.id;
        zoneMap["azimuth"] = zone.azimuth;
        zoneMap["elevation"] = zone.elevation;
        zoneMap["locationPage"] = zone.locationPage;
        zoneMap["trpInPage"] = zone.trpInPage;
        result.append(zoneMap);
    }

    return result;
}

void ZoneMapViewModel::setAccentColor(const QColor& color) {
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
    }
}