/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    Info.cpp
*   Time:        2013/04/05
*   Author:      Lysine
*   Contributor: Chaserhkj
*
*   Lysine is a student majoring in Software Engineering
*   from the School of Software, SUN YAT-SEN UNIVERSITY.
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.

*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
=========================================================================*/

#include "Common.h"
#include "Info.h"
#include "../Interface.h"
#include "../../Config.h"
#include "../../Local.h"
#include "../../Utils.h"
#include "../../Player/APlayer.h"
#include "../../Model/Shield.h"
#include "../../Model/List.h"

using namespace UI;

namespace
{
	// 底栏恒为深色，图标统一浅灰；QPainter 矢量绘制（2x pixmap 供高分屏取用）
	QIcon barIcon(int kind)
	{
		QPixmap pm(32, 32);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		QPen pen(QColor(232, 232, 234), 1.7);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.scale(2, 2);
		switch (kind) {
		case 0:	// open：文件夹
			p.drawPolyline(QPolygon() << QPoint(2, 5) << QPoint(6, 5) << QPoint(8, 7) << QPoint(14, 7));
			p.drawRect(QRect(2, 7, 12, 6));
			break;
		case 1:	// settings：调节杆
			p.drawLine(3, 4, 13, 4);
			p.drawLine(3, 8, 13, 8);
			p.drawLine(3, 12, 13, 12);
			pen.setWidthF(3.4);
			p.setPen(pen);
			p.drawPoint(10.5, 4);
			p.drawPoint(5.5, 8);
			p.drawPoint(9, 12);
			break;
		case 2:	// fullscreen：四角
			p.drawPolyline(QPolygon() << QPoint(3, 6) << QPoint(3, 3) << QPoint(6, 3));
			p.drawPolyline(QPolygon() << QPoint(10, 3) << QPoint(13, 3) << QPoint(13, 6));
			p.drawPolyline(QPolygon() << QPoint(13, 10) << QPoint(13, 13) << QPoint(10, 13));
			p.drawPolyline(QPolygon() << QPoint(6, 13) << QPoint(3, 13) << QPoint(3, 10));
			break;
		case 3:	// playlist：三行
		default:
			p.drawLine(3, 4, 13, 4);
			p.drawLine(3, 8, 11, 8);
			p.drawLine(3, 12, 13, 12);
			break;
		}
		return QIcon(pm);
	}

	// 播放/暂停也用 QPainter 画：原来取 16x16 的 png，在 150%/200% 缩放下被拉伸发糊，
	// 而且 QIcon::fromTheme 在 Windows 上没有图标主题、永远走 fallback。
	// 按 dpr 逐档位原生绘制，任一级缩放都清晰
	QIcon playIcon(bool pause)
	{
		QIcon icon;
		for (double dpr : { 1.0, 1.5, 2.0, 3.0 }){
			const int side = qRound(18 * dpr);
			QPixmap pm(side, side);
			pm.fill(Qt::transparent);
			QPainter p(&pm);
			p.setRenderHint(QPainter::Antialiasing);
			p.scale(side / 16.0, side / 16.0);	// 统一按 16x16 坐标系画
			p.setPen(Qt::NoPen);
			p.setBrush(QColor(232, 232, 234));
			if (pause){
				p.drawRoundedRect(QRectF(4.0, 3.0, 3.0, 10.0), 0.8, 0.8);
				p.drawRoundedRect(QRectF(9.0, 3.0, 3.0, 10.0), 0.8, 0.8);
			}
			else{
				p.drawPolygon(QPolygonF() << QPointF(5.0, 3.0) << QPointF(13.0, 8.0) << QPointF(5.0, 13.0));
			}
			p.end();
			pm.setDevicePixelRatio(dpr);
			icon.addPixmap(pm);
		}
		return icon;
	}
}

