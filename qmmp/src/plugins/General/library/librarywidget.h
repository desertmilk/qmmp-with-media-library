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

#ifndef LIBRARYWIDGET_H
#define LIBRARYWIDGET_H

#include <QWidget>
#include <QPixmap>

namespace Ui {
class LibraryWidget;
}

class QMenu;
class QAction;
class QContextMenuEvent;
class QCloseEvent;
class QEvent;
class QPaintEvent;
class QResizeEvent;
class QMouseEvent;
class QLabel;
class LibraryModel;
class QStandardItemModel;
class QTableView;
class QSortFilterProxyModel;
class QAbstractItemView;
class QHeaderView;

class LibraryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LibraryWidget(bool dialog, QWidget *parent = nullptr);
    ~LibraryWidget();
    void refresh();

    void setBusyMode(bool enabled);

signals:
    void closed();
    void shadedChanged(bool shaded, int delta);

private:
    void closeEvent(QCloseEvent *) override;
    void contextMenuEvent(QContextMenuEvent *e) override;

private slots:
    void on_filterLineEdit_textChanged(const QString &text);
    void addSelected();
    void replaceSelected();
    void playSelected(const QModelIndex &index);
    void showTrackInformation();
    void showLibraryInformation();
    void showSettings();
    void setArtistView();
    void setAlbumView();
    void setMostPlayedView();
    void setRecentlyPlayedView();
    void setUnratedView();
    void refreshSummaryViews();
    void refreshArtists();
    void refreshAlbums();
    void updateTrackFilter();
    void replaceArtists();
    void replaceAlbums();

private:
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void applyPalette();
    void adjustSummaryColumnSpace(QTableView *tableView, int excludedColumn = -1,
                                  int sectionDelta = 0);
    void initializeSummaryColumnWidths(QTableView *tableView);
    bool loadSkinChrome();
    void toggleShade();
    void adjustColumnSpace(QAbstractItemView *view, QHeaderView *header, int excludedColumn = -1, int sectionDelta = 0);
    Ui::LibraryWidget *m_ui;
    LibraryModel *m_model;
    QMenu *m_menu;
    QWidget *m_contextSource = nullptr;
    QAction *m_filterAction;
    QLabel *m_busyIndicator = nullptr;
    QStandardItemModel *m_artistsModel;
    QStandardItemModel *m_albumsModel;
    QSortFilterProxyModel *m_artistsProxy;
    QSortFilterProxyModel *m_albumsProxy;
    QString m_selectedArtist;
    QString m_selectedAlbum;
    QPixmap m_skinPlaylist;
    QWidget *m_resizeWidget = nullptr;
    QPoint m_dragOffset;
    bool m_dragging = false;
    bool m_resizing = false;
    bool m_closePressed = false;
    bool m_shadePressed = false;
    bool m_shaded = false;
    bool m_distributingColumnSpace = false;
    bool m_artistsColumnsInitialized = false;
    bool m_albumsColumnsInitialized = false;
    int m_unshadedHeight = 0;
};

#endif // LIBRARYWIDGET_H
