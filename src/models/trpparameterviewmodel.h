#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <QObject>

/**
 * @brief ViewModel for TRP parameter panel
 */
class TRPParameterViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(int locationPage READ locationPage NOTIFY locationPageChanged)
    Q_PROPERTY(int trpInPage READ trpInPage NOTIFY trpInPageChanged)
    Q_PROPERTY(float haltTime READ haltTime NOTIFY haltTimeChanged)
    Q_PROPERTY(int activeField READ activeField NOTIFY activeFieldChanged)
    Q_PROPERTY(bool isEditingValue READ isEditingValue NOTIFY isEditingValueChanged)

public:
    enum Field {
        LocationPage = 0,
        TrpInPage = 1,
        HaltTime = 2,
        ValidateButton = 3,
        CancelButton = 4,
        None = -1
    };
    Q_ENUM(Field)

    explicit TRPParameterViewModel(QObject* parent = nullptr);

    int locationPage() const {
        return m_locationPage;
    }
    int trpInPage() const {
        return m_trpInPage;
    }
    float haltTime() const {
        return m_haltTime;
    }
    int activeField() const {
        return m_activeField;
    }
    bool isEditingValue() const {
        return m_isEditingValue;
    }

public slots:
    void setLocationPage(int page);
    void setTrpInPage(int trp);
    void setHaltTime(float time);
    void setActiveField(int field);
    void setIsEditingValue(bool editing);

signals:
    void locationPageChanged();
    void trpInPageChanged();
    void haltTimeChanged();
    void activeFieldChanged();
    void isEditingValueChanged();

private:
    int m_locationPage = 1;
    int m_trpInPage = 1;
    float m_haltTime = 1.0f;
    int m_activeField = Field::LocationPage;
    bool m_isEditingValue = false;
};


