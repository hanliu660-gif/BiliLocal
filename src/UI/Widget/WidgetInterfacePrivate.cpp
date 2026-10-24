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

	if (dark){
		qApp->setStyleSheet(QStringLiteral(
			"QWidget{color:#f0f0f0;font-family:'Segoe UI Variable Text','Segoe UI','Microsoft YaHei UI','Microsoft YaHei';}"
			"QMainWindow,QDialog{background:#202020;}"
			"QFrame#Card{background:#2b2b2b;border:1px solid #343434;border-radius:8px;}"
			"QLabel#CardTitle{font-weight:bold;color:#f0f0f0;background:transparent;padding:2px 0;}"
			"QLabel#CardNote{color:#9e9e9e;background:transparent;}"
			"QFrame#SepLine{background:#383838;border:none;}"
			"QListWidget#NavList{background:transparent;border:none;outline:none;}"
			"QListWidget#NavList::item{border-radius:6px;padding:9px 12px;margin:1px 4px;color:#f0f0f0;}"
			"QListWidget#NavList::item:selected{background:#2d3947;color:#ffffff;}"
			"QListWidget#NavList::item:hover:!selected{background:#2a2a2a;}"
			"QPushButton{background:#333333;border:1px solid #454545;border-radius:5px;padding:5px 16px;color:#f0f0f0;}"
			"QPushButton:hover{background:#3a3a3a;border-color:#525252;}"
			"QPushButton:pressed{background:#2e2e2e;}"
			"QPushButton:disabled{color:#7a7a7a;border-color:#3a3a3a;background:#2e2e2e;}"
			"QPushButton:focus{border-color:#4cc2ff;}"
			"QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox{background:#2f2f2f;border:1px solid #4a4a4a;border-radius:5px;padding:4px 8px;color:#f0f0f0;selection-background-color:#4cc2ff;selection-color:#000000;}"
			"QLineEdit:focus,QComboBox:focus,QSpinBox:focus,QDoubleSpinBox:focus{border-color:#4cc2ff;}"
			"QLineEdit:disabled,QComboBox:disabled,QSpinBox:disabled,QDoubleSpinBox:disabled{color:#7a7a7a;border-color:#3a3a3a;}"
			"QComboBox::drop-down{border:none;width:22px;}"
			"QComboBox QAbstractItemView{background:#2f2f2f;border:1px solid #4a4a4a;selection-background-color:#4cc2ff;selection-color:#000000;outline:none;}"
			"QCheckBox,QRadioButton{spacing:7px;background:transparent;}"
			"QCheckBox::indicator,QRadioButton::indicator{width:17px;height:17px;border:1px solid #5a5a5a;border-radius:4px;background:#2f2f2f;}"
			"QRadioButton::indicator{border-radius:9px;}"
			"QCheckBox::indicator:checked,QRadioButton::indicator:checked{background:#4cc2ff;border-color:#4cc2ff;}"
			"QCheckBox::indicator:hover,QRadioButton::indicator:hover{border-color:#4cc2ff;}"
			"QCheckBox::indicator:disabled,QRadioButton::indicator:disabled{border-color:#3f3f3f;background:#2a2a2a;}"
			"QCheckBox:disabled{color:#7a7a7a;}"
			"QSlider{background:transparent;}"
			"QSlider::groove:horizontal{height:4px;background:#4a4a4a;border-radius:2px;}"
			"QSlider::sub-page:horizontal{background:#4cc2ff;border-radius:2px;}"
			"QSlider::handle:horizontal{width:14px;height:14px;margin:-6px 0;border-radius:7px;background:#e8e8ea;}"
			"QSlider::handle:horizontal:hover{background:#ffffff;}"
			"QSlider::groove:vertical{width:4px;background:#4a4a4a;border-radius:2px;}"
			"QSlider::add-page:vertical{background:#4cc2ff;border-radius:2px;}"
			"QSlider::handle:vertical{height:14px;width:14px;margin:0 -6px;border-radius:7px;background:#e8e8ea;}"
			"QMenu{background:#2b2b2b;border:1px solid #3f3f3f;border-radius:8px;padding:6px 0;}"
			"QMenu::item{padding:6px 26px 6px 18px;border-radius:5px;margin:0 4px;}"
			"QMenu::item:selected{background:#4cc2ff;color:#000000;}"
			"QMenu::item:disabled{color:#7a7a7a;}"
			"QMenu::separator{height:1px;background:#3a3a3a;margin:5px 8px;}"
			"QHeaderView::section{background:#2b2b2b;color:#c8c8cc;border:none;border-right:1px solid #3a3a3a;border-bottom:1px solid #3a3a3a;padding:5px 8px;}"
			"QTreeView,QListView,QTableView{background:#262626;border:1px solid #383838;border-radius:6px;alternate-background-color:#2a2a2a;outline:none;selection-background-color:#4cc2ff;selection-color:#000000;}"
			"QTreeView::item,QListView::item,QTableView::item{padding:4px 6px;border-radius:4px;}"
			"QTreeView::item:selected,QListView::item:selected,QTableView::item:selected{background:#4cc2ff;color:#000000;}"
			"QTreeView::item:hover,QListView::item:hover,QTableView::item:hover{background:#333338;}"
			"QScrollBar:vertical{background:transparent;width:10px;margin:2px;}"
			"QScrollBar::handle:vertical{background:#4a4a4a;border-radius:4px;min-height:30px;}"
			"QScrollBar::handle:vertical:hover{background:#5a5a5a;}"
			"QScrollBar::add-line,QScrollBar::sub-line{height:0;width:0;}"
			"QScrollBar:horizontal{background:transparent;height:10px;margin:2px;}"
			"QScrollBar::handle:horizontal{background:#4a4a4a;border-radius:4px;min-width:30px;}"
			"QScrollBar::handle:horizontal:hover{background:#5a5a5a;}"
			"QGroupBox{border:1px solid #383838;border-radius:7px;margin-top:9px;padding:7px 9px 5px 9px;font-weight:bold;background:transparent;}"
			"QGroupBox::title{subcontrol-origin:margin;left:11px;padding:0 4px;color:#9e9e9e;}"
			"QTabWidget::pane{border:1px solid #383838;border-radius:6px;top:-1px;}"
			"QTabBar::tab{padding:7px 18px;border-top-left-radius:6px;border-top-right-radius:6px;margin-right:2px;background:transparent;color:#c8c8cc;}"
			"QTabBar::tab:selected{color:#4cc2ff;border-bottom:2px solid #4cc2ff;}"
			"QTabBar::tab:hover:!selected{color:#f0f0f0;}"
			"QProgressBar{border:1px solid #4a4a4a;border-radius:5px;background:#2f2f2f;text-align:center;height:14px;color:#f0f0f0;}"
			"QProgressBar::chunk{background:#4cc2ff;border-radius:4px;}"
			"QToolTip{background:#2d2d2d;color:#f0f0f0;border:1px solid #4a4a4a;padding:4px 8px;border-radius:5px;}"
			"QSplitter::handle{background:transparent;}"
			"QScrollArea{border:none;background:transparent;}"
			"QScrollArea>QWidget>QWidget{background:transparent;}"
			"QFileDialog QTreeView,QFileDialog QListView{border:none;}"
			));
	}
	else{
		qApp->setStyleSheet(QStringLiteral(
			"QWidget{color:#1b1b1b;font-family:'Segoe UI Variable Text','Segoe UI','Microsoft YaHei UI','Microsoft YaHei';}"
			"QMainWindow,QDialog{background:#f3f3f3;}"
			"QFrame#Card{background:#ffffff;border:1px solid #ebebeb;border-radius:8px;}"
			"QLabel#CardTitle{font-weight:bold;color:#1b1b1b;background:transparent;padding:2px 0;}"
			"QLabel#CardNote{color:#616161;background:transparent;}"
			"QFrame#SepLine{background:#f0f0f0;border:none;}"
			"QListWidget#NavList{background:transparent;border:none;outline:none;}"
			"QListWidget#NavList::item{border-radius:6px;padding:9px 12px;margin:1px 4px;color:#1b1b1b;}"
			"QListWidget#NavList::item:selected{background:#e9f2fb;color:#0067c0;font-weight:bold;}"
			"QListWidget#NavList::item:hover:!selected{background:#ededed;}"
			"QPushButton{background:#fdfdfd;border:1px solid #cfcfcf;border-radius:5px;padding:5px 16px;color:#1b1b1b;}"
			"QPushButton:hover{background:#f7f7f7;border-color:#bdbdbd;}"
			"QPushButton:pressed{background:#efefef;}"
			"QPushButton:disabled{color:#9a9aa2;border-color:#dcdcdc;}"
			"QPushButton:focus{border-color:#0067c0;}"
			"QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox{background:#ffffff;border:1px solid #cfcfcf;border-radius:5px;padding:4px 8px;color:#1b1b1b;selection-background-color:#0067c0;selection-color:#ffffff;}"
			"QLineEdit:focus,QComboBox:focus,QSpinBox:focus,QDoubleSpinBox:focus{border-color:#0067c0;}"
			"QLineEdit:disabled,QComboBox:disabled,QSpinBox:disabled,QDoubleSpinBox:disabled{color:#9a9aa2;border-color:#dcdcdc;}"
			"QComboBox::drop-down{border:none;width:22px;}"
			"QComboBox QAbstractItemView{background:#ffffff;border:1px solid #d0d0d0;selection-background-color:#0067c0;selection-color:#ffffff;outline:none;}"
			"QCheckBox,QRadioButton{spacing:7px;background:transparent;}"
			"QCheckBox::indicator,QRadioButton::indicator{width:17px;height:17px;border:1px solid #a0a0a0;border-radius:4px;background:#ffffff;}"
			"QRadioButton::indicator{border-radius:9px;}"
			"QCheckBox::indicator:checked,QRadioButton::indicator:checked{background:#0067c0;border-color:#0067c0;}"
			"QCheckBox::indicator:hover,QRadioButton::indicator:hover{border-color:#0067c0;}"
			"QCheckBox::indicator:disabled,QRadioButton::indicator:disabled{border-color:#dcdcdc;background:#f5f5f5;}"
			"QCheckBox:disabled{color:#9a9aa2;}"
			"QSlider{background:transparent;}"
			"QSlider::groove:horizontal{height:4px;background:#d6d6d6;border-radius:2px;}"
			"QSlider::sub-page:horizontal{background:#0067c0;border-radius:2px;}"
			"QSlider::handle:horizontal{width:14px;height:14px;margin:-6px 0;border-radius:7px;background:#ffffff;border:1px solid #a0a0a0;}"
			"QSlider::handle:horizontal:hover{border-color:#0067c0;}"
			"QSlider::groove:vertical{width:4px;background:#d6d6d6;border-radius:2px;}"
			"QSlider::add-page:vertical{background:#0067c0;border-radius:2px;}"
			"QSlider::handle:vertical{height:14px;width:14px;margin:0 -6px;border-radius:7px;background:#ffffff;border:1px solid #a0a0a0;}"
			"QMenu{background:#ffffff;border:1px solid #dcdcdc;border-radius:8px;padding:6px 0;}"
			"QMenu::item{padding:6px 26px 6px 18px;border-radius:5px;margin:0 4px;}"
			"QMenu::item:selected{background:#0067c0;color:#ffffff;}"
			"QMenu::item:disabled{color:#9a9aa2;}"
			"QMenu::separator{height:1px;background:#e8e8e8;margin:5px 8px;}"
			"QHeaderView::section{background:#f5f5f5;color:#5a5a64;border:none;border-right:1px solid #e4e4e4;border-bottom:1px solid #e4e4e4;padding:5px 8px;}"
			"QTreeView,QListView,QTableView{background:#ffffff;border:1px solid #e0e0e0;border-radius:6px;alternate-background-color:#f6f6f8;outline:none;selection-background-color:#0067c0;selection-color:#ffffff;}"
			"QTreeView::item,QListView::item,QTableView::item{padding:4px 6px;border-radius:4px;}"
			"QTreeView::item:selected,QListView::item:selected,QTableView::item:selected{background:#0067c0;color:#ffffff;}"
			"QTreeView::item:hover,QListView::item:hover,QTableView::item:hover{background:#f0f6fc;}"
			"QScrollBar:vertical{background:transparent;width:10px;margin:2px;}"
			"QScrollBar::handle:vertical{background:#c8c8c8;border-radius:4px;min-height:30px;}"
			"QScrollBar::handle:vertical:hover{background:#a8a8a8;}"
			"QScrollBar::add-line,QScrollBar::sub-line{height:0;width:0;}"
			"QScrollBar:horizontal{background:transparent;height:10px;margin:2px;}"
			"QScrollBar::handle:horizontal{background:#c8c8c8;border-radius:4px;min-width:30px;}"
			"QScrollBar::handle:horizontal:hover{background:#a8a8a8;}"
			"QGroupBox{border:1px solid #e0e0e0;border-radius:7px;margin-top:9px;padding:7px 9px 5px 9px;font-weight:bold;background:transparent;}"
			"QGroupBox::title{subcontrol-origin:margin;left:11px;padding:0 4px;color:#616161;}"
			"QTabWidget::pane{border:1px solid #e0e0e0;border-radius:6px;top:-1px;background:#ffffff;}"
			"QTabBar::tab{padding:7px 18px;border-top-left-radius:6px;border-top-right-radius:6px;margin-right:2px;background:transparent;color:#5a5a64;}"
			"QTabBar::tab:selected{color:#1b1b1b;border-bottom:2px solid #0067c0;}"
			"QTabBar::tab:hover:!selected{color:#1b1b1b;}"
			"QProgressBar{border:1px solid #cfcfcf;border-radius:5px;background:#ffffff;text-align:center;height:14px;color:#1b1b1b;}"
			"QProgressBar::chunk{background:#0067c0;border-radius:4px;}"
			"QToolTip{background:#ffffff;color:#1b1b1b;border:1px solid #cfcfcf;padding:4px 8px;border-radius:5px;}"
			"QSplitter::handle{background:transparent;}"
			"QScrollArea{border:none;background:transparent;}"
			"QScrollArea>QWidget>QWidget{background:transparent;}"
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
