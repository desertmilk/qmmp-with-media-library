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
#include <qmmp/qmmp.h>
#include <qmmpui/qmmpuiskin.h>
#include "librarymodel.h"
#include "librarysettingsdialog.h"
#include "ui_librarywidget.h"
#include "librarywidget.h"

LibraryWidget::LibraryWidget(bool dialog, QWidget *parent) :
    QWidget(parent),
    m_ui(new Ui::LibraryWidget)
{
    if(dialog)
        setObjectName(u"MediaLibrary"_s);
    m_ui->setupUi(this);
    m_model = new LibraryModel(this);
    m_ui->treeView->setModel(m_model);
    connect(m_ui->treeView, &QTreeView::doubleClicked, this, &LibraryWidget::playSelected);
        m_artistsModel = new QStandardItemModel(this);
        m_artistsModel->setHorizontalHeaderLabels({tr("Artist"), tr("Albums"), tr("Tracks")});
        m_ui->artistsTableView->setModel(m_artistsModel);
            m_ui->artistsTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_albumsModel = new QStandardItemModel(this);
        m_albumsModel->setHorizontalHeaderLabels({tr("Album"), tr("Year"), tr("Tracks")});
        m_ui->albumsTableView->setModel(m_albumsModel);
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

    QSettings settings;
    m_filterAction->setChecked(settings.value(u"Library/quick_search_visible"_s, true).toBool());
    m_ui->filterLineEdit->setVisible(m_filterAction->isChecked());
    if(dialog)
        restoreGeometry(settings.value(u"Library/geometry"_s).toByteArray());
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
    painter.drawPixmap(width - 22, 6,
                      m_skinPlaylist.copy(m_shadePressed ? 62 : 158,
                                          m_shadePressed ? 42 : 3, 9, 9));
    painter.drawPixmap(width - 13, 6,
                      m_skinPlaylist.copy(m_closePressed ? 52 : 167,
                                          m_closePressed ? 42 : 3, 9, 9));
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
        emit closed();
    }
}

void LibraryWidget::contextMenuEvent(QContextMenuEvent *e)
{
    m_menu->exec(mapToGlobal(e->pos()));
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
    m_ui->artistsTableView->resizeColumnsToContents();
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
    m_ui->albumsTableView->resizeColumnsToContents();
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
    m_model->replaceFiltered();
}

void LibraryWidget::replaceAlbums()
{
    m_model->replaceFiltered();
}

void LibraryWidget::addSelected()
{
    m_model->add(m_ui->treeView->selectionModel()->selectedIndexes());
}

void LibraryWidget::replaceSelected()
{
    m_model->replace(m_ui->treeView->selectionModel()->selectedIndexes());
}

void LibraryWidget::playSelected(const QModelIndex &index)
{
    m_model->replaceAndPlay(index);
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
