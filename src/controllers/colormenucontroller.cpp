#include "controllers/colormenucontroller.h"

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

ColorMenuController::ColorMenuController(QObject* parent) : QObject(parent) {}

// ============================================================================
// INITIALIZATION
// ============================================================================

void ColorMenuController::initialize() {
    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_osdViewModel);
    Q_ASSERT(m_stateModel);

    connect(m_viewModel, &MenuViewModel::optionSelected, this,
            &ColorMenuController::handleMenuOptionSelected);

    // Connect to currentIndexChanged to update preview as user navigates
    // Use Qt::QueuedConnection to prevent re-entrant calls during hardware button processing
    connect(
        m_viewModel, &MenuViewModel::currentIndexChanged, this,
        [this]() { handleCurrentItemChanged(m_viewModel->currentIndex()); }, Qt::QueuedConnection);

    connect(m_stateModel, &SystemStateModel::colorStyleChanged, this,
            &ColorMenuController::onColorStyleChanged);

    // Set initial color
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);
}

void ColorMenuController::setViewModel(MenuViewModel* viewModel) {
    m_viewModel = viewModel;
}

void ColorMenuController::setOsdViewModel(OsdViewModel* osdViewModel) {
    m_osdViewModel = osdViewModel;
}

void ColorMenuController::setStateModel(SystemStateModel* stateModel) {
    m_stateModel = stateModel;
}

// ============================================================================
// CONVERSION HELPERS
// ============================================================================

QStringList ColorMenuController::buildColorOptions() const {
    QStringList options;

    for (int i = 0; i < static_cast<int>(ColorStyle::COUNT); ++i) {
        options << colorStyleToString(static_cast<ColorStyle>(i));
    }

    options << "Return ...";
    return options;
}

QString ColorMenuController::colorStyleToString(ColorStyle style) const {
    switch (style) {
    case ColorStyle::Green:
        return "Green";
    case ColorStyle::Red:
        return "Red";
    case ColorStyle::White:
        return "White";
    default:
        return "Green";
    }
}

ColorStyle ColorMenuController::stringToColorStyle(const QString& str) const {
    if (str == "Green")
        return ColorStyle::Green;
    if (str == "Red")
        return ColorStyle::Red;
    if (str == "White")
        return ColorStyle::White;
    return ColorStyle::Green;
}

QColor ColorMenuController::colorStyleToQColor(ColorStyle style) const {
    switch (style) {
    case ColorStyle::Green:
        return QColor("#04db85");
    case ColorStyle::Red:
        return QColor("#910000");
    case ColorStyle::White:
        return QColor("#FFFFFF");
    default:
        return QColor("#04db85");
    }
}

// ============================================================================
// UI CONTROL
// ============================================================================

void ColorMenuController::show() {
    // Save current color for potential restore
    const auto& data = m_stateModel->data();

    // Convert QColor to ColorStyle enum
    QColor currentColor = data.colorStyle;
    if (currentColor == QColor("#00FF99") || currentColor == QColor(70, 226, 165) ||
        currentColor == QColor("#04db85")) {
        m_originalColorStyle = ColorStyle::Green;
    } else if (currentColor == QColor("#FF0000") || currentColor == QColor(255, 0, 0) ||
               currentColor == QColor("#910000")) {
        m_originalColorStyle = ColorStyle::Red;
    } else if (currentColor == Qt::white || currentColor == QColor("#FFFFFF")) {
        m_originalColorStyle = ColorStyle::White;
    } else {
        m_originalColorStyle = ColorStyle::Green;  // Default
    }

    QStringList options = buildColorOptions();
    m_viewModel->showMenu("Personalize Colors", "Select OSD Color", options);

    // Set current selection to match current color
    int currentIndex = static_cast<int>(m_originalColorStyle);
    if (currentIndex >= 0 && currentIndex < options.size()) {
        m_viewModel->setCurrentIndex(currentIndex);
        handleCurrentItemChanged(currentIndex);
    }
}

void ColorMenuController::hide() {
    m_viewModel->hideMenu();
}

// ============================================================================
// BUTTON HANDLERS
// ============================================================================

void ColorMenuController::onUpButtonPressed() {
    m_viewModel->moveSelectionUp();
}

void ColorMenuController::onDownButtonPressed() {
    m_viewModel->moveSelectionDown();
}

void ColorMenuController::onSelectButtonPressed() {
    m_viewModel->selectCurrentItem();
}

void ColorMenuController::onBackButtonPressed() {
    hide();
    emit returnToMainMenu();
    emit menuFinished();
}

// ============================================================================
// MENU HANDLERS
// ============================================================================

void ColorMenuController::handleCurrentItemChanged(int index) {
    QStringList options = buildColorOptions();

    // Exclude "Return ..." option from preview
    if (index >= 0 && index < options.size() - 1) {
        QString optionText = options.at(index);
        ColorStyle previewStyle = stringToColorStyle(optionText);
        QColor previewColor = colorStyleToQColor(previewStyle);

        m_stateModel->setColorStyle(previewColor);

        qDebug() << "[ColorMenuController] Previewing" << optionText << previewColor;
    }
}

void ColorMenuController::handleMenuOptionSelected(const QString& option) {
    qDebug() << "[ColorMenuController] Selected" << option;

    hide();

    if (option == "Return ...") {
        // Restore original color if cancelled
        QColor originalColor = colorStyleToQColor(m_originalColorStyle);
        m_stateModel->setColorStyle(originalColor);
        emit returnToMainMenu();
    } else {
        // Apply selected color permanently
        ColorStyle selectedStyle = stringToColorStyle(option);
        QColor selectedColor = colorStyleToQColor(selectedStyle);

        m_stateModel->setColorStyle(selectedColor);

        qDebug() << "[ColorMenuController] Applied" << option << selectedColor;
        emit returnToMainMenu();
    }

    emit menuFinished();
}

// ============================================================================
// COLOR STYLE HANDLER
// ============================================================================

void ColorMenuController::onColorStyleChanged(const QColor& color) {
    qDebug() << "[ColorMenuController] Color changed to" << color;

    if (m_viewModel) {
        m_viewModel->setAccentColor(color);
    }
}