Info::Info(QWidget *parent) :
QWidget(parent)
{
	setObjectName("Info");
	// Embed 输出时视频是原生子窗口，非原生控件会被它盖住；底栏必须自己成为原生窗口
	setAttribute(Qt::WA_NativeWindow, true);
	setAttribute(Qt::WA_StyledBackground, true);
	isPoped = updating = dragging = false;
	setStyleSheet("QWidget#Info{background:rgba(20,22,26,215);}"
		"QWidget#Info QPushButton{color:#e8e8ea;background:transparent;"
		"border:1px solid rgba(255,255,255,45);border-radius:4px;padding:2px 10px;}"
		"QWidget#Info QPushButton:hover{background:rgba(255,255,255,35);}"
		"QWidget#Info QPushButton:pressed{background:rgba(255,255,255,60);}");
	duration = -1;
	timeT = new QLabel(tr("Time"), this);
	timeS = new QSlider(this);
	volmS = new QSlider(this);
	timeS->setOrientation(Qt::Horizontal);
	volmS->setOrientation(Qt::Horizontal);
	timeS->setRange(0, 0);
	volmS->setRange(0, 100);
	timeS->setValue(0);
	volmS->setValue(Config::getValue("/Player/Volume", 50));
	timeS->setTracking(false);
	timeS->setSingleStep(5000);
	timeS->setPageStep(30000);
	timeS->installEventFilter(this);
	const QString sliderStyle =
		"QSlider{background:transparent;}"
		"QSlider::groove:horizontal{height:6px;background:#c9ccd2;border-radius:3px;}"
		"QSlider::sub-page:horizontal{background:#3ea6ff;border-radius:3px;}"
		"QSlider::handle:horizontal{width:14px;height:14px;margin:-5px 0;border-radius:7px;"
		"background:#ffffff;border:1px solid #9aa0a8;}";
	timeS->setStyleSheet(sliderStyle);
	volmS->setStyleSheet(sliderStyle);
	connect(timeS, &QSlider::valueChanged, [this](int _time){
		if (duration > 0 && !updating && !timeS->isSliderDown()){
			lApp->findObject<APlayer>()->setTime((qint64)_time);
			updateTimeText((qint64)_time);
		}
	});
	connect(timeS, &QSlider::sliderMoved, [this](int _time){
		updateTimeText((qint64)_time);
	});
	connect(volmS, &QSlider::sliderMoved, [this](int _volm){
		QPoint p;
		p.setX(QCursor::pos().x());
		p.setY(volmS->mapToGlobal(volmS->rect().center()).y());
		QToolTip::showText(p, QString::number(_volm));
	});
	connect(volmS, &QSlider::valueChanged, [this](int _volm){
		if (!updating){
			lApp->findObject<APlayer>()->setVolume(_volm);
		}
	});
	playB = new QPushButton(this);
	listB = new QPushButton(tr("Playlist"), this);
	listB->setIcon(barIcon(3));
	openB = new QPushButton(tr("Open"), this);
	openB->setIcon(barIcon(0));
	danmB = new QPushButton(tr("Danmaku On"), this);
	rateB = new QPushButton(QStringLiteral("1.0x"), this);
	prefB = new QPushButton(tr("Settings"), this);
	prefB->setIcon(barIcon(1));
	fullB = new QPushButton(tr("Fullscreen"), this);
	fullB->setIcon(barIcon(2));
	// 图标承担识别后文字隐藏（tooltip 保留说明），弹幕开关与倍速仍是文字按钮
	openB->setToolTip(tr("Open"));
	prefB->setToolTip(tr("Settings"));
	openB->setText(QString());
	prefB->setText(QString());
	fullB->setText(QString());
	listB->setText(QString());
	for (QPushButton *b : { openB, danmB, rateB, prefB, fullB, listB }){
		b->setFocusPolicy(Qt::NoFocus);
		b->setCursor(Qt::PointingHandCursor);
		b->setIconSize(QSize(18, 18));
	}
	listB->setToolTip(tr("Playlist"));
	fullB->setToolTip(tr("Fullscreen toggle (F / F11 / double-click)"));
	playI = playIcon(false);
	pausI = playIcon(true);
	playB->setIcon(playI);
	playA = new QAction(playI, tr("Play"), this);
	playA->setObjectName("Play");
	QList<QKeySequence> playS;
	playS << Config::getValue("/Shortcut/Play", QString("Space")) <<
		Qt::Key_MediaPlay <<
		Qt::Key_MediaPause <<
		Qt::Key_MediaTogglePlayPause;
	playA->setShortcuts(playS);
	addAction(playA);
	connect(playA, &QAction::triggered, lApp->findObject<APlayer>(), &APlayer::play);
	connect(playB, &QPushButton::clicked, playA, &QAction::trigger);
	// 打开视频：起始目录用 /Interface/OpenDir（与 Menu 面板的打开共用同一份记忆，
	// 选完写回，重启后仍在）。原来传空值，只靠原生对话框的进程内记忆，
	// 重启后每次都要重新找目录。
	connect(openB, &QPushButton::clicked, [this](){
		const QString dir = Config::getValue("/Interface/OpenDir",
			lApp->findObject<List>()->defaultPath(Utils::Video));
		QString f = QFileDialog::getOpenFileName(this, tr("Open File"), dir,
			tr("Video (*.mp4 *.flv *.mkv *.ts *.mov *.wmv *.avi *.m4v *.webm);;All files (*)"));
		if (!f.isEmpty()){
			Config::setValue("/Interface/OpenDir", QFileInfo(f).absolutePath());
			lApp->tryLocal(f);
		}
	});
	// 设置 / 全屏 / 弹幕开关：转发 Home 上对应的 QAction（Info 构造时它们还没建，点击时再找）
	connect(prefB, &QPushButton::clicked, [this](){
		if (QAction *a = parentWidget()->findChild<QAction *>("Conf")){
			a->trigger();
		}
	});
	connect(fullB, &QPushButton::clicked, [this](){
		if (QAction *a = parentWidget()->findChild<QAction *>("Full")){
			a->trigger();
		}
	});
	connect(danmB, &QPushButton::clicked, [this](){
		if (QAction *a = parentWidget()->findChild<QAction *>("Togg")){
			a->trigger();
		}
	});
	auto syncDanm = [this](){
		// 直接读屏蔽库,不经过 Togg action 的勾选状态——杜绝任何时序差
		const bool blocked = lApp->findObject<Shield>()->contains(QString("m=%1").arg(Shield::Whole));
		danmB->setText(blocked ? tr("Danmaku Off") : tr("Danmaku On"));
	};
		connect(lApp->findObject<Shield>(), &Shield::shieldChanged, this, syncDanm);
		syncDanm();
	// 倍速按钮：文字实时跟随（包括长按右键 3 倍速）
	APlayer *ap = lApp->findObject<APlayer>();
	auto syncRate = [this](double r){ rateB->setText(QString("%1x").arg(r < 0.05 ? 1.0 : r, 0, 'f', 1)); };
	connect(ap, &APlayer::rateChanged, this, syncRate);
	syncRate(ap->getRate());
	// 倍速按钮：单击在常用档位间循环（精细档位仍可从右键菜单 Video>Speed 选）
	connect(rateB, &QPushButton::clicked, [this](){
		static const double rates[] = { 1.0, 1.25, 1.5, 2.0, 3.0 };
		const double cur = lApp->findObject<APlayer>()->getRate();
		double next = rates[0];
		for (double r : rates){
			if (cur < r - 0.01){
				next = r;
				break;
			}
		}
		lApp->findObject<APlayer>()->setRate(next);
	});
	animation = new QPropertyAnimation(this, "pos", this);
	animation->setDuration(200);
	animation->setEasingCurve(QEasingCurve::OutCubic);
	connect(animation, &QPropertyAnimation::finished, [this](){
		if (!isPoped){
			hide();
			lApp->findObject<Interface>()->widget()->setFocus();
		}
	});

	connect(lApp->findObject<APlayer>(), &APlayer::timeChanged, this, &Info::setTime);
	connect(lApp->findObject<APlayer>(), &APlayer::volumeChanged, [this](int volume){
		updating = 1;
		volmS->setValue(volume);
		updating = 0;
	});
	connect(lApp->findObject<APlayer>(), &APlayer::begin, [this](){
		setDuration(lApp->findObject<APlayer>()->getDuration());
	});
	connect(lApp->findObject<APlayer>(), &APlayer::reach, [this](){
		setDuration(-1);
	});
	connect(lApp->findObject<APlayer>(), &APlayer::stateChanged, [this](int state){
		bool playing = state == APlayer::Play;
		playB->setIcon(playing ? pausI : playI);
		playA->setIcon(playing ? pausI : playI);
		playA->setText(playing ? tr("Pause") : tr("Play"));
	});
	hide();
}

