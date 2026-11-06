#include "Common.h"
#include "HomeGestures.h"
#include "Home.h"
#include "HomeShortcuts.h"
#include "HomeContextMenu.h"
#include "Info.h"
#include "Menu.h"
#include "../../Config.h"
#include "../../Local.h"
#include "../../Model/Running.h"
#include "../../Player/APlayer.h"
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDateTime>
#include <QToolTip>
#include <QCursor>

using namespace UI;

HomeGestures::HomeGestures(Home *parent)
	: QObject(parent)
	, home(parent)
	, timer(nullptr)
	, delay(nullptr)
	, holdTimer(nullptr)
	, clickTimer(nullptr)
	, speedHold(false)
	, prevRate(1.0)
	, seekdrag(false)
	, lastMove(0)
{
	initTimers();
}

void HomeGestures::initTimers()
{
	timer = new QTimer(this);
	timer->setSingleShot(true);
	timer->setInterval(400);
	connect(timer, &QTimer::timeout, [this]() {
		auto *info = home->infoWidget();
		if (seekdrag) {
			lastMove = QDateTime::currentMSecsSinceEpoch();
			timer->start(400);
			return;
		}
		const QPoint cur = home->mapFromGlobal(QCursor::pos());
		const bool inside = home->rect().contains(cur);
		const bool overUi = info != nullptr && info->isVisible() && info->geometry().contains(cur);
		const qint64 idle = QDateTime::currentMSecsSinceEpoch() - lastMove;
		if (inside && (overUi || idle < 2000)) {
			if (info && !info->isShown()) {
				info->pop();
			}
			timer->start(400);
			return;
		}
		if (info) {
			info->push();
		}
	});
	timer->start(400);

	delay = new QTimer(this);
	delay->setSingleShot(true);
	connect(delay, &QTimer::timeout, [this]() {
		auto *aplayer = lApp->findObject<APlayer>();
		auto *menu = home->menuWidget();
		auto *info = home->infoWidget();
		if (aplayer && aplayer->getState() == APlayer::Play &&
			(!menu || !menu->isVisible()) &&
			(!info || !info->isVisible())) {
			home->setCursor(QCursor(Qt::BlankCursor));
		}
	});

	holdTimer = new QTimer(this);
	holdTimer->setSingleShot(true);
	connect(holdTimer, &QTimer::timeout, [this]() {
		auto *aplayer = lApp->findObject<APlayer>();
		if (!aplayer || aplayer->getState() != APlayer::Play) {
			return;
		}
		speedHold = true;
		prevRate = aplayer->getRate();
		if (prevRate < 0.1) {
			prevRate = 1.0;
		}
		aplayer->setRate(3.0);
	});

	clickTimer = new QTimer(this);
	clickTimer->setSingleShot(true);
	clickTimer->setInterval(250);
	connect(clickTimer, &QTimer::timeout, [this]() {
		qWarning("click-toggle: play/pause");
		auto *aplayer = lApp->findObject<APlayer>();
		if (aplayer) {
			aplayer->play();
		}
	});
}

void HomeGestures::handleDoubleClick(QMouseEvent *e)
{
	clickTimer->stop();
	const QPoint p = e->position().toPoint();
	auto *info = home->infoWidget();
	auto *menu = home->menuWidget();
	const bool overUi = (info && info->isVisible() && info->geometry().contains(p))
		|| (menu && menu->isShown() && !menu->isWindow() && menu->geometry().contains(p));
	if (!overUi && home->shortcutsController()) {
		home->shortcutsController()->fullAction()->toggle();
	}
}

void HomeGestures::handleMouseMove(QMouseEvent *e)
{
	lastMove = QDateTime::currentMSecsSinceEpoch();
	QPoint cur = e->position().toPoint();
	auto *info = home->infoWidget();
	auto *running = lApp->findObject<Running>();

	if (seekdrag || (info && info->isVisible() && info->geometry().contains(cur))) {
		if (running) {
			running->setHover(QPointF(), false);
		}
	}
	else {
		if (running) {
			running->setHover(cur, true);
		}
	}
	if (seekdrag) {
		if (info && !info->isShown()) {
			info->pop();
		}
		timer->start(400);
		seekTo(cur, false);
		return;
	}
	if (info && !info->isShown()) {
		info->pop();
	}
	timer->start(400);
	if (home->cursor().shape() == Qt::BlankCursor) {
		home->unsetCursor();
	}
	delay->start(4000);
}

