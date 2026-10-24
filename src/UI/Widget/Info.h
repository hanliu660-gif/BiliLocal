/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    Info.h
*   Time:        2013/04/05
*   Author:      Lysine
*   Contributor: Chaserhkj
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
#include <QtWidgets>

namespace UI
{
	class Info :public QWidget
	{
		Q_OBJECT
	public:
		explicit Info(QWidget *parent = 0);

		bool isShown()
		{
			// 以真实可见性为准：动画被打断时 isPoped 可能与实际状态不一致
			// （以前会出现「以为已经弹出、其实已经隐藏」，之后再也弹不出来）
			return isVisible();
		}

		QPushButton *listB;

		// 外部（Home 的底栏拖动）预览时间时只更新显示，不触发 seek
		void previewTime(qint64 _time)
		{
			updateTimeText(_time);
			if (duration > 0) {
				updating = 1;
				timeS->setValue(int(qBound<qint64>(0, _time, duration)));
				updating = 0;
			}
		}

	private:
		bool isPoped;
		bool updating;
		bool dragging;
		qint64 duration;

		QLabel *timeT;
		QSlider *timeS;
		QSlider *volmS;
		QPushButton *playB;
		QPushButton *openB;
		QPushButton *danmB;
		QPushButton *rateB;
		QPushButton *prefB;
		QPushButton *fullB;
		QAction *playA;
		QIcon playI, pausI;
		QPropertyAnimation *animation;
		void resizeEvent(QResizeEvent *e);
		bool eventFilter(QObject *o, QEvent *e);
		void setTime(qint64 _time);
		void setDuration(qint64 _duration);
		void updateTimeText(qint64 _time);
		QPoint restPos() const;

	public slots:
		void pop();
		void push(bool force = false);
		void terminate();
	};
}
