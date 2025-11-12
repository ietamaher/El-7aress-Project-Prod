#include "controllers/colormenucontroller.h"
#include "models/osdviewmodel.h"
#include "models/domain/systemstatemodel.h"
#include <QDebug>

ColorMenuController::ColorMenuController(QObject *parent)
    : QObject(parent)
    , m_viewModel(nullptr)
    , m_osdViewModel(nullptr)
    , m_originalColorStyle(ColorStyle::Green)
{
}

void ColorMenuController::initialize()
{
    // Get the COLOR menu's ViewModel specifically by name
    /*m_viewModel = ServiceManager::instance()->get<MenuViewModel>(QString("ColorMenuViewModel"));
    m_osdViewModel = ServiceManager::instance()->get<OsdViewModel>();
    m_stateModel = ServiceManager::instance()->get<SystemStateModel>();*/

    qDebug() << "ColorMenuController::initialize()";
    qDebug() << "  m_viewModel:" << m_viewModel;
    qDebug() << "  m_osdViewModel:" << m_osdViewModel;
    qDebug() << "  m_stateModel:" << m_stateModel;  // ✅ CHECK THIS

    Q_ASSERT(m_viewModel);
    Q_ASSERT(m_osdViewModel);
    Q_ASSERT(m_stateModel);

    connect(m_viewModel, &MenuViewModel::optionSelected,
            this, &ColorMenuController::handleMenuOptionSelected);

    // Connect to currentIndexChanged to update preview as user navigates
    // Use Qt::QueuedConnection to prevent re-entrant calls during hardware button processing
    connect(m_viewModel, &MenuViewModel::currentIndexChanged,
            this, [this]() {
                int currentIndex = m_viewModel->currentIndex();
                handleCurrentItemChanged(currentIndex);
            }, Qt::QueuedConnection);

    connect(m_stateModel, &SystemStateModel::colorStyleChanged,
            this, &ColorMenuController::onColorStyleChanged);

    // Set initial color
    const auto& data = m_stateModel->data();
    m_viewModel->setAccentColor(data.colorStyle);
}

QStringList ColorMenuController::buildColorOptions() const
{
    QStringList options;

    for (int i = 0; i < static_cast<int>(ColorStyle::COUNT); ++i) {
        options << colorStyleToString(static_cast<ColorStyle>(i));
    }

    options << "Return ...";
    return options;
}

QString ColorMenuController::colorStyleToString(ColorStyle style) const
{
    switch(style) {
    case ColorStyle::Green: return "Green";
    case ColorStyle::Red: return "Red";
    case ColorStyle::White: return "White";
    //case ColorStyle::Yellow: return "Yellow";
    //case ColorStyle::Cyan: return "Cyan";
    default: return "Green";
    }
}

ColorStyle ColorMenuController::stringToColorStyle(const QString& str) const
{
    if (str == "Green") return ColorStyle::Green;
    if (str == "Red") return ColorStyle::Red;
    if (str == "White") return ColorStyle::White;
    //if (str == "Yellow") return ColorStyle::Yellow;
    //if (str == "Cyan") return ColorStyle::Cyan;
    return ColorStyle::Green;
}

QColor ColorMenuController::colorStyleToQColor(ColorStyle style) const
{
    switch(style) {
    case ColorStyle::Green: return QColor("#04db85"); //   #00FF99");
    case ColorStyle::Red: return QColor("#800000");
    case ColorStyle::White: return QColor("#FFFFFF");
    //case ColorStyle::Yellow: return QColor("#FFFF00");
    //case ColorStyle::Cyan: return QColor("#00FFFF");
    default: return QColor("#00FF99");
    }
}

void ColorMenuController::show()
{
    // ✅ Save current color for potential restore
    const auto& data = m_stateModel->data();

    // Convert QColor to ColorStyle enum
    QColor currentColor = data.colorStyle;
    if (currentColor == QColor("#00FF99") || currentColor == QColor(70, 226, 165)) {
        m_originalColorStyle = ColorStyle::Green;
    } else if (currentColor == QColor("#FF0000") || currentColor == QColor(255, 0, 0)) {
        m_originalColorStyle = ColorStyle::Red;
    } else if (currentColor == Qt::white) {
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
        // ✅ FIX: Apply initial preview to match the selected option
        handleCurrentItemChanged(currentIndex);
    }
}

void ColorMenuController::hide()
{
    m_viewModel->hideMenu();
}

void ColorMenuController::onUpButtonPressed()
{
    m_viewModel->moveSelectionUp();
}

void ColorMenuController::onDownButtonPressed()
{
    m_viewModel->moveSelectionDown();
}

void ColorMenuController::onSelectButtonPressed()
{
    m_viewModel->selectCurrentItem();
}

void ColorMenuController::onBackButtonPressed()
{
    // Restore original color on cancel
    // QColor originalColor = colorStyleToQColor(m_originalColorStyle);
    // m_osdViewModel->setOsdColor(originalColor);

    hide();
    emit returnToMainMenu();
    emit menuFinished();
}

void ColorMenuController::handleCurrentItemChanged(int index)
{
    qDebug() << "ColorMenuController::handleCurrentItemChanged() called with index:" << index;  // ✅ ADD

    QStringList options = buildColorOptions();
    if (index >= 0 && index < options.size() - 1) {
        QString optionText = options.at(index);
        ColorStyle previewStyle = stringToColorStyle(optionText);
        QColor previewColor = colorStyleToQColor(previewStyle);

        qDebug() << "Applying preview color:" << optionText << previewColor;  // ✅ ADD
        m_stateModel->setColorStyle(previewColor);

        qDebug() << "ColorMenuController: Previewing" << optionText << previewColor;
    }
}

void ColorMenuController::handleMenuOptionSelected(const QString& option)
{
    qDebug() << "ColorMenuController: Selected" << option;

    hide();

    if (option == "Return ...") {
        // Restore original color if cancelled
        QColor originalColor = colorStyleToQColor(m_originalColorStyle);
        m_stateModel->setColorStyle(originalColor);  // ✅ Restore
        emit returnToMainMenu();
    } else {
        // Apply selected color permanently
        ColorStyle selectedStyle = stringToColorStyle(option);
        QColor selectedColor = colorStyleToQColor(selectedStyle);

        // ✅ UPDATE: Apply via SystemStateModel (propagates to ALL)
        m_stateModel->setColorStyle(selectedColor);

        // ✅ OPTIONAL: Persist to file if you have a save method
        // m_stateModel->saveSettings();  // Implement if needed

        qDebug() << "ColorMenuController: Applied" << option << selectedColor;
        emit returnToMainMenu();
    }

    emit menuFinished();
}

void ColorMenuController::onColorStyleChanged(const QColor& color)
{
    qDebug() << "ColorMenuController: Color changed to" << color;
    if (m_viewModel) {
        m_viewModel->setAccentColor(color);
    }
}
void ColorMenuController::setViewModel(MenuViewModel* viewModel)
{
    m_viewModel = viewModel;
}

void ColorMenuController::setOsdViewModel(OsdViewModel* osdViewModel)
{
    m_osdViewModel = osdViewModel;
}

void ColorMenuController::setStateModel(SystemStateModel* stateModel)
{
    m_stateModel = stateModel;
}
