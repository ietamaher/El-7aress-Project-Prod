#ifndef COLORMENUCONTROLLER_H
#define COLORMENUCONTROLLER_H

// ============================================================================
// Qt Framework
// ============================================================================
#include <QObject>
#include <QColor>

// ============================================================================
// Project
// ============================================================================
#include "models/menuviewmodel.h"
#include "models/domain/systemstatedata.h"

// ============================================================================
// Forward Declarations
// ============================================================================
class OsdViewModel;
class SystemStateModel;

/**
 * @brief OSD color style selection menu controller
 *
 * This class manages the color personalization menu:
 * - Displays available color styles (Green, Red, White)
 * - Provides live preview as user navigates
 * - Applies selected color or restores original on cancel
 */
class ColorMenuController : public QObject {
    Q_OBJECT

public:
    // ============================================================================
    // Constructor
    // ============================================================================
    explicit ColorMenuController(QObject* parent = nullptr);

    // ============================================================================
    // Initialization
    // ============================================================================
    void initialize();
    void setViewModel(MenuViewModel* viewModel);
    void setOsdViewModel(OsdViewModel* osdViewModel);
    void setStateModel(SystemStateModel* stateModel);

public slots:
    // ============================================================================
    // UI Control
    // ============================================================================
    void show();
    void hide();

    // ============================================================================
    // Button Handlers
    // ============================================================================
    void onUpButtonPressed();
    void onDownButtonPressed();
    void onSelectButtonPressed();
    void onBackButtonPressed();

signals:
    // ============================================================================
    // Navigation Signals
    // ============================================================================
    void menuFinished();
    void returnToMainMenu();

private slots:
    // ============================================================================
    // Menu Handlers
    // ============================================================================
    void handleMenuOptionSelected(const QString& option);
    void handleCurrentItemChanged(int index);
    void onColorStyleChanged(const QColor& color);

private:
    // ============================================================================
    // Conversion Helpers
    // ============================================================================
    QStringList buildColorOptions() const;
    QString colorStyleToString(ColorStyle style) const;
    ColorStyle stringToColorStyle(const QString& str) const;
    QColor colorStyleToQColor(ColorStyle style) const;

    // ============================================================================
    // Dependencies
    // ============================================================================
    MenuViewModel* m_viewModel = nullptr;
    OsdViewModel* m_osdViewModel = nullptr;
    SystemStateModel* m_stateModel = nullptr;

    // ============================================================================
    // State
    // ============================================================================
    ColorStyle m_originalColorStyle = ColorStyle::Green;
};

#endif  // COLORMENUCONTROLLER_H