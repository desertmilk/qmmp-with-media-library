/***************************************************************************
 *   Copyright (C) 2020-2026 by Ilya Kotov                                 *
 *   forkotov02@ya.ru                                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 or (at your option)    *
 *   any later version.                                                     *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include <QSettings>
#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QContextMenuEvent>
#include <QIcon>
#include <QLabel>
#include <QHeaderView>
#include <QPalette>
#include <QFont>
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QItemSelectionModel>
#include <QAbstractItemView>
#include <QTableView>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <qmmp/qmmp.h>
#include <qmmpui/qmmpuiskin.h>
#include "librarymodel.h"
#include "librarysettingsdialog.h"
#include "ui_librarywidget.h"
#include "librarywidget.h"

// Exposes the protected QTableView::sizeHintForColumn() for column-space distribution.
class SummaryTableAccessor : public QTableView
{
public:
    static int columnSizeHint(const QTableView *view, int column)
    {
        return static_cast<const SummaryTableAccessor *>(view)->sizeHintForColumn(column);
    }
};
class LibrarySummarySortModel : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override
    {
        m_sortOrder = order;
        QSortFilterProxyModel::sort(column, Qt::AscendingOrder);
    }

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        const bool leftIsAll = left.siblingAtColumn(0).data(Qt::UserRole).toBool();
        const bool rightIsAll = right.siblingAtColumn(0).data(Qt::UserRole).toBool();
        if(leftIsAll != rightIsAll)
            return leftIsAll;

        if(left.column() == 1 || left.column() == 2)
        {
            const int result = left.data().toInt() - right.data().toInt();
            return m_sortOrder == Qt::AscendingOrder ? result < 0 : result > 0;
        }

        const int result = QString::localeAwareCompare(left.data().toString(), right.data().toString());
        return m_sortOrder == Qt::AscendingOrder ? result < 0 : result > 0;
    }

private:
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};

LibraryWidget::LibraryWidget(bool dialog, QWidget *parent) :
    QWidget(parent),
    m_ui(new Ui::LibraryWidget)
{
    if(dialog)
        setObjectName(u"MediaLibrary"_s);
    m_ui->setupUi(this);
    m_ui->filterLineEdit->addAction(QIcon::fromTheme(u"edit-find"_s), QLineEdit::LeadingPosition);
    m_model = new LibraryModel(this);
    m_ui->treeView->setModel(m_model);
    connect(m_ui->treeView, &QTreeView::doubleClicked, this, &LibraryWidget::playSelected);
    QHeaderView *treeHeader = m_ui->treeView->header();
    treeHeader->setSectionsMovable(true);
    treeHeader->setFirstSectionMovable(true);
        m_artistsModel = new QStandardItemModel(this);
        m_artistsModel->setHorizontalHeaderLabels({tr("Artist"), tr("Albums"), tr("Tracks")});
        m_artistsProxy = new LibrarySummarySortModel(this);
        m_artistsProxy->setSourceModel(m_artistsModel);
        m_ui->artistsTableView->setModel(m_artistsProxy);
        m_ui->artistsTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        m_ui->artistsTableView->horizontalHeader()->setStretchLastSection(false);
        m_ui->artistsTableView->horizontalHeader()->setMinimumSectionSize(
            QFontMetrics(m_ui->artistsTableView->font()).horizontalAdvance(tr("Tracks")) + 16);
        m_ui->artistsTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_ui->artistsTableView->installEventFilter(this);
        connect(m_ui->artistsTableView->horizontalHeader(), &QHeaderView::sectionResized,
            this, [this](int column, int oldSize, int newSize)
            {
                adjustSummaryColumnSpace(m_ui->artistsTableView, column, newSize - oldSize);
            });
            m_ui->artistsTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_albumsModel = new QStandardItemModel(this);
        m_albumsModel->setHorizontalHeaderLabels({tr("Album"), tr("Year"), tr("Tracks")});
        m_albumsProxy = new LibrarySummarySortModel(this);
        m_albumsProxy->setSourceModel(m_albumsModel);
        m_ui->albumsTableView->setModel(m_albumsProxy);
        m_ui->albumsTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        m_ui->albumsTableView->horizontalHeader()->setStretchLastSection(false);
        m_ui->albumsTableView->horizontalHeader()->setMinimumSectionSize(
            QFontMetrics(m_ui->albumsTableView->font()).horizontalAdvance(tr("Album")) + 16);
        m_ui->albumsTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_ui->albumsTableView->installEventFilter(this);
        connect(m_ui->albumsTableView->horizontalHeader(), &QHeaderView::sectionResized,
            this, [this](int column, int oldSize, int newSize)
            {
                adjustSummaryColumnSpace(m_ui->albumsTableView, column, newSize - oldSize);
            });
        m_ui->albumsTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        connect(m_ui->artistsTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &LibraryWidget::refreshAlbums);
        connect(m_ui->albumsTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &LibraryWidget::updateTrackFilter);
        connect(m_ui->artistsTableView, &QTableView::doubleClicked,
            this, &LibraryWidget::replaceArtists);
        connect(m_ui->albumsTableView, &QTableView::doubleClicked,
            this, &LibraryWidget::replaceAlbums);
        refreshSummaryViews();
    applyPalette();
    m_ui->treeView->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_ui->treeView->header()->setStretchLastSection(false);
    m_ui->treeView->setColumnWidth(0, 60);
    m_ui->treeView->setColumnWidth(1, 180);
    m_ui->treeView->setColumnWidth(2, 180);
    m_ui->treeView->setColumnWidth(3, 240);
    m_ui->treeView->setColumnWidth(4, 60);

    if(dialog)
    {
        if(loadSkinChrome())
        {
            setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
            m_ui->buttonBox->hide();
        }
        else
            setWindowFlags(Qt::Dialog);
        setAttribute(Qt::WA_DeleteOnClose);
        setAttribute(Qt::WA_QuitOnClose, false);
    }
    else
    {
        m_ui->buttonBox->hide();
    }

    m_menu = new QMenu(this);
    m_menu->addAction(QIcon::fromTheme(u"list-add"_s), tr("&Add to Playlist"), this, &LibraryWidget::addSelected);
    m_menu->addAction(QIcon::fromTheme(u"media-eject"_s), tr("Replace Playlist"), this, &LibraryWidget::replaceSelected);
    m_menu->addAction(QIcon::fromTheme(u"dialog-information"_s), tr("&View Track Details"), this, &LibraryWidget::showTrackInformation);
    m_menu->addSeparator();

    QMenu *viewMenu = m_menu->addMenu(tr("Library View"));
    viewMenu->addAction(tr("All Artists"), this, &LibraryWidget::setArtistView);
    viewMenu->addAction(tr("All Albums"), this, &LibraryWidget::setAlbumView);
    viewMenu->addAction(tr("Most Played"), this, &LibraryWidget::setMostPlayedView);
    viewMenu->addAction(tr("Recently Played"), this, &LibraryWidget::setRecentlyPlayedView);
    viewMenu->addAction(tr("Unrated"), this, &LibraryWidget::setUnratedView);

    m_filterAction = m_menu->addAction(tr("Quick Search"), m_ui->filterLineEdit, &QLineEdit::setVisible);
    m_menu->addAction(QIcon::fromTheme(u"preferences-system"_s), tr("&Library Settings"), this, &LibraryWidget::showSettings);
    m_menu->addAction(tr("&Library Information"), this, &LibraryWidget::showLibraryInformation);
    m_filterAction->setCheckable(true);

    QMenu *columnsMenu = m_menu->addMenu(tr("Columns"));
    const QStringList columnNames = {tr("Track #"), tr("Artist"), tr("Title"),
                                     tr("Album"), tr("Year")};
    for(int column = 0; column < columnNames.size(); ++column)
    {
        QAction *action = columnsMenu->addAction(columnNames.at(column));
        action->setCheckable(true);
        action->setChecked(!treeHeader->isSectionHidden(column));
        connect(action, &QAction::toggled, this, [treeHeader, column](bool visible)
        {
            treeHeader->setSectionHidden(column, !visible);
        });
    }

    QSettings settings;
    m_filterAction->setChecked(settings.value(u"Library/quick_search_visible"_s, true).toBool());
    m_ui->filterLineEdit->setVisible(m_filterAction->isChecked());
    if(dialog)
        restoreGeometry(settings.value(u"Library/geometry"_s).toByteArray());

    QTimer::singleShot(0, this, [this]
    {
        initializeSummaryColumnWidths(m_ui->artistsTableView);
        initializeSummaryColumnWidths(m_ui->albumsTableView);
        adjustSummaryColumnSpace(m_ui->artistsTableView);
        adjustSummaryColumnSpace(m_ui->albumsTableView);
    });
}

LibraryWidget::~LibraryWidget()
{
    QSettings settings;
    settings.setValue(u"Library/quick_search_visible"_s, m_filterAction->isChecked());

    delete m_ui;
}

void LibraryWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if(event->type() == QEvent::PaletteChange)
        applyPalette();
}

void LibraryWidget::applyPalette()
{
    const QPalette applicationPalette = qApp->palette();
    QPalette panelPalette = applicationPalette;
    const QColor background = QmmpUiSkin::backgroundColor();
    const QColor foreground = QmmpUiSkin::foregroundColor();
    QColor selectedBackground;
    selectedBackground.setNamedColor(QmmpUiSkin::playlistValue(u"SelectedBG"_s));
    QColor current;
    current.setNamedColor(QmmpUiSkin::playlistValue(u"Current"_s));
    if(QmmpUiSkin::isSkinnedUi() && background.isValid() && foreground.isValid())
    {
        panelPalette.setColor(QPalette::Window, background);
        panelPalette.setColor(QPalette::WindowText, foreground);
        panelPalette.setColor(QPalette::Base, background);
        panelPalette.setColor(QPalette::Text, foreground);
        panelPalette.setColor(QPalette::AlternateBase, background.lighter(115));
        panelPalette.setColor(QPalette::Button, background);
        panelPalette.setColor(QPalette::ButtonText, foreground);
        if(selectedBackground.isValid())
            panelPalette.setColor(QPalette::Highlight, selectedBackground);
        if(current.isValid())
            panelPalette.setColor(QPalette::HighlightedText, current);

        const QString fontFamily = QmmpUiSkin::playlistValue(u"Font"_s);
        if(!fontFamily.isEmpty())
        {
            QFont skinFont = font();
            skinFont.setFamily(fontFamily);
            setFont(skinFont);
            m_ui->filterLineEdit->setFont(skinFont);
        }
    }
    else
    {
        panelPalette.setColor(QPalette::Window,
                              applicationPalette.color(QPalette::Base));
        panelPalette.setColor(QPalette::WindowText,
                              applicationPalette.color(QPalette::Text));
    }

    setPalette(panelPalette);
    m_ui->artistsPanel->setPalette(panelPalette);
    m_ui->albumsPanel->setPalette(panelPalette);
    m_ui->filterLineEdit->setPalette(panelPalette);
    if(QmmpUiSkin::isSkinnedUi() && background.isValid())
    {
        QPalette filterPalette = panelPalette;
        filterPalette.setColor(QPalette::Base, background.lighter(130));
        filterPalette.setColor(QPalette::AlternateBase, background.lighter(130));
        if(foreground.isValid())
            filterPalette.setColor(QPalette::PlaceholderText, foreground);
        m_ui->filterLineEdit->setPalette(filterPalette);
    }
    m_ui->artistsTableView->setPalette(panelPalette);
    m_ui->albumsTableView->setPalette(panelPalette);
    m_ui->treeView->setPalette(panelPalette);
}

bool LibraryWidget::loadSkinChrome()
{
    if(!QmmpUiSkin::isSkinnedUi())
        return false;

    m_skinPlaylist = QPixmap(QmmpUiSkin::filePath(u"pledit.png"_s));
    if(!m_skinPlaylist.isNull())
    {
        setMinimumSize(640, 420);
        setSizeIncrement(25, 29);
        m_resizeWidget = new QWidget(this);
        m_resizeWidget->resize(25, 29);
        m_resizeWidget->move(width() - m_resizeWidget->width(), height() - m_resizeWidget->height());
        m_resizeWidget->setCursor(Qt::SizeFDiagCursor);
        m_resizeWidget->installEventFilter(this);
    }
    return !m_skinPlaylist.isNull();
}

void LibraryWidget::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    if(m_skinPlaylist.isNull())
        return;

    QPainter painter(this);
    const int width = qMax(275, this->width());
    painter.drawTiledPixmap(0, 0, width, 20, m_skinPlaylist.copy(127, 0, 25, 20));
    painter.drawPixmap(0, 0, m_skinPlaylist.copy(0, 0, 25, 20));
    painter.drawPixmap(width - 25, 0, m_skinPlaylist.copy(153, 0, 25, 20));
    if(m_shadePressed)
        painter.drawPixmap(width - 22, 6, m_skinPlaylist.copy(62, 42, 9, 9));
    if(m_closePressed)
        painter.drawPixmap(width - 13, 6, m_skinPlaylist.copy(52, 42, 9, 9));
    const QString title = tr("Media Library").toLower();
    const int titleWidth = title.size() * 5;
    int x = (width - titleWidth) / 2;
    for(const QChar character : title)
    {
        painter.drawPixmap(x, 7, QmmpUiSkin::letter(character));
        x += 5;
    }
}

void LibraryWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if(m_resizeWidget)
        m_resizeWidget->move(width() - m_resizeWidget->width(), height() - m_resizeWidget->height());
}

void LibraryWidget::mousePressEvent(QMouseEvent *event)
{
    if(m_skinPlaylist.isNull() || event->button() != Qt::LeftButton)
    {
        QWidget::mousePressEvent(event);
        return;
    }

    if(event->position().y() < 20)
    {
        if(event->position().x() >= width() - 20)
        {
            m_closePressed = true;
            update();
            return;
        }
        if(event->position().x() >= width() - 29)
        {
            m_shadePressed = true;
            update();
            return;
        }
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
    else if(event->position().x() >= width() - 12 && event->position().y() >= height() - 12)
    {
        m_resizing = true;
        event->accept();
    }
    else
    {
        QWidget::mousePressEvent(event);
    }
}

void LibraryWidget::mouseMoveEvent(QMouseEvent *event)
{
    if(m_closePressed || m_shadePressed)
    {
        const bool closeHovered = event->position().y() < 20 &&
                event->position().x() >= width() - 20;
        const bool shadeHovered = event->position().y() < 20 &&
                event->position().x() >= width() - 29 &&
                event->position().x() < width() - 20;
        m_closePressed = closeHovered;
        m_shadePressed = shadeHovered;
        update();
        event->accept();
    }
    else if(m_dragging)
    {
        QPoint position = event->globalPosition().toPoint() - m_dragOffset;
        const QSize windowSize = frameGeometry().size();
        constexpr int snapDistance = 13;
        int horizontalSnapDistance = snapDistance;
        int verticalSnapDistance = snapDistance;
        auto snapCoordinate = [](int &coordinate, int target, int &distance)
        {
            const int delta = qAbs(coordinate - target);
            if(delta < distance)
            {
                coordinate = target;
                distance = delta;
            }
        };

        QScreen *screen = QGuiApplication::screenAt(event->globalPosition().toPoint());
        if(screen)
        {
            const QRect available = screen->availableGeometry();
            snapCoordinate(position.rx(), available.left(), horizontalSnapDistance);
            snapCoordinate(position.ry(), available.top(), verticalSnapDistance);
            snapCoordinate(position.rx(), available.right() - windowSize.width() + 1,
                           horizontalSnapDistance);
            snapCoordinate(position.ry(), available.bottom() - windowSize.height() + 1,
                           verticalSnapDistance);
        }

        for(QWidget *anchor : qApp->topLevelWidgets())
        {
            if(anchor == this || !anchor->isVisible())
                continue;

            const Qt::WindowType type = anchor->windowType();
            if(type != Qt::Window && type != Qt::Dialog && type != Qt::Tool && type != Qt::Drawer)
                continue;

            const QRect anchorGeometry = anchor->frameGeometry();
            snapCoordinate(position.rx(), anchorGeometry.left(), horizontalSnapDistance);
            snapCoordinate(position.rx(), anchorGeometry.right() + 1, horizontalSnapDistance);
            snapCoordinate(position.rx(), anchorGeometry.right() - windowSize.width() + 1,
                           horizontalSnapDistance);
            snapCoordinate(position.rx(), anchorGeometry.left() - windowSize.width(),
                           horizontalSnapDistance);
            snapCoordinate(position.ry(), anchorGeometry.top(), verticalSnapDistance);
            snapCoordinate(position.ry(), anchorGeometry.bottom() + 1, verticalSnapDistance);
            snapCoordinate(position.ry(), anchorGeometry.bottom() - windowSize.height() + 1,
                           verticalSnapDistance);
            snapCoordinate(position.ry(), anchorGeometry.top() - windowSize.height(),
                           verticalSnapDistance);
        }
        move(position);
        event->accept();
    }
    else if(m_resizing)
    {
        const QPoint delta = event->globalPosition().toPoint() - frameGeometry().topLeft();
        const int width = qMax(minimumWidth(), ((delta.x() - minimumWidth() + 12) / 25) * 25 + minimumWidth());
        const int height = qMax(minimumHeight(), ((delta.y() - minimumHeight() + 14) / 29) * 29 + minimumHeight());
        resize(width, height);
        event->accept();
    }
    else
    {
        QWidget::mouseMoveEvent(event);
    }
}

void LibraryWidget::mouseReleaseEvent(QMouseEvent *event)
{
    const bool close = m_closePressed && event->position().y() < 20 &&
            event->position().x() >= width() - 20;
    const bool shade = m_shadePressed && event->position().y() < 20 &&
            event->position().x() >= width() - 29 && event->position().x() < width() - 20;
    m_closePressed = false;
    m_shadePressed = false;
    m_dragging = false;
    m_resizing = false;
    update();
    if(close)
        this->close();
    else if(shade)
        toggleShade();
    QWidget::mouseReleaseEvent(event);
}

bool LibraryWidget::eventFilter(QObject *watched, QEvent *event)
{
    if((watched == m_ui->artistsTableView || watched == m_ui->albumsTableView) &&
            event->type() == QEvent::Resize)
    {
        if(watched == m_ui->artistsTableView && !m_artistsColumnsInitialized)
            initializeSummaryColumnWidths(m_ui->artistsTableView);
        else if(watched == m_ui->albumsTableView && !m_albumsColumnsInitialized)
            initializeSummaryColumnWidths(m_ui->albumsTableView);
        adjustSummaryColumnSpace(static_cast<QTableView *>(watched));
    }
    if(watched == m_resizeWidget && event->type() == QEvent::MouseButtonPress)
    {
        m_resizing = true;
        setCursor(m_resizeWidget->cursor());
        return true;
    }
    if(watched == m_resizeWidget && event->type() == QEvent::MouseMove && m_resizing)
    {
        const QMouseEvent *mouseEvent = static_cast<const QMouseEvent *>(event);
        const QPoint delta = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
        const int width = qMax(minimumWidth(), ((delta.x() - minimumWidth() + 12) / 25) * 25 + minimumWidth());
        const int height = qMax(minimumHeight(), ((delta.y() - minimumHeight() + 14) / 29) * 29 + minimumHeight());
        resize(width, height);
        return true;
    }
    if(watched == m_resizeWidget && event->type() == QEvent::MouseButtonRelease)
    {
        m_resizing = false;
        setCursor(Qt::ArrowCursor);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void LibraryWidget::adjustSummaryColumnSpace(QTableView *tableView, int excludedColumn,
                                              int sectionDelta)
{
    if(m_distributingColumnSpace)
        return;

    QHeaderView *header = tableView->horizontalHeader();
    const int difference = tableView->viewport()->width() - header->length();
    if((difference == 0 && sectionDelta == 0) || header->count() == 0)
        return;

    m_distributingColumnSpace = true;
    if(difference > 0)
    {
        int targetColumn = -1;
        int largestDeficit = 0;
        for(int column = 0; column < header->count(); ++column)
        {
            const int deficit = SummaryTableAccessor::columnSizeHint(tableView, column) - header->sectionSize(column);
            if(deficit > largestDeficit)
            {
                largestDeficit = deficit;
                targetColumn = column;
            }
        }
        if(targetColumn < 0)
        {
            targetColumn = 0;
            for(int column = 1; column < header->count(); ++column)
            {
                if(header->sectionSize(column) > header->sectionSize(targetColumn))
                    targetColumn = column;
            }
        }
        header->resizeSection(targetColumn, header->sectionSize(targetColumn) + difference);
    }
    else if(difference == 0 && sectionDelta != 0)
    {
        int remaining = qAbs(sectionDelta);
        while(remaining > 0)
        {
            int targetColumn = -1;
            int largestDeficit = 0;
            for(int column = 0; column < header->count(); ++column)
            {
                if(column == excludedColumn)
                    continue;
                const int deficit = SummaryTableAccessor::columnSizeHint(tableView, column) - header->sectionSize(column);
                if(deficit > largestDeficit)
                {
                    largestDeficit = deficit;
                    targetColumn = column;
                }
            }
            if(targetColumn < 0)
            {
                for(int column = 0; column < header->count(); ++column)
                {
                    if(column != excludedColumn &&
                            (targetColumn < 0 || header->sectionSize(column) > header->sectionSize(targetColumn)))
                        targetColumn = column;
                }
            }
            if(targetColumn < 0)
                break;
            const int available = header->sectionSize(targetColumn);
            const int amount = sectionDelta > 0 ?
                        qMin(remaining, available - header->minimumSectionSize()) : remaining;
            header->resizeSection(targetColumn, sectionDelta > 0 ?
                                  qMax(header->minimumSectionSize(), available - amount) :
                                  available + amount);
            remaining -= amount;
            if(amount == 0)
                break;
        }
    }
    else
    {
        int remaining = -difference;
        while(remaining > 0)
        {
            int widestColumn = -1;
            for(int column = 0; column < header->count(); ++column)
            {
                if(column == excludedColumn || header->sectionSize(column) <= header->minimumSectionSize())
                    continue;
                if(widestColumn < 0 || header->sectionSize(column) > header->sectionSize(widestColumn))
                    widestColumn = column;
            }
            if(widestColumn < 0)
                break;
            const int reduction = qMin(remaining,
                                       header->sectionSize(widestColumn) - header->minimumSectionSize());
            header->resizeSection(widestColumn, header->sectionSize(widestColumn) - reduction);
            remaining -= reduction;
        }
    }
    m_distributingColumnSpace = false;
}

void LibraryWidget::initializeSummaryColumnWidths(QTableView *tableView)
{
    if((tableView == m_ui->artistsTableView && m_artistsColumnsInitialized) ||
            (tableView == m_ui->albumsTableView && m_albumsColumnsInitialized))
        return;

    const int width = tableView->viewport()->width();
    if(width <= 0)
        return;

    m_distributingColumnSpace = true;
    QHeaderView *header = tableView->horizontalHeader();
    const int first = qMax(header->minimumSectionSize(), width * 3 / 5);
    const int second = qMax(header->minimumSectionSize(), (width - first) / 2);
    header->resizeSection(0, first);
    header->resizeSection(1, second);
    header->resizeSection(2, width - first - second);
    m_distributingColumnSpace = false;

    if(tableView == m_ui->artistsTableView)
        m_artistsColumnsInitialized = true;
    else
        m_albumsColumnsInitialized = true;
}

void LibraryWidget::toggleShade()
{
    const int oldHeight = height();
    if(m_shaded)
    {
        setMinimumHeight(420);
        setMaximumHeight(QWIDGETSIZE_MAX);
        resize(width(), m_unshadedHeight);
    }
    else
    {
        m_unshadedHeight = oldHeight;
        setFixedHeight(20);
    }
    m_shaded = !m_shaded;
    emit shadedChanged(m_shaded, height() - oldHeight);
    update();
}

void LibraryWidget::refresh()
{
    m_ui->filterLineEdit->clear();
    m_model->refresh();
    refreshSummaryViews();
}

void LibraryWidget::setBusyMode(bool enabled)
{
    if(m_busyIndicator)
    {
        delete m_busyIndicator;
        m_busyIndicator = nullptr;
    }

    if(enabled)
    {
        m_busyIndicator = new QLabel(tr("Scanning directories..."), this);
        m_busyIndicator->setFrameShape(QFrame::Box);
        m_busyIndicator->resize(m_busyIndicator->sizeHint());
        m_busyIndicator->move(width() / 2 - m_busyIndicator->width() / 2 , height() / 2 - m_busyIndicator->height() / 2);
        m_busyIndicator->setAutoFillBackground(true);
        m_busyIndicator->show();

        m_ui->treeView->setEnabled(false);
        m_ui->filterLineEdit->setEnabled(false);
    }
    else
    {
        m_ui->treeView->setEnabled(true);
        m_ui->filterLineEdit->setEnabled(true);
    }
}

void LibraryWidget::closeEvent(QCloseEvent *)
{
    if(isWindow())
    {
        QSettings settings;
        settings.setValue(u"Library/geometry"_s, saveGeometry());
        if(!qApp->closingDown())
            settings.setValue(u"Library/visible"_s, false);
        emit closed();
    }
}

void LibraryWidget::contextMenuEvent(QContextMenuEvent *e)
{
    m_contextSource = childAt(e->pos());
    m_menu->exec(mapToGlobal(e->pos()));
    m_contextSource = nullptr;
}

void LibraryWidget::on_filterLineEdit_textChanged(const QString &text)
{
    m_model->setFilter(text);
    m_model->refresh();
    refreshSummaryViews();
    if(m_model->rowCount() <= 4)
        m_ui->treeView->expandAll();
}

void LibraryWidget::refreshSummaryViews()
{
    refreshArtists();
    refreshAlbums();
    updateTrackFilter();
}

void LibraryWidget::refreshArtists()
{
    QSqlDatabase db = QSqlDatabase::database(u"qmmp_library_view"_s);
    if(!db.isOpen())
        return;

    m_artistsModel->removeRows(0, m_artistsModel->rowCount());
    QSqlQuery query(db);
    QString filter = m_ui->filterLineEdit->text();
    query.prepare(u"SELECT Artist, COUNT(DISTINCT Album), COUNT(*) FROM track_library "
                  "WHERE SearchString LIKE :filter GROUP BY Artist ORDER BY Artist"_s);
    query.bindValue(u":filter"_s, QStringLiteral("%%1%").arg(filter.toLower()));
    if(!query.exec())
        return;

    int artists = 0;
    int albums = 0;
    int tracks = 0;
    QList<QVariantList> rows;
    while(query.next())
    {
        ++artists;
        albums += query.value(1).toInt();
        tracks += query.value(2).toInt();
        rows << QVariantList{query.value(0), query.value(1), query.value(2)};
    }

    QList<QStandardItem *> allRow;
    allRow << new QStandardItem(tr("All (%n artists)", "", artists));
    allRow << new QStandardItem(QString::number(albums));
    allRow << new QStandardItem(QString::number(tracks));
    allRow.first()->setData(true, Qt::UserRole);
    m_artistsModel->appendRow(allRow);
    for(const QVariantList &row : rows)
    {
        QList<QStandardItem *> items;
        items << new QStandardItem(row.at(0).toString());
        items << new QStandardItem(row.at(1).toString());
        items << new QStandardItem(row.at(2).toString());
        items.first()->setData(row.at(0), Qt::UserRole);
        m_artistsModel->appendRow(items);
    }
    m_ui->artistsTableView->selectRow(0);
}

void LibraryWidget::refreshAlbums()
{
    QSqlDatabase db = QSqlDatabase::database(u"qmmp_library_view"_s);
    if(!db.isOpen())
        return;

    QModelIndex artistIndex = m_ui->artistsTableView->currentIndex();
    m_selectedArtist = artistIndex.isValid() && artistIndex.row() > 0 ?
                artistIndex.siblingAtColumn(0).data(Qt::UserRole).toString() : QString();
    m_albumsModel->removeRows(0, m_albumsModel->rowCount());

    QSqlQuery query(db);
    QString filter = m_ui->filterLineEdit->text();
    QString sql = u"SELECT Album, MAX(Year), COUNT(*) FROM track_library "
                  "WHERE SearchString LIKE :filter"_s;
    if(!m_selectedArtist.isEmpty())
        sql += u" AND Artist = :artist"_s;
    sql += u" GROUP BY Album ORDER BY Album"_s;
    query.prepare(sql);
    query.bindValue(u":filter"_s, QStringLiteral("%%1%").arg(filter.toLower()));
    if(!m_selectedArtist.isEmpty())
        query.bindValue(u":artist"_s, m_selectedArtist);
    if(!query.exec())
        return;

    QList<QVariantList> rows;
    int tracks = 0;
    while(query.next())
    {
        tracks += query.value(2).toInt();
        rows << QVariantList{query.value(0), query.value(1), query.value(2)};
    }
    QList<QStandardItem *> allRow;
    allRow << new QStandardItem(tr("All (%n albums)", "", rows.count()));
    allRow << new QStandardItem;
    allRow << new QStandardItem(QString::number(tracks));
    allRow.first()->setData(true, Qt::UserRole);
    m_albumsModel->appendRow(allRow);
    for(const QVariantList &row : rows)
    {
        QList<QStandardItem *> items;
        items << new QStandardItem(row.at(0).toString());
        items << new QStandardItem(row.at(1).toInt() > 0 ? row.at(1).toString() : QString());
        items << new QStandardItem(row.at(2).toString());
        items.first()->setData(row.at(0), Qt::UserRole);
        m_albumsModel->appendRow(items);
    }
    m_ui->albumsTableView->selectRow(0);
}

void LibraryWidget::updateTrackFilter()
{
    QModelIndex albumIndex = m_ui->albumsTableView->currentIndex();
    m_selectedAlbum = albumIndex.isValid() && albumIndex.row() > 0 ?
                albumIndex.siblingAtColumn(0).data(Qt::UserRole).toString() : QString();
    m_model->setTrackFilter(m_selectedArtist, m_selectedAlbum);
}

void LibraryWidget::replaceArtists()
{
    m_model->replaceFiltered(true);
}

void LibraryWidget::replaceAlbums()
{
    m_model->replaceFiltered(true);
}

void LibraryWidget::addSelected()
{
    if((m_ui->artistsTableView->isAncestorOf(m_contextSource) ||
        m_ui->albumsTableView->isAncestorOf(m_contextSource)))
    {
        m_model->addFiltered();
        return;
    }
    m_model->add(m_ui->treeView->selectionModel()->selectedIndexes());
}

void LibraryWidget::replaceSelected()
{
    if((m_ui->artistsTableView->isAncestorOf(m_contextSource) ||
        m_ui->albumsTableView->isAncestorOf(m_contextSource)))
    {
        m_model->replaceFiltered();
        return;
    }
    m_model->replace(m_ui->treeView->selectionModel()->selectedIndexes());
}

void LibraryWidget::playSelected(const QModelIndex &index)
{
    m_model->replaceAndPlay(index.siblingAtColumn(0));
}

void LibraryWidget::showTrackInformation()
{
    m_model->showTrackInformation(m_ui->treeView->selectionModel()->selectedIndexes(), qApp->activeWindow());
}

void LibraryWidget::showLibraryInformation()
{
    m_model->showLibraryInformation(qApp->activeWindow());
}

void LibraryWidget::showSettings()
{
    LibrarySettingsDialog dialog(this);
    dialog.exec();
}

void LibraryWidget::setArtistView()
{
    m_model->setViewMode(LibraryModel::ArtistView);
    m_model->refresh();
}

void LibraryWidget::setAlbumView()
{
    m_model->setViewMode(LibraryModel::AlbumView);
    m_model->refresh();
}

void LibraryWidget::setMostPlayedView()
{
    m_model->setViewMode(LibraryModel::MostPlayedView);
    m_model->refresh();
}

void LibraryWidget::setRecentlyPlayedView()
{
    m_model->setViewMode(LibraryModel::RecentlyPlayedView);
    m_model->refresh();
}

void LibraryWidget::setUnratedView()
{
    m_model->setViewMode(LibraryModel::UnratedView);
    m_model->refresh();
}
