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

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>
#include <QMimeData>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWidget>
#include <QMessageBox>
#include <algorithm>
#include <qmmp/qmmp.h>
#include <qmmp/soundcore.h>
#include <qmmpui/playlistparser.h>
#include <qmmpui/playlistmanager.h>
#include <qmmpui/detailsdialog.h>
#include <qmmpui/mediaplayer.h>
#include "librarymodel.h"

#define CONNECTION_NAME u"qmmp_library_view"_s

class LibraryTreeItem
{
public:
    LibraryTreeItem() {}
    ~LibraryTreeItem()
    {
        clear();
    }

    void clear()
    {
        name.clear();
        album.clear();
        track = 0;
        id = -1;
        year = 0;
        playCount = 0;
        lastPlayed = 0;
        type = Qmmp::UNKNOWN;
        parent = nullptr;
        qDeleteAll(children);
        children.clear();
    }

    QString name;
    QString artist;
    QString album;
    qint64 id = -1;
    int track = 0;
    int year = 0;
    int playCount = 0;
    qint64 lastPlayed = 0;
    Qmmp::MetaData type = Qmmp::UNKNOWN;
    QList<LibraryTreeItem *> children;
    LibraryTreeItem *parent = nullptr;
};

LibraryModel::LibraryModel(QObject *parent) : QAbstractItemModel(parent)
{
    m_rootItem = new LibraryTreeItem;
    QSettings settings;
    m_showYear = settings.value(u"Library/show_year"_s, true).toBool();
    refresh();
}

LibraryModel::~LibraryModel()
{
    delete m_rootItem;

    if(QSqlDatabase::contains(CONNECTION_NAME))
    {
        QSqlDatabase::database(CONNECTION_NAME).close();
        QSqlDatabase::removeDatabase(CONNECTION_NAME);
    }
}

Qt::ItemFlags LibraryModel::flags(const QModelIndex &index) const
{
    if(index.isValid())
        return QAbstractItemModel::flags(index) | Qt::ItemIsDragEnabled;

    return QAbstractItemModel::flags(index);
}

QStringList LibraryModel::mimeTypes() const
{
    return { u"application/json"_s };
}

QMimeData *LibraryModel::mimeData(const QModelIndexList &indexes) const
{
    QList<PlayListTrack *> tracks = getTracks(indexes);

    if(!tracks.isEmpty())
    {
        QMimeData *mimeData = new QMimeData;
        mimeData->setData(u"application/json"_s, PlayListParser::serialize(tracks));
        qDeleteAll(tracks);
        return mimeData;
    }

    return nullptr;
}

bool LibraryModel::canFetchMore(const QModelIndex &parent) const
{
    if(!parent.isValid())
        return false;

    LibraryTreeItem *parentItem = static_cast<LibraryTreeItem *>(parent.internalPointer());
    if(parentItem == m_rootItem || parentItem->type == Qmmp::TITLE)
        return false;

    return parentItem->children.isEmpty();
}

