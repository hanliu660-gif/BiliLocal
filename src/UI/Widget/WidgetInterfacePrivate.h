#pragma once

#include "../InterfacePrivate.h"
#include <QString>

namespace Utils
{
	void setCenter(QWidget *widget);
	void setGround(QWidget *widget, QColor color);
}

class QWidget;
class QWindow;

namespace UI
{
	class Home;
}

class WidgetInterfacePrivate : public InterfacePrivate
{
public:
	WidgetInterfacePrivate();
	virtual ~WidgetInterfacePrivate();

	virtual void show() override;
	virtual QWidget *widget() override;
	virtual QWindow *window() override;

private:
	UI::Home *home;
};
