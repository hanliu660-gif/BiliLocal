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
			// 不再点击穿透：覆盖层就是视频区的输入面，事件进来后按位置路由给
			// 工具条/底栏/Home（见 route()）。VLC 子窗口不需要任何鼠标输入。
			setFlags(flags() | Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
			QObject::connect(this, &EWindow::frameSwapped, std::bind(&OpenGLRenderPrivate::onSwapped, render));
#ifdef BILILOCAL_EMBED_TEST
			// 自动化验证：进程内注入合成鼠标事件走完整路由（正式构建不编译，
			// 且仅当设置了 BILILOCAL_EMBED_TEST 环境变量才启动）
			if (qEnvironmentVariableIsSet("BILILOCAL_EMBED_TEST")){
				QTimer *wiggle = new QTimer(this);
				int n = 0;
				QObject::connect(wiggle, &QTimer::timeout, [this, &n](){
					const QPointF local(200.0 + (n++ % 10) * 80.0, 320.0);
					QMouseEvent ev(QEvent::MouseMove, local, mapToGlobal(local.toPoint()),
						Qt::NoButton, Qt::NoButton, Qt::NoModifier);
					QCoreApplication::sendEvent(this, &ev);
				});
				wiggle->start(300);
			}
#endif
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

		// 把覆盖层收到的鼠标事件路由到覆盖层之下真正应该收它的部件：
		// 工具条/底栏/它们的子控件 → host->childAt；视频区 → Home 本体。
		void route(QMouseEvent *e)
		{
			QWidget *host = render->hostWidget();
			if (host == nullptr){
				return;
			}
			const QPoint gp = e->globalPos();
			const QPoint hp = host->mapFromGlobal(gp);
			QWidget *w = host->childAt(hp);
			if (w == nullptr){
				w = host;
			}
			if (e->type() == QEvent::MouseButtonPress){
				// 覆盖层不接收焦点，点击必须手动把主窗口带到前台，模拟原生行为
				host->activateWindow();
			}
			setCursor(host->cursor());
			QMouseEvent copy(e->type(), w->mapFromGlobal(gp), gp,
				e->button(), e->buttons(), e->modifiers());
			QCoreApplication::sendEvent(w, &copy);
		}

		void mouseMoveEvent(QMouseEvent *e) override { route(e); }
		void mousePressEvent(QMouseEvent *e) override { route(e); }
		void mouseReleaseEvent(QMouseEvent *e) override { route(e); }
		void mouseDoubleClickEvent(QMouseEvent *e) override { route(e); }
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
}

OpenGLEmbedRenderPrivate::~OpenGLEmbedRenderPrivate()
{
	delete overlay;
}

void OpenGLEmbedRenderPrivate::syncOverlay()
{
	if (host == nullptr || overlay == nullptr){
		return;
	}
	// 主窗口最小化/隐藏时弹幕层必须跟着收，否则透明层会孤零零地留在屏幕上
	if (host->isMinimized() || !host->isVisible()){
		overlay->hide();
		return;
	}
	const QPoint tl = host->mapToGlobal(QPoint(0, 0));
	const QSize s = host->size();
	overlay->setGeometry(QRect(tl, s));
}

void OpenGLEmbedRenderPrivate::showOverlay()
{
	if (overlay){
		// 挂成主窗口的 owned window：始终盖住主窗口（含其中的 VLC 子窗口），
		// 主窗口失焦后又不会像 WS_EX_TOPMOST 那样浮在别的应用上。
		if (host != nullptr && host->windowHandle() != nullptr && overlay->transientParent() != host->windowHandle()){
			overlay->setTransientParent(host->windowHandle());
		}
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
#ifdef Q_OS_WIN
	// 只在真的出问题时才抖动：交换链卡在旧尺寸时，VLC 的视频子窗口会停在
	// 创建时的小块上（正常时它会铺满 ESurface）。先量一下子窗口尺寸，
	// 铺满了就什么都不做——nudge 本身会让画面缩放一次，不能无条件跑。
	const HWND sparent = (HWND)surface->winId();
	qint64 childArea = 0;
	HWND child = nullptr;
	while ((child = FindWindowExW(sparent, child, nullptr, nullptr)) != nullptr){
		RECT rc = {};
		if (GetWindowRect(child, &rc)){
			childArea = qMax(childArea, qint64(rc.right - rc.left) * (rc.bottom - rc.top));
		}
		HWND sub = nullptr;
		while ((sub = FindWindowExW(child, sub, nullptr, nullptr)) != nullptr){
			RECT rc2 = {};
			if (GetWindowRect(sub, &rc2)){
				childArea = qMax(childArea, qint64(rc2.right - rc2.left) * (rc2.bottom - rc2.top));
			}
		}
	}
	RECT src = {};
	if (!GetClientRect(sparent, &src)){
		return;
	}
	const qint64 surfaceArea = qint64(src.right - src.left) * (src.bottom - src.top);
	// 5% 容差：铺满即视为健康
	if (surfaceArea <= 0 || childArea * 20 >= surfaceArea * 19){
		return;
	}
	qWarning("Embed: video surface stuck (%lld / %lld), nudging swapchain", childArea, surfaceArea);
#endif
	const QSize s = surface->size();
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
	});
}

void OpenGLEmbedRenderPrivate::draw(QRect rect)
{
	if (overlay){
		overlay->update(rect);
	}
}
