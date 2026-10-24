/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    Preference.h
*   Time:        2015/05/13
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
#include <QtWidgets>

namespace UI
{
	class Prefer :public QDialog
	{
		Q_OBJECT
	public:
		static void exec(QWidget *parent = 0, int index = 0);

	private:
		// Win11 式设置布局：左侧导航列表 + 右侧堆叠页（替代旧的 QTabWidget）
		QListWidget *nav;
		QStackedWidget *stack;
		QWidget *page[5];

		//Playing
		QCheckBox *load[5];
		QCheckBox *fitted[2];
		QDoubleSpinBox *factor;
		QCheckBox *bold;
		QComboBox *dmfont;
		QComboBox *effect;
		QComboBox *play[2];

		//Interface
		QComboBox *font;
		QComboBox *reop;
		QCheckBox *sens;
		QComboBox *loca;
		QComboBox *stay;
		QComboBox *theme;
		QDoubleSpinBox *jump;
		QSpinBox *sizeW;
		QSpinBox *sizeH;
		QCheckBox *sizeLock;

		//Shield
		QCheckBox *check[8];
		QListView *regexp;
		QListView *sender;
		QStringListModel *rm;
		QStringListModel *sm;
		QAction *action[3];
		QSlider *same;
		QSpinBox *limit;

		//Shortcut
		QTreeWidget *hotkey;

		//About（Thanks + License 合并）
		QTextEdit *thanks;
		QTextEdit *license;

		//Shared
		QHash<QString, QVariant> restart;
		QHash<QString, QVariant> reparse;

		explicit Prefer(QWidget *parent = 0);
		QHash<QString, QVariant> getRestart();
		QHash<QString, QVariant> getReparse();
	};
}