void HomeGestures::handleMousePress(QMouseEvent *e)
{
	lastMove = QDateTime::currentMSecsSinceEpoch();
	auto *aplayer = lApp->findObject<APlayer>();
	if (e->button() == Qt::RightButton && aplayer && aplayer->getState() == APlayer::Play) {
		holdTimer->start(350);
	}
	if (e->button() == Qt::LeftButton) {
		QPoint p = e->position().toPoint();
		if (p.y() >= home->height() - infoHeight()) {
			seekdrag = true;
			auto *info = home->infoWidget();
			if (info && !info->isShown()) {
				info->pop();
			}
			timer->start(400);
			seekTo(p, false);
			return;
		}
		if (sta.isNull()) {
			sta = e->globalPosition().toPoint();
			wgd = home->pos();
		}
		auto *info = home->infoWidget();
		auto *menu = home->menuWidget();
		bool inInfo = info && info->isShown() && info->geometry().contains(p);
		bool inMenu = menu && !menu->isWindow() && menu->isShown() && menu->geometry().contains(p);
		if (!inInfo && !inMenu) {
			if (Config::getValue("/Interface/Sensitive", true)) {
				qWarning("press@video -> clickTimer start");
				clickTimer->start();
			}
		}
	}
}

void HomeGestures::handleMouseRelease(QMouseEvent *e)
{
	if (seekdrag) {
		seekdrag = false;
		seekTo(e->position().toPoint(), true);
		sta = wgd = QPoint();
		return;
	}
	auto *info = home->infoWidget();
	auto *aplayer = lApp->findObject<APlayer>();
	if (info && !info->geometry().contains(e->position().toPoint())) {
		if (aplayer && aplayer->getState() == APlayer::Play) {
			info->push(true);
		}
	}
	sta = wgd = QPoint();
	if (e->button() == Qt::RightButton) {
		if (speedHold) {
			speedHold = false;
			if (aplayer) {
				aplayer->setRate(prevRate);
			}
		}
		else {
			holdTimer->stop();
			HomeContextMenu::show(home, e->position().toPoint());
		}
	}
}

void HomeGestures::handleWheel(QWheelEvent *e)
{
	const int dy = e->angleDelta().y();
	if (dy != 0) {
		if (e->modifiers() & Qt::ControlModifier) {
			const int alpha = qBound(0, Config::getValue("/Danmaku/Alpha", 80) + (dy > 0 ? 5 : -5), 100);
			Config::setValue("/Danmaku/Alpha", alpha);
			QToolTip::showText(e->globalPosition().toPoint(), Home::tr("Danmaku alpha %1%").arg(alpha));
		}
		else {
			auto *aplayer = lApp->findObject<APlayer>();
			const int vol = aplayer ? qBound(0, aplayer->getVolume() + (dy > 0 ? 5 : -5), 100) : 50;
			if (aplayer) {
				aplayer->setVolume(vol);
			}
			QToolTip::showText(e->globalPosition().toPoint(), QString::number(vol));
		}
	}
}

int HomeGestures::infoHeight() const
{
	return int(4.2 * home->logicalDpiY() * home->font().pointSizeF() / 72);
}

void HomeGestures::seekTo(QPoint p, bool commit)
{
	auto *aplayer = lApp->findObject<APlayer>();
	if (!aplayer) return;
	qint64 d = aplayer->getDuration();
	if (d <= 0) {
		return;
	}
	const double ratio = qBound(0.0, p.x() / double(qMax(home->width() - 1, 1)), 1.0);
	auto *info = home->infoWidget();
	qint64 t = info ? info->timeAtX(p.x()) : -1;
	if (t < 0) {
		t = qint64(d * ratio);
	}
	if (info) {
		info->previewTime(t);
	}
	if (commit) {
		aplayer->setTime(t);
	}
}
