/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    Utils.h
*   Time:        2013/05/10
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

// Comment/Record 等弹幕数据类型已挪到 Model/Comment.h
#include "Model/Comment.h"

namespace Utils
{
	enum Site
	{
		Unknown,
		Bilibili,
		AcFun,
		Tudou,
		Letv,
		AcfunLocalizer,
		Niconico,
		TuCao,
		ASS
	};

	enum Type
	{
		Video = 1,
		Audio = 2,
		Subtitle = 4,
		Danmaku = 8
	};

	enum Path
	{
		Cache,
		Config,
		Locale,
		Plugin,
		Script
	};

	QString defaultFont(bool monospace = false);
	QString localPath(Path path);
	void applyTheme(int mode);
	QString decodeTxt(QString &&txt);
	QString decodeXml(QString &&xml, bool fast = false);
	QStringList getSuffix(int type, QString format = QString());
	double evaluate(QString expression);
}