// 底栏「归位点」：父窗口把它摆在哪，就滑回哪。不用 pos() 的历史值当基准，
// 否则动画被打断时 pos() 停在半路，下一次弹出就会从错的地方算。
QPoint Info::restPos() const
{
	QWidget * parent = parentWidget();
	if (parent == nullptr) {
		return pos();
	}
	return QPoint(0, parent->height() - height());
}

void Info::pop()
{
	if (isVisible() && pos() == restPos()) {
		isPoped = true;
		return;
	}
	// 不管上一个动画是什么状态，先停下再重来：动画被打断后停在 Running 是最容易
	// 让底栏永久拉不出来的原因（pop 会被 state != Stopped 挡掉）。
	animation->stop();
	show();
	raise();
	animation->setStartValue(pos());
	animation->setEndValue(restPos());
	animation->start();
	isPoped = true;
}

void Info::push(bool force)
{
	Q_UNUSED(force);
	if (!isVisible()) {
		isPoped = false;
		return;
	}
	animation->stop();
	animation->setStartValue(pos());
	animation->setEndValue(restPos() + QPoint(0, height()));
	animation->start();
	isPoped = false;
}
void Info::terminate()
{
	if (animation->state() != QAbstractAnimation::Stopped){
		animation->setCurrentTime(animation->totalDuration());
	}
}

