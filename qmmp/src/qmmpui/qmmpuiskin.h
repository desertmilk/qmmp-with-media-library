/***************************************************************************
 *   Copyright (C) 2026 by Ilya Kotov                                     *
 *   forkotov02@ya.ru                                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef QMMPUISKIN_H
#define QMMPUISKIN_H

#include "qmmpui_export.h"
#include <QString>

class QMMPUI_EXPORT QmmpUiSkin
{
public:
    static bool isSkinnedUi();
    static QString currentSkinPath();
    static QString filePath(const QString &fileName);
};

#endif // QMMPUISKIN_H