void LibraryModel::fetchMore(const QModelIndex &parent)
{
    if(!parent.isValid())
        return;

    LibraryTreeItem *parentItem = static_cast<LibraryTreeItem *>(parent.internalPointer());

    QSqlDatabase db = QSqlDatabase::database(CONNECTION_NAME);
    if(!db.isOpen())
        return;

    if(parentItem->type == Qmmp::ARTIST)
    {
        QSqlQuery query(db);
        if(m_filter.isEmpty())
        {
            query.prepare(u"SELECT DISTINCT Album, Year from track_library WHERE Artist = :artist ORDER BY Album, Year"_s);
        }
        else
        {
            query.prepare(u"SELECT DISTINCT Album, Year from track_library WHERE Artist = :artist AND SearchString LIKE :filter ORDER BY Album, Year"_s);
            query.bindValue(u":filter"_s, QStringLiteral("%%1%").arg(m_filter.toLower()));
        }
        query.bindValue(u":artist"_s, parentItem->name);

        if(!query.exec())
        {
            qCWarning(plugin, "exec error: %s", qPrintable(query.lastError().text()));
            return;
        }

        while(query.next())
        {
            LibraryTreeItem *item = new LibraryTreeItem;
            item->name = query.value(u"Album"_s).toString();
            item->year = query.value(u"Year"_s).toInt();
            item->type = Qmmp::ALBUM;
            item->parent = parentItem;
            parentItem->children << item;
        }

        if(m_sortColumn >= 0)
            sort(m_sortColumn, m_sortOrder);
    }
    else if(parentItem->type == Qmmp::ALBUM)
    {
        QSqlQuery query(db);
        if(m_filter.isEmpty())
        {
            query.prepare(u"SELECT ID, Title, Track from track_library WHERE Artist = :artist AND Album = :album "
                          "ORDER BY DiscNumber, Track, ID"_s);
        }
        else
        {
            query.prepare(u"SELECT ID, Title, Track from track_library WHERE Artist = :artist AND Album = :album "
                          "AND SearchString LIKE :filter ORDER BY DiscNumber, Track, ID"_s);
            query.bindValue(u":filter"_s, QStringLiteral("%%1%").arg(m_filter.toLower()));
        }
        query.bindValue(u":artist"_s, parentItem->artist.isEmpty() ? parentItem->parent->name : parentItem->artist);
        query.bindValue(u":album"_s, parentItem->name);

        if(!query.exec())
        {
            qCWarning(plugin, "exec error: %s", qPrintable(query.lastError().text()));
            return;
        }

        while(query.next())
        {
            LibraryTreeItem *item = new LibraryTreeItem;
            item->id = query.value(u"ID"_s).toLongLong();
            item->name = query.value(u"Title"_s).toString();
            item->track = query.value(u"Track"_s).toInt();
            item->type = Qmmp::TITLE;
            item->parent = parentItem;
            parentItem->children << item;
        }

        if(m_sortColumn >= 0)
            sort(m_sortColumn, m_sortOrder);
    }
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    LibraryTreeItem *item = static_cast<LibraryTreeItem *>(index.internalPointer());
    switch(index.column())
    {
    case 0:
        return item->type == Qmmp::TITLE && item->track > 0 ? QString::number(item->track) : QString();
    case 1:
        if(item->type == Qmmp::ARTIST && m_viewMode == MostPlayedView && item->playCount > 0)
            return tr("%1 (%2)").arg(item->name).arg(item->playCount);
        if(item->type == Qmmp::ARTIST && m_viewMode == RecentlyPlayedView && item->lastPlayed > 0)
            return tr("%1 (%2)").arg(item->name).arg(QDateTime::fromMSecsSinceEpoch(item->lastPlayed).toString(Qt::ISODate));
        if(item->type == Qmmp::ARTIST)
            return item->name;
        if(item->type == Qmmp::ALBUM)
            return item->artist.isEmpty() && item->parent ? item->parent->name : item->artist;
        if(item->type == Qmmp::TITLE && m_viewMode == TrackView)
            return item->artist;
        if(item->type == Qmmp::TITLE && item->parent && item->parent->parent)
            return item->parent->artist.isEmpty() ? item->parent->parent->name : item->parent->artist;
        return QString();
    case 2:
        if(item->type == Qmmp::ALBUM)
            return item->name;
        if(item->type == Qmmp::TITLE && !item->album.isEmpty())
            return item->album;
        if(item->type == Qmmp::TITLE && item->parent)
            return item->parent->name;
        return QString();
    case 3:
        return item->type == Qmmp::TITLE ? item->name : QString();
    case 4:
        if(!m_showYear)
            return QString();
        if(item->type == Qmmp::ALBUM)
            return item->year > 0 ? QString::number(item->year) : QString();
        if(item->type == Qmmp::TITLE && m_viewMode == TrackView)
            return item->year > 0 ? QString::number(item->year) : QString();
        if(item->type == Qmmp::TITLE && item->parent)
            return item->parent->year > 0 ? QString::number(item->parent->year) : QString();
        return QString();
    default:
        return QVariant();
    }
}

