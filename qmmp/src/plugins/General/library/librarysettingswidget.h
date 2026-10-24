/***************************************************************************
 *   Copyright (C) 2026 by Ilya Kotov                                      *
 *   forkotov02@ya.ru                                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef LIBRARYSETTINGSWIDGET_H
#define LIBRARYSETTINGSWIDGET_H

#include <QWidget>
#include <QStringList>

class QListWidget;
class QCheckBox;

class LibrarySettingsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LibrarySettingsWidget(QWidget *parent = nullptr);

signals:
    void directoriesChanged();

private slots:
    void addDirectory();
    void removeDirectories();
    void saveSettings();
    void restoreSettings();

private:
    QStringList directories() const;

    QListWidget *m_dirsListWidget;
    QCheckBox *m_showYearCheckBox;
    QCheckBox *m_recreateDatabaseCheckBox;
    QString m_lastPath;
    QStringList m_initialDirectories;
};

#endif // LIBRARYSETTINGSWIDGET_H
