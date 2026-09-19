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
    if(m_model->rowCount() <= 4)
        m_ui->treeView->expandAll();
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