QVariant LibraryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch(section)
    {
    case 0:
        return tr("Track #");
    case 1:
        return tr("Artist");
    case 2:
        return tr("Album");
    case 3:
        return tr("Track");
    case 4:
        return tr("Year");
    default:
        return QVariant();
    }
}

void LibraryModel::sort(int column, Qt::SortOrder order)
{
    if(column < 0 || column >= columnCount(QModelIndex()))
        return;

    m_sortColumn = column;
    m_sortOrder = order;

    const auto sortChildren = [this](LibraryTreeItem *parentItem, auto &&sortChildren) -> void
    {
        std::stable_sort(parentItem->children.begin(), parentItem->children.end(), [this](const LibraryTreeItem *left, const LibraryTreeItem *right)
        {
            const QModelIndex leftIndex = createIndex(0, m_sortColumn, const_cast<LibraryTreeItem *>(left));
            const QModelIndex rightIndex = createIndex(0, m_sortColumn, const_cast<LibraryTreeItem *>(right));
            const QVariant leftValue = data(leftIndex);
            const QVariant rightValue = data(rightIndex);
            const int result = (m_sortColumn == 0 || m_sortColumn == 4) ?
                        leftValue.toInt() - rightValue.toInt() :
                        QString::localeAwareCompare(leftValue.toString(), rightValue.toString());

            return m_sortOrder == Qt::AscendingOrder ? result < 0 : result > 0;
        });

        for(LibraryTreeItem *child : parentItem->children)
            sortChildren(child, sortChildren);
    };

    beginResetModel();
    sortChildren(m_rootItem, sortChildren);
    endResetModel();
}

QModelIndex LibraryModel::parent(const QModelIndex &child) const
{
    if(!child.isValid())
        return QModelIndex();

    LibraryTreeItem *childItem = static_cast<LibraryTreeItem *>(child.internalPointer());
    LibraryTreeItem *parentItem = childItem->parent;

    if(parentItem == m_rootItem || !parentItem || !parentItem->parent)
        return QModelIndex();

    return createIndex(parentItem->parent->children.indexOf(parentItem), 0, parentItem);
}

QModelIndex LibraryModel::index(int row, int column, const QModelIndex &parent) const
{
    if(parent.isValid() && parent.column() != 0)
        return QModelIndex();

    LibraryTreeItem *parentItem = parent.isValid() ? static_cast<LibraryTreeItem *>(parent.internalPointer()) :
                                                      m_rootItem;

    if(row >= 0 && row < parentItem->children.count())
        return createIndex(row, column, parentItem->children.at(row));

    return QModelIndex();
}

int LibraryModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 5;
}

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    if(!parent.isValid())
        return m_rootItem->children.count();

    LibraryTreeItem *parentItem = static_cast<LibraryTreeItem *>(parent.internalPointer());
    if(parentItem->type == Qmmp::TITLE)
        return 0;

    return qMax(1, parentItem->children.count());
}

void LibraryModel::setFilter(const QString &filter)
{
    m_filter = filter;
}

void LibraryModel::setTrackFilter(const QString &artist, const QString &album)
{
    m_viewMode = TrackView;
    m_artistFilter = artist;
    m_albumFilter = album;
    refresh();
}

void LibraryModel::setViewMode(ViewMode mode)
{
    if(m_viewMode == mode)
        return;

    beginResetModel();
    m_viewMode = mode;
    refresh();
    endResetModel();
}

LibraryModel::ViewMode LibraryModel::viewMode() const
{
    return m_viewMode;
}

