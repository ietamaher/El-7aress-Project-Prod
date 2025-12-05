#ifndef RETICLEMENUCONTROLLER_H
#define RETICLEMENUCONTROLLER_H

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
 * @brief Reticle style selection menu controller
 *
 * This class manages the reticle personalization menu:
 * - Displays available reticle styles
 * - Provides live preview as user navigates
 * - Applies selected style or restores original on cancel
 */
class ReticleMenuController : public QObject
{
    Q_OBJECT

public:
    // ========================================================================
    // Constructor
    // ========================================================================
    explicit ReticleMenuController(QObject* parent = nullptr);

    // ========================================================================
    // Initialization
    // ========================================================================
    void initialize();
    void setViewModel(MenuViewModel* viewModel);
    void setOsdViewModel(OsdViewModel* osdViewModel);
    void setStateModel(SystemStateModel* stateModel);

public slots:
    // ========================================================================
    // UI Control
    // ========================================================================
    void show();
    void hide();

    // ========================================================================
    // Button Handlers
    // ========================================================================
    void onUpButtonPressed();
    void onDownButtonPressed();
    void onSelectButtonPressed();
    void onBackButtonPressed();

signals:
    // ========================================================================
    // Navigation Signals
    // ========================================================================
    void menuFinished();
    void returnToMainMenu();

private slots:
    // ========================================================================
    // Menu Handlers
    // ========================================================================
    void handleMenuOptionSelected(const QString& option);
    void handleCurrentItemChanged(int index);
    void onColorStyleChanged(const QColor& color);

private:
    // ========================================================================
    // Conversion Helpers
    // ========================================================================
    QStringList buildReticleOptions() const;
    QString reticleTypeToString(ReticleType type) const;
    ReticleType stringToReticleType(const QString& str) const;

    // ========================================================================
    // Dependencies
    // ========================================================================
    MenuViewModel* m_viewModel = nullptr;
    OsdViewModel* m_osdViewModel = nullptr;
    SystemStateModel* m_stateModel = nullptr;

    // ========================================================================
    // State
    // ========================================================================
    ReticleType m_originalReticleType = ReticleType::BoxCrosshair;
};

#endif // RETICLEMENUCONTROLLER_H