// 进度条按「点哪跳哪、拖到哪松手就跳哪」处理，不依赖 QSlider 默认的
// 「点一下滚一页」语义；命中区域是整条底栏高度的横带。
bool Info::eventFilter(QObject *o, QEvent *e)
{
	if (o == timeS && duration > 0){
		const QEvent::Type t = e->type();
		if (t == QEvent::MouseButtonPress || t == QEvent::MouseMove || t == QEvent::MouseButtonRelease){
			QMouseEvent *me = static_cast<QMouseEvent *>(e);
			if (t == QEvent::MouseButtonPress && me->button() != Qt::LeftButton){
				return false;
			}
			if (t == QEvent::MouseMove && !dragging){
				return QWidget::eventFilter(o, e);
			}
			const int w = qMax(timeS->width() - 1, 1);
			const double ratio = qBound(0.0, me->pos().x() / double(w), 1.0);
			const qint64 target = qint64(duration * ratio);
			updating = 1;
			timeS->setValue(int(target));
			updating = 0;
			updateTimeText(target);
			if (t == QEvent::MouseButtonPress){
				dragging = true;
			}
			else if (t == QEvent::MouseButtonRelease){
				dragging = false;
				lApp->findObject<APlayer>()->setTime(target);
			}
			return true;
		}
	}
	return QWidget::eventFilter(o, e);
}

void Info::resizeEvent(QResizeEvent *e)
{	if (!playB || !timeT || !timeS || !volmS || !listB){
		QWidget::resizeEvent(e);
		return;
	}
	int w = e->size().width(), h = e->size().height();
	double f = font().pointSizeF();
	int x = logicalDpiX()*f / 72, y = logicalDpiY()*f / 72;
	int cy = int((h - 2.08*y) / 2);
	int bx = int(0.83*x), bw = int(2.5*x), bh = int(2.08*y);
	playB->setGeometry(QRect(bx, cy, bw, bh));
	int left = bx + bw + int(0.6*x);
	timeT->setGeometry(QRect(left, cy, int(13*x), bh));
	timeT->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	int slx = left + int(13.5*x);
	// 右侧从右往左摆：列表/全屏/设置/打开/音量/倍速/弹幕开关
	int xr = w - bx;
	auto place = [&](QWidget *b){
		// 用 sizeHint 自适应文字宽度，避免手工估宽与实际渲染不符导致重叠
		int wd = qMax(b->sizeHint().width(), int(3*x));
		xr -= wd;
		b->setGeometry(QRect(xr, cy, wd, bh));
		xr -= int(0.4*x);
	};
	place(listB);
	place(fullB);
	place(prefB);
	place(openB);
	volmS->setGeometry(QRect(xr - int(9*x), int(0.4*y), int(9*x), h - int(0.8*y)));
	xr -= int(9.4*x);
	place(rateB);
	place(danmB);
	int slw = xr - int(0.5*x) - slx;
	// 进度条铺满整条底栏的高度，鼠标在整条横带上任意位置都能拖动
	timeS->setGeometry(QRect(slx, int(0.4*y), qMax(slw, int(8*x)), h - int(0.8*y)));
	QWidget::resizeEvent(e);
}

void Info::updateTimeText(qint64 _time)
{
	qint64 dur = duration > 0 ? duration : 0;
	// 长录播动辄数小时：总分钟数显示（1400:20）没人读得懂，超 1 小时用 h:mm:ss
	auto fmt = [](qint64 ms){
		qint64 sec = ms / 1000;
		const QString sign = sec < 0 ? "-" : "";
		sec = qAbs(sec);
		const qint64 h = sec / 3600, m = sec % 3600 / 60, s = sec % 60;
		auto two = [](qint64 n){ return QString("%1").arg(n, 2, 10, QChar('0')); };
		return h > 0
			? sign + QString::number(h) + ':' + two(m) + ':' + two(s)
			: sign + two(m) + ':' + two(s);
	};
	timeT->setText(fmt(_time) + " / " + fmt(dur));
}

void Info::setTime(qint64 _time)
{
	if (duration > 0){
		if (!dragging){
			updating = 1;
			timeS->setValue(int(qBound<qint64>(0, _time, duration)));
			updating = 0;
		}
		updateTimeText(_time);
	}
}

void Info::setDuration(qint64 _duration)
{
	if (_duration > 0){
		duration = _duration;
		timeS->setRange(0, int(qMin<qint64>(_duration, 0x7fffffff)));
		timeS->setPageStep(int(qMin<qint64>(_duration / 10, 60000)));
		updateTimeText(0);
	}
	else{
		duration = -1;
		updating = 1;
		timeS->setValue(0);
		timeS->setRange(0, 0);
		updating = 0;
		timeT->setText("00:00 / 00:00");
	}
}