void LibraryModel::refresh()
{
    beginResetModel();
    m_rootItem->clear();

    QSqlDatabase db;

    if(QSqlDatabase::contains(CONNECTION_NAME))
    {
        db = QSqlDatabase::database(CONNECTION_NAME);
    }
    else
    {
        db = QSqlDatabase::addDatabase(u"QSQLITE"_s, CONNECTION_NAME);
        db.setDatabaseName(Qmmp::configDir() + u"/library.sqlite"_s);
        db.open();
    }

    if(!db.isOpen())
    {
        endResetModel();
        return;
    }

    QSqlQuery query(db);
    QString sql;

    if(m_filter.isEmpty())
    {
        switch(m_viewMode)
        {
        case TrackView:
            sql = u"SELECT ID, Title, Artist, Album, Year, Track FROM track_library"_s;
            break;
        case ArtistView:
            sql = u"SELECT DISTINCT Artist FROM track_library ORDER BY Artist"_s;
            break;
        case AlbumView:
            sql = u"SELECT Album, Artist, MAX(Year) AS Year FROM track_library GROUP BY Artist, Album ORDER BY Album, Artist"_s;
            break;
        case MostPlayedView:
            sql = u"SELECT Artist, MAX(PlayCount) AS PlayCount FROM track_library WHERE PlayCount > 0 GROUP BY Artist ORDER BY PlayCount DESC, Artist"_s;
            break;
        case RecentlyPlayedView:
            sql = u"SELECT Artist, MAX(LastPlayed) AS LastPlayed FROM track_library WHERE LastPlayed > 0 GROUP BY Artist ORDER BY LastPlayed DESC, Artist"_s;
            break;
        case UnratedView:
            sql = u"SELECT DISTINCT Artist FROM track_library WHERE Rating = 0 ORDER BY Artist"_s;
            break;
        }
    }
    else
    {
        switch(m_viewMode)
        {
        case TrackView:
            sql = u"SELECT ID, Title, Artist, Album, Year, Track FROM track_library WHERE SearchString LIKE :filter"_s;
            break;
        case ArtistView:
            sql = u"SELECT DISTINCT Artist FROM track_library WHERE SearchString LIKE :filter ORDER BY Artist"_s;
            break;
        case AlbumView:
            sql = u"SELECT Album, Artist, MAX(Year) AS Year FROM track_library WHERE SearchString LIKE :filter "
                  "GROUP BY Artist, Album ORDER BY Album, Artist"_s;
            break;
        case MostPlayedView:
            sql = u"SELECT Artist, MAX(PlayCount) AS PlayCount FROM track_library WHERE SearchString LIKE :filter AND PlayCount > 0 GROUP BY Artist ORDER BY PlayCount DESC, Artist"_s;
            break;
        case RecentlyPlayedView:
            sql = u"SELECT Artist, MAX(LastPlayed) AS LastPlayed FROM track_library WHERE SearchString LIKE :filter AND LastPlayed > 0 GROUP BY Artist ORDER BY LastPlayed DESC, Artist"_s;
            break;
        case UnratedView:
            sql = u"SELECT DISTINCT Artist FROM track_library WHERE SearchString LIKE :filter AND Rating = 0 ORDER BY Artist"_s;
            break;
        }
        query.bindValue(u":filter"_s, QStringLiteral("%%1%").arg(m_filter.toLower()));
    }

    if(m_viewMode == TrackView)
    {
        QStringList conditions;
        if(!m_artistFilter.isEmpty())
            conditions << u"Artist = :artist"_s;
        if(!m_albumFilter.isEmpty())
            conditions << u"Album = :album"_s;
        if(!conditions.isEmpty())
            sql += (sql.contains(u" WHERE "_s) ? u" AND "_s : u" WHERE "_s) + conditions.join(u" AND "_s);
        sql += u" ORDER BY Artist, Album, DiscNumber, Track, ID"_s;
    }

    query.prepare(sql);
    if(m_viewMode == TrackView && !m_filter.isEmpty())
        query.bindValue(u":filter"_s, QStringLiteral("%%1%").arg(m_filter.toLower()));
    if(m_viewMode == TrackView)
    {
        if(!m_artistFilter.isEmpty())
            query.bindValue(u":artist"_s, m_artistFilter);
        if(!m_albumFilter.isEmpty())
            query.bindValue(u":album"_s, m_albumFilter);
    }
    if(!query.exec())
        qCWarning(plugin, "exec error: %s", qPrintable(query.lastError().text()));

    while(query.next())
    {
        LibraryTreeItem *item = new LibraryTreeItem;
        item->name = query.value(u"Artist"_s).toString();
        if(m_viewMode == TrackView)
        {
            item->id = query.value(u"ID"_s).toLongLong();
            item->name = query.value(u"Title"_s).toString();
            item->artist = query.value(u"Artist"_s).toString();
            item->album = query.value(u"Album"_s).toString();
            item->track = query.value(u"Track"_s).toInt();
            item->year = query.value(u"Year"_s).toInt();
            item->type = Qmmp::TITLE;
        }
        if(m_viewMode == AlbumView)
        {
            item->name = query.value(u"Album"_s).toString();
            item->artist = query.value(u"Artist"_s).toString();
            item->year = query.value(u"Year"_s).toInt();
            item->type = Qmmp::ALBUM;
        }
        if(m_viewMode == MostPlayedView)
            item->playCount = query.value(u"PlayCount"_s).toInt();
        else if(m_viewMode == RecentlyPlayedView)
            item->lastPlayed = query.value(u"LastPlayed"_s).toLongLong();
        if(item->type == Qmmp::UNKNOWN)
            item->type = Qmmp::ARTIST;
        item->parent = m_rootItem;
        m_rootItem->children << item;
    }
    endResetModel();
}

