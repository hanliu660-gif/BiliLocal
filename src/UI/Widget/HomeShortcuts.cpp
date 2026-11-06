#include "Common.h"
#include "HomeShortcuts.h"
#include "Home.h"
#include "Editor.h"
#include "Prefer.h"
#include "../../Config.h"
#include "../../Local.h"
#include "../../Model/Danmaku.h"
#include "../../Model/List.h"
#include "../../Model/Shield.h"
#include "../../Player/APlayer.h"

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
}

HomeShortcuts::HomeShortcuts(Home *parent)
	: QObject(parent)
	, home(parent)
	, quitA(nullptr)
	, fullA(nullptr)
	, confA(nullptr)
	, toggA(nullptr)
	, listA(nullptr)
	, snapA(nullptr)
	, spd(nullptr)
	, prevVolume(0)
{
	initActions();
}

void HomeShortcuts::initActions()
{
	APlayer *aplayer = lApp->findObject<APlayer>();
	Danmaku *danmaku = lApp->findObject<Danmaku>();
	List *list = lApp->findObject<List>();

	quitA = new QAction(Home::tr("Quit"), home);
	quitA->setObjectName("Quit");
	quitA->setShortcut(Config::getValue("/Shortcut/Quit", QString("Ctrl+Q")));
	home->addAction(quitA);
	connect(quitA, &QAction::triggered, home, &Home::close);

	fullA = new QAction(Home::tr("Full Screen"), home);
	fullA->setObjectName("Full");
	fullA->setCheckable(true);
	fullA->setShortcut(Config::getValue("/Shortcut/Full", QString("F")));
	home->addAction(fullA);
	connect(fullA, &QAction::toggled, home, &Home::applyFullScreen);

	confA = new QAction(Home::tr("Config"), home);
	confA->setObjectName("Conf");
	confA->setShortcut(Config::getValue("/Shortcut/Conf", QString("Ctrl+I")));
	home->addAction(confA);
	connect(confA, &QAction::triggered, [this]() {
		Prefer::exec(home);
	});

	toggA = new QAction(Home::tr("Block All"), home);
	toggA->setObjectName("Togg");
	toggA->setCheckable(true);
	QString wholeShield = QString("m=%1").arg(Shield::Whole);
	toggA->setChecked(lApp->findObject<Shield>()->contains(wholeShield));
	toggA->setShortcut(Config::getValue("/Shortcut/Togg", QString("Ctrl+T")));
	home->addAction(toggA);
	connect(toggA, &QAction::triggered, [danmaku, wholeShield](bool b) {
		if (b) {
			lApp->findObject<Shield>()->insert(wholeShield);
		}
		else {
			lApp->findObject<Shield>()->remove(wholeShield);
		}
		danmaku->parse(Danmaku::Block);
	});
	connect(lApp->findObject<Shield>(), &Shield::shieldChanged, [this, wholeShield]() {
		toggA->setChecked(lApp->findObject<Shield>()->contains(wholeShield));
	});

	listA = new QAction(Home::tr("Playlist"), home);
	listA->setObjectName("List");
	listA->setShortcut(Config::getValue("/Shortcut/List", QString("Ctrl+L")));
	home->addAction(listA);
	connect(listA, &QAction::triggered, [this]() {
		Editor::exec(home);
	});

	snapA = new QAction(Home::tr("Snapshot"), home);
	snapA->setObjectName("Snap");
	snapA->setShortcut(Config::getValue("/Shortcut/Snap", QString("Ctrl+S")));
	home->addAction(snapA);
	connect(snapA, &QAction::triggered, [aplayer]() {
		aplayer->snapshot();
	});

	QAction *fwdA = new QAction(Home::tr("Forward"), home);
	fwdA->setObjectName("Fowd");
	fwdA->setShortcut(Config::getValue("/Shortcut/Fowd", QString("Right")));
	connect(fwdA, &QAction::triggered, [aplayer]() {
		aplayer->setTime(aplayer->getTime() + Config::getValue("/Interface/Interval", 10) * 1000);
	});
	QAction *bwdA = new QAction(Home::tr("Backward"), home);
	bwdA->setObjectName("Bkwd");
	bwdA->setShortcut(Config::getValue("/Shortcut/Bkwd", QString("Left")));
	connect(bwdA, &QAction::triggered, [aplayer]() {
		aplayer->setTime(aplayer->getTime() - Config::getValue("/Interface/Interval", 10) * 1000);
	});
	home->addAction(fwdA);
	home->addAction(bwdA);

	QAction *delA = new QAction(Home::tr("Delay"), home);
	delA->setObjectName("Dely");
	delA->setShortcut(Config::getValue("/Shortcut/Dely", QString("Ctrl+Right")));
	connect(delA, &QAction::triggered, std::bind(&Danmaku::delayAll, danmaku, +1000));
	QAction *ahdA = new QAction(Home::tr("Ahead"), home);
	ahdA->setObjectName("Ahed");
	ahdA->setShortcut(Config::getValue("/Shortcut/Ahed", QString("Ctrl+Left")));
	connect(ahdA, &QAction::triggered, std::bind(&Danmaku::delayAll, danmaku, -1000));
	home->addAction(delA);
	home->addAction(ahdA);

	QAction *escA = new QAction(home);
	escA->setShortcut(Qt::Key_Escape);
	connect(escA, &QAction::triggered, [this]() {
		fullA->setChecked(false);
	});
	home->addAction(escA);

	QAction *vouA = new QAction(Home::tr("VolUp"), home);
	vouA->setObjectName("VoUp");
	vouA->setShortcut(Config::getValue("/Shortcut/VoUp", QString("Up")));
	connect(vouA, &QAction::triggered, [aplayer]() { aplayer->setVolume(aplayer->getVolume() + 5); });
	QAction *vodA = new QAction(Home::tr("VolDn"), home);
	vodA->setObjectName("VoDn");
	vodA->setShortcut(Config::getValue("/Shortcut/VoDn", QString("Down")));
	connect(vodA, &QAction::triggered, [aplayer]() { aplayer->setVolume(aplayer->getVolume() - 5); });
	home->addAction(vouA);
	home->addAction(vodA);

	QAction *pguA = new QAction(Home::tr("Last Media"), home);
	pguA->setObjectName("PgUp");
	pguA->setShortcut(Config::getValue("/Shortcut/PgUp", QString("PgUp")));
	connect(pguA, &QAction::triggered, [list]() { list->jumpToLast(); });
	QAction *pgdA = new QAction(Home::tr("Next Media"), home);
	pgdA->setObjectName("PgDn");
	pgdA->setShortcut(Config::getValue("/Shortcut/PgDn", QString("PgDown")));
	connect(pgdA, &QAction::triggered, [list]() { list->jumpToNext(); });
	home->addAction(pguA);
	home->addAction(pgdA);

	QAction *spuA = new QAction(Home::tr("SpeedUp"), home);
	spuA->setObjectName("SpUp");
	spuA->setShortcut(Config::getValue("/Shortcut/SpUp", QString("Ctrl+Up")));
	connect(spuA, &QAction::triggered, [aplayer]() { aplayer->setRate(aplayer->getRate() + 0.1); });
	QAction *spdA = new QAction(Home::tr("SpeedDn"), home);
	spdA->setObjectName("SpDn");
	spdA->setShortcut(Config::getValue("/Shortcut/SpDn", QString("Ctrl+Down")));
	connect(spdA, &QAction::triggered, [aplayer]() { aplayer->setRate(aplayer->getRate() - 0.1); });
	home->addAction(spuA);
	home->addAction(spdA);

	QAction *rstA = new QAction(Home::tr("Reset"), home);
	rstA->setObjectName("Rest");
	rstA->setShortcut(Config::getValue("/Shortcut/Rest", QString("Home")));
	connect(rstA, &QAction::triggered, [aplayer, this]() {
		aplayer->setRate(1.0);
		spd->defaultAction()->trigger();
	});
	home->addAction(rstA);

	for (int d = 0; d <= 9; ++d){
		QAction *pctA = new QAction(QString("Jump %1%").arg(d * 10), home);
		pctA->setObjectName(QString("Pct%1").arg(d));
		pctA->setShortcut(QKeySequence(QString::number(d)));
		home->addAction(pctA);
		connect(pctA, &QAction::triggered, [aplayer, d]() {
			const qint64 dur = aplayer->getDuration();
			if (dur > 0){
				aplayer->setTime(dur * d / 10);
			}
		});
	}

	QAction *fwd2A = new QAction(Home::tr("Forward 60s"), home);
	fwd2A->setObjectName("Fowd2");
	fwd2A->setShortcut(Config::getValue("/Shortcut/Fowd2", QString("Shift+Right")));
	home->addAction(fwd2A);
	connect(fwd2A, &QAction::triggered, [aplayer]() {
		aplayer->setTime(aplayer->getTime() + 60000);
	});
	QAction *bwd2A = new QAction(Home::tr("Backward 60s"), home);
	bwd2A->setObjectName("Bkwd2");
	bwd2A->setShortcut(Config::getValue("/Shortcut/Bkwd2", QString("Shift+Left")));
	home->addAction(bwd2A);
	connect(bwd2A, &QAction::triggered, [aplayer]() {
		aplayer->setTime(aplayer->getTime() - 60000);
	});

	QAction *muteA = new QAction(Home::tr("Mute"), home);
	muteA->setObjectName("Mute");
	muteA->setShortcut(Config::getValue("/Shortcut/Mute", QString("M")));
	home->addAction(muteA);
	connect(muteA, &QAction::triggered, [aplayer, this]() {
		if (aplayer->getVolume() > 0){
			prevVolume = aplayer->getVolume();
			aplayer->setVolume(0);
		}
		else {
			aplayer->setVolume(prevVolume > 0 ? prevVolume : 50);
		}
	});

	QAction *topA = new QAction(Home::tr("Stay On Top"), home);
	topA->setObjectName("Top");
	topA->setShortcut(Config::getValue("/Shortcut/Top", QString("T")));
	topA->setCheckable(true);
	topA->setChecked(Config::getValue("/Interface/Top", 0) >= 2);
	home->addAction(topA);
	connect(topA, &QAction::toggled, [this](bool on) {
		Config::setValue("/Interface/Top", on ? 2 : 0);
		home->setWindowFlags();
	});

	spd = new QMenu(Home::tr("Speed"), home);
	spd->setEnabled(false);
	spd->addAction(QStringLiteral("×5.0"))->setData(5.0);
	spd->addAction(QStringLiteral("×2.0"))->setData(2.0);
	spd->addAction(QStringLiteral("×1.5"))->setData(1.5);
	spd->setDefaultAction(spd->addAction(QStringLiteral("×1.0")));
	spd->addAction(QStringLiteral("×0.8"))->setData(0.8);
	spd->addAction(QStringLiteral("×0.5"))->setData(0.5);
	spd->addAction(QStringLiteral("×0.2"))->setData(0.2);
	connect(spd, &QMenu::triggered, [aplayer](QAction *action) {
		auto data = action->data();
		aplayer->setRate(data.isValid() ? data.toDouble() : 1.0);
	});
	setSingle(spd);
}
