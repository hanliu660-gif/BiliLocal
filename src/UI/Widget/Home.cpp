#include "Common.h"
#include "Home.h"
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

namespace
{
	void setSingle(QMenu *menu)
	{
		QActionGroup *g = new QActionGroup(menu);
		for (auto *a : menu->actions()) {
			g->addAction(a)->setCheckable(true);
		}
	}

	void addTrack(QMenu *menu, int type)
	{
		QMenu *sub = new QMenu(Home::tr("Track"), menu);
		auto group = new QActionGroup(sub);
		auto track = lApp->findObject<APlayer>()->getTracks(type);
		for (int i = 0; i < track.size(); ++i) {
			auto action = group->addAction(track[i]);
			action->setCheckable(true);
			action->setChecked(lApp->findObject<APlayer>()->getTrack(type) == i);
			QObject::connect(action, &QAction::triggered, [=]() {
				lApp->findObject<APlayer>()->setTrack(type, i);
			});
		}
		sub->addActions(group->actions());
		sub->setEnabled(!sub->isEmpty());
		menu->addMenu(sub);
	}

	void addDelay(QMenu *menu, int type, qint64 step)
	{
		QMenu *sub = new QMenu(Home::tr("Sync"), menu);
		auto fake = sub->addAction(QString());
		fake->setEnabled(false);
		auto edit = new QLineEdit(sub);
		edit->setFrame(false);
		edit->setText(Home::tr("Delay: %1s").arg(lApp->findObject<APlayer>()->getDelay(type) / 1000.0));
		edit->setGeometry(sub->actionGeometry(fake).adjusted(24, 1, -1, -1));
		QObject::connect(edit, &QLineEdit::editingFinished, [=]() {
			QString e = QRegularExpression("[\\d\\.\\+\\-\\(][\\s\\d\\.\\:\\+\\-\\*\\/\\(\\)]*").match(edit->text()).captured();
			lApp->findObject<APlayer>()->setDelay(type, e.isEmpty() ? 0 : Utils::evaluate(e) * 1000);
		});
		QObject::connect(sub->addAction(Home::tr("%1s Faster").arg(step / 1000.0)), &QAction::triggered, [=]() {
			lApp->findObject<APlayer>()->setDelay(type, lApp->findObject<APlayer>()->getDelay(type) + step);
		});
		QObject::connect(sub->addAction(Home::tr("%1s Slower").arg(step / 1000.0)), &QAction::triggered, [=]() {
			lApp->findObject<APlayer>()->setDelay(type, lApp->findObject<APlayer>()->getDelay(type) - step);
		});
		sub->setEnabled(lApp->findObject<APlayer>()->getState() != APlayer::Stop);
		menu->addMenu(sub);
	}
}

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
	list = lApp->findObject<List>();
	load = lApp->findObject<Load>();

	timer = new QTimer(this);
	timer->setSingleShot(true);
	timer->setInterval(400);
	delay = new QTimer(this);
	connect(timer, &QTimer::timeout, [this]() {
		// 轮询判据：光标在窗口里、并且最近 2 秒内动过，才保持显形。
		// 之前的版本只看「光标在不在窗口内」，光标停在画面中间不动时两条会一直挂着。
		if (seekdrag) {
			lastMove = QDateTime::currentMSecsSinceEpoch();
			timer->start(400);
			return;
		}
		const QPoint cur = mapFromGlobal(QCursor::pos());
		const bool inside = rect().contains(cur);
		const bool overUi = info != nullptr && info->isVisible() && info->geometry().contains(cur);
		const qint64 idle = QDateTime::currentMSecsSinceEpoch() - lastMove;
		if (inside && (overUi || idle < 2000)) {
			if (!info->isShown()) {
				info->pop();
			}
			timer->start(400);
			return;
		}
		info->push();
	});
	// Embed 后端下视频是 VLC 的原生子窗口，鼠标消息原则上会被转发过来，
	// 但转发只覆盖"进入子窗口之后"，起播瞬间仍需自己把两条唤出来，所以常驻轮询。
	timer->start(400);
		delay->setSingleShot(true);
	connect(delay, &QTimer::timeout, lApp->findObject<APlayer>(), [this]() {
		if (aplayer->getState() == APlayer::Play && !menu->isVisible() && !info->isVisible()) {
			setCursor(QCursor(Qt::BlankCursor));
		}
	});

	connect(danmaku, &Danmaku::modelAppend, this, [this]() {
		qWarning("modelAppend: pool=%d busy=%d", danmaku->getPool().size(), load->busy());
		if (danmaku->getPool().size() >= 2 && !load->busy()) {
			UI::Editor::exec(this);
		}
	}, Qt::QueuedConnection);

	connect(aplayer, &APlayer::begin, this, [this]() {
		spd->defaultAction()->setChecked(true);
		spd->setEnabled(true);
		setWindowFilePath(aplayer->getMedia());
	});

	connect(aplayer, &APlayer::reach, this, [this](bool m) {
		Q_UNUSED(m);
		spd->setEnabled(false);
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

	seekdrag = false;
	lastMove = 0;
	wasMaximized = false;
	prevRate = 1.0;
	prevVolume = 0;
	speedHold = false;
	holdTimer = new QTimer(this);
	holdTimer->setSingleShot(true);
	connect(holdTimer, &QTimer::timeout, [this]() {
		// PotPlayer 式长按右键快进：按住 350ms 后进入 3 倍速，松开恢复；
		// 短按右键仍然是上下文菜单（见 mouseReleaseEvent）
		if (aplayer->getState() != APlayer::Play){
			return;
		}
		speedHold = true;
		prevRate = aplayer->getRate();
		if (prevRate < 0.1){
			prevRate = 1.0;
		}
		aplayer->setRate(3.0);
	});
	// 单击画面=暂停/继续：延迟判定，给双击全屏让路（双击会先到一次 press）
	clickTimer = new QTimer(this);
	clickTimer->setSingleShot(true);
	clickTimer->setInterval(250);
	connect(clickTimer, &QTimer::timeout, this, [this]() {
		qWarning("click-toggle: play/pause");
		aplayer->play();
	});

	addActions(menu->actions());
	addActions(info->actions());

	quitA = new QAction(tr("Quit"), this);
	quitA->setObjectName("Quit");
	quitA->setShortcut(Config::getValue("/Shortcut/Quit", QString("Ctrl+Q")));
	addAction(quitA);
	connect(quitA, &QAction::triggered, this, &Home::close);

	fullA = new QAction(tr("Full Screen"), this);
	fullA->setObjectName("Full");
	fullA->setCheckable(true);
	fullA->setShortcut(Config::getValue("/Shortcut/Full", QString("F")));
	addAction(fullA);
	connect(fullA, &QAction::toggled, [this](bool b) {
		if (b) {
			// 记下进全屏前的状态：从最大化进的全屏，退出后要回到最大化，
			// 而不是被 showNormal() 打回最初的普通窗口尺寸。
			wasMaximized = isMaximized();
			showFullScreen();
		}
		else if (wasMaximized) {
			// 退出全屏要回到最大化，但「全屏 → 最大化」会走 DWM 的窗口状态过渡动画
			// （先扩张再最大化）。DWMWA_TRANSITIONS_FORCEDISABLED 可以只对这一个窗口
			// 关掉过渡动画，切换完再放开，不影响系统其它程序。
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
		// Window 后端用 createWindowContainer 包住 QOpenGLWindow，全屏这种
		// 「窗口被重新摆放」的切换偶尔会让渲染容器停在旧尺寸 —— 表现就是
		// 只在左上角画了一小块、其余部分露出白底。这里补几次强制重排 + 重绘，
		// 等原生窗口的尺寸同步真正完成。
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
	});

	confA = new QAction(tr("Config"), this);
	confA->setObjectName("Conf");
	confA->setShortcut(Config::getValue("/Shortcut/Conf", QString("Ctrl+I")));
	addAction(confA);
	connect(confA, &QAction::triggered, [this]() {
		Prefer::exec(this);
	});

	toggA = new QAction(tr("Block All"), this);
	toggA->setObjectName("Togg");
	toggA->setCheckable(true);
	QString wholeShield = QString("m=%1").arg(Shield::Whole);
	toggA->setChecked(lApp->findObject<Shield>()->contains(wholeShield));
	toggA->setShortcut(Config::getValue("/Shortcut/Togg", QString("Ctrl+T")));
	addAction(toggA);
	connect(toggA, &QAction::triggered, [=](bool b) {
		if (b) {
			lApp->findObject<Shield>()->insert(wholeShield);
		}
		else {
			lApp->findObject<Shield>()->remove(wholeShield);
		}
		danmaku->parse(Danmaku::Block);
	});
	connect(lApp->findObject<Shield>(), &Shield::shieldChanged, [=]() {
		toggA->setChecked(lApp->findObject<Shield>()->contains(wholeShield));
	});

	listA = new QAction(tr("Playlist"), this);
	listA->setObjectName("List");
	listA->setShortcut(Config::getValue("/Shortcut/List", QString("Ctrl+L")));
	addAction(listA);
	connect(listA, &QAction::triggered, [this]() {
		Editor::exec(this);
	});


	QAction *fwdA = new QAction(tr("Forward"), this);
	fwdA->setObjectName("Fowd");
	fwdA->setShortcut(Config::getValue("/Shortcut/Fowd", QString("Right")));
	connect(fwdA, &QAction::triggered, [this]() {
		aplayer->setTime(aplayer->getTime() + Config::getValue("/Interface/Interval", 10) * 1000);
	});
	QAction *bwdA = new QAction(tr("Backward"), this);
	bwdA->setObjectName("Bkwd");
	bwdA->setShortcut(Config::getValue("/Shortcut/Bkwd", QString("Left")));
	connect(bwdA, &QAction::triggered, [this]() {
		aplayer->setTime(aplayer->getTime() - Config::getValue("/Interface/Interval", 10) * 1000);
	});
	addAction(fwdA);
	addAction(bwdA);

	QAction *delA = new QAction(tr("Delay"), this);
	delA->setObjectName("Dely");
	delA->setShortcut(Config::getValue("/Shortcut/Dely", QString("Ctrl+Right")));
	connect(delA, &QAction::triggered, std::bind(&Danmaku::delayAll, lApp->findObject<Danmaku>(), +1000));
	QAction *ahdA = new QAction(tr("Ahead"), this);
	ahdA->setObjectName("Ahed");
	ahdA->setShortcut(Config::getValue("/Shortcut/Ahed", QString("Ctrl+Left")));
	connect(ahdA, &QAction::triggered, std::bind(&Danmaku::delayAll, lApp->findObject<Danmaku>(), -1000));
	addAction(delA);
	addAction(ahdA);

	QAction *escA = new QAction(this);
	escA->setShortcut(Qt::Key_Escape);
	connect(escA, &QAction::triggered, [this]() {
		fullA->setChecked(false);
	});
	addAction(escA);

	QAction *vouA = new QAction(tr("VolUp"), this);
	vouA->setObjectName("VoUp");
	vouA->setShortcut(Config::getValue("/Shortcut/VoUp", QString("Up")));
	connect(vouA, &QAction::triggered, [this]() {aplayer->setVolume(aplayer->getVolume() + 5); });
	QAction *vodA = new QAction(tr("VolDn"), this);
	vodA->setObjectName("VoDn");
	vodA->setShortcut(Config::getValue("/Shortcut/VoDn", QString("Down")));
	connect(vodA, &QAction::triggered, [this]() {aplayer->setVolume(aplayer->getVolume() - 5); });
	addAction(vouA);
	addAction(vodA);

	QAction *pguA = new QAction(tr("Last Media"), this);
	pguA->setObjectName("PgUp");
	pguA->setShortcut(Config::getValue("/Shortcut/PgUp", QString("PgUp")));
	connect(pguA, &QAction::triggered, [this]() {list->jumpToLast(); });
	QAction *pgdA = new QAction(tr("Next Media"), this);
	pgdA->setObjectName("PgDn");
	pgdA->setShortcut(Config::getValue("/Shortcut/PgDn", QString("PgDown")));
	connect(pgdA, &QAction::triggered, [this]() {list->jumpToNext(); });
	addAction(pguA);
	addAction(pgdA);

	QAction *spuA = new QAction(tr("SpeedUp"), this);
	spuA->setObjectName("SpUp");
	spuA->setShortcut(Config::getValue("/Shortcut/SpUp", QString("Ctrl+Up")));
	connect(spuA, &QAction::triggered, [this]() {aplayer->setRate(aplayer->getRate() + 0.1); });
	QAction *spdA = new QAction(tr("SpeedDn"), this);
	spdA->setObjectName("SpDn");
	spdA->setShortcut(Config::getValue("/Shortcut/SpDn", QString("Ctrl+Down")));
	connect(spdA, &QAction::triggered, [this]() {aplayer->setRate(aplayer->getRate() - 0.1); });
	addAction(spuA);
	addAction(spdA);

	QAction *rstA = new QAction(tr("Reset"), this);
	rstA->setObjectName("Rest");
	rstA->setShortcut(Config::getValue("/Shortcut/Rest", QString("Home")));
	connect(rstA, &QAction::triggered, [this]() {
		aplayer->setRate(1.0);
		spd->defaultAction()->trigger();
	});
	addAction(rstA);

	// 数字键 0-9：按百分比跳转（长录播快速定位）
	for (int d = 0; d <= 9; ++d){
		QAction *pctA = new QAction(QString("Jump %1%").arg(d * 10), this);
		pctA->setObjectName(QString("Pct%1").arg(d));
		pctA->setShortcut(QKeySequence(QString::number(d)));
		addAction(pctA);
		connect(pctA, &QAction::triggered, [this, d]() {
			const qint64 dur = aplayer->getDuration();
			if (dur > 0){
				aplayer->setTime(dur * d / 10);
			}
		});
	}

	// Shift+←/→：大步长跳转（录播几十小时，10 秒一步太慢）
	QAction *fwd2A = new QAction(tr("Forward 60s"), this);
	fwd2A->setObjectName("Fowd2");
	fwd2A->setShortcut(Config::getValue("/Shortcut/Fowd2", QString("Shift+Right")));
	addAction(fwd2A);
	connect(fwd2A, &QAction::triggered, [this]() {
		aplayer->setTime(aplayer->getTime() + 60000);
	});
	QAction *bwd2A = new QAction(tr("Backward 60s"), this);
	bwd2A->setObjectName("Bkwd2");
	bwd2A->setShortcut(Config::getValue("/Shortcut/Bkwd2", QString("Shift+Left")));
	addAction(bwd2A);
	connect(bwd2A, &QAction::triggered, [this]() {
		aplayer->setTime(aplayer->getTime() - 60000);
	});

	// M：静音切换，记住静音前的音量
	QAction *muteA = new QAction(tr("Mute"), this);
	muteA->setObjectName("Mute");
	muteA->setShortcut(Config::getValue("/Shortcut/Mute", QString("M")));
	addAction(muteA);
	connect(muteA, &QAction::triggered, [this]() {
		if (aplayer->getVolume() > 0){
			prevVolume = aplayer->getVolume();
			aplayer->setVolume(0);
		}
		else {
			aplayer->setVolume(prevVolume > 0 ? prevVolume : 50);
		}
	});

	// T：窗口置顶切换（0=不置顶 / 2=始终置顶，见 setWindowFlags 的逻辑）
	QAction *topA = new QAction(tr("Stay On Top"), this);
	topA->setObjectName("Top");
	topA->setShortcut(Config::getValue("/Shortcut/Top", QString("T")));
	topA->setCheckable(true);
	topA->setChecked(Config::getValue("/Interface/Top", 0) >= 2);
	addAction(topA);
	connect(topA, &QAction::toggled, [this](bool on) {
		Config::setValue("/Interface/Top", on ? 2 : 0);
		setWindowFlags();
	});

		spd = new QMenu(tr("Speed"), this);
	spd->setEnabled(false);
	spd->addAction(QStringLiteral("×5.0"))->setData(5.0);
	spd->addAction(QStringLiteral("×2.0"))->setData(2.0);
	spd->addAction(QStringLiteral("×1.5"))->setData(1.5);
	spd->setDefaultAction(spd->addAction(QStringLiteral("×1.0")));
	spd->addAction(QStringLiteral("×0.8"))->setData(0.8);
	spd->addAction(QStringLiteral("×0.5"))->setData(0.5);
	spd->addAction(QStringLiteral("×0.2"))->setData(0.2);
	connect(spd, &QMenu::triggered, [this](QAction *action) {
		auto data = action->data();
		aplayer->setRate(data.isValid() ? data.toDouble() : 1.0);
	});
	setSingle(spd);

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
	// 双击全屏优先：掐掉挂起的单击暂停
	clickTimer->stop();
	// 按位置判断而不是按"条子是否可见"：以前条子可见的 2 秒窗口内双击会被忽略，
	// 全屏里想双击退出得先等条子收走。现在只要双击点不在条子上就切换。
	const QPoint p = e->position().toPoint();
	const bool overUi = (info->isVisible() && info->geometry().contains(p))
		|| (menu->isShown() && !menu->isWindow() && menu->geometry().contains(p));
		if (!overUi) {
			fullA->toggle();
		}
		// 故意不调 QWidget::mouseDoubleClickEvent：它的默认实现会把事件转发给
		// mousePressEvent，双击会额外触发一次单击暂停
	}

void Home::mouseMoveEvent(QMouseEvent * e)
{
	lastMove = QDateTime::currentMSecsSinceEpoch();
	QPoint cur = e->position().toPoint();
	// 悬停冻结：光标压在弹幕上就把它停住（移开自动恢复,无超时——
	// 鼠标静止时不产生 mousemove,靠续期的定时器会把冻结拆台）。
	// 拖进度条或悬停在底栏上时不冻结——那不是在看弹幕。
	// 悬浮面板是独立窗口，光标过去后事件不再进来，过期定时器自然解冻。
	if (seekdrag || (info->isVisible() && info->geometry().contains(cur))) {
		running->setHover(QPointF(), false);
	}
	else {
		running->setHover(cur, true);
	}
	if (seekdrag) {
		if (!info->isShown()) {
			info->pop();
		}
		timer->start(400);
		seekTo(cur, false);
		return;
	}
	if (!info->isShown()) {
		info->pop();
	}
	timer->start(400);
	if (cursor().shape() == Qt::BlankCursor) {
		unsetCursor();
	}
	delay->start(4000);
	QWidget::mouseMoveEvent(e);
}

int Home::infoHeight() const
{
	return int(4.2 * logicalDpiY() * font().pointSizeF() / 72);
}

// 底栏横带上的定位逻辑：按 x 比例算出时间点，commit 为真时才真正跳转
void Home::seekTo(QPoint p, bool commit)
{
	qint64 d = aplayer->getDuration();
	if (d <= 0) {
		return;
	}
	const double ratio = qBound(0.0, p.x() / double(qMax(width() - 1, 1)), 1.0);
	// 落在进度条上时按滑块的映射算（Info::timeAtX），否则退回整窗宽度；
	// 两条路径原来各算各的，同一个像素能差出半个进度条的长度。
	qint64 t = info ? info->timeAtX(p.x()) : -1;
	if (t < 0){
		t = qint64(d * ratio);
	}
	if (info) {
		info->previewTime(t);
	}
	if (commit) {
		aplayer->setTime(t);
	}
}

void Home::mousePressEvent(QMouseEvent * e)
{
	lastMove = QDateTime::currentMSecsSinceEpoch();
	// 长按右键快进的启动（短按松开仍是右键菜单，见 mouseReleaseEvent）
	if (e->button() == Qt::RightButton && aplayer->getState() == APlayer::Play){
		holdTimer->start(350);
	}
	if (e->button() == Qt::LeftButton) {
		QPoint p = e->position().toPoint();
		// 底栏整条横带都能点：即使 Info 还没滑出来，按下也直接定位
		if (p.y() >= height() - infoHeight()) {
			seekdrag = true;
			if (!info->isShown()) {
				info->pop();
			}
			timer->start(400);
			seekTo(p, false);
			return;
		}
		if (sta.isNull()) {
			sta = e->globalPosition().toPoint();
			wgd = pos();
		}
		bool inInfo = info->isShown() && info->geometry().contains(p);
		bool inMenu = !menu->isWindow() && menu->isShown() && menu->geometry().contains(p);
		if (!inInfo && !inMenu) {
			// 单击暂停/继续（默认开）：不立刻切，等 250ms——
			// 若下一拍是双击全屏，mouseDoubleClickEvent 会掐掉这个定时器
			if (Config::getValue("/Interface/Sensitive", true)) {
				qWarning("press@video -> clickTimer start");
				clickTimer->start();
			}
		}
	}
	QWidget::mousePressEvent(e);
}

void Home::mouseReleaseEvent(QMouseEvent * e)
{
	if (seekdrag) {
		seekdrag = false;
		seekTo(e->position().toPoint(), true);
		sta = wgd = QPoint();
		return;
	}
	if (!info->geometry().contains(e->position().toPoint())) {
		if (aplayer->getState() == APlayer::Play) {
			info->push(true);
		}
	}
	sta = wgd = QPoint();
	if (e->button() == Qt::RightButton) {
		if (speedHold){
			// 长按快进结束，恢复原来的倍速
			speedHold = false;
			aplayer->setRate(prevRate);
		}
		else {
			holdTimer->stop();
			showContextMenu(e->position().toPoint());
		}
	}
	QWidget::mouseReleaseEvent(e);
}

// 滚轮：直接调音量（现代播放器惯例），光标处带数字提示；Ctrl+滚轮调弹幕不透明度
void Home::wheelEvent(QWheelEvent * e)
{
	const int dy = e->angleDelta().y();
	if (dy != 0){
		if (e->modifiers() & Qt::ControlModifier){
			const int alpha = qBound(0, Config::getValue("/Danmaku/Alpha", 80) + (dy > 0 ? 5 : -5), 100);
			Config::setValue("/Danmaku/Alpha", alpha);
			QToolTip::showText(e->globalPosition().toPoint(), tr("Danmaku alpha %1%").arg(alpha));
		}
		else{
			const int vol = qBound(0, aplayer->getVolume() + (dy > 0 ? 5 : -5), 100);
			aplayer->setVolume(vol);
			QToolTip::showText(e->globalPosition().toPoint(), QString::number(vol));
		}
	}
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


void Home::showContextMenu(QPoint p)
{
	bool flag = true;
	flag = flag && !(menu->isVisible() && menu->geometry().contains(p));
	flag = flag && !(info->isVisible() && info->geometry().contains(p));
	if (flag) {
		QMenu top(this);
		const Comment *cur = running->commentAt(p);
		// 子菜单声明必须在函数作用域(exec 在 if 外),否则 if 闭合时
		// 栈对象析构,exec 时子菜单已销毁、菜单项凭空消失
		QMenu vid(tr("Video"), this);
		QMenu aud(tr("Audio"), this);
		QMenu sub(tr("Subtitle"), this);
		QMenu dmk(tr("Danmaku"), this);
		if (cur) {
			// 弹幕全文：QMenu 自带省略号看不全,换 QLabel 换行展示 + 可选中复制
			QLabel *title = new QLabel(cur->string, &top);
			title->setWordWrap(true);
			// 左缘对齐 QMenu::item 文字起点(QSS padding 18px + margin 4px)
			title->setContentsMargins(22, 2, 10, 2);
			title->setMaximumWidth(340 * logicalDpiX() / 96);
			title->setTextInteractionFlags(Qt::TextSelectableByMouse);
			QWidgetAction *text = new QWidgetAction(&top);
			text->setDefaultWidget(title);
			top.addAction(text);
			// 发送者数据块：禁用态菜单项，只读展示。
			// 录播姬 2.x 与新 B 站弹幕的 uid 已脱敏（一律 0）,按"匿名"展示
			const QString sender = cur->sender;
			const bool noId = sender.isEmpty() || sender == QLatin1String("0");
			const QDateTime sent = cur->date > 0 ? QDateTime::fromSecsSinceEpoch(cur->date) : QDateTime();
			auto infoItem = [&top](const QString &text) {
				QAction *a = top.addAction(text);
				a->setEnabled(false);
			};
			const bool hasHash = !cur->nick.isEmpty() && !cur->sender.isEmpty() && cur->sender != QLatin1String("0");
			infoItem(tr("User: %1").arg(cur->nick.isEmpty() ? (noId ? tr("anonymous") : sender) : cur->nick));
			if (hasHash) {
				infoItem(tr("Hash: %1").arg(cur->sender));
			}
			if (sent.isValid()) {
				infoItem(tr("Sent at: %1").arg(sent.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
			}
			{
				const qint64 sec = cur->time / 1000;
				const QString pos = sec >= 3600
					? QString("%1:%2:%3").arg(sec / 3600).arg(sec % 3600 / 60, 2, 10, QChar('0')).arg(sec % 60, 2, 10, QChar('0'))
					: QString("%1:%2").arg(sec / 60, 2, 10, QChar('0')).arg(sec % 60, 2, 10, QChar('0'));
				infoItem(tr("Position: %1").arg(pos));
			}
			const bool hasId = hasHash || (!sender.isEmpty() && sender != QLatin1String("0"));
			int same = 0;
			if (hasId) {
				for (const Record &r : danmaku->getPool()) {
					for (const Comment &c : r.danmaku) {
						if (c.sender == cur->sender) {
							++same;
						}
					}
				}
			}
			if (same > 1) {
				infoItem(tr("%1 danmaku from this sender").arg(same));
			}
			top.addSeparator();
			connect(top.addAction(tr("Copy")), &QAction::triggered, [=]() {
				qApp->clipboard()->setText(cur->string);
			});
			QAction *elim = top.addAction(tr("Eliminate The Sender"));
			elim->setEnabled(hasId);	// 屏蔽 u=0 等于清空整个池,禁用防手滑
			connect(elim, &QAction::triggered, [=]() {
				QString sender = cur->sender;
				if (!sender.isEmpty()) {
					lApp->findObject<Shield>()->insert("u=" + sender);
					danmaku->parse(Danmaku::Block);
				}
			});
		}
	// 点在弹幕上:这是弹幕专属菜单,只保留信息/复制/屏蔽;通用功能只在空白处右键出现
	if (cur == nullptr) {
			top.addSeparator();
			top.addActions(info->actions());
			top.addAction(menu->findChild<QAction *>("File"));
			top.addAction(listA);
			addTrack(&vid, Utils::Video);
			vid.addMenu(spd);
			vid.addAction(findChild<QAction *>("Rest"));
			top.addMenu(&vid);
			addTrack(&aud, Utils::Audio);
			addDelay(&aud, Utils::Audio, 100);
			top.addMenu(&aud);
			addTrack(&sub, Utils::Subtitle);
			addDelay(&sub, Utils::Subtitle, 500);
			connect(sub.addAction(tr("Load Subtitle")), &QAction::triggered, [this]() {
				QFileInfo info(aplayer->getMedia());
				QString file = QFileDialog::getOpenFileName(this,
					tr("Open File"),
					info.absolutePath(),
					tr("Subtitle files (%1);;All files (*.*)").arg(Utils::getSuffix(Utils::Subtitle, "*.%1").join(' ')));
				if (!file.isEmpty()) {
					aplayer->addSubtitle(file);
				}
			});
			top.addMenu(&sub);
			QMenu *dts = new QMenu(tr("Track"), &dmk);	// 堆分配:同作用域陷阱
			for (const Record r : danmaku->getPool()) {
				dts->addAction(r.string);
			}
			dts->setEnabled(!dts->isEmpty());
			dmk.addMenu(dts);
			connect(dmk.addAction(tr("Sync")), &QAction::triggered, [this]() {
				Editor::exec(this, 2);
			});
			QAction *loadA = menu->findChild<QAction *>("Danm");
			// 开关行显示为普通动作,文案随状态翻转(打开弹幕/关闭弹幕),
			// 点击走 Togg 的完整逻辑——不用勾选行:QSS 下勾选列会把文字
			// 顶出一条与其他行错位的怪缩进
			const bool blockedNow = lApp->findObject<Shield>()->contains(QString("m=%1").arg(Shield::Whole));
			dmk.addAction(blockedNow ? tr("Open Danmaku") : tr("Close Danmaku"),
				toggA, SLOT(trigger()), toggA->shortcut());
			dmk.addAction(tr("Load"), loadA, SLOT(trigger()), loadA->shortcut())->setEnabled(loadA->isEnabled());
			top.addMenu(&dmk);

			// 快捷键列宽度补偿:QSS 给 QMenu::item 的左右大内边距会把
			// 右对齐的快捷键压进长文字(播放列表/Ctrl+L 重叠),统一把
			// 主菜单与四个子菜单加宽到 容纳最长快捷键 为止
			int shortcutW = 0;
			QList<const QMenu *> menus{ &top, &vid, &aud, &sub, &dmk };
			for (const QMenu *m : menus){
				for (const QAction *a : m->actions()){
					if (!a->shortcut().isEmpty()){
						shortcutW = qMax(shortcutW,
							m->fontMetrics().horizontalAdvance(a->shortcut().toString(QKeySequence::NativeText)));
					}
				}
			}
			const int menuW = top.sizeHint().width() + shortcutW + 6;
			top.setMinimumWidth(menuW);
			for (QMenu *m : { &vid, &aud, &sub, &dmk }){
				m->setMinimumWidth(menuW);
			}
	}
		if (cur == nullptr) {
			top.addAction(confA);
			top.addAction(quitA);
		}
		// 菜单开着时钉住冻结：嵌套事件循环里过期定时器还会跑，
		// 不钉住的话菜单开着、弹幕却飞了
		if (cur) {
			running->pinHover(true);
		}
		top.exec(mapToGlobal(p));
		if (cur) {
			running->pinHover(false);
		}
	}
}
