#pragma once

// ============================================================================
// INCLUDES
// ============================================================================
#include <QObject>
#include <QStringList>
#include <QColor>

/**
 * @brief Main ViewModel for Zone Definition overlay
 * Exposes UI state to QML for the zone definition workflow
 */
class ZoneDefinitionViewModel : public QObject {
    Q_OBJECT

    // Visibility flags
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(bool showMainMenu READ showMainMenu NOTIFY showMainMenuChanged)
    Q_PROPERTY(
        bool showZoneSelectionList READ showZoneSelectionList NOTIFY showZoneSelectionListChanged)
    Q_PROPERTY(bool showParameterPanel READ showParameterPanel NOTIFY showParameterPanelChanged)
    Q_PROPERTY(bool showMap READ showMap NOTIFY showMapChanged)
    Q_PROPERTY(bool showConfirmDialog READ showConfirmDialog NOTIFY showConfirmDialogChanged)

    // Current panel type (determines which parameter panel to show)
    Q_PROPERTY(int activePanelType READ activePanelType NOTIFY activePanelTypeChanged)

    // Text content
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString instruction READ instruction NOTIFY instructionChanged)

    // Menu/List content
    Q_PROPERTY(QStringList menuOptions READ menuOptions NOTIFY menuOptionsChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)

    // Gimbal position display
    Q_PROPERTY(float gimbalAz READ gimbalAz NOTIFY gimbalAzChanged)
    Q_PROPERTY(float gimbalEl READ gimbalEl NOTIFY gimbalElChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged)
public:
    enum PanelType { None = 0, AreaZone = 1, SectorScan = 2, TRP = 3 };
    Q_ENUM(PanelType)

    explicit ZoneDefinitionViewModel(QObject* parent = nullptr);

    // Getters
    bool visible() const {
        return m_visible;
    }
    bool showMainMenu() const {
        return m_showMainMenu;
    }
    bool showZoneSelectionList() const {
        return m_showZoneSelectionList;
    }
    bool showParameterPanel() const {
        return m_showParameterPanel;
    }
    bool showMap() const {
        return m_showMap;
    }
    bool showConfirmDialog() const {
        return m_showConfirmDialog;
    }
    int activePanelType() const {
        return m_activePanelType;
    }
    QString title() const {
        return m_title;
    }
    QString instruction() const {
        return m_instruction;
    }
    QStringList menuOptions() const {
        return m_menuOptions;
    }
    int currentIndex() const {
        return m_currentIndex;
    }
    float gimbalAz() const {
        return m_gimbalAz;
    }
    float gimbalEl() const {
        return m_gimbalEl;
    }
    QColor accentColor() const {
        return m_accentColor;
    }
public slots:
    // Setters
    void setVisible(bool visible);
    void setShowMainMenu(bool show);
    void setShowZoneSelectionList(bool show);
    void setShowParameterPanel(bool show);
    void setShowMap(bool show);
    void setShowConfirmDialog(bool show);
    void setActivePanelType(int type);
    void setTitle(const QString& title);
    void setInstruction(const QString& instruction);
    void setMenuOptions(const QStringList& options);
    void setCurrentIndex(int index);
    void setGimbalPosition(float az, float el);
    void setAccentColor(const QColor& color);
signals:
    void visibleChanged();
    void showMainMenuChanged();
    void showZoneSelectionListChanged();
    void showParameterPanelChanged();
    void showMapChanged();
    void showConfirmDialogChanged();
    void activePanelTypeChanged();
    void titleChanged();
    void instructionChanged();
    void menuOptionsChanged();
    void currentIndexChanged();
    void gimbalAzChanged();
    void gimbalElChanged();
    void accentColorChanged();

private:
    bool m_visible = false;
    bool m_showMainMenu = false;
    bool m_showZoneSelectionList = false;
    bool m_showParameterPanel = false;
    bool m_showMap = true;  // Map usually visible
    bool m_showConfirmDialog = false;
    int m_activePanelType = PanelType::None;
    QString m_title;
    QString m_instruction;
    QStringList m_menuOptions;
    int m_currentIndex = 0;
    float m_gimbalAz = 0.0f;
    float m_gimbalEl = 0.0f;
    QColor m_accentColor = QColor(70, 226, 165);  // Default green
};


