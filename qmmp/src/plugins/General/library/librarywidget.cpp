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
#include <QMenu>
#include <QContextMenuEvent>
#include <QIcon>
#include <QLabel>
#include <QHeaderView>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QItemSelectionModel>
#include <qmmp/qmmp.h>
#include "librarymodel.h"
#include "librarysettingsdialog.h"
#include "ui_librarywidget.h"
#include "librarywidget.h"

LibraryWidget::LibraryWidget(bool dialog, QWidget *parent) :
    QWidget(parent),
    m_ui(new Ui::LibraryWidget)
{
    m_ui->setupUi(this);
    m_model = new LibraryModel(this);
    m_ui->treeView->setModel(m_model);
        m_artistsModel = new QStandardItemModel(this);
        m_artistsModel->setHorizontalHeaderLabels({tr("Artist"), tr("Albums"), tr("Tracks")});
        m_ui->artistsTableView->setModel(m_artistsModel);
        m_albumsModel = new QStandardItemModel(this);
        m_albumsModel->setHorizontalHeaderLabels({tr("Album"), tr("Year"), tr("Tracks")});
        m_ui->albumsTableView->setModel(m_albumsModel);
        connect(m_ui->artistsTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &LibraryWidget::refreshAlbums);
        connect(m_ui->albumsTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &LibraryWidget::updateTrackFilter);
        refreshSummaryViews();
    m_ui->treeView->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_ui->treeView->header()->setStretchLastSection(false);
    m_ui->treeView->setColumnWidth(0, 60);
    m_ui->treeView->setColumnWidth(1, 180);
    m_ui->treeView->setColumnWidth(2, 180);
    m_ui->treeView->setColumnWidth(3, 240);
    m_ui->treeView->setColumnWidth(4, 60);

    if(dialog)
    {
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
    query.prepare(u"SELECT Album, MAX(Year), COUNT(*) FROM track_library "
                  "WHERE SearchString LIKE :filter AND (:artist = '' OR Artist = :artist) "
                  "GROUP BY Album ORDER BY Album"_s);
    query.bindValue(u":filter"_s, QStringLiteral("%%1%").arg(filter.toLower()));
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

void LibraryWidget::addSelected()
{
    m_model->add(m_ui->treeView->selectionModel()->selectedIndexes());
}

void LibraryWidget::replaceSelected()
{
    m_model->replace(m_ui->treeView->selectionModel()->selectedIndexes());
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
