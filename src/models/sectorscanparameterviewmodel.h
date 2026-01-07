#ifndef SECTORSCANPARAMETERVIEWMODEL_H
#define SECTORSCANPARAMETERVIEWMODEL_H

#include <QObject>

/**
 * @brief ViewModel for SectorScan parameter panel
 */
class SectorScanParameterViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isEnabled READ isEnabled NOTIFY isEnabledChanged)
    Q_PROPERTY(int scanSpeed READ scanSpeed NOTIFY scanSpeedChanged)
    Q_PROPERTY(int activeField READ activeField NOTIFY activeFieldChanged)
    Q_PROPERTY(bool isEditingValue READ isEditingValue NOTIFY isEditingValueChanged)

public:
    enum Field { Enabled = 0, ScanSpeed = 1, ValidateButton = 2, CancelButton = 3, None = -1 };
    Q_ENUM(Field)

    explicit SectorScanParameterViewModel(QObject* parent = nullptr);

    bool isEnabled() const {
        return m_isEnabled;
    }
    int scanSpeed() const {
        return m_scanSpeed;
    }
    int activeField() const {
        return m_activeField;
    }
    bool isEditingValue() const {
        return m_isEditingValue;
    }

public slots:
    void setIsEnabled(bool enabled);
    void setScanSpeed(int speed);
    void setActiveField(int field);
    void setIsEditingValue(bool editing);

signals:
    void isEnabledChanged();
    void scanSpeedChanged();
    void activeFieldChanged();
    void isEditingValueChanged();

private:
    bool m_isEnabled = true;
    int m_scanSpeed = 5;  // deg/s
    int m_activeField = Field::Enabled;
    bool m_isEditingValue = false;
};

#endif  // SECTORSCANPARAMETERVIEWMODEL_H
