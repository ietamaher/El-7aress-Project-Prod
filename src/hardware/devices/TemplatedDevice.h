#ifndef TEMPLATEDDEVICE_H
#define TEMPLATEDDEVICE_H

#include "hardware/interfaces/IDevice.h"
#include <QReadWriteLock>
#include <memory>

/**
 * @brief Template base class providing thread-safe data access for devices
 *
 * This template wraps device-specific data (TData) with thread-safe
 * read/write access using shared pointers and read-write locks.
 *
 * @tparam TData Device-specific data structure type
 */
template<typename TData>
class TemplatedDevice : public IDevice {
public:
    using DataPtr = std::shared_ptr<const TData>;

    explicit TemplatedDevice(QObject* parent = nullptr) : IDevice(parent) {
        m_data = std::make_shared<const TData>();
    }

    virtual ~TemplatedDevice() = default;

    /**
     * @brief Thread-safe read access to device data
     * @return Shared pointer to const device data
     */
    DataPtr data() const {
        QReadLocker locker(&m_dataLock);
        return m_data;
    }

protected:
    /**
     * @brief Thread-safe update of device data
     * @param newData New data to update
     */
    void updateData(DataPtr newData) {
        QWriteLocker locker(&m_dataLock);
        m_data = newData;
    }

private:
    mutable QReadWriteLock m_dataLock;
    DataPtr m_data;
};

#endif // TEMPLATEDDEVICE_H
