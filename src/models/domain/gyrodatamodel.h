#ifndef GYRODATAMODEL_H
#define GYRODATAMODEL_H

#include <QObject>
#include "hardware/devices/imudevice.h"

class GyroDataModel : public QObject {
    Q_OBJECT

public:
    explicit GyroDataModel(QObject* parent = nullptr) : QObject(parent) {}

    ImuData data() const {
        return m_data;
    }

signals:
    // Notifies observers that new data is available
    void dataChanged(const ImuData& newData);

public slots:
    // Called by the device class whenever updated lens data is available
    void updateData(const ImuData& newData) {
        if (newData != m_data) {
            m_data = newData;
            emit dataChanged(m_data);
        }
    }

private:
    ImuData m_data;
};
#endif  // GYRODATAMODEL_H
