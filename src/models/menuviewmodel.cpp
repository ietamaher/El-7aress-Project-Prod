#include "menuviewmodel.h"
#include <QDebug>

MenuViewModel::MenuViewModel(QObject *parent)
    : QObject(parent)
{
}

void MenuViewModel::showMenu(const QString& title, const QString& description, const QStringList& options)
{
    m_title = title;
    emit titleChanged();

    m_description = description;
    emit descriptionChanged();

    m_optionsModel.setStringList(options);

    m_currentIndex = findNextSelectable(-1, 1);
    emit currentIndexChanged();

    m_visible = true;
    emit visibleChanged();
}

void MenuViewModel::hideMenu()
{
    if (m_visible) {
        m_visible = false;
        emit visibleChanged();
    }
}

bool MenuViewModel::isSelectable(int index) const
{
    if (index < 0 || index >= m_optionsModel.rowCount()) {
        return false;
    }

    QString text = m_optionsModel.stringList().at(index);
    return !text.startsWith("---");
}

void MenuViewModel::setCurrentIndex(int index)
{
    if (index >= 0 && index < m_optionsModel.rowCount()) {
        if (isSelectable(index)) {
            m_currentIndex = index;
            emit currentIndexChanged();
        } else {
            // If not selectable, find nearest selectable
            int nearest = findNextSelectable(index, 1);
            if (nearest == -1) {
                nearest = findNextSelectable(index, -1);
            }
            if (nearest != -1) {
                m_currentIndex = nearest;
                emit currentIndexChanged();
            }
        }
    }
}

void MenuViewModel::moveSelectionUp()
{
    if (!m_visible) return;

    int nextIndex = findNextSelectableNoWrap(m_currentIndex, -1);
    if (nextIndex != -1 && nextIndex != m_currentIndex) {
        m_currentIndex = nextIndex;
        qDebug() << "MenuViewModel::moveSelectionUp() - Changed to:" << m_currentIndex;
        emit currentIndexChanged();
    }
}

void MenuViewModel::moveSelectionDown()
{
    if (!m_visible) return;

    int nextIndex = findNextSelectableNoWrap(m_currentIndex, 1);
    if (nextIndex != -1 && nextIndex != m_currentIndex) {
        m_currentIndex = nextIndex;
        qDebug() << "MenuViewModel::moveSelectionDown() - Changed to:" << m_currentIndex;
        emit currentIndexChanged();
    }
}

void MenuViewModel::selectCurrentItem()
{
    if (!m_visible || m_currentIndex < 0 || m_currentIndex >= m_optionsModel.rowCount())
        return;

    QString selectedOption = m_optionsModel.stringList().at(m_currentIndex);
    emit optionSelected(selectedOption);
}

int MenuViewModel::findNextSelectable(int start, int direction)
{
    if (m_optionsModel.rowCount() == 0) return -1;

    int current = start;
    int count = m_optionsModel.rowCount();

    for (int i = 0; i < count; ++i) {
        current += direction;

        // Wrap around
        if (current >= count) current = 0;
        if (current < 0) current = count - 1;

        if (isSelectable(current)) {
            return current;
        }
    }

    return -1;
}

int MenuViewModel::findNextSelectableNoWrap(int start, int direction)
{
    if (m_optionsModel.rowCount() == 0) return -1;

    int current = start;
    int count = m_optionsModel.rowCount();

    for (int i = 0; i < count; ++i) {
        current += direction;

        // Stop at boundaries - don't wrap around
        if (current >= count || current < 0) {
            return -1;  // Return -1 to indicate we've hit the boundary
        }

        if (isSelectable(current)) {
            return current;
        }
    }

    return -1;
}

void MenuViewModel::setAccentColor(const QColor& color) {
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
    }
}
