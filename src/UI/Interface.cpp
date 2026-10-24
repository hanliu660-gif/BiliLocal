/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    Interface.cpp
*   Time:        2013/03/18
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

#include "Common.h"
#include "Interface.h"
#include "../Config.h"
#include <QtCore>

#include "Widget/WidgetInterfacePrivate.h"

// 单平台化后唯一界面后端：Widget（2026-10-24，Quick2 已删除）

QStringList Interface::getModules()
{
	return{ "Widget" };
}

Interface::Interface(QObject *parent)
	: QObject(parent), d_ptr(nullptr)
{
	setObjectName("Interface");
}

Interface::~Interface()
{
	delete d_ptr;
}

void Interface::setup()
{
	d_ptr = new WidgetInterfacePrivate();
}

void Interface::show()
{
	Q_D(Interface);
	d->show();
}

QWidget *Interface::widget()
{
	Q_D(Interface);
	return d ? d->widget() : nullptr;
}

QWindow *Interface::window()
{
	Q_D(Interface);
	return d ? d->window() : nullptr;
}
