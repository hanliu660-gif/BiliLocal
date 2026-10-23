#include "Common.h"
#include "EmbedPrivate.h"
#include "../../UI/Interface.h"
#include "../../Player/APlayer.h"
#include <functional>
#include <QOpenGLWindow>
#include <QTimer>
#include <QWidget>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

namespace
{
	class EWindow :public QOpenGLWindow
	{
	public:
		explicit EWindow(OpenGLEmbedRenderPrivate *render) :
			render(render)
		{
			QSurfaceFormat f = format();
			f.setAlphaBufferSize(8);
			setFormat(f);
			setFlags(flags() | Qt::Tool | Qt::FramelessWindowHint | Qt::WindowTransparentForInput | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
			QObject::connect(this, &EWindow::frameSwapped, std::bind(&OpenGLRenderPrivate::onSwapped, render));
		}

	private:
		OpenGLEmbedRenderPrivate *const render;

		void initializeGL()
		{
			render->initialize();
		}

		void paintGL()
		{
			render->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			render->glClear(GL_COLOR_BUFFER_BIT);
			QPainter painter(this);
			painter.setRenderHints(QPainter::SmoothPixmapTransform);
			QRect rect(QPoint(0, 0), lApp->findObject<ARender>()->getActualSize());
			render->drawDanm(&painter, rect);
		}
	};

	/*	The native child window that libvlc draws into.
	*/
	class ESurface :public QWidget
	{
	public:
		explicit ESurface(QWidget *parent) :QWidget(parent)
		{
			setAttribute(Qt::WA_NativeWindow, true);
			setAttribute(Qt::WA_TransparentForMouseEvents, true);
			setAutoFillBackground(false);
			setFocusPolicy(Qt::NoFocus);
			QPalette p = palette();
			p.setColor(QPalette::Window, Qt::black);
			setPalette(p);
			lower();
		}
	};

	class EFilter :public QObject
	{
	public:
		EFilter(OpenGLEmbedRenderPrivate *render, QObject *parent) :
			QObject(parent), render(render)
		{
		}

	private:
		OpenGLEmbedRenderPrivate *const render;

		bool eventFilter(QObject *o, QEvent *e) override
		{
			Q_UNUSED(o);
			switch (e->type()){
			case QEvent::Move:
			case QEvent::Resize:
			case QEvent::WindowStateChange:
				render->syncOverlay();
				break;
			case QEvent::Show:
				render->showOverlay();
				render->syncOverlay();
				break;
			case QEvent::Hide:
				render->hideOverlay();
				break;
			default:
				break;
			}
			return false;
		}
	};

#ifdef Q_OS_WIN
	/*	libvlc 在自己的线程里创建 "VLC video output" 之类的原生子窗口，它盖满整个画面，
		鼠标消息全被它吃掉，于是界面自己的「移动鼠标显形 / 点击跳转」在 Embed 模式下失效。
		办法：接管这些子窗口的 WndProc，把鼠标消息换算成主窗口坐标后转发回去。
	*/
	QHash<HWND, WNDPROC> g_orig;
	HWND g_target = nullptr;
	UINT g_mouse = 0;

	LRESULT CALLBACK forwardMouse(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		switch (msg){
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEWHEEL:
			if (g_target != nullptr){
				POINT p;
				p.x = GET_X_LPARAM(lparam);
				p.y = GET_Y_LPARAM(lparam);
				if (msg != WM_MOUSEWHEEL){
					ClientToScreen(hwnd, &p);
				}
				ScreenToClient(g_target, &p);
				// 直接把原始鼠标消息 PostMessage 过去，Qt 的窗口层会当成"没有真实光标"而丢掉，
				// 所以改发自定义消息，由 Home::nativeEvent 自己组装成 Qt 事件。
				PostMessage(g_target, g_mouse, (WPARAM)msg, MAKELPARAM(p.x, p.y));
			}
			return 0;
		default:
			break;
		}
		const WNDPROC proc = g_orig.value(hwnd, nullptr);
		if (proc != nullptr){
			return CallWindowProcW(proc, hwnd, msg, wparam, lparam);
		}
		return DefWindowProcW(hwnd, msg, wparam, lparam);
	}

