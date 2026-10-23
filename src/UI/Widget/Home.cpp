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

	QAction *addScale(QMenu *menu, double src, double dst)
	{
		QAction *s = menu->addAction(QString("%1:%2").arg(src).arg(dst));
		s->setEnabled(false);
		s->setData(dst / src);
		return s;
	}

	QAction *addRatio(QMenu *menu, double num, double den)
	{
		QAction *r = menu->addAction(QString("%1:%2").arg(num).arg(den));
		r->setData(num / den);
		return r;
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
	QPalette palette = qApp->setStyle("Fusion")->standardPalette();
	palette.setColor(QPalette::Inactive, QPalette::ToolTipBase, Qt::white);
	qApp->setPalette(palette);
	QToolTip::setPalette(palette);

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
	type = new Type(this);
	list = lApp->findObject<List>();
	load = lApp->findObject<Load>();
	post = lApp->findObject<Post>();
	seek = lApp->findObject<Seek>();
	sign = lApp->findObject<Sign>();

	timer = new QTimer(this);
	timer->setSingleShot(true);
	timer->setInterval(400);
	delay = new QTimer(this);
	connect(timer, &QTimer::timeout, [this]() {
		// 轮询判据：光标在窗口里、并且最近 2 秒内动过，才保持显形。
		// 之前的版本只看「光标在不在窗口内」，光标停在画面中间不动时两条会一直挂着。
		if (seekdrag || sliding) {
			lastMove = QDateTime::currentMSecsSinceEpoch();
			timer->start(400);
			return;
		}
		const QPoint cur = mapFromGlobal(QCursor::pos());
		const bool inside = rect().contains(cur);
		const bool overBar = (bar != nullptr && bar->isVisible() && bar->geometry().contains(cur))
			|| (info != nullptr && info->isVisible() && info->geometry().contains(cur));
		const qint64 idle = QDateTime::currentMSecsSinceEpoch() - lastMove;
		if (inside && (overBar || idle < 2000)) {
			if (bar && !bar->isVisible()) {
				bar->show();
				bar->raise();
			}
			if (!info->isShown()) {
				info->pop();
			}
			timer->start(400);
			return;
		}
		info->push();
		if (bar) {
			bar->hide();
		}
	});
	// Embed 后端下视频是 VLC 的原生子窗口，鼠标消息原则上会被转发过来，
	// 但转发只覆盖"进入子窗口之后"，起播瞬间仍需自己把两条唤出来，所以常驻轮询。
	timer->start(400);
		delay->setSingleShot(true);
	connect(delay, &QTimer::timeout, lApp->findObject<APlayer>(), [this]() {
		if (aplayer->getState() == APlayer::Play && !menu->isVisible() && !info->isVisible()) {
			setCursor(QCursor(Qt::BlankCursor));
		}
		if (!sliding) {
			showprg = false;
			arender->setDisplayTime(0);
		}
	});

	connect(danmaku, &Danmaku::modelAppend, this, [this]() {
		if (danmaku->getPool().size() >= 2 && load->getHead() == nullptr) {
			UI::Editor::exec(this);
		}
	}, Qt::QueuedConnection);

	connect(aplayer, &APlayer::begin, this, [this]() {
		rat->defaultAction()->setChecked(true);
		spd->defaultAction()->setChecked(true);
		for (auto iter : sca->actions()) {
			if (iter->data().type() == QVariant::Double) {
				iter->setEnabled(true);
			}
		}
		if (!isFullScreen()) {
			// Embed 直出：起播完全不动窗口，用户摆什么样就什么样
			// （vmem 模式保持原行为：按视频尺寸开窗，超屏仍会被 setGeometry 拒绝）
			const bool embed = Config::getValue("/Render/Option/OpenGL/Output", QString()) == "Embed";
			if (geo.isEmpty() && !embed) {
				geo = saveGeometry();
				setGeometry(arender->getPreferSize(), false);
			}
		}
		rat->setEnabled(true);
		spd->setEnabled(true);
		setWindowFilePath(aplayer->getMedia());
	});

	connect(aplayer, &APlayer::reach, this, [this](bool m) {
		rat->setEnabled(false);
		spd->setEnabled(false);
		for (auto iter : sca->actions()) {
			if (iter->data().type() == QVariant::Double) {
				iter->setEnabled(false);
			}
		}
		arender->setVideoAspectRatio(0);
		if (!geo.isEmpty() && (list->finished() || m)) {
			fullA->setChecked(false);
			restoreGeometry(geo);
			geo.clear();
		}
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

	connect(load, &Load::progressChanged, this, &Home::percent);

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
	connect(post, &Post::errorOccured, this, alertNetworkError);
	connect(seek, &Seek::errorOccured, this, alertNetworkError);
	connect(sign, &Sign::errorOccured, this, alertNetworkError);

	showprg = sliding = seekdrag = false;
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
	connect(aplayer, &APlayer::timeChanged, [this](qint64 t) {
		if (!sliding&&aplayer->getState() != APlayer::Stop) {
			arender->setDisplayTime(showprg ? t / (double)aplayer->getDuration() : -1);
		}
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
				if (bar != nullptr) {
					bar->setGeometry(0, 0, sz.width(), bar->height());
					bar->raise();
				}
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

	postA = new QAction(tr("Post Danmaku"), this);
	postA->setObjectName("Post");
	postA->setEnabled(false);
	postA->setShortcut(Config::getValue("/Shortcut/Post", QString("Ctrl+P")));
	addAction(postA);
	connect(postA, &QAction::triggered, [this]() {
		menu->push();
		info->push();
		type->show();
	});
	connect(danmaku, &Danmaku::modelReset, [this]() {
		bool allow = false;
		for (const Record &r : danmaku->getPool()) {
			if (post->canPost(r.access)) {
				allow = true;
				break;
			}
		}
		postA->setEnabled(allow);
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
		rat->defaultAction()->trigger();
		sca->defaultAction()->trigger();
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

	// F10：截图，存到 图片\BiliLocal（libvlc 快照，直出模式下同样可用）
	QAction *snapA = new QAction(tr("Snapshot"), this);
	snapA->setObjectName("Snap");
	snapA->setShortcut(Config::getValue("/Shortcut/Snap", QString("F10")));
	snapA->setToolTip(tr("Snapshot to Pictures\\BiliLocal"));
	addAction(snapA);
	connect(snapA, &QAction::triggered, [this]() {
		aplayer->snapshot();
	});

	rat = new QMenu(tr("Ratio"), this);
	rat->setEnabled(false);
	rat->setDefaultAction(rat->addAction(tr("Default")));
	addRatio(rat, 4, 3);
	addRatio(rat, 16, 10);
	addRatio(rat, 16, 9);
	addRatio(rat, 1.85, 1);
	addRatio(rat, 2.35, 1);
	connect(rat, &QMenu::triggered, [this](QAction *action) {
		arender->setVideoAspectRatio(action->data().toDouble());
		if (aplayer->getState() == APlayer::Pause) {
			arender->draw();
		}
	});
	setSingle(rat);

	sca = new QMenu(tr("Scale"), this);
	connect(sca->addAction(fullA->text()), &QAction::toggled, fullA, &QAction::setChecked);
	sca->addAction(tr("Init Size"))->setData(QSize());
	sca->addAction(QStringLiteral("541×384"))->setData(QSize(540, 383));
	sca->addAction(QStringLiteral("672×438"))->setData(QSize(671, 437));
	sca->addSeparator();
	addScale(sca, 1, 4);
	addScale(sca, 1, 2);
	sca->setDefaultAction(addScale(sca, 1, 1));
	addScale(sca, 2, 1);
	addScale(sca, 4, 1);
	connect(sca, &QMenu::triggered, [this](QAction *action) {
		QVariant d = action->data();
		switch (d.type()) {
		case QVariant::Double:
			setGeometry(arender->getPreferSize() * d.toDouble(), false);
			break;
		case QVariant::Size:
			setGeometry(d.toSize(), false);
			break;
		default:
			break;
		}
	});
	setSingle(sca);

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

	bar = new QWidget(this);
	bar->setObjectName("BiliBar");
	bar->setAttribute(Qt::WA_NativeWindow, true);
	bar->setAttribute(Qt::WA_StyledBackground, true);
	bar->setStyleSheet(QStringLiteral(
		"QWidget#BiliBar{background:rgba(20,22,26,215);}"
		"QWidget#BiliBar QPushButton{color:#e8e8ea;background:transparent;"
		"border:1px solid rgba(255,255,255,45);border-radius:4px;padding:3px 12px;}"
		"QWidget#BiliBar QPushButton:hover{background:rgba(255,255,255,35);}"
		"QWidget#BiliBar QPushButton:pressed{background:rgba(255,255,255,60);}"));
	{
		QHBoxLayout *lay = new QHBoxLayout(bar);
		lay->setContentsMargins(8, 4, 8, 4);
		lay->setSpacing(6);
		lay->addStretch(1);
		auto addBtn = [&](QString text, QString tip, std::function<void()> slot) {
			QPushButton *b = new QPushButton(text, bar);
			b->setToolTip(tip);
			b->setFocusPolicy(Qt::NoFocus);
			b->setCursor(Qt::PointingHandCursor);
			connect(b, &QPushButton::clicked, slot);
			lay->addWidget(b);
		};
		addBtn(tr("打开"), tr("打开本地视频"), [this]() {
			QString f = QFileDialog::getOpenFileName(this, tr("打开"), QString(),
				tr("视频 (*.mp4 *.flv *.mkv *.ts *.mov *.wmv *.avi *.m4v *.webm);;所有文件 (*)"));
			if (!f.isEmpty()) {
				lApp->tryLocal(f);
			}
		});
		addBtn(tr("弹幕"), tr("加载本地弹幕文件 (xml / json)"), [this]() {
			QString f = QFileDialog::getOpenFileName(this, tr("加载弹幕"), QString(),
				tr("弹幕 (*.xml *.json);;所有文件 (*)"));
			if (!f.isEmpty()) {
				load->loadDanmaku(f);
			}
		});
		addBtn(tr("播放"), tr("播放 / 暂停 (Space)"), [this]() {
			aplayer->play();
		});
		addBtn(tr("设置"), tr("首选项"), [this]() {
			Prefer::exec(this, 0);
		});
		addBtn(tr("全屏"), tr("全屏 / 退出全屏 (F11)"), [this]() {
			fullA->setChecked(!fullA->isChecked());
		});
	}
	menu->setWindowFlags(Qt::Window);
	menu->setWindowTitle(tr("文件与弹幕"));
	menu->resize(320, 600);
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
	checkForUpdate();
	setGeometry(QSize(), true);
}

void Home::percent(double degree)
{
	auto p = msg ? qobject_cast<QProgressDialog *>(msg) : nullptr;
	QWidget *c = qApp->activeWindow();
	if (!p || (p != c && p->parent() != c && c)) {
		delete msg;
		p = new QProgressDialog(c);
		p->setMaximum(1000);
		p->setWindowTitle(tr("Loading"));
		p->setFixedSize(p->sizeHint());
		p->show();
		connect(p, &QProgressDialog::canceled, lApp->findObject<Load>(), &Load::dequeue);
		msg = p;
	}
	p->setValue(1000 * degree);
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
		for (const QString &item : QString(e->mimeData()->data("text/uri-list")).split('\n', QString::SkipEmptyParts)) {
			lApp->tryLocal(QUrl(item).toLocalFile().trimmed());
		}
	}
}

void Home::mouseDoubleClickEvent(QMouseEvent * e)
{
	if (!menu->isShown() && !info->isShown()) {
		fullA->toggle();
	}
	QWidget::mouseDoubleClickEvent(e);
}

void Home::mouseMoveEvent(QMouseEvent * e)
{
	lastMove = QDateTime::currentMSecsSinceEpoch();
	QPoint cur = e->pos();
	if (seekdrag) {
		if (bar && !bar->isVisible()) {
			bar->show();
			bar->raise();
		}
		if (!info->isShown()) {
			info->pop();
		}
		timer->start(400);
		seekTo(cur, false);
		return;
	}
	int x = cur.x(), w = width();
	if (!showprg&&aplayer->getState() != APlayer::Stop) {
		arender->setDisplayTime(aplayer->getTime() / (double)aplayer->getDuration());
	}
	showprg = true;
	if (bar && !bar->isVisible()) {
		bar->show();
		bar->raise();
	}
	if (!info->isShown()) {
		info->pop();
	}
	timer->start(400);
	if (cursor().shape() == Qt::BlankCursor) {
		unsetCursor();
	}
	delay->start(4000);
	if (!sta.isNull() && (windowFlags()&Qt::CustomizeWindowHint) != 0 && !isFullScreen()) {
		move(wgd + e->globalPos() - sta);
	}
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
	const qint64 t = qint64(d * ratio);
	if (arender) {
		arender->setDisplayTime(ratio);
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
		QPoint p = e->pos();
		// 底栏整条横带都能点：即使 Info 还没滑出来，按下也直接定位
		if (p.y() >= height() - infoHeight()) {
			seekdrag = true;
			if (bar) {
				bar->show();
				bar->raise();
			}
			if (!info->isShown()) {
				info->pop();
			}
			timer->start(400);
			sliding = true;
			seekTo(p, false);
			return;
		}
		if (sta.isNull()) {
			sta = e->globalPos();
			wgd = pos();
		}
		bool inInfo = info->isShown() && info->geometry().contains(p);
		bool inMenu = !menu->isWindow() && menu->isShown() && menu->geometry().contains(p);
		if (!inInfo && !inMenu) {
			if (Config::getValue("/Interface/Sensitive", false)) {
				aplayer->play();
			}
		}
	}
	QWidget::mousePressEvent(e);
}

void Home::mouseReleaseEvent(QMouseEvent * e)
{
	if (seekdrag) {
		seekdrag = false;
		sliding = false;
		seekTo(e->pos(), true);
		sta = wgd = QPoint();
		return;
	}
	if (!info->geometry().contains(e->pos())) {
		if (aplayer->getState() == APlayer::Play) {
			info->push(true);
		}
		type->hide();
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
			showContextMenu(e->pos());
		}
	}
	QWidget::mouseReleaseEvent(e);
}

// 滚轮：直接调音量（现代播放器惯例），光标处带数字提示
void Home::wheelEvent(QWheelEvent * e)
{
	const int dy = e->angleDelta().y();
	if (dy != 0){
		const int vol = qBound(0, aplayer->getVolume() + (dy > 0 ? 5 : -5), 100);
		aplayer->setVolume(vol);
		QToolTip::showText(e->globalPos(), QString::number(vol));
	}
	QWidget::wheelEvent(e);
}

void Home::resizeEvent(QResizeEvent * e)
{
	const QSize size = e->size();
	arender->resize(size);
	int w = size.width(), h = size.height();
	menu->terminate();
	info->terminate();
	int l = Config::getValue("/Interface/Popup/Width", 24 * font().pointSizeF())*logicalDpiX() / 72;
	if (bar) {
		int bh = bar->sizeHint().height();
		if (bh <= 0) {
			bh = int(fontMetrics().height() * 2.2) + 8;
		}
		bar->setGeometry(0, 0, w, bh);
		bar->raise();
	}
	if (!menu->isWindow()) {
		menu->setGeometry(menu->isShown() ? 0 : 0 - l, 0, l, h);
	}
	int ih = int(4.2 * logicalDpiY() * font().pointSizeF() / 72);
	info->setGeometry(0, h - ih, w, ih);
	info->raise();
	type->move(w / 2 - type->width() / 2, h - type->height() - 2);
	for (QAction *action : sca->actions()) {
		if (action->isSeparator() || !action->isEnabled()) {
			continue;
		}
		QVariant d = action->data();
		switch (d.type()) {
		case QVariant::Double:
		{
			QSize will = arender->getPreferSize() * d.toDouble();
			action->setChecked(will == size && !fullA->isChecked());
			break;
		}
		case QVariant::Size:
		{
			QSize will = d.toSize();
			if (will.isEmpty()) {
				will = parseSize(Config::getValue("/Interface/Size", QString("720,405")));
			}
			action->setChecked(will == size && !fullA->isChecked());
			break;
		}
		default:
			action->setChecked(fullA->isChecked());
			break;
		}
	}
	QWidget::resizeEvent(e);
}

QSize Home::parseSize(QString string)
{
	QStringList l = string.trimmed().split(',', QString::SkipEmptyParts);
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
	QRect s = QApplication::desktop()->screenGeometry(this);
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
	Qt::WindowFlags flags = windowFlags();
	if (Config::getValue("/Interface/Frameless", false)) {
		flags = Qt::CustomizeWindowHint;
	}
	if ((Config::getValue("/Interface/Top", 0) + (aplayer->getState() == APlayer::Play)) >= 2) {
		flags |= Qt::WindowStaysOnTopHint;
	}
	else {
		flags &= ~Qt::WindowStaysOnTopHint;
	}
	if (!testAttribute(Qt::WA_WState_Created)) {
		QWidget::setWindowFlags(flags);
	}
	else {
		emit lApp->findObject<Interface>()->flagsChanged(flags);
	}
}

void Home::checkForUpdate()
{
	QString url("https://raw.githubusercontent.com/AncientLysine/BiliLocal/master/res/INFO");
	auto update = Config::getValue("/Interface/Update", QVariant(url));
	if (QVariant::Bool == update.type()) {
		if (!update.toBool()) {
			return;
		}
	}
	else {
		url = update.toString();
	}
	QNetworkAccessManager *manager = new QNetworkAccessManager(this);
	manager->get(QNetworkRequest(url));
	connect(manager, &QNetworkAccessManager::finished, [=](QNetworkReply *info) {
		QUrl redirect = info->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
		if (redirect.isValid()) {
			manager->get(QNetworkRequest(redirect));
			return;
		}
		manager->deleteLater();
		if (info->error() != QNetworkReply::NoError) {
			return;
		}
		QFile local(":/Text/DATA");
		local.open(QIODevice::ReadOnly);
		QJsonObject l = QJsonDocument::fromJson(local.readAll()).object();
		QJsonObject r = QJsonDocument::fromJson(info->readAll()).object();
		if (!r.contains("Version") || l["Version"].toString() >= r["Version"].toString()) {
			return;
		}
		QMessageBox::StandardButton button;
		QString information = r["String"].toString();
		button = QMessageBox::information(this,
			tr("Update"),
			information,
			QMessageBox::Ok | QMessageBox::Cancel);
		if (button == QMessageBox::Ok) {
			QDesktopServices::openUrl(r["Url"].toString());
		}
	});
}

void Home::showContextMenu(QPoint p)
{
	bool flag = true;
	flag = flag && !(menu->isVisible() && menu->geometry().contains(p));
	flag = flag && !(info->isVisible() && info->geometry().contains(p));
	if (flag) {
		QMenu top(this);
		const Comment *cur = running->commentAt(p);
		if (cur) {
			QAction *text = new QAction(&top);
			top.addAction(text);
			int w = top.actionGeometry(text).width() - 24;
			text->setText(top.fontMetrics().elidedText(cur->string, Qt::ElideRight, w));
			top.addSeparator();
			connect(top.addAction(tr("Copy")), &QAction::triggered, [=]() {
				qApp->clipboard()->setText(cur->string);
			});
			connect(top.addAction(tr("Eliminate The Sender")), &QAction::triggered, [=]() {
				QString sender = cur->sender;
				if (!sender.isEmpty()) {
					lApp->findObject<Shield>()->insert("u=" + sender);
					danmaku->parse(Danmaku::Block);
				}
			});
			top.addSeparator();
		}
		top.addActions(info->actions());
		top.addAction(menu->findChild<QAction *>("File"));
		top.addAction(listA);
		QMenu vid(tr("Video"), this);
		vid.setFixedWidth(top.sizeHint().width());
		addTrack(&vid, Utils::Video);
		vid.addMenu(sca);
		vid.addMenu(rat);
		vid.addMenu(spd);
		vid.addAction(findChild<QAction *>("Rest"));
		top.addMenu(&vid);
		QMenu aud(tr("Audio"), this);
		aud.setFixedWidth(top.sizeHint().width());
		addTrack(&aud, Utils::Audio);
		addDelay(&aud, Utils::Audio, 100);
		aud.addAction(findChild<QAction *>("VoUp"));
		aud.addAction(findChild<QAction *>("VoDn"));
		top.addMenu(&aud);
		QMenu sub(tr("Subtitle"), this);
		sub.setFixedWidth(top.sizeHint().width());
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
		QMenu dmk(tr("Danmaku"), this);
		dmk.setFixedWidth(top.sizeHint().width());
		QMenu dts(tr("Track"), &dmk);
		for (const Record r : danmaku->getPool()) {
			dts.addAction(r.string);
		}
		dts.setEnabled(!dts.isEmpty());
		dmk.addMenu(&dts);
		connect(dmk.addAction(tr("Sync")), &QAction::triggered, [this]() {
			Editor::exec(this, 2);
		});
		QAction *loadA = menu->findChild<QAction *>("Danm");
		QAction *seekA = menu->findChild<QAction *>("Sech");
		dmk.addAction(toggA);
		dmk.addAction(tr("Load"), loadA, SLOT(trigger()), loadA->shortcut())->setEnabled(loadA->isEnabled());
		dmk.addAction(tr("Post"), postA, SLOT(trigger()), postA->shortcut())->setEnabled(postA->isEnabled());
		dmk.addAction(tr("Seek"), seekA, SLOT(trigger()), seekA->shortcut())->setEnabled(seekA->isEnabled());
		top.addMenu(&dmk);
		top.addAction(confA);
		top.addAction(quitA);
		top.exec(mapToGlobal(p));
	}
}