void LibraryModel::add(const QModelIndexList &indexes)
{
    PlayListManager::instance()->addTracks(getTracks(indexes));
}

void LibraryModel::replace(const QModelIndexList &indexes)
{
    QList<PlayListTrack *> tracks = getTracks(indexes);
    if(tracks.isEmpty())
        return;

    SoundCore *core = SoundCore::instance();
    PlayListManager *manager = PlayListManager::instance();
    PlayListModel *model = PlayListManager::instance()->selectedPlayList();

    bool play = (core->state() == Qmmp::Playing || core->state() == Qmmp::Paused || core->state() == Qmmp::Buffering) &&
            model == manager->currentPlayList();

    model->clear();
    model->addTracks(tracks);

    if(play)
    {
        MediaPlayer::instance()->stop();
        MediaPlayer::instance()->play();
    }
}

void LibraryModel::replaceAndPlay(const QModelIndex &index)
{
    if(!index.isValid() || index.column() != 0)
        return;

    const LibraryTreeItem *item = static_cast<const LibraryTreeItem *>(index.internalPointer());
    if(item->type != Qmmp::TITLE)
        return;

    QList<PlayListTrack *> clickedTracks = getTracks(index);
    if(clickedTracks.isEmpty())
        return;
    const QString clickedPath = clickedTracks.constFirst()->path();
    qDeleteAll(clickedTracks);

    QList<PlayListTrack *> tracks = getFilteredTracks();
    if(tracks.isEmpty())
        return;

    PlayListManager *manager = PlayListManager::instance();
    PlayListModel *model = manager->selectedPlayList();
    model->clear();
    model->addTracks(tracks);

    for(int i = 0; i < tracks.size(); ++i)
    {
        if(tracks.at(i)->path() == clickedPath)
        {
            model->setCurrent(i);
            break;
        }
    }
    manager->activateSelectedPlayList();
    MediaPlayer::instance()->play();
}

