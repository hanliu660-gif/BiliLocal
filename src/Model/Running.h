/*=======================================================================
*
*   Copyright (C) 2013-2016 Lysine.
*
*   Filename:    Running.h
*   Time:        2016/05/29
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

#pragma once

#include <QtCore>
#include <QtGui>

class Comment;
class Graphic;
class RunningPrivate;

class Running : public QObject
{
	Q_OBJECT
public:
	explicit Running(QObject *parent = nullptr);
	virtual ~Running();
	void setup();

private:
	RunningPrivate *d_ptr;
	Q_DECLARE_PRIVATE(Running);

public slots:
	void clear(bool soft = false);
	void insert(Graphic *graphic, int index = -1);
	void moveTime(qint64 time);
	void jumpTime(qint64 time);
	void draw(QPainter *painter, double move);
	const Comment *commentAt(QPointF point) const;
	int size() const;

	// 悬停冻结：光标压在某条弹幕上就停住它，移开（或离开窗口）才解冻。
	// **没有超时**——鼠标静止时 Windows 不发 mousemove，靠定时器续期反而
	// 会在"光标静止压在弹幕上"时把弹幕解冻。point 无效坐标 = 请求解冻
	// （被 pin 时忽略）。由 Home 的 mouseMove 驱动，EWindow::leaveEvent 解冻。
	void setHover(QPointF point, bool active);
	// 右键菜单弹出会触发 EWindow leave，钉住以免菜单开着、弹幕飞了
	void pinHover(bool pin);
};