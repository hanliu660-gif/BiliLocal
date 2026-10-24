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

	// 全局扁平样式：圆角控件、明确 hover 态、轻边框；底栏(Info)有自己的深色 QSS 不受影响
	if (dark){
		qApp->setStyleSheet(QStringLiteral(
			"QWidget{background:#2d2d30;color:#e8e8ea;font-family:'Microsoft YaHei';}"
			"QMainWindow,QDialog{background:#2d2d30;}"
			"QPushButton{background:#3a3a40;border:1px solid #4a4a52;border-radius:5px;padding:5px 14px;}"
			"QPushButton:hover{background:#45454d;border-color:#5a5a64;}"
			"QPushButton:pressed{background:#2f2f36;}"
			"QPushButton:disabled{color:#7a7a82;border-color:#3c3c44;}"
			"QLineEdit,QComboBox,QSpinBox{background:#252529;border:1px solid #4a4a52;border-radius:5px;padding:4px 8px;selection-background-color:#3e8ceb;}"
			"QLineEdit:focus,QComboBox:focus{border-color:#3e8ceb;}"
			"QComboBox::drop-down{border:none;width:22px;}"
			"QComboBox QAbstractItemView{background:#252529;border:1px solid #4a4a52;selection-background-color:#3e8ceb;outline:none;}"
			"QCheckBox,QRadioButton{spacing:7px;background:transparent;}"
			"QCheckBox::indicator,QRadioButton::indicator{width:16px;height:16px;border:1px solid #5a5a64;border-radius:3px;background:#252529;}"
			"QRadioButton::indicator{border-radius:8px;}"
			"QCheckBox::indicator:checked,QRadioButton::indicator:checked{background:#3e8ceb;border-color:#3e8ceb;}"
			"QCheckBox::indicator:hover,QRadioButton::indicator:hover{border-color:#3e8ceb;}"
			"QSlider::groove:horizontal{height:4px;background:#4a4a52;border-radius:2px;}"
			"QSlider::sub-page:horizontal{background:#3e8ceb;border-radius:2px;}"
			"QSlider::handle:horizontal{width:14px;height:14px;margin:-6px 0;border-radius:7px;background:#e8e8ea;}"
			"QSlider::handle:horizontal:hover{background:#ffffff;}"
			"QSlider::groove:vertical{width:4px;background:#4a4a52;border-radius:2px;}"
			"QSlider::add-page:vertical{background:#3e8ceb;border-radius:2px;}"
			"QSlider::handle:vertical{height:14px;width:14px;margin:0 -6px;border-radius:7px;background:#e8e8ea;}"
			"QMenu{background:#2f2f34;border:1px solid #45454d;border-radius:8px;padding:6px 0;}"
			"QMenu::item{padding:6px 26px 6px 18px;border-radius:4px;margin:0 4px;}"
			"QMenu::item:selected{background:#3e8ceb;color:#ffffff;}"
			"QMenu::item:disabled{color:#7a7a82;}"
			"QMenu::separator{height:1px;background:#45454d;margin:5px 8px;}"
			"QHeaderView::section{background:#333338;color:#c8c8cc;border:none;border-right:1px solid #45454d;border-bottom:1px solid #45454d;padding:5px 8px;}"
			"QTreeView,QListView,QTableView{background:#252529;border:1px solid #3c3c44;border-radius:6px;alternate-background-color:#2a2a2f;outline:none;}"
			"QTreeView::item,QListView::item,QTableView::item{padding:4px 6px;border-radius:4px;}"
			"QTreeView::item:selected,QListView::item:selected,QTableView::item:selected{background:#3e8ceb;color:#ffffff;}"
			"QTreeView::item:hover,QListView::item:hover,QTableView::item:hover{background:#33333a;}"
			"QScrollBar:vertical{background:transparent;width:10px;margin:2px;}"
			"QScrollBar::handle:vertical{background:#4a4a52;border-radius:4px;min-height:30px;}"
			"QScrollBar::handle:vertical:hover{background:#5a5a64;}"
			"QScrollBar::add-line,QScrollBar::sub-line{height:0;width:0;}"
			"QScrollBar:horizontal{background:transparent;height:10px;margin:2px;}"
			"QScrollBar::handle:horizontal{background:#4a4a52;border-radius:4px;min-width:30px;}"
			"QScrollBar::handle:horizontal:hover{background:#5a5a64;}"
			"QGroupBox{border:1px solid #3c3c44;border-radius:7px;margin-top:9px;padding:7px 9px 5px 9px;font-weight:bold;}"
			"QGroupBox::title{subcontrol-origin:margin;left:11px;padding:0 4px;color:#a8a8b0;}"
			"QTabWidget::pane{border:1px solid #3c3c44;border-radius:6px;top:-1px;}"
			"QTabBar::tab{padding:7px 18px;border-top-left-radius:6px;border-top-right-radius:6px;margin-right:2px;background:#333338;color:#c8c8cc;}"
			"QTabBar::tab:selected{background:#3e8ceb;color:#ffffff;}"
			"QTabBar::tab:hover:!selected{background:#3a3a42;}"
			"QProgressBar{border:1px solid #4a4a52;border-radius:5px;background:#252529;text-align:center;height:14px;}"
			"QProgressBar::chunk{background:#3e8ceb;border-radius:4px;}"
			"QToolTip{background:#1f1f24;color:#e8e8ea;border:1px solid #4a4a52;padding:4px 8px;border-radius:4px;}"
			"QSplitter::handle{background:#3c3c44;}"
			"QFileDialog QTreeView,QFileDialog QListView{border:none;}"
			));
	}
	else{
		qApp->setStyleSheet(QStringLiteral(
			"QWidget{color:#1f1f24;font-family:'Microsoft YaHei';}"
			"QMainWindow,QDialog{background:#f2f2f4;}"
			"QPushButton{background:#ffffff;border:1px solid #c8c8ce;border-radius:5px;padding:5px 14px;}"
			"QPushButton:hover{background:#eef4fd;border-color:#3e8ceb;}"
			"QPushButton:pressed{background:#e2ecfa;}"
			"QPushButton:disabled{color:#9a9aa2;border-color:#d8d8de;}"
			"QLineEdit,QComboBox,QSpinBox{background:#ffffff;border:1px solid #c8c8ce;border-radius:5px;padding:4px 8px;selection-background-color:#3e8ceb;selection-color:#ffffff;}"
			"QLineEdit:focus,QComboBox:focus{border-color:#3e8ceb;}"
			"QComboBox::drop-down{border:none;width:22px;}"
			"QComboBox QAbstractItemView{background:#ffffff;border:1px solid #c8c8ce;selection-background-color:#3e8ceb;selection-color:#ffffff;outline:none;}"
			"QCheckBox,QRadioButton{spacing:7px;background:transparent;}"
			"QCheckBox::indicator,QRadioButton::indicator{width:16px;height:16px;border:1px solid #b0b0b8;border-radius:3px;background:#ffffff;}"
			"QRadioButton::indicator{border-radius:8px;}"
			"QCheckBox::indicator:checked,QRadioButton::indicator:checked{background:#3e8ceb;border-color:#3e8ceb;}"
			"QCheckBox::indicator:hover,QRadioButton::indicator:hover{border-color:#3e8ceb;}"
			"QSlider::groove:horizontal{height:4px;background:#d8d8de;border-radius:2px;}"
			"QSlider::sub-page:horizontal{background:#3e8ceb;border-radius:2px;}"
			"QSlider::handle:horizontal{width:14px;height:14px;margin:-6px 0;border-radius:7px;background:#ffffff;border:1px solid #b0b0b8;}"
			"QSlider::handle:horizontal:hover{border-color:#3e8ceb;}"
			"QSlider::groove:vertical{width:4px;background:#d8d8de;border-radius:2px;}"
			"QSlider::add-page:vertical{background:#3e8ceb;border-radius:2px;}"
			"QSlider::handle:vertical{height:14px;width:14px;margin:0 -6px;border-radius:7px;background:#ffffff;border:1px solid #b0b0b8;}"
			"QMenu{background:#ffffff;border:1px solid #d8d8de;border-radius:8px;padding:6px 0;}"
			"QMenu::item{padding:6px 26px 6px 18px;border-radius:4px;margin:0 4px;}"
			"QMenu::item:selected{background:#3e8ceb;color:#ffffff;}"
			"QMenu::item:disabled{color:#9a9aa2;}"
			"QMenu::separator{height:1px;background:#e4e4e8;margin:5px 8px;}"
			"QHeaderView::section{background:#ececf0;color:#5a5a64;border:none;border-right:1px solid #dcdce0;border-bottom:1px solid #dcdce0;padding:5px 8px;}"
			"QTreeView,QListView,QTableView{background:#ffffff;border:1px solid #d8d8de;border-radius:6px;alternate-background-color:#f6f6f8;outline:none;selection-background-color:#3e8ceb;selection-color:#ffffff;}"
			"QTreeView::item,QListView::item,QTableView::item{padding:4px 6px;border-radius:4px;}"
			"QTreeView::item:hover,QListView::item:hover,QTableView::item:hover{background:#eef4fd;}"
			"QScrollBar:vertical{background:transparent;width:10px;margin:2px;}"
			"QScrollBar::handle:vertical{background:#c8c8ce;border-radius:4px;min-height:30px;}"
			"QScrollBar::handle:vertical:hover{background:#a8a8b0;}"
			"QScrollBar::add-line,QScrollBar::sub-line{height:0;width:0;}"
			"QScrollBar:horizontal{background:transparent;height:10px;margin:2px;}"
			"QScrollBar::handle:horizontal{background:#c8c8ce;border-radius:4px;min-width:30px;}"
			"QScrollBar::handle:horizontal:hover{background:#a8a8b0;}"
			"QGroupBox{border:1px solid #dcdce0;border-radius:7px;margin-top:9px;padding:7px 9px 5px 9px;font-weight:bold;background:#ffffff;}"
			"QGroupBox::title{subcontrol-origin:margin;left:11px;padding:0 4px;color:#7a7a84;}"
			"QTabWidget::pane{border:1px solid #dcdce0;border-radius:6px;top:-1px;background:#ffffff;}"
			"QTabBar::tab{padding:7px 18px;border-top-left-radius:6px;border-top-right-radius:6px;margin-right:2px;background:#ececf0;color:#5a5a64;}"
			"QTabBar::tab:selected{background:#ffffff;color:#1f1f24;border-bottom:2px solid #3e8ceb;}"
			"QTabBar::tab:hover:!selected{background:#e4e4ea;}"
			"QProgressBar{border:1px solid #c8c8ce;border-radius:5px;background:#ffffff;text-align:center;height:14px;}"
			"QProgressBar::chunk{background:#3e8ceb;border-radius:4px;}"
			"QToolTip{background:#1f1f24;color:#e8e8ea;border:1px solid #4a4a52;padding:4px 8px;border-radius:4px;}"
			"QSplitter::handle{background:#dcdce0;}"
			"QFileDialog QTreeView,QFileDialog QListView{border:none;}"
			));
	}

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
