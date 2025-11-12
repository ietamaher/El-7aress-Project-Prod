#include "controllers/reticlemenucontroller.h"
#include "models/osdviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>

ReticleMenuController::ReticleMenuController(QObject *parent)
    : QObject(parent)
    , m_viewModel(nullptr)
    , m_osdViewModel(nullptr)
    , m_originalReticleType(ReticleType::CircleDotReticle)
{
}

void ReticleMenuController::initialize()
{
    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_osdViewModel);
    Q_ASSERT(m_stateModel);

    connect(m_viewModel, &MenuViewModel::optionSelected,
            this, &ReticleMenuController::handleMenuOptionSelected);

    // Connect to currentIndexChanged to update preview as user navigates
    connect(m_viewModel, &MenuViewModel::currentIndexChanged,
            this, [this]() {
                int currentIndex = m_viewModel->currentIndex();
                handleCurrentItemChanged(currentIndex);
            });

    // Connect to color changes
    connect(m_stateModel, &SystemStateModel::colorStyleChanged,
            this, &ReticleMenuController::onColorStyleChanged);

    // Set initial color
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);

}

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
    switch(type) {
    case ReticleType::CircleDotReticle: return "Circle-Dot Reticle";
    case ReticleType::BoxCrosshair: return "Box Crosshair";
    case ReticleType::TacticalCrosshair: return "Tactical Crosshair";
    case ReticleType::CCIPFireControl: return "CCIP Fire Control";
    case ReticleType::MilDot: return "Mil-Dot Ranging";
    default: return "Unknown";
    }
}

ReticleType ReticleMenuController::stringToReticleType(const QString& str) const
{
    if (str == "Circle-Dot Reticle") return ReticleType::CircleDotReticle;
    if (str == "Box Crosshair") return ReticleType::BoxCrosshair;
    if (str == "Tactical Crosshair") return ReticleType::TacticalCrosshair;
    if (str == "CCIP Fire Control") return ReticleType::CCIPFireControl;
    if (str == "Mil-Dot Ranging") return ReticleType::MilDot;
    return ReticleType::CircleDotReticle;
}

void ReticleMenuController::show()
{
    // Save current reticle type from system state
    const auto& data = m_stateModel->data();
    m_originalReticleType = data.reticleType;

    QStringList options = buildReticleOptions();
    m_viewModel->showMenu("Personalize Reticle", "Select Reticle Style", options);

    // ✅ FIX: Set current selection to match current reticle
    int currentIndex = static_cast<int>(m_originalReticleType);
    if (currentIndex >= 0 && currentIndex < options.size()) {
        m_viewModel->setCurrentIndex(currentIndex);
        // ✅ FIX: Apply initial preview to match the selected option
        handleCurrentItemChanged(currentIndex);
    }
}

void ReticleMenuController::hide()
{
    m_viewModel->hideMenu();
}

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
    // Restore original reticle on cancel
    // m_osdViewModel->setReticleType(m_originalReticleType);

    hide();
    emit returnToMainMenu();
    emit menuFinished();
}

void ReticleMenuController::handleCurrentItemChanged(int index)
{
    // Get the option text at this index for preview
    QStringList options = buildReticleOptions();
    if (index >= 0 && index < options.size() - 1) { // Exclude "Return ..."
        QString optionText = options.at(index);
        ReticleType previewType = stringToReticleType(optionText);

        // Update OSD with preview
        m_stateModel->setReticleStyle(previewType);

        qDebug() << "ReticleMenuController: Previewing" << optionText;
    }
}

void ReticleMenuController::handleMenuOptionSelected(const QString& option)
{
    qDebug() << "ReticleMenuController: Selected" << option;

    hide();

    if (option == "Return ...") {
        // Restore original
        m_stateModel->setReticleStyle(m_originalReticleType);
        emit returnToMainMenu();
    } else {
        // Apply the selected reticle permanently
        ReticleType selectedType = stringToReticleType(option);
        m_stateModel->setReticleStyle(selectedType);
        //m_osdViewModel->saveReticleType(); // Persist to settings

        qDebug() << "ReticleMenuController: Applied" << option;
        emit returnToMainMenu();
    }

    emit menuFinished();
}

void ReticleMenuController::onColorStyleChanged(const QColor& color)
{
    qDebug() << "ReticleMenuController: Color changed to" << color;
    if (m_viewModel) {
        m_viewModel->setAccentColor(color);
    }
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
