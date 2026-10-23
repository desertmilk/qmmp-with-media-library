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
#include <QImage>
#include <QImageReader>
#include <QFile>
#include <QSettings>
#include <QTextStream>
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
    QString directory;
    if(skinPath.startsWith(u":"_s))
        directory = skinPath;
    else if(QFileInfo(skinPath).isDir())
        directory = skinPath;
    else
        directory = Qmmp::cacheDir() + u"/skinned/skin"_s;

    QDir skinDirectory(directory);
    const QString requestedPath = skinDirectory.filePath(fileName);
    if(QFileInfo::exists(requestedPath))
        return requestedPath;

    const QString baseName = QFileInfo(fileName).completeBaseName();
    const QFileInfoList matches = skinDirectory.entryInfoList({baseName + u".*"_s},
                                                               QDir::Files | QDir::Hidden,
                                                               QDir::Name);
    for(const QFileInfo &match : matches)
    {
        if(!QImageReader::imageFormat(match.filePath()).isEmpty())
            return match.filePath();
    }
    return requestedPath;
}

QString QmmpUiSkin::playlistValue(const QString &key)
{
    QFile file(filePath(u"pledit.txt"_s));
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QTextStream stream(&file);
    while(!stream.atEnd())
    {
        QString line = stream.readLine().trimmed();
        line.remove(u'"');
        const qsizetype comment = line.indexOf(u"//"_s);
        if(comment >= 0)
            line.truncate(comment);
        const qsizetype separator = line.indexOf(u'=');
        if(separator < 0)
            continue;

        if(line.left(separator).trimmed().compare(key, Qt::CaseInsensitive) == 0)
            return line.mid(separator + 1).trimmed();
    }
    return QString();
}

QColor QmmpUiSkin::generalColor(int x)
{
    const QImage image(filePath(u"genex.bmp"_s));
    if(image.isNull() || x < 0 || x >= image.width())
        return {};
    return QColor::fromRgb(image.pixel(x, 0));
}

QPixmap QmmpUiSkin::letter(QChar ch)
{
    const QPixmap image(filePath(u"text.png"_s));
    if(image.isNull())
        return QPixmap();

    const QChar lower = ch.toLower();
    int row = 2;
    int column = 5;
    if(lower >= u'a' && lower <= u'z')
    {
        row = 0;
        column = lower.unicode() - u'a';
    }
    else if(lower >= u'0' && lower <= u'9')
    {
        row = 1;
        column = lower.unicode() - u'0';
    }
    else if(lower == QChar::Space)
    {
        row = 2;
        column = 5;
    }
    else
    {
        const QString special = u"\"@:()-'!_+/[]^&%.,=$#"_s;
        column = special.indexOf(lower);
        if(column < 0)
            return image.copy(25, 12, 5, 6);
        row = column < 2 ? 0 : 1;
        column = row == 0 ? column + 27 : column + 10;
    }
    return image.copy(column * 5, row * 6, 5, 6);
}

QColor QmmpUiSkin::backgroundColor()
{
    const QColor generalColor = QmmpUiSkin::generalColor(52);
    if(generalColor.isValid())
        return generalColor;

    QColor playlistColor;
    playlistColor.setNamedColor(playlistValue(u"NormalBG"_s));
    if(playlistColor.isValid())
        return playlistColor;
    const QImage image(filePath(u"text.png"_s));
    return image.isNull() ? QColor() : QColor::fromRgb(image.pixel(144, 3));
}

QColor QmmpUiSkin::foregroundColor()
{
    const QColor generalColor = QmmpUiSkin::generalColor(56);
    if(generalColor.isValid())
        return generalColor;

    QColor playlistColor;
    playlistColor.setNamedColor(playlistValue(u"Normal"_s));
    if(playlistColor.isValid())
        return playlistColor;
    const QImage image(filePath(u"text.png"_s));
    if(image.isNull())
        return QColor();

    const QRgb background = image.pixel(144, 3);
    QRgb foreground = 0;
    uint difference = 0;
    for(int x = 0; x < image.width(); ++x)
    {
        for(int y = 0; y < image.height(); ++y)
        {
            const QRgb color = image.pixel(x, y);
            const uint currentDifference = qAbs(qRed(background) - qRed(color)) +
                    qAbs(qGreen(background) - qGreen(color)) +
                    qAbs(qBlue(background) - qBlue(color));
            if(currentDifference > difference)
            {
                difference = currentDifference;
                foreground = color;
            }
        }
    }
    return QColor::fromRgb(foreground);
}