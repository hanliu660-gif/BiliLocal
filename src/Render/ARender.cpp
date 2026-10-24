/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    ARender.cpp
*   Time:        2013/12/27
*   Author:      Lysine
*
*   Lysine is a student majoring in Software Engineering
*   from the School of Software, SUN YAT-SEN UNIVERSITY.
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.

*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
=========================================================================*/

#include "Common.h"
#include "ARender.h"
#include "ARenderPrivate.h"
#include "../Local.h"
#include "../Model/Running.h"
#include "../Player/APlayer.h"

#include "OpenGL/OpenGLRender.h"

// 2026-10-24 单平台化：视频由 libvlc 经 Embed 输出直出（d3d11），
// ARender 只负责弹幕渲染调度；vmem 视频绘制路径（drawPlay/drawStop/
// setFormat/setBuffer/宽高比/首选尺寸）已随之移除。

QStringList ARender::getModules()
{
	return{ "OpenGL" };
}

ARender::ARender(QObject *parent)
	: QObject(parent), d_ptr(nullptr)
{
	setObjectName("ARender");
}

ARender *ARender::create(QObject *parent, QString name)
{
	Q_UNUSED(name);
	return new OpenGLRender(parent);
}

void ARender::setup()
{
	// 播放状态切换时让弹幕动画计时失效，暂停后恢复不追帧
	connect(lApp->findObject<APlayer>(), &APlayer::stateChanged, this, [this](){
		d_ptr->timer.invalidate();
	});
}

ARender::~ARender()
{
	delete d_ptr;
}

void ARender::draw()
{
	QRect rect;
	rect.setSize(getActualSize());
	draw(rect);
}

void ARender::draw(QRect rect)
{
	draw(rect.isValid() ? rect : QRect(QPoint(0, 0), getActualSize()));
}

void ARenderPrivate::drawDanm(QPainter *painter, QRect)
{
	lApp->findObject<Running>()->draw(painter, timer.step());
}
