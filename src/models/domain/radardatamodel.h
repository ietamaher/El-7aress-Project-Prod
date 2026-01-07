#ifndef RADARDATAMODEL_H
#define RADARDATAMODEL_H

#include <QObject>
#include "hardware/devices/radardevice.h"
#include <QVector>

class RadarDataModel : public QObject {
    Q_OBJECT

public:
    explicit RadarDataModel(QObject* parent = nullptr) : QObject(parent) {}

    QVector<RadarData> data() const {
        return m_data;
    }

signals:
    void dataChanged(const QVector<RadarData>& newData);

public slots:
    // Called whenever the device has new data
    void updateData(const QVector<RadarData>& newData) {
        if (newData != m_data) {
            m_data = newData;
            emit dataChanged(m_data);
        }
    }

private:
    QVector<RadarData> m_data;
};

#endif  // RADARDATAMODEL_H
