/***************************************************************************
 *   Copyright (C) 2020-2026 by Ilya Kotov                                 *
 *   forkotov02@ya.ru                                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
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

#ifndef LIBRARYMODEL_H
#define LIBRARYMODEL_H

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QAbstractItemModel>

class QWidget;
class QSqlDatabase;
class LibraryTreeItem;
class PlayListTrack;
class QSqlQuery;

class LibraryModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum ViewMode
    {
        ArtistView,
        AlbumView,
        MostPlayedView,
        RecentlyPlayedView,
        UnratedView,
        TrackView
    };

    LibraryModel(QObject *parent = nullptr);
    ~LibraryModel();

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    QModelIndex parent(const QModelIndex &child) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    void setFilter(const QString &filter);
    void setTrackFilter(const QString &artist, const QString &album);
    void setViewMode(ViewMode mode);
    ViewMode viewMode() const;
    void refresh();
    void add(const QModelIndexList &indexes);
    void addFiltered();
    void replace(const QModelIndexList &indexes);
    void replaceFiltered(bool play = false);
    void replaceAndPlay(const QModelIndex &index);
    void showTrackInformation(const QModelIndexList &indexes, QWidget *parent = nullptr);
    void showLibraryInformation(QWidget *parent = nullptr);

private:
    QList<PlayListTrack *> getTracks(const QModelIndexList &indexes) const;
    QList<PlayListTrack *> getTracks(const QModelIndex &index) const;
    QList<PlayListTrack *> getFilteredTracks() const;
    PlayListTrack *createTrack(const QSqlQuery &query) const;

    LibraryTreeItem *m_rootItem;
    QString m_filter;
    QString m_artistFilter;
    QString m_albumFilter;
    bool m_showYear;
    ViewMode m_viewMode = ArtistView;
    int m_sortColumn = -1;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};

#endif // LIBRARYMODEL_H
