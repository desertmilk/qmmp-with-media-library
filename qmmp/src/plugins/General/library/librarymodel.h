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
        MostPlayedView,
        RecentlyPlayedView,
        UnratedView
    };

    LibraryModel(QObject *parent = nullptr);
    ~LibraryModel();

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    void setFilter(const QString &filter);
    void setViewMode(ViewMode mode);
    ViewMode viewMode() const;
    void refresh();
    void add(const QModelIndexList &indexes);
    void replace(const QModelIndexList &indexes);
    void showTrackInformation(const QModelIndexList &indexes, QWidget *parent = nullptr);
    void showLibraryInformation(QWidget *parent = nullptr);

private:
    QList<PlayListTrack *> getTracks(const QModelIndexList &indexes) const;
    QList<PlayListTrack *> getTracks(const QModelIndex &index) const;
    PlayListTrack *createTrack(const QSqlQuery &query) const;

    LibraryTreeItem *m_rootItem;
    QString m_filter;
    bool m_showYear;
    ViewMode m_viewMode = ArtistView;
};

#endif // LIBRARYMODEL_H