	void subclassTree(HWND parent)
	{
		HWND child = nullptr;
		while ((child = FindWindowExW(parent, child, nullptr, nullptr)) != nullptr){
			if (!g_orig.contains(child)){
				const WNDPROC old = (WNDPROC)SetWindowLongPtrW(child, GWLP_WNDPROC, (LONG_PTR)forwardMouse);
				g_orig.insert(child, old);
			}
			subclassTree(child);
		}
	}

	void restoreTree(HWND parent)
	{
		HWND child = nullptr;
		while ((child = FindWindowExW(parent, child, nullptr, nullptr)) != nullptr){
			const WNDPROC old = g_orig.take(child);
			if (old != nullptr){
				SetWindowLongPtrW(child, GWLP_WNDPROC, (LONG_PTR)old);
			}
			restoreTree(child);
		}
	}
#endif
}

OpenGLEmbedRenderPrivate::OpenGLEmbedRenderPrivate()
{
	tv.disconnect();
	host = lApp->findObject<Interface>()->widget();
	surface = new ESurface(host);
	overlay = new EWindow(this);
	overlay->create();
	if (host){
		host->installEventFilter(new EFilter(this, host));
	}
	QObject::connect(lApp->findObject<APlayer>(), &APlayer::begin, overlay, &QWindow::show);
	// libvlc 在 set_hwnd 时会按当时的窗口尺寸建交换链，若那一刻尺寸还没定下来，
	// 视频就会一直画在左上角很小的一块；起播后轻微抖动一次尺寸，逼它重新 ResizeBuffers。
	QObject::connect(lApp->findObject<APlayer>(), &APlayer::begin, [this]() {
		for (int ms : { 600, 1500, 3000 }){
			QTimer::singleShot(ms, [this]() { nudge(); });
		}
	});
	syncOverlay();
#ifdef Q_OS_WIN
	g_target = (HWND)host->winId();
	g_mouse = RegisterWindowMessageW(L"BiliLocalEmbedMouse");
	subclassTree((HWND)surface->winId());
	// VLC 会按需重建视频子窗口（起播、换 vout、全屏切换），所以定期补挂一次。
	QTimer *scan = new QTimer(overlay);
	QObject::connect(scan, &QTimer::timeout, [this]() {
		if (surface != nullptr){
			subclassTree((HWND)surface->winId());
		}
	});
	scan->start(1000);
#endif
}

OpenGLEmbedRenderPrivate::~OpenGLEmbedRenderPrivate()
{
#ifdef Q_OS_WIN
	if (surface != nullptr){
		restoreTree((HWND)surface->winId());
	}
	g_target = nullptr;
#endif
	delete overlay;
}

void OpenGLEmbedRenderPrivate::syncOverlay()
{
	if (host == nullptr || overlay == nullptr){
		return;
	}
	const QPoint tl = host->mapToGlobal(QPoint(0, 0));
	const QSize s = host->size();
	overlay->setGeometry(QRect(tl, s));
}

void OpenGLEmbedRenderPrivate::showOverlay()
{
	if (overlay){
		overlay->show();
	}
}

void OpenGLEmbedRenderPrivate::hideOverlay()
{
	if (overlay){
		overlay->hide();
	}
}

void OpenGLEmbedRenderPrivate::resize(QSize size)
{
	qWarning("embed resize: %dx%d", size.width(), size.height());
	if (surface){
		surface->setGeometry(0, 0, size.width(), size.height());
		surface->lower();
		surface->show();
	}
	syncOverlay();
}

void OpenGLEmbedRenderPrivate::nudge()
{
	if (surface == nullptr){
		return;
	}
	const QSize s = surface->size();
	qWarning("nudge: surface=%dx%d dpr=%.2f visible=%d", s.width(), s.height(), surface->devicePixelRatioF(), int(surface->isVisible()));
	if (s.isEmpty()){
		return;
	}
	// Qt 会把同一次事件循环里的两次 resize 合并成最后一次，这里必须分两步、跨事件循环，
	// 否则子窗口收不到 WM_SIZE，VLC 的交换链就停在旧尺寸（表现为视频缩在左上角一小块）。
	surface->resize(qMax(s.width() * 3 / 4, 1), qMax(s.height() * 3 / 4, 1));
	QTimer::singleShot(120, [this, s]() {
		if (surface == nullptr){
			return;
		}
		surface->resize(s);
		syncOverlay();
		qWarning("nudge: restored %dx%d", s.width(), s.height());
	});
}

void OpenGLEmbedRenderPrivate::draw(QRect rect)
{
	if (overlay){
		overlay->update(rect);
	}
}
