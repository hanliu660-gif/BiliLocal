#include "Common.h"
#include "Home.h"
#include "HomeShortcuts.h"
#include "HomeContextMenu.h"
#include "HomeGestures.h"
#include <QtGui>
#include <QtNetwork>
#include <functional>
#include <QDateTime>
#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
#endif

using namespace UI;

Home::Home(QWidget *parent)
	: QWidget(parent)
{
	qApp->setStyle("Fusion");
	Utils::applyTheme(Config::getValue("/Interface/Theme", 0));

	setMouseTracking(true);
	setAcceptDrops(true);
	setWindowIcon(QIcon(":/Picture/icon.png"));
	setMinimumSize(360 * logicalDpiX() / 72, 270 * logicalDpiY() / 72);

	aplayer = lApp->findObject<APlayer>();
	danmaku = lApp->findObject<Danmaku>();
	running = lApp->findObject<Running>();
	arender = lApp->findObject<ARender>();
	menu = new Menu(this);
	info = new Info(this);
	shortcuts = new HomeShortcuts(this);
	gestures = new HomeGestures(this);
	list = lApp->findObject<List>();
	load = lApp->findObject<Load>();

	connect(danmaku, &Danmaku::modelAppend, this, [this]() {
		qWarning("modelAppend: pool=%d busy=%d", danmaku->getPool().size(), load->busy());
		if (danmaku->getPool().size() >= 2 && !load->busy()) {
			UI::Editor::exec(this);
		}
	}, Qt::QueuedConnection);

	connect(aplayer, &APlayer::begin, this, [this]() {
		shortcuts->speedMenu()->defaultAction()->setChecked(true);
		shortcuts->speedMenu()->setEnabled(true);
		setWindowFilePath(aplayer->getMedia());
	});

	connect(aplayer, &APlayer::reach, this, [this](bool m) {
		Q_UNUSED(m);
		shortcuts->speedMenu()->setEnabled(false);
		setWindowFilePath(QString());
	});

	connect(aplayer, &APlayer::stateChanged, this, &Home::setWindowFlags);

	connect(aplayer, &APlayer::errorOccurred, [this](int code) {
		QString text;
		switch (code) {
		case APlayer::ResourceError:
			text = tr("A media resource couldn't be resolved.");
			break;
		case APlayer::FormatError:
			text = tr("The format of a media resource isn't (fully) supported. "
				"Playback may still be possible, "
				"but without an audio or video component.");
			break;
		case APlayer::NetworkError:
			text = tr("A network error occurred.");
			break;
		case APlayer::AccessDeniedError:
			text = tr("There are not the appropriate permissions to play a media resource.");
			break;
		case APlayer::ServiceMissingError:
			text = tr("A valid playback service was not found, playback cannot proceed.");
			break;
		default:
			text = tr("An error occurred.");
			break;
		}
		warning(tr("Warning"), text);
	});


	auto alertNetworkError = [this](int code) {
		QString info = tr("Network error occurred, error code: %1").arg(code);
		QString sugg;
		switch (code) {
		case 3:
		case 4:
			sugg = tr("check your network connection");
			break;
		case 203:
		case 403:
		case -8:
			sugg = tr("access denied, try login");
			break;
		default:
			break;
		}
		warning(tr("Network Error"), sugg.isEmpty() ? info : (info + '\n' + sugg));
	};
	connect(load, &Load::errorOccured, this, alertNetworkError);

	wasMaximized = false;

	addActions(menu->actions());
	addActions(info->actions());


	// 不激活式悬浮窗：弹出时不抢主窗口键盘焦点，快捷键全程可用
	menu->setWindowFlags(Qt::Window | Qt::WindowDoesNotAcceptFocus);
	menu->setAttribute(Qt::WA_ShowWithoutActivating);
	menu->setWindowTitle(tr("Files and Danmaku"));
	menu->resize(320, 420);
	connect(info->listB, &QPushButton::clicked, this, [this]() {
		if (menu->isShown()) {
			menu->push(true);
		}
		else {
			if (!menu->property("placed").toBool()) {
				menu->move(x() + width() - menu->width() - 24, y() + 48);
				menu->setProperty("placed", true);
			}
			menu->pop();
		}
	});

	setWindowFlags();
	setGeometry(QSize(), true);

#ifdef BILILOCAL_EMBED_TEST
	// 自动化验证钩子（与 EWindow 注入钩子同一编译开关、独立环境变量）：
	// BILILOCAL_UI_TEST=prefer/editor/both 时定时自动打开对应对话框，供外部脚本
	// 截图验证（外部对 Qt 弹窗投递消息/UIA 动作均不可靠，只能进程内触发）。
	// 正常使用不设此变量，零影响。
	if (const QString uiTest = qEnvironmentVariable("BILILOCAL_UI_TEST"); !uiTest.isEmpty()){
		QTimer::singleShot(1500, this, [this, uiTest]() {
			if (uiTest == QLatin1String("editor")){
				Editor::exec(this);
				return;
			}
			Prefer::exec(this);
			if (uiTest == QLatin1String("both")){
				Editor::exec(this);
			}
		});
	}
#endif
}

