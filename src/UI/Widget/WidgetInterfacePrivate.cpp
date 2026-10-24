#include "Common.h"
#include "WidgetInterfacePrivate.h"
#include "Home.h"
#include "../../Config.h"
#include "../../Local.h"
#include <QTimer>
#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#endif

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

namespace
{
	bool systemPrefersLight()
	{
#ifdef Q_OS_WIN
		QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
			QSettings::NativeFormat);
		return settings.value("AppsUseLightTheme", 1).toInt() != 0;
#else
		return true;
#endif
	}

	void setTitleBarDark(QWidget *window, bool dark)
	{
#ifdef Q_OS_WIN
		if (window == nullptr || window->windowHandle() == nullptr){
			return;
		}
		// DWMWA_USE_IMMERSIVE_DARK_MODE：Win10 2004+ 是 20，更早的 1903/1909 用 19
		const HWND hwnd = (HWND)window->winId();
		BOOL value = dark;
		for (int attr : { 20, 19 }){
			if (DwmSetWindowAttribute(hwnd, attr, &value, sizeof(value)) == S_OK){
				break;
			}
		}
#else
		Q_UNUSED(window);
		Q_UNUSED(dark);
#endif
	}
}

// /Interface/Theme：0=浅色（默认，原外观） 1=深色 2=跟随系统
void Utils::applyTheme(int mode)
{
	const bool dark = mode == 1 || (mode == 2 && !systemPrefersLight());

	QPalette palette;
	if (dark){
		palette.setColor(QPalette::Window, QColor(45, 45, 48));
		palette.setColor(QPalette::WindowText, Qt::white);
		palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
		palette.setColor(QPalette::Base, QColor(37, 37, 40));
		palette.setColor(QPalette::AlternateBase, QColor(45, 45, 48));
		palette.setColor(QPalette::Text, Qt::white);
		palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
		palette.setColor(QPalette::Button, QColor(53, 53, 56));
		palette.setColor(QPalette::ButtonText, Qt::white);
		palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
		palette.setColor(QPalette::BrightText, Qt::red);
		palette.setColor(QPalette::Link, QColor(62, 140, 235));
		palette.setColor(QPalette::Highlight, QColor(62, 140, 235));
		palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(90, 90, 90));
		palette.setColor(QPalette::HighlightedText, Qt::white);
		palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
		palette.setColor(QPalette::ToolTipBase, Qt::white);
		palette.setColor(QPalette::Inactive, QPalette::ToolTipBase, Qt::white);
	}
	else{
		palette = qApp->style()->standardPalette();
		palette.setColor(QPalette::Inactive, QPalette::ToolTipBase, Qt::white);
	}
	qApp->setPalette(palette);
	QToolTip::setPalette(palette);

	// 已存在的控件重新 polish 一遍，保证立即换色
	QStyle *style = qApp->style();
	for (QWidget *w : qApp->allWidgets()){
		style->unpolish(w);
		style->polish(w);
	}

	if (lApp){
		setTitleBarDark(lApp->findObject<Interface>()->widget(), dark);
	}
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
	// 首帧后再应用一次主题：标题栏的 DWM 属性要等 windowHandle 就绪才能设
	QTimer::singleShot(0, [](){
		Utils::applyTheme(Config::getValue("/Interface/Theme", 0));
	});
}

QWidget * WidgetInterfacePrivate::widget()
{
	return home;
}

QWindow * WidgetInterfacePrivate::window()
{
	return home->backingStore()->window();
}
