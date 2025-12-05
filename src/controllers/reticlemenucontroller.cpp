#include "controllers/reticlemenucontroller.h"

// ============================================================================
// Project
// ============================================================================
#include "models/osdviewmodel.h"
#include "models/domain/systemstatemodel.h"

// ============================================================================
// Qt Framework
// ============================================================================
#include <QDebug>

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ReticleMenuController::ReticleMenuController(QObject* parent)
    : QObject(parent)
{
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void ReticleMenuController::initialize()
{
    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_osdViewModel);
    Q_ASSERT(m_stateModel);

    connect(m_viewModel, &MenuViewModel::optionSelected,
            this, &ReticleMenuController::handleMenuOptionSelected);

    // Connect to currentIndexChanged to update preview as user navigates
    // Use Qt::QueuedConnection to prevent re-entrant calls during hardware button processing
    connect(m_viewModel, &MenuViewModel::currentIndexChanged,
            this, [this]() {
                handleCurrentItemChanged(m_viewModel->currentIndex());
            }, Qt::QueuedConnection);

    connect(m_stateModel, &SystemStateModel::colorStyleChanged,
            this, &ReticleMenuController::onColorStyleChanged);

    // Set initial color
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);
}

void ReticleMenuController::setViewModel(MenuViewModel* viewModel)
{
    m_viewModel = viewModel;
}

void ReticleMenuController::setOsdViewModel(OsdViewModel* osdViewModel)
{
    m_osdViewModel = osdViewModel;
}

void ReticleMenuController::setStateModel(SystemStateModel* stateModel)
{
    m_stateModel = stateModel;
}

// ============================================================================
// CONVERSION HELPERS
// ============================================================================

QStringList ReticleMenuController::buildReticleOptions() const
{
    QStringList options;

    for (int i = 0; i < static_cast<int>(ReticleType::COUNT); ++i) {
        options << reticleTypeToString(static_cast<ReticleType>(i));
    }

    options << "Return ...";
    return options;
}

QString ReticleMenuController::reticleTypeToString(ReticleType type) const
{
    switch (type) {
    case ReticleType::BoxCrosshair:    return "Box Crosshair";
    case ReticleType::BracketsReticle: return "Brackets Reticle";
    case ReticleType::DuplexCrosshair: return "Duplex Crosshair";
    case ReticleType::FineCrosshair:   return "Fine Crosshair";
    case ReticleType::ChevronReticle:  return "Chevron Reticle";
    default:                           return "Unknown";
    }
}

ReticleType ReticleMenuController::stringToReticleType(const QString& str) const
{
    if (str == "Box Crosshair")    return ReticleType::BoxCrosshair;
    if (str == "Brackets Reticle") return ReticleType::BracketsReticle;
    if (str == "Duplex Crosshair") return ReticleType::DuplexCrosshair;
    if (str == "Fine Crosshair")   return ReticleType::FineCrosshair;
    if (str == "Chevron Reticle")  return ReticleType::ChevronReticle;
    return ReticleType::BoxCrosshair;
}

// ============================================================================
// UI CONTROL
// ============================================================================

void ReticleMenuController::show()
{
    // Save current reticle type from system state
    const auto& data = m_stateModel->data();
    m_originalReticleType = data.reticleType;

    QStringList options = buildReticleOptions();
    m_viewModel->showMenu("Personalize Reticle", "Select Reticle Style", options);

    // Set current selection to match current reticle
    int currentIndex = static_cast<int>(m_originalReticleType);
    if (currentIndex >= 0 && currentIndex < options.size()) {
        m_viewModel->setCurrentIndex(currentIndex);
        handleCurrentItemChanged(currentIndex);
    }
}

void ReticleMenuController::hide()
{
    m_viewModel->hideMenu();
}

// ============================================================================
// BUTTON HANDLERS
// ============================================================================

void ReticleMenuController::onUpButtonPressed()
{
    m_viewModel->moveSelectionUp();
}

void ReticleMenuController::onDownButtonPressed()
{
    m_viewModel->moveSelectionDown();
}

void ReticleMenuController::onSelectButtonPressed()
{
    m_viewModel->selectCurrentItem();
}

void ReticleMenuController::onBackButtonPressed()
{
    hide();
    emit returnToMainMenu();
    emit menuFinished();
}

// ============================================================================
// MENU HANDLERS
// ============================================================================

void ReticleMenuController::handleCurrentItemChanged(int index)
{
    QStringList options = buildReticleOptions();

    // Exclude "Return ..." option from preview
    if (index >= 0 && index < options.size() - 1) {
        QString optionText = options.at(index);
        ReticleType previewType = stringToReticleType(optionText);

        m_stateModel->setReticleStyle(previewType);

        qDebug() << "[ReticleMenuController] Previewing" << optionText;
    }
}

void ReticleMenuController::handleMenuOptionSelected(const QString& option)
{
    qDebug() << "[ReticleMenuController] Selected" << option;

    hide();

    if (option == "Return ...") {
        // Restore original
        m_stateModel->setReticleStyle(m_originalReticleType);
        emit returnToMainMenu();
    } else {
        // Apply the selected reticle permanently
        ReticleType selectedType = stringToReticleType(option);
        m_stateModel->setReticleStyle(selectedType);

        qDebug() << "[ReticleMenuController] Applied" << option;
        emit returnToMainMenu();
    }

    emit menuFinished();
}

// ============================================================================
// COLOR STYLE HANDLER
// ============================================================================

void ReticleMenuController::onColorStyleChanged(const QColor& color)
{
    qDebug() << "[ReticleMenuController] Color changed to" << color;

    if (m_viewModel) {
        m_viewModel->setAccentColor(color);
    }
}