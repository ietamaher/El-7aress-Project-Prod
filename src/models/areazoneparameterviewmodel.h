#ifndef AREAZONEPARAMETERVIEWMODEL_H
#define AREAZONEPARAMETERVIEWMODEL_H

#include <QObject>

/**
 * @brief ViewModel for AreaZone parameter panel
 */
class AreaZoneParameterViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isEnabled READ isEnabled NOTIFY isEnabledChanged)
    Q_PROPERTY(bool isOverridable READ isOverridable NOTIFY isOverridableChanged)
    Q_PROPERTY(int activeField READ activeField NOTIFY activeFieldChanged)

public:
    enum Field { Enabled = 0, Overridable = 1, ValidateButton = 2, CancelButton = 3, None = -1 };
    Q_ENUM(Field)

    explicit AreaZoneParameterViewModel(QObject* parent = nullptr);

    bool isEnabled() const {
        return m_isEnabled;
    }
    bool isOverridable() const {
        return m_isOverridable;
    }
    int activeField() const {
        return m_activeField;
    }

public slots:
    void setIsEnabled(bool enabled);
    void setIsOverridable(bool overridable);
    void setActiveField(int field);

signals:
    void isEnabledChanged();
    void isOverridableChanged();
    void activeFieldChanged();

private:
    bool m_isEnabled = true;
    bool m_isOverridable = false;
    int m_activeField = Field::Enabled;
};

#endif  // AREAZONEPARAMETERVIEWMODEL_H