void LibraryModel::showTrackInformation(const QModelIndexList &indexes, QWidget *parent)
{
    QList<PlayListTrack *> tracks = getTracks(indexes);
    if(tracks.isEmpty())
        return;

    DetailsDialog *dialog = new DetailsDialog(tracks, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->show();
    connect(dialog, &QObject::destroyed, [=]() { qDeleteAll(tracks); });
}

void LibraryModel::showLibraryInformation(QWidget *parent)
{
    QSqlDatabase db = QSqlDatabase::database(CONNECTION_NAME);
    if(!db.isOpen())
    {
        QMessageBox::critical(parent, tr("Error"), tr("Unable to connect to database"));
        return;
    }

    QSqlQuery query(db);
    query.prepare(u"select COUNT(id),COUNT(DISTINCT Album||Artist),COUNT(DISTINCT Artist),SUM(Duration),SUM(PlayCount),MAX(LastPlayed) from track_library"_s);

    if(!query.exec())
    {
        qCWarning(plugin, "exec error: %s", qPrintable(query.lastError().text()));
        return;
    }
    query.next();

    int tracks = query.value(0).toInt();
    int albums = query.value(1).toInt();
    int artists = query.value(2).toInt();
    qint64 duration = query.value(3).toLongLong() / 1000;
    qint64 totalPlays = query.value(4).toLongLong();
    qint64 lastPlayed = query.value(5).toLongLong();
    int days = duration / (3600 * 24);
    int hours = (duration / 3600) % 24;

    QString daysText = tr("%n day(s)", "", days);
    QString hoursText = tr("%n hour(s)", "", hours);
    QString minutesText = tr("%n minute(s)", "", duration % 3600 / 60);
    QString secondsText = tr("%n second(s)", "", duration % 60);
    QString durationText;

    if(days > 0)
        durationText = tr("%1 %2 %3 %4", "days hours minutes seconds").arg(daysText, hoursText, minutesText, secondsText);
    else if(hours > 0)
        durationText = tr("%1 %2 %3", "hours minutes seconds").arg(hoursText, minutesText, secondsText);
    else
        durationText = tr("%1 %2", "minutes seconds").arg(minutesText, secondsText);

    QStringList lines = {
        tr("Number of tracks: <b>%1</b>").arg(tracks),
        tr("Number of albums: <b>%1</b>").arg(albums),
        tr("Number of artists: <b>%1</b>").arg(artists),
        tr("Total duration: <b>%1</b>").arg(durationText),
        tr("Total plays: <b>%1</b>").arg(totalPlays),
        tr("Latest play: <b>%1</b>").arg(lastPlayed > 0 ? QLocale().toString(QDateTime::fromMSecsSinceEpoch(lastPlayed), QLocale::ShortFormat) : tr("Never")),
    };

    QMessageBox::information(parent, tr("Library Information"), lines.join(u"<br>"_s));
}

QList<PlayListTrack *> LibraryModel::getTracks(const QModelIndexList &indexes) const
{
    QList<PlayListTrack *> tracks;

    for(const QModelIndex &index : indexes)
    {
        if(index.isValid() && index.column() == 0)
        {
            tracks << getTracks(index);
        }
    }

    return tracks;
}

QList<PlayListTrack *> LibraryModel::getTracks(const QModelIndex &index) const
{
    QSqlDatabase db = QSqlDatabase::database(CONNECTION_NAME);
    QList<PlayListTrack *> tracks;
    if(!db.isOpen())
        return tracks;

    const LibraryTreeItem *item = static_cast<const LibraryTreeItem *>(index.internalPointer());

    if(item->type == Qmmp::TITLE)
    {
        QSqlQuery query(db);
        query.prepare(u"SELECT * from track_library WHERE ID = :id"_s);
        query.bindValue(u":id"_s, item->id);

        if(!query.exec())
        {
            qCWarning(plugin, "exec error: %s", qPrintable(query.lastError().text()));
            return tracks;
        }

        if(query.next())
        {
            tracks << createTrack(query);
        }
    }
    else if(item->type == Qmmp::ALBUM)
    {
        QSqlQuery query(db);
        query.prepare(u"SELECT * from track_library WHERE Artist = :artist AND Album = :album "
                  "ORDER BY DiscNumber, Track, ID"_s);
        query.bindValue(u":artist"_s, item->artist.isEmpty() ? item->parent->name : item->artist);
        query.bindValue(u":album"_s, item->name);

        if(!query.exec())
        {
            qCWarning(plugin, "exec error: %s", qPrintable(query.lastError().text()));
            return tracks;
        }

        while(query.next())
        {
           tracks << createTrack(query);
        }
    }
    else if(item->type == Qmmp::ARTIST)
    {
        QSqlQuery query(db);
        query.prepare(u"SELECT * from track_library WHERE Artist = :artist"_s);
        query.bindValue(u":artist"_s, item->name);

        if(!query.exec())
        {
            qCWarning(plugin, "exec error: %s", qPrintable(query.lastError().text()));
            return tracks;
        }

        while(query.next())
        {
            tracks << createTrack(query);
        }
    }

    return tracks;
}

QList<PlayListTrack *> LibraryModel::getFilteredTracks() const
{
    QSqlDatabase db = QSqlDatabase::database(CONNECTION_NAME);
    QList<PlayListTrack *> tracks;
    if(!db.isOpen())
        return tracks;

    QString sql = u"SELECT * from track_library WHERE SearchString LIKE :filter"_s;
    if(m_viewMode == TrackView && !m_artistFilter.isEmpty())
        sql += u" AND Artist = :artist"_s;
    if(m_viewMode == TrackView && !m_albumFilter.isEmpty())
        sql += u" AND Album = :album"_s;
    sql += u" ORDER BY Artist, Album, DiscNumber, Track, ID"_s;

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(u":filter"_s, QStringLiteral("%%1%").arg(m_filter.toLower()));
    if(m_viewMode == TrackView && !m_artistFilter.isEmpty())
        query.bindValue(u":artist"_s, m_artistFilter);
    if(m_viewMode == TrackView && !m_albumFilter.isEmpty())
        query.bindValue(u":album"_s, m_albumFilter);

    if(!query.exec())
    {
        qCWarning(plugin, "exec error: %s", qPrintable(query.lastError().text()));
        return tracks;
    }

    while(query.next())
        tracks << createTrack(query);

    return tracks;
}

PlayListTrack *LibraryModel::createTrack(const QSqlQuery &query) const
{
    static const QHash<Qmmp::MetaData , QString> metaColumns = {
        { Qmmp::TITLE, u"Title"_s },
        { Qmmp::ARTIST, u"Artist"_s },
        { Qmmp::ALBUMARTIST, u"AlbumArtist"_s },
        { Qmmp::ALBUM, u"Album"_s },
        { Qmmp::COMMENT, u"Comment"_s },
        { Qmmp::GENRE, u"Genre"_s },
        { Qmmp::COMPOSER, u"Composer"_s },
        { Qmmp::YEAR, u"Year"_s },
        { Qmmp::TRACK, u"Track"_s },
        { Qmmp::DISCNUMBER, u"DiscNumber"_s }
    };

    PlayListTrack *track = new PlayListTrack;
    track->setPath(query.value(u"URL"_s).toString());
    track->setDuration(query.value(u"Duration"_s).toLongLong());

    for(auto it = metaColumns.cbegin(); it != metaColumns.cend(); ++it)
    {
       QString value = query.value(it.value()).toString();
       track->setValue(it.key(), value);
    }

    QJsonDocument document = QJsonDocument::fromJson(query.value(u"AudioInfo"_s).toByteArray());
    QJsonObject obj = document.object();
    track->setValue(Qmmp::BITRATE, obj.value(u"bitrate"_s).toInt());
    track->setValue(Qmmp::SAMPLERATE, obj.value(u"samplerate"_s).toInt());
    track->setValue(Qmmp::CHANNELS, obj.value(u"channels"_s).toInt());
    track->setValue(Qmmp::BITS_PER_SAMPLE, obj.value(u"bitsPerSample"_s).toInt());
    track->setValue(Qmmp::FORMAT_NAME, obj.value(u"formatName"_s).toString());
    track->setValue(Qmmp::DECODER, obj.value(u"decoder"_s).toString());
    track->setValue(Qmmp::FILE_SIZE, qint64(obj.value(u"fileSize"_s).toDouble()));
    return track;
}