void Home::warning(QString title, QString text)
{
	auto w = msg ? qobject_cast<QMessageBox *>(msg) : nullptr;
	QWidget *c = qApp->activeWindow();
	if (!w || (w != c && w->parent() != c && c)) {
		delete msg;
		w = new QMessageBox(c);
		w->setIcon(QMessageBox::Warning);
		msg = w;
	}
	w->setText(text);
	w->setWindowTitle(title);
	w->show();
}

void Home::applyFullScreen(bool b)
{
	if (b) {
		wasMaximized = isMaximized();
		showFullScreen();
	}
	else if (wasMaximized) {
#ifdef Q_OS_WIN
		BOOL transitions = TRUE;
		DwmSetWindowAttribute((HWND)winId(), 3 /* DWMWA_TRANSITIONS_FORCEDISABLED */, &transitions, sizeof(transitions));
#endif
		hide();
		showMaximized();
		show();
#ifdef Q_OS_WIN
		QTimer::singleShot(700, this, [this]() {
			BOOL transitions = FALSE;
			DwmSetWindowAttribute((HWND)winId(), 3 /* DWMWA_TRANSITIONS_FORCEDISABLED */, &transitions, sizeof(transitions));
		});
#endif
	}
	else {
		showNormal();
	}
	for (int ms : { 0, 80, 240, 600 }) {
		QTimer::singleShot(ms, this, [this]() {
			if (arender == nullptr) {
				return;
			}
			const QSize sz = size();
			arender->resize(sz);
			arender->draw(QRect(QPoint(0, 0), sz));
			if (info != nullptr) {
				info->setGeometry(0, sz.height() - info->height(), sz.width(), info->height());
				info->raise();
			}
		});
	}
}

void Home::closeEvent(QCloseEvent * e)
{
	if (aplayer->getState() == APlayer::Stop && !isFullScreen() && !isMaximized()) {
		QString conf = Config::getValue("/Interface/Size", QString("720,405"));
		QString size = QString("%1,%2").arg(width() * 72 / logicalDpiX()).arg(height() * 72 / logicalDpiY());
		Config::setValue("/Interface/Size", conf.endsWith(' ') ? conf.trimmed() : size);
	}
	QWidget::closeEvent(e);
	qApp->exit();
}

void Home::dragEnterEvent(QDragEnterEvent * e)
{
	if (e->mimeData()->hasFormat("text/uri-list")) {
		e->acceptProposedAction();
	}
	QWidget::dragEnterEvent(e);
}

void Home::dropEvent(QDropEvent * e)
{
	if (e->mimeData()->hasFormat("text/uri-list")) {
		for (const QString &item : QString(e->mimeData()->data("text/uri-list")).split('\n', Qt::SkipEmptyParts)) {
			lApp->tryLocal(QUrl(item).toLocalFile().trimmed());
		}
	}
}

void Home::mouseDoubleClickEvent(QMouseEvent * e)
{
	gestures->handleDoubleClick(e);
}

void Home::mouseMoveEvent(QMouseEvent * e)
{
	gestures->handleMouseMove(e);
	QWidget::mouseMoveEvent(e);
}

void Home::mousePressEvent(QMouseEvent * e)
{
	gestures->handleMousePress(e);
	QWidget::mousePressEvent(e);
}

