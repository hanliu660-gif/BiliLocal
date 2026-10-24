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
	// 主题 QSS 外置到 res/Style/*.qss 并打包进 qrc：调配色不必重编译，
	// 也不用把 65 行规则挤成一串 C++ 字面量
	QString themeStyleSheet(bool dark)
	{
		QFile file(dark ? QStringLiteral(":/Style/dark.qss") : QStringLiteral(":/Style/light.qss"));
		if (!file.open(QIODevice::ReadOnly)){
			return QString();
		}
		return QString::fromUtf8(file.readAll());
	}
}

// /Interface/Theme：0=浅色（默认） 1=深色 2=跟随系统
// 设计语言对齐 Windows 11：圆角卡片（Card）、柔和选中态、Segoe UI Variable 字体栈、
// 主题强调色（浅色 #0067c0 / 深色 #4cc2ff）。视觉集中在下面两份 QSS，
// 业务代码（Editor/Track 绘制等）一律走 palette 取色，不写死颜色。
void Utils::applyTheme(int mode)
{
	const bool dark = mode == 1 || (mode == 2 && !systemPrefersLight());

	QPalette palette;
	if (dark){
		palette.setColor(QPalette::Window, QColor(32, 32, 32));
		palette.setColor(QPalette::WindowText, Qt::white);
		palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(138, 138, 138));
		palette.setColor(QPalette::Base, QColor(43, 43, 43));
		palette.setColor(QPalette::AlternateBase, QColor(38, 38, 38));
		palette.setColor(QPalette::Text, Qt::white);
		palette.setColor(QPalette::Disabled, QPalette::Text, QColor(138, 138, 138));
		palette.setColor(QPalette::Button, QColor(51, 51, 51));
		palette.setColor(QPalette::ButtonText, Qt::white);
		palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(122, 122, 122));
		palette.setColor(QPalette::BrightText, Qt::red);
		palette.setColor(QPalette::Link, QColor(76, 194, 255));
		palette.setColor(QPalette::Highlight, QColor(76, 194, 255));
		palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(85, 85, 85));
		palette.setColor(QPalette::HighlightedText, Qt::black);
		palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(122, 122, 122));
		palette.setColor(QPalette::ToolTipBase, QColor(45, 45, 45));
		palette.setColor(QPalette::ToolTipText, QColor(240, 240, 240));
		palette.setColor(QPalette::Inactive, QPalette::ToolTipBase, QColor(45, 45, 45));
		palette.setColor(QPalette::Inactive, QPalette::ToolTipText, QColor(240, 240, 240));
	}
	else{
		palette = qApp->style()->standardPalette();
		palette.setColor(QPalette::Inactive, QPalette::ToolTipBase, Qt::white);
		palette.setColor(QPalette::Inactive, QPalette::ToolTipText, Qt::black);
	}
	qApp->setPalette(palette);
	QToolTip::setPalette(palette);

	// 样式表内容见 res/Style/dark.qss 与 light.qss（随 qrc 打包）
	qApp->setStyleSheet(themeStyleSheet(dark));

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
