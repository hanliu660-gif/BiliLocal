#pragma once

#include <QtGui>
#include <QtCore>
#include "ElapsedTimer.h"

class ARenderPrivate
{
public:
	virtual ~ARenderPrivate() = default;
	ElapsedTimer timer;

	virtual void drawDanm(QPainter *painter, QRect rect);
};