void Home::mouseReleaseEvent(QMouseEvent * e)
{
	gestures->handleMouseRelease(e);
	QWidget::mouseReleaseEvent(e);
}

void Home::wheelEvent(QWheelEvent * e)
{
	gestures->handleWheel(e);
	QWidget::wheelEvent(e);
}

void Home::resizeEvent(QResizeEvent * e)
{
	const QSize size = e->size();
	arender->resize(size);
	int w = size.width(), h = size.height();
	// 窗口窄于底栏所需宽度时，进度条会压住右侧按钮（Info::resizeEvent 里
	// 只能把进度条钳到 8x 宽），所以这里把下限抬到底栏的真实需求。
	const int minW = qMax(int(360 * logicalDpiX() / 72), info->barMinWidth());
	if (minimumWidth() != minW){
		setMinimumWidth(minW);
	}
	info->terminate();
	int ih = int(4.2 * logicalDpiY() * font().pointSizeF() / 72);
	info->setGeometry(0, h - ih, w, ih);
	info->raise();
	QWidget::resizeEvent(e);
}

QSize Home::parseSize(QString string)
{
	QStringList l = string.trimmed().split(',', Qt::SkipEmptyParts);
	double x = logicalDpiX() / 72.0, y = logicalDpiY() / 72.0;
	return l.size() >= 2 ? QSize(l[0].toInt() * x, l[1].toInt() * y) : QSize(720 * x, 405 * y);
}

void Home::setGeometry(QSize size, bool center)
{
	if (size.isEmpty()) {
		size = parseSize(Config::getValue("/Interface/Size", QString("720,405")));
	}
	QSize m = minimumSize();
	QRect r;
	r.setSize(QSize(qMax(m.width(), size.width()), qMax(m.height(), size.height())));
	QRect s = screen()->geometry();
	QRect t = center ? s : geometry();
	if ((windowFlags()&Qt::CustomizeWindowHint) == 0) {
		s.setTop(s.top() + style()->pixelMetric(QStyle::PM_TitleBarHeight));
	}
	if (r.width() >= s.width() || r.height() >= s.height()) {
		// 原来这里直接 fullA->setChecked(true) 把窗口拽进全屏：起播 1440p/4K 大视频
		// 时用户会被强制全屏。改成等比缩到塞满可用屏幕为止，全屏留给用户自己
		// （F / 双击 / 全屏按钮）。
		const double k = qMin(double(s.width()) / double(r.width()), double(s.height()) / double(r.height()));
		r.setSize(QSize(qMax(m.width(), int(r.width() * k)), qMax(m.height(), int(r.height() * k))));
	}
	r.moveCenter(t.center());
	if (r.top() < s.top()) {
		r.moveTop(s.top());
	}
	if (r.bottom() > s.bottom()) {
		r.moveBottom(s.bottom());
	}
	if (r.left() < s.left()) {
		r.moveLeft(s.left());
	}
	if (r.right() > s.right()) {
		r.moveRight(s.right());
	}
	QWidget::setGeometry(r);
}

void Home::setWindowFlags()
{
	// /Interface/Top 是三态：0=从不置顶、1=只在播放时置顶、2=始终置顶。
	// 旧写法把三态压成">=2"，1 这一档等于 0，设置页选了"playing"也没用。
	const int mode = Config::getValue("/Interface/Top", 0);
	const bool onTop = (mode >= 2) || (mode == 1 && aplayer->getState() == APlayer::Play);
	Qt::WindowFlags flags = windowFlags();
	if (onTop) {
		flags |= Qt::WindowStaysOnTopHint;
	}
	else {
		flags &= ~Qt::WindowStaysOnTopHint;
	}
	if (!testAttribute(Qt::WA_WState_Created)) {
		QWidget::setWindowFlags(flags);
		return;
	}
	// 窗口已经建出来了：再调 QWidget::setWindowFlags 会重建原生窗口，画面会闪一下，
	// 而播放/暂停每次都会走到这个槽。置顶其实只是改 Z 序，直接让系统改，别重建。
	const HWND hwnd = (HWND)winId();
	::SetWindowPos(hwnd, onTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}


