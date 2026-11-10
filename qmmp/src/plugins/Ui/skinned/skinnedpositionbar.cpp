/***************************************************************************
 *   Copyright (C) 2009-2026 by Ilya Kotov                                 *
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

#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <math.h>
#include <qmmp/qmmp.h>
#include "skin.h"
#include "skinnedbutton.h"
#include "skinnedpositionbar.h"

SkinnedPositionBar::SkinnedPositionBar(QWidget *parent) : PixmapWidget(parent)
{
    const QPixmap posBar = skin()->getPosBar();
    qCInfo(plugin) << "SkinnedPositionBar constructing"
                   << "parent" << static_cast<const void *>(parent)
                   << "pixmapSize" << posBar.size()
                   << "pixmapNull" << posBar.isNull()
                   << "pixelSize" << posBar.depth();
    setPixmap(posBar);
    draw(false);
    setCursor(skin()->getCursor(Skin::CUR_POSBAR));
    qCInfo(plugin) << "SkinnedPositionBar constructed"
                   << "parent" << static_cast<const void *>(parentWidget())
                   << "geometry" << geometry()
                   << "visible" << isVisible()
                   << "enabled" << isEnabled()
                   << "transparentForMouseEvents"
                   << testAttribute(Qt::WA_TransparentForMouseEvents);
}

void SkinnedPositionBar::mousePressEvent(QMouseEvent *e)
{
    qCInfo(plugin) << "SkinnedPositionBar press"
                   << "button" << e->button()
                   << "position" << e->position()
                   << "globalPosition" << e->globalPosition()
                   << "geometry" << geometry()
                   << "value" << m_value
                   << "maximum" << m_max;
    e->accept();
    if(m_max <= 0)
        return;
    m_moving = true;
    press_pos = e->position().x();
    if(m_pos < e->position().x() && e->position().x() < m_pos + 29 * skin()->ratio())
    {
        press_pos = e->position().x() - m_pos;
        emit sliderPressed();
    }
    else
    {
        m_value = convert(qMax(qMin(width() - qRound(30 * skin()->ratio()),
                                    qRound(e->position().x()) - qRound(15 * skin()->ratio())), 0));
        press_pos = qRound(15 * skin()->ratio());
        emit sliderPressed();
        if(m_value != m_old)
        {
            emit sliderMoved(m_value);

        }
    }
    draw();
}

void SkinnedPositionBar::mouseMoveEvent(QMouseEvent *e)
{
    e->accept();
    if(m_moving)
    {
        qint64 po = e->position().x();
        po = po - press_pos;

        if(0<=po && po <= width() - 30 * skin()->ratio())
        {
            m_value = convert(po);
            draw();
            qCInfo(plugin) << "SkinnedPositionBar moved" << "value" << m_value;
            emit sliderMoved(m_value);
        }
    }
}

void SkinnedPositionBar::wheelEvent(QWheelEvent *e)
{
    if(m_max == 0)
        return;

    if(e->angleDelta().y() > 0)
        m_value += 5000;
    else
        m_value -= 5000;

    m_value = qBound(qint64(0), m_value, m_max);
    draw();
    e->accept();
    emit sliderReleased();
}

void SkinnedPositionBar::mouseReleaseEvent(QMouseEvent *e)
{
    qCInfo(plugin) << "SkinnedPositionBar release"
                   << "position" << e->position()
                   << "moving" << m_moving
                   << "value" << m_value
                   << "maximum" << m_max;
    e->accept();
    draw(false);
    if(m_value != m_old && m_max > 0)
        m_old = m_value;
    m_moving = false;
    if(m_max > 0)
        emit sliderReleased();
}

void SkinnedPositionBar::setValue(qint64 v)
{
    if(m_moving || m_max == 0)
        return;
    m_value = v;
    draw(false);
}

void SkinnedPositionBar::setMaximum(qint64 max)
{
    m_max = max;
    qCInfo(plugin) << "SkinnedPositionBar maximum changed" << m_max;
    draw(false);
}

void SkinnedPositionBar::updateSkin()
{
    const QPixmap posBar = skin()->getPosBar();
    qCInfo(plugin) << "SkinnedPositionBar skin update"
                   << "pixmapSize" << posBar.size()
                   << "pixmapNull" << posBar.isNull()
                   << "oldGeometry" << geometry();
    resize(posBar.size());
    draw(false);
    setCursor(skin()->getCursor(Skin::CUR_POSBAR));
    qCInfo(plugin) << "SkinnedPositionBar skin updated"
                   << "geometry" << geometry()
                   << "visible" << isVisible()
                   << "enabled" << isEnabled();
}

void SkinnedPositionBar::draw(bool pressed)
{
    qint64 p = qint64(ceil(double(m_value-m_min) * (width() - 30 * skin()->ratio()) / (m_max-m_min)));
    m_pixmap = skin()->getPosBar();
    if(m_max > 0)
    {
        QPainter paint(&m_pixmap);
        if(pressed)
            paint.drawPixmap(p,0,skin()->getButton(Skin::BT_POSBAR_P));
        else
            paint.drawPixmap(p,0,skin()->getButton(Skin::BT_POSBAR_N));
    }
    setPixmap(m_pixmap);
    m_pos = p;
}

qint64 SkinnedPositionBar::convert(qint64 p)
{
    return qint64(ceil(double(m_max - m_min) * (p) / (width() - 30 * skin()->ratio()) + m_min));
}
