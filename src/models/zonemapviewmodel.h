#ifndef ZONEMAPVIEWMODEL_H
#define ZONEMAPVIEWMODEL_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QPointF>
#include <QColor>

class SystemStateModel;

/**
 * @brief ViewModel for ZoneMapCanvas - provides zone data for rendering
 */
class ZoneMapViewModel : public QObject
{
    Q_OBJECT

    // Gimbal position
    Q_PROPERTY(float gimbalAz READ gimbalAz NOTIFY gimbalAzChanged)
    Q_PROPERTY(float gimbalEl READ gimbalEl NOTIFY gimbalElChanged)

    // Zone data for rendering (as QVariantList for QML)
    Q_PROPERTY(QVariantList areaZones READ areaZones NOTIFY areaZonesChanged)
    Q_PROPERTY(QVariantList sectorScans READ sectorScans NOTIFY sectorScansChanged)
    Q_PROPERTY(QVariantList trps READ trps NOTIFY trpsChanged)

    // WIP zone
    Q_PROPERTY(bool hasWipZone READ hasWipZone NOTIFY hasWipZoneChanged)
    Q_PROPERTY(QVariantMap wipZone READ wipZone NOTIFY wipZoneChanged)
    Q_PROPERTY(int wipZoneType READ wipZoneType NOTIFY wipZoneTypeChanged)
    Q_PROPERTY(bool isDefiningStart READ isDefiningStart NOTIFY isDefiningStartChanged)
    Q_PROPERTY(bool isDefiningEnd READ isDefiningEnd NOTIFY isDefiningEndChanged)

    // Highlighted zone
    Q_PROPERTY(int highlightedZoneId READ highlightedZoneId NOTIFY highlightedZoneIdChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged)


public:
    explicit ZoneMapViewModel(QObject *parent = nullptr);

    // Getters
    float gimbalAz() const { return m_gimbalAz; }
    float gimbalEl() const { return m_gimbalEl; }
    QVariantList areaZones() const { return m_areaZones; }
    QVariantList sectorScans() const { return m_sectorScans; }
    QVariantList trps() const { return m_trps; }
    bool hasWipZone() const { return m_hasWipZone; }
    QVariantMap wipZone() const { return m_wipZone; }
    int wipZoneType() const { return m_wipZoneType; }
    bool isDefiningStart() const { return m_isDefiningStart; }
    bool isDefiningEnd() const { return m_isDefiningEnd; }
    int highlightedZoneId() const { return m_highlightedZoneId; }
    QColor accentColor() const { return m_accentColor; }

public slots:
    void setGimbalPosition(float az, float el);
    void updateZones(SystemStateModel* model);
    void setWipZone(const QVariantMap& zone, int type, bool definingStart, bool definingEnd);
    void clearWipZone();
    void setHighlightedZone(int id);
    void setAccentColor(const QColor& color);

    // Utility functions for QML Canvas
    Q_INVOKABLE QPointF azElToPixel(float az, float el, float width, float height) const;
    Q_INVOKABLE float normalizeAzimuth(float az) const;

signals:
    void gimbalAzChanged();
    void gimbalElChanged();
    void areaZonesChanged();
    void sectorScansChanged();
    void trpsChanged();
    void hasWipZoneChanged();
    void wipZoneChanged();
    void wipZoneTypeChanged();
    void isDefiningStartChanged();
    void isDefiningEndChanged();
    void highlightedZoneIdChanged();
    void accentColorChanged();

private:
    QVariantList convertAreaZonesToVariant(SystemStateModel* model);
    QVariantList convertSectorScansToVariant(SystemStateModel* model);
    QVariantList convertTRPsToVariant(SystemStateModel* model);

    float m_gimbalAz = 0.0f;
    float m_gimbalEl = 0.0f;
    QVariantList m_areaZones;
    QVariantList m_sectorScans;
    QVariantList m_trps;
    bool m_hasWipZone = false;
    QVariantMap m_wipZone;
    int m_wipZoneType = 0; // 0=None, 1=AreaZone, 2=SectorScan, 3=TRP
    bool m_isDefiningStart = false;
    bool m_isDefiningEnd = false;
    int m_highlightedZoneId = -1;

    // Display constants
    // Gimbal operates between -15° and +46°, so display -20° to 60° for margin
    static constexpr float AZ_MIN = 0.0f;
    static constexpr float AZ_MAX = 360.0f;
    static constexpr float EL_MIN = -20.0f;
    static constexpr float EL_MAX = 60.0f;

    QColor m_accentColor = QColor(70, 226, 165); // Default green
};

#endif // ZONEMAPVIEWMODEL_H
