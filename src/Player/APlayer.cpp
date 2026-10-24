/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    VPlayer.cpp
*   Time:        2013/03/18
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
#include "APlayer.h"
#include "../Config.h"
#include "../Utils.h"

#include "VPlayer.h"

APlayer * APlayer::create(QObject *parent, QString name)
{
	Q_UNUSED(name);
	return new VPlayer(parent);
}

APlayer::APlayer(QObject *parent)
	: QObject(parent)
{
	setObjectName("APlayer");
}

void APlayer::setRate(double)
{
}

double APlayer::getRate()
{
	return 0;
}

qint64 APlayer::getDelay(int)
{
	return 0;
}

void APlayer::setDelay(int, qint64)
{
}

int APlayer::getTrack(int)
{
	return -1;
}

void APlayer::setTrack(int, int)
{
}

QStringList APlayer::getTracks(int)
{
	return QStringList();
}

void APlayer::addSubtitle(QString)
{
}

void APlayer::event(int)
{
}
