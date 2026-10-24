#pragma once

#include <QtCore>

class QWidget;
class QWindow;

class InterfacePrivate
{
public:
	virtual ~InterfacePrivate() = default;
	virtual void show() = 0;
	virtual QWidget *widget() = 0;
	virtual QWindow *window() = 0;
};
