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
#include <QPushButton>
#include <QDir>
#include <QDateTime> 
#include <QLocale>
#include <QHeaderView>
#include <QPalette>
#include <QFont>
#include <QPainter>
#include <QMouseEvent>
#include <QFileInfo>
#include <QResizeEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QItemSelectionModel>
#include <QAbstractItemView>
#include <QTableView>
#include <QScopedValueRollback>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <qmmp/qmmp.h>
#include <qmmpui/filedialog.h>
#include <qmmpui/qmmpuiskin.h>
#include <qmmpui/uihelper.h>
#include "librarymodel.h"
#include "librarysettingsdialog.h"
#include "ui_librarywidget.h"
#include "librarywidget.h"

class LibrarySummarySortModel : public QSortFilterProxyModel
{
public:
    explicit LibrarySummarySortModel(LibraryModel *libraryModel, QObject *parent = nullptr) :
        QSortFilterProxyModel(parent), m_libraryModel(libraryModel)
    {}

    QStringList mimeTypes() const override
    {
        return m_libraryModel->mimeTypes();
    }

    QMimeData *mimeData(const QModelIndexList &indexes) const override
    {
        return indexes.isEmpty() ? nullptr : m_libraryModel->filteredMimeData();
    }

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
    LibraryModel *m_libraryModel;
};
LibraryWidget::LibraryWidget(bool dialog, QWidget *parent) :
    QWidget(parent),
    m_ui(new Ui::LibraryWidget)
{
    connect(UiHelper::instance(), &UiHelper::exitRequested, this, [this] {
        m_appExiting = true;
    });
    if(dialog)
        setObjectName(u"MediaLibrary"_s);
    m_ui->setupUi(this);
    m_ui->filterLineEdit->addAction(QIcon::fromTheme(u"edit-find"_s), QLineEdit::LeadingPosition);
    m_model = new LibraryModel(this);
    m_ui->treeView->setModel(m_model);
    connect(UiHelper::instance(), &UiHelper::skinChanged, this, &LibraryWidget::reloadSkin);
    connect(m_ui->treeView, &QTreeView::doubleClicked, this, &LibraryWidget::playSelected);
    QHeaderView *treeHeader = m_ui->treeView->header();
    treeHeader->setSectionsMovable(true);
    treeHeader->setFirstSectionMovable(true);
        m_artistsModel = new QStandardItemModel(this);
        m_artistsModel->setHorizontalHeaderLabels({tr("Artist"), tr("Albums"), tr("Tracks")});
        m_artistsProxy = new LibrarySummarySortModel(m_model, this);
        m_artistsProxy->setSourceModel(m_artistsModel);
        m_ui->artistsTableView->setModel(m_artistsProxy);
        m_ui->artistsTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        m_ui->artistsTableView->horizontalHeader()->setStretchLastSection(false);
        m_ui->artistsTableView->horizontalHeader()->setMinimumSectionSize(
            QFontMetrics(m_ui->artistsTableView->font()).horizontalAdvance(tr("Tracks")) + 16);
        m_ui->artistsTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_ui->artistsTableView->viewport()->installEventFilter(this);
        m_ui->artistsTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_albumsModel = new QStandardItemModel(this);
        m_albumsModel->setHorizontalHeaderLabels({tr("Album"), tr("Year"), tr("Tracks")});
        m_albumsProxy = new LibrarySummarySortModel(m_model, this);
        m_albumsProxy->setSourceModel(m_albumsModel);
        m_ui->albumsTableView->setModel(m_albumsProxy);
        m_ui->albumsTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        m_ui->albumsTableView->horizontalHeader()->setStretchLastSection(false);
        m_ui->albumsTableView->horizontalHeader()->setMinimumSectionSize(
            QFontMetrics(m_ui->albumsTableView->font()).horizontalAdvance(tr("Album")) + 16);
        m_ui->albumsTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_ui->albumsTableView->viewport()->installEventFilter(this);
        m_ui->albumsTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        connect(m_ui->artistsTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]
        {
            if(m_refreshingSummary)
                return;
            {
                QScopedValueRollback<bool> guard(m_refreshingSummary, true);
                refreshAlbums();
            }
            updateTrackFilter();
        });
        connect(m_ui->albumsTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]
        {
            if(!m_refreshingSummary)
                updateTrackFilter();
        });
        connect(m_ui->artistsTableView, &QTableView::doubleClicked,
            this, &LibraryWidget::replaceArtists);
        connect(m_ui->albumsTableView, &QTableView::doubleClicked,
            this, &LibraryWidget::replaceAlbums);
        refreshSummaryViews();
    applyPalette();
    m_ui->treeView->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_ui->treeView->header()->setStretchLastSection(false);
    m_ui->treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_ui->treeView->setColumnWidth(0, 60);
    m_ui->treeView->setColumnWidth(1, 180);
    m_ui->treeView->setColumnWidth(2, 180);
    m_ui->treeView->setColumnWidth(3, 240);
    m_ui->treeView->setColumnWidth(4, 60);
    m_ui->treeView->viewport()->installEventFilter(this);
    if(dialog)
    {
        if(loadSkinChrome())
        {
            setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
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
    m_menu->addAction(QIcon::fromTheme(u"list-add"_s), tr("Add after currently playing"), this, &LibraryWidget::addAfterCurrent);
    m_menu->addAction(QIcon::fromTheme(u"media-eject"_s), tr("Replace Playlist"), this, &LibraryWidget::replaceSelected);
    m_menu->addAction(QIcon::fromTheme(u"dialog-information"_s), tr("&View Track Details"), this, &LibraryWidget::showTrackInformation);
    m_menu->addSeparator();
    m_menu->addAction(QIcon::fromTheme(u"list-add"_s), tr("Add Media to Library"),
                      this, &LibraryWidget::showSettings);
    m_menu->addAction(QIcon::fromTheme(u"folder"_s), tr("Add Media Directory..."),
                      this, &LibraryWidget::addMediaDirectory);

    QMenu *viewMenu = m_menu->addMenu(tr("Library View"));
    viewMenu->addAction(tr("All Artists"), this, &LibraryWidget::setArtistView);
    viewMenu->addAction(tr("All Albums"), this, &LibraryWidget::setAlbumView);
    viewMenu->addAction(tr("Most Played"), this, &LibraryWidget::setMostPlayedView);
    viewMenu->addAction(tr("Recently Played"), this, &LibraryWidget::setRecentlyPlayedView);
    viewMenu->addAction(tr("Unrated"), this, &LibraryWidget::setUnratedView);

    m_filterAction = m_menu->addAction(tr("Quick Search"));
    m_settingsAction = m_menu->addAction(QIcon::fromTheme(u"preferences-system"_s), tr("&Library Settings"), this, &LibraryWidget::showSettings);
    m_menu->addAction(tr("&Library Information"), this, &LibraryWidget::showLibraryInformation);
    m_filterAction->setCheckable(true);
    connect(m_filterAction, &QAction::toggled, this, [this] { updateEmptyState(); });

    QMenu *columnsMenu = m_menu->addMenu(tr("Columns"));
    const QStringList columnNames = {tr("Track #"), tr("Artist"), tr("Title"),
                                     tr("Album"), tr("Year")};
    for(int column = 0; column < columnNames.size(); ++column)
    {
        QAction *action = columnsMenu->addAction(columnNames.at(column));
        action->setCheckable(true);
        action->setChecked(!treeHeader->isSectionHidden(column));
        connect(action, &QAction::toggled, this, [this, treeHeader, column](bool visible)
        {
            treeHeader->setSectionHidden(column, !visible);
        });
    }

    QSettings settings;
    m_filterAction->setChecked(settings.value(u"Library/quick_search_visible"_s, true).toBool());
    m_ui->filterLineEdit->setVisible(m_filterAction->isChecked());
    if(dialog)
        restoreGeometry(settings.value(u"Library/geometry"_s).toByteArray());
    connect(m_ui->addMediaButton, &QPushButton::clicked, this, &LibraryWidget::showSettings);
    updateEmptyState();

    QTimer::singleShot(0, this, [this]
    {
        initializeSummaryColumnWidths(m_ui->artistsTableView);
        initializeSummaryColumnWidths(m_ui->albumsTableView);
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
    const QColor itemBackground = QmmpUiSkin::generalColor(48);
    const QColor itemForeground = QmmpUiSkin::generalColor(50);
    const QColor buttonText = QmmpUiSkin::generalColor(54);
    QColor selectedBackground = QmmpUiSkin::generalColor(60);
    if(!selectedBackground.isValid())
        selectedBackground.setNamedColor(QmmpUiSkin::playlistValue(u"SelectedBG"_s));
    const QColor current(QmmpUiSkin::playlistValue(u"Current"_s));
    if(QmmpUiSkin::isSkinnedUi() && background.isValid() && foreground.isValid())
    {
        panelPalette.setColor(QPalette::Window, background);
        panelPalette.setColor(QPalette::WindowText, foreground);
        panelPalette.setColor(QPalette::Base, itemBackground.isValid() ? itemBackground : background);
        panelPalette.setColor(QPalette::Text, itemForeground.isValid() ? itemForeground : foreground);
        panelPalette.setColor(QPalette::AlternateBase,
                      (itemBackground.isValid() ? itemBackground : background).lighter(115));
        panelPalette.setColor(QPalette::Button, background);
        panelPalette.setColor(QPalette::ButtonText, buttonText.isValid() ? buttonText : foreground);
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
    m_skinPlaylist = QPixmap();
    m_hasSkinBorders = false;
    QSettings settings;
    m_skinRatio = settings.value(u"Skinned/double_size"_s, false).toBool() ? 2 : 1;
    m_ui->verticalLayout->setContentsMargins(6 * m_skinRatio, 26 * m_skinRatio,
                                             6 * m_skinRatio, 6 * m_skinRatio);
    if(!QmmpUiSkin::isSkinnedUi())
        return false;

    m_skinPlaylist = QPixmap(QmmpUiSkin::filePath(u"pledit"_s));
    if(m_skinPlaylist.isNull())
    {
        const QColor background = QmmpUiSkin::backgroundColor();
        const QColor foreground = QmmpUiSkin::foregroundColor();
        if(!background.isValid() || !foreground.isValid())
            return false;

        const QColor titlebar = background.lightness() < 128
                ? background.lighter(145) : background.darker(115);
        m_skinPlaylist = QPixmap(178, 51);
        m_skinPlaylist.fill(background);
        QPainter painter(&m_skinPlaylist);
        painter.fillRect(0, 0, 178, 20, titlebar);
        painter.setPen(foreground);
        painter.drawLine(0, 19, 177, 19);
        painter.drawLine(158, 9, 164, 9);
        painter.drawLine(169, 6, 175, 12);
        painter.drawLine(175, 6, 169, 12);
    }
    if(!m_skinPlaylist.isNull())
    {
        m_hasSkinBorders = m_skinPlaylist.width() >= 276 && m_skinPlaylist.height() >= 110;
        if(m_hasSkinBorders)
            m_ui->verticalLayout->setContentsMargins(12 * m_skinRatio, 26 * m_skinRatio,
                                                     20 * m_skinRatio, 6 * m_skinRatio);

        const int widthIncrement = 25 * m_skinRatio;
        const int minimumWidth = ((640 + widthIncrement - 1) / widthIncrement) * widthIncrement;
        setMinimumSize(minimumWidth, m_shaded ? 20 : 420);
        setSizeIncrement(widthIncrement, 29 * m_skinRatio);
        if(!m_resizeWidget)
        {
            m_resizeWidget = new QWidget(this);
            m_resizeWidget->setCursor(Qt::SizeFDiagCursor);
            m_resizeWidget->installEventFilter(this);
        }
        m_resizeWidget->resize(25 * m_skinRatio, 29 * m_skinRatio);
        m_resizeWidget->move(width() - m_resizeWidget->width(), height() - m_resizeWidget->height());
    }
    return !m_skinPlaylist.isNull();
}

void LibraryWidget::reloadSkin()
{
    applyPalette();
    if(isWindow())
    {
        const bool wasVisible = isVisible();
        const bool hasChrome = loadSkinChrome();
        const Qt::WindowFlags flags = hasChrome ? (Qt::Tool | Qt::FramelessWindowHint) : Qt::Dialog;
        if(windowFlags() != flags)
            setWindowFlags(flags);

        if(!hasChrome)
        {
            if(m_shaded)
            {
                const int oldHeight = height();
                m_shaded = false;
                setMinimumHeight(0);
                setMaximumHeight(QWIDGETSIZE_MAX);
                resize(width(), m_unshadedHeight > 0 ? m_unshadedHeight : 420);
                emit shadedChanged(false, height() - oldHeight);
            }
            setMinimumSize(0, 0);
            setSizeIncrement(0, 0);
            delete m_resizeWidget;
            m_resizeWidget = nullptr;
        }
        m_ui->buttonBox->setVisible(!hasChrome);
        if(wasVisible)
            show();
    }
    update();
}

void LibraryWidget::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    if(m_skinPlaylist.isNull())
        return;

    QPainter painter(this);
    painter.scale(m_skinRatio, m_skinRatio);
    const int width = qMax(275, this->width() / m_skinRatio);
    const int height = this->height() / m_skinRatio;
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

    if(m_hasSkinBorders && height > 25)
    {
        painter.drawTiledPixmap(QRect(0, 20, 12, height - 25),
                                m_skinPlaylist.copy(0, 42, 12, 29));
        painter.drawTiledPixmap(QRect(width - 20, 20, 20, height - 25),
                                m_skinPlaylist.copy(31, 42, 20, 29));

        const int bottom = height - 5;
        painter.drawPixmap(0, bottom, m_skinPlaylist.copy(0, 105, 125, 5));
        for(int x = 125; x < width - 150; x += 25)
            painter.drawPixmap(x, bottom, m_skinPlaylist.copy(179, 33, 25, 5));
        painter.drawPixmap(width - 150, bottom, m_skinPlaylist.copy(126, 105, 150, 5));
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

    const QPointF position = event->position() / m_skinRatio;
    if(position.y() < 20)
    {
        if(position.x() >= width() / m_skinRatio - 20)
        {
            m_closePressed = true;
            update();
            return;
        }
        if(position.x() >= width() / m_skinRatio - 29)
        {
            m_shadePressed = true;
            update();
            return;
        }
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
    else if(position.x() >= width() / m_skinRatio - 12 &&
            position.y() >= height() / m_skinRatio - 12)
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
        const QPointF position = event->position() / m_skinRatio;
        const qreal logicalWidth = width() / m_skinRatio;
        const bool closeHovered = position.y() < 20 &&
                position.x() >= logicalWidth - 20;
        const bool shadeHovered = position.y() < 20 &&
                position.x() >= logicalWidth - 29 &&
                position.x() < logicalWidth - 20;
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
        const int widthIncrement = 25 * m_skinRatio;
        const int heightIncrement = 29 * m_skinRatio;
        const int width = qMax(minimumWidth(),
                       ((delta.x() - minimumWidth() + widthIncrement / 2) / widthIncrement) *
                           widthIncrement + minimumWidth());
        const int height = qMax(minimumHeight(),
                    ((delta.y() - minimumHeight() + heightIncrement / 2) / heightIncrement) *
                        heightIncrement + minimumHeight());
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
    const QPointF position = event->position() / m_skinRatio;
    const qreal logicalWidth = width() / m_skinRatio;
    const bool close = m_closePressed && position.y() < 20 &&
            position.x() >= logicalWidth - 20;
    const bool shade = m_shadePressed && position.y() < 20 &&
            position.x() >= logicalWidth - 29 && position.x() < logicalWidth - 20;
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

void LibraryWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    const QPointF position = event->position() / m_skinRatio;
    if(!m_skinPlaylist.isNull() && event->button() == Qt::LeftButton &&
            position.y() < 20 && position.x() < width() / m_skinRatio - 29)
    {
        toggleShade();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

bool LibraryWidget::eventFilter(QObject *watched, QEvent *event)
{
    if((watched == m_ui->artistsTableView->viewport() ||
        watched == m_ui->albumsTableView->viewport()) && event->type() == QEvent::Resize)
    {
        QTableView *table = watched == m_ui->artistsTableView->viewport() ?
                    m_ui->artistsTableView : m_ui->albumsTableView;
        initializeSummaryColumnWidths(table);
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
        const int widthIncrement = 25 * m_skinRatio;
        const int heightIncrement = 29 * m_skinRatio;
        const int width = qMax(minimumWidth(),
                       ((delta.x() - minimumWidth() + widthIncrement / 2) / widthIncrement) *
                           widthIncrement + minimumWidth());
        const int height = qMax(minimumHeight(),
                    ((delta.y() - minimumHeight() + heightIncrement / 2) / heightIncrement) *
                        heightIncrement + minimumHeight());
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

void LibraryWidget::initializeSummaryColumnWidths(QTableView *tableView)
{
    if((tableView == m_ui->artistsTableView && m_artistsColumnsInitialized) ||
            (tableView == m_ui->albumsTableView && m_albumsColumnsInitialized))
        return;

    const int width = tableView->viewport()->width();
    if(width <= 0)
        return;

    QHeaderView *header = tableView->horizontalHeader();
    const int first = qMax(header->minimumSectionSize(), width * 3 / 5);
    const int second = qMax(header->minimumSectionSize(), (width - first) / 2);
    header->resizeSection(0, first);
    header->resizeSection(1, second);
    header->resizeSection(2, width - first - second);
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
    updateEmptyState();
}

void LibraryWidget::updateEmptyState()
{
    const bool hasTracks = m_model->hasTracks();
    m_ui->contentSplitter->setVisible(hasTracks);
    m_ui->emptyPage->setVisible(!hasTracks);
    m_ui->filterLineEdit->setVisible(hasTracks && m_filterAction->isChecked());
}

void LibraryWidget::addMenuAction(QAction *action)
{
    if(!action || m_menu->actions().contains(action))
        return;
    m_menu->insertAction(m_settingsAction, action); // shown just above "Library Settings"
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
        if(!m_appExiting && !qApp->closingDown())
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
    {
        QScopedValueRollback<bool> guard(m_refreshingSummary, true);
        refreshArtists();
        refreshAlbums();
    }
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
                 "WHERE SearchString LIKE :filter ESCAPE '\\' GROUP BY Artist ORDER BY Artist"_s);
    query.bindValue(u":filter"_s, LibraryModel::likePattern(filter));
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

    m_selectedArtists.clear();
    for(const QModelIndex &index : m_ui->artistsTableView->selectionModel()->selectedRows(0))
    {
        if(index.row() > 0)
            m_selectedArtists << index.data(Qt::UserRole).toString();
    }
    m_albumsModel->removeRows(0, m_albumsModel->rowCount());

    QSqlQuery query(db);
    QString filter = m_ui->filterLineEdit->text();
    QString sql = u"SELECT Album, MAX(Year), COUNT(*) FROM track_library "
                  "WHERE SearchString LIKE :filter ESCAPE '\\'"_s;
    if(!m_selectedArtists.isEmpty())
    {
        QStringList placeholders;
        for(int i = 0; i < m_selectedArtists.size(); ++i)
            placeholders << u":artist%1"_s.arg(i);
        sql += u" AND Artist IN ("_s + placeholders.join(u", "_s) + u")"_s;
    }
    sql += u" GROUP BY Album ORDER BY Album"_s;
    query.prepare(sql);
    query.bindValue(u":filter"_s, LibraryModel::likePattern(filter));
    for(int i = 0; i < m_selectedArtists.size(); ++i)
        query.bindValue(u":artist%1"_s.arg(i), m_selectedArtists.at(i));
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
    m_selectedAlbums.clear();
    for(const QModelIndex &index : m_ui->albumsTableView->selectionModel()->selectedRows(0))
    {
        if(index.row() > 0)
            m_selectedAlbums << index.data(Qt::UserRole).toString();
    }
    m_model->setTrackFilter(m_selectedArtists, m_selectedAlbums);
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

void LibraryWidget::addAfterCurrent()
{
    if((m_ui->artistsTableView->isAncestorOf(m_contextSource) ||
        m_ui->albumsTableView->isAncestorOf(m_contextSource)))
    {
        m_model->addFilteredAfterCurrent();
        return;
    }
    m_model->addAfterCurrent(m_ui->treeView->selectionModel()->selectedIndexes());
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
    if(dialog.exec() == QDialog::Accepted)
        emit settingsChanged();
}

void LibraryWidget::addMediaDirectory()
{
    QSettings settings;
    const QString lastPath = settings.value(u"Library/last_path"_s, QDir::homePath()).toString();
    const QString path = FileDialog::getExistingDirectory(this, tr("Select Directory for Scanning"), lastPath);
    if(path.isEmpty())
        return;

    const QString directory = QDir::cleanPath(path);
    QStringList directories = settings.value(u"Library/dirs"_s).toStringList();
    for(const QString &existingDirectory : directories)
    {
        if(QDir::cleanPath(existingDirectory) == directory)
            return;
    }

    directories << directory;
    settings.setValue(u"Library/dirs"_s, directories);
    settings.setValue(u"Library/last_path"_s, QFileInfo(directory).absolutePath());
    emit settingsChanged();
}

void LibraryWidget::setArtistView()
{
    m_model->setViewMode(LibraryModel::ArtistView);
    m_model->refresh(); // (seems superfluous)
}

void LibraryWidget::setAlbumView()
{
    m_model->setViewMode(LibraryModel::AlbumView);
    m_model->refresh(); // (seems superfluous)
}

void LibraryWidget::setMostPlayedView()
{
    m_model->setViewMode(LibraryModel::MostPlayedView);
    m_model->refresh(); // (seems superfluous)
}

void LibraryWidget::setRecentlyPlayedView()
{
    m_model->setViewMode(LibraryModel::RecentlyPlayedView);
    m_model->refresh(); // (seems superfluous)
}

void LibraryWidget::setUnratedView()
{
    m_model->setViewMode(LibraryModel::UnratedView);
    m_model->refresh(); // (seems superfluous)
}
