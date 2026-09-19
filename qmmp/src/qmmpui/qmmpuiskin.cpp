/***************************************************************************
 *   Copyright (C) 2026 by Ilya Kotov                                     *
 *   forkotov02@ya.ru                                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <qmmp/qmmp.h>
#include "qmmpuiskin.h"

bool QmmpUiSkin::isSkinnedUi()
{
    return QSettings().value(u"Ui/current_plugin"_s, u"skinned"_s).toString() == u"skinned"_s;
}

QString QmmpUiSkin::currentSkinPath()
{
    return QSettings().value(u"Skinned/skin_path"_s, u":/glare"_s).toString();
}

QString QmmpUiSkin::filePath(const QString &fileName)
{
    const QString skinPath = currentSkinPath();
    if(skinPath.startsWith(u":"_s))
        return skinPath + u"/"_s + fileName;
    if(QFileInfo(skinPath).isDir())
        return QDir(skinPath).filePath(fileName);

    return QDir(Qmmp::cacheDir() + u"/skinned/skin"_s).filePath(fileName);
}