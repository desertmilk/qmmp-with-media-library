/***************************************************************************
 *   Copyright (C) 2026 by Ilya Kotov                                      *
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

#include <QCheckBox>
#include <QAbstractItemView>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QDialog>
#include <QSettings>
#include <QVBoxLayout>
#include <qmmp/qmmp.h>
#include <qmmpui/filedialog.h>
#include "librarysettingswidget.h"

LibrarySettingsWidget::LibrarySettingsWidget(QWidget *parent) :
    QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    auto *title = new QLabel(tr("Media Library"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    title->setFont(titleFont);
    layout->addWidget(title);
    layout->addWidget(new QLabel(tr("Choose the folders Qmmp should scan for music."), this));

    auto *foldersGroup = new QGroupBox(tr("Music folders"), this);
    auto *foldersLayout = new QVBoxLayout(foldersGroup);
    m_dirsListWidget = new QListWidget(foldersGroup);
    m_dirsListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    foldersLayout->addWidget(m_dirsListWidget);

    auto *buttonsLayout = new QHBoxLayout;
    auto *addButton = new QPushButton(QIcon::fromTheme(u"list-add"_s), tr("Add Folder..."), foldersGroup);
    auto *removeButton = new QPushButton(QIcon::fromTheme(u"list-remove"_s), tr("Remove"), foldersGroup);
    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(removeButton);
    buttonsLayout->addStretch();
    foldersLayout->addLayout(buttonsLayout);
    layout->addWidget(foldersGroup, 1);

    auto *optionsGroup = new QGroupBox(tr("Library options"), this);
    auto *optionsLayout = new QVBoxLayout(optionsGroup);
    m_showYearCheckBox = new QCheckBox(tr("Show album year"), optionsGroup);
    m_recreateDatabaseCheckBox = new QCheckBox(tr("Recreate database on next startup"), optionsGroup);
    optionsLayout->addWidget(m_showYearCheckBox);
    optionsLayout->addWidget(m_recreateDatabaseCheckBox);
    layout->addWidget(optionsGroup);
    layout->addStretch();

    QSettings settings;
    m_lastPath = settings.value(u"Library/last_path"_s, QDir::homePath()).toString();
    m_dirsListWidget->addItems(settings.value(u"Library/dirs"_s).toStringList());
    m_initialDirectories = settings.value(u"Library/dirs"_s).toStringList();
    m_showYearCheckBox->setChecked(settings.value(u"Library/show_year"_s, false).toBool());
    m_recreateDatabaseCheckBox->setChecked(settings.value(u"Library/recreate_db"_s, false).toBool());

    connect(addButton, &QPushButton::clicked, this, &LibrarySettingsWidget::addDirectory);
    connect(removeButton, &QPushButton::clicked, this, &LibrarySettingsWidget::removeDirectories);
    if(auto *settingsDialog = qobject_cast<QDialog *>(parent))
    {
        connect(settingsDialog, &QDialog::accepted, this, &LibrarySettingsWidget::saveSettings);
        connect(settingsDialog, &QDialog::rejected, this, &LibrarySettingsWidget::restoreSettings);
    }
}

void LibrarySettingsWidget::addDirectory()
{
    const QString path = FileDialog::getExistingDirectory(this, tr("Select Directory for Scanning"), m_lastPath);
    if(path.isEmpty())
        return;

    const QString normalizedPath = QDir::cleanPath(path);
    for(int i = 0; i < m_dirsListWidget->count(); ++i)
    {
        if(QDir::cleanPath(m_dirsListWidget->item(i)->text()) == normalizedPath)
            return;
    }

    m_dirsListWidget->addItem(normalizedPath);
    m_lastPath = QFileInfo(normalizedPath).absolutePath();
}

void LibrarySettingsWidget::removeDirectories()
{
    const QList<QListWidgetItem *> selectedItems = m_dirsListWidget->selectedItems();
    if(selectedItems.isEmpty())
        return;

    qDeleteAll(selectedItems);
}

QStringList LibrarySettingsWidget::directories() const
{
    QStringList dirs;
    for(int i = 0; i < m_dirsListWidget->count(); ++i)
        dirs << m_dirsListWidget->item(i)->text();

    return dirs;
}

void LibrarySettingsWidget::saveSettings()
{
    const QStringList dirs = directories();
    QSettings settings;
    settings.setValue(u"Library/dirs"_s, dirs);
    settings.setValue(u"Library/last_path"_s, m_lastPath);
    settings.setValue(u"Library/show_year"_s, m_showYearCheckBox->isChecked());
    settings.setValue(u"Library/recreate_db"_s, m_recreateDatabaseCheckBox->isChecked());
    if(dirs != m_initialDirectories)
        emit directoriesChanged();
}

void LibrarySettingsWidget::restoreSettings()
{
    QSettings settings;
    m_dirsListWidget->clear();
    m_dirsListWidget->addItems(m_initialDirectories);
    m_lastPath = settings.value(u"Library/last_path"_s, QDir::homePath()).toString();
    m_showYearCheckBox->setChecked(settings.value(u"Library/show_year"_s, false).toBool());
    m_recreateDatabaseCheckBox->setChecked(settings.value(u"Library/recreate_db"_s, false).toBool());
}
