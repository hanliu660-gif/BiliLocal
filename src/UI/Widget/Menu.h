/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    Menu.h
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
	// 「文件与弹幕」悬浮面板：最近播放列表 + 搜索 + 双击即播。
	// 2026-10-24 重做（原为 2015 年的文件补全器布局）。
	class RecentProxy;
	class SearchProxy;

	class Menu :public QWidget
	{
		Q_OBJECT
	public:
		explicit Menu(QWidget *parent = 0);

		bool isShown()
		{
			return isPoped;
		}

	private:
		bool isPoped;
		RecentProxy *recent;
		SearchProxy *filtered;
		QLineEdit *search;
		QListView *view;
		QPushButton *openB;
		QPushButton *danmB;
		QAction *fileA;
		QAction *danmA;
		QLabel *alphaT;
		QSlider *alphaS;
		QCheckBox *subC;
		QCheckBox *loopC;

	public slots:
		void pop();
		void push(bool force = false);
		void terminate();
	};
}
