#include "Common.h"
#include "WidgetInterfacePrivate.h"
#include "Home.h"

void Utils::setCenter(QWidget *widget)
{
	QRect rect = widget->geometry();
	QWidget *parent = widget->parentWidget();
	if (!parent) {
		rect.moveCenter(QApplication::desktop()->screenGeometry(widget).center());
	}
	else {
		if (widget->isWindow()) {
			QPoint center = parent->geometry().center();
			if ((parent->windowFlags()&Qt::CustomizeWindowHint)) {
				center.ry() += widget->style()->pixelMetric(QStyle::PM_TitleBarHeight) / 2;
			}
			rect.moveCenter(center);
		}
		else {
			rect.moveCenter(parent->rect().center());
		}
	}
	widget->setGeometry(rect);
}

void Utils::setGround(QWidget *widget, QColor color)
{
	widget->setAutoFillBackground(true);
	QPalette palette = widget->palette();
	palette.setColor(QPalette::Window, color);
	widget->setPalette(palette);
}

using namespace UI;

WidgetInterfacePrivate::WidgetInterfacePrivate()
{
	home = new Home();
}

WidgetInterfacePrivate::~WidgetInterfacePrivate()
{
	delete home;
}

void WidgetInterfacePrivate::show()
{
	if (home->testAttribute(Qt::WA_WState_ExplicitShowHide) == false) {
		home->show();
	}
}

QWidget * WidgetInterfacePrivate::widget()
{
	return home;
}

QWindow * WidgetInterfacePrivate::window()
{
	return home->backingStore()->window();
}
