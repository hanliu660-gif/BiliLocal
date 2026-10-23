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
#include "Editor.h"
#include "Prefer.h"
#include "WidgetInterfacePrivate.h"
#include "../Interface.h"
#include "../../Config.h"
#include "../../Local.h"
#include "../../Utils.h"
#include "../../Access/Load.h"
#include "../../Player/APlayer.h"
#include "../../Model/Danmaku.h"
#include "../../Model/List.h"
#include "../../Model/Shield.h"

using namespace UI;

namespace
{
	class MTableView :public QTableView
	{
	public:
		MTableView(QWidget *parent = 0) :
			QTableView(parent)
		{
		}

		void currentChanged(const QModelIndex &c,
			const QModelIndex &p)
		{
			QTableView::currentChanged(c, p);
			selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
		}
	};
}

Info::Info(QWidget *parent) :
QWidget(parent)
{
	setObjectName("Info");
	// Embed 输出时视频是原生子窗口，非原生控件会被它盖住；底栏必须自己成为原生窗口
	setAttribute(Qt::WA_NativeWindow, true);
	setAttribute(Qt::WA_StyledBackground, true);
	isStay = isPoped = updating = dragging = false;
	setStyleSheet("QWidget#Info{background:rgba(20,22,26,215);}"
		"QWidget#Info QPushButton{color:#e8e8ea;background:transparent;"
		"border:1px solid rgba(255,255,255,45);border-radius:4px;padding:2px 10px;}"
		"QWidget#Info QPushButton:hover{background:rgba(255,255,255,35);}"
		"QWidget#Info QPushButton:pressed{background:rgba(255,255,255,60);}");
	setAttribute(Qt::WA_StyledBackground, true);
	duration = -1;
	animation = new QPropertyAnimation(this, "pos", this);
	animation->setDuration(200);
	animation->setEasingCurve(QEasingCurve::OutCubic);
	timeT = new QLabel(tr("Time"), this);
	volmT = new QLabel(tr("Volume"), this);
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
	listB = new QPushButton(tr("列表"), this);
	openB = new QPushButton(tr("打开"), this);
	danmB = new QPushButton(tr("弹幕·开"), this);
	rateB = new QPushButton(QStringLiteral("1.0x"), this);
	prefB = new QPushButton(tr("设置"), this);
	fullB = new QPushButton(tr("全屏"), this);
	for (QPushButton *b : { openB, danmB, rateB, prefB, fullB, listB }){
		b->setFocusPolicy(Qt::NoFocus);
		b->setCursor(Qt::PointingHandCursor);
	}
	listB->setToolTip(tr("播放列表"));
	fullB->setToolTip(tr("全屏 / 退出全屏 (F / F11 / 双击画面)"));
	playI = QIcon::fromTheme("media-playback-start", QIcon(":/Picture/play.png"));
	pausI = QIcon::fromTheme("media-playback-pause", QIcon(":/Picture/pause.png"));
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
	connect(playA, SIGNAL(triggered()), lApp->findObject<APlayer>(), SLOT(play()));
	connect(playB, &QPushButton::clicked, playA, &QAction::trigger);
	// 打开视频
	connect(openB, &QPushButton::clicked, [this](){
		QString f = QFileDialog::getOpenFileName(this, tr("打开"), QString(),
			tr("视频 (*.mp4 *.flv *.mkv *.ts *.mov *.wmv *.avi *.m4v *.webm);;所有文件 (*)"));
		if (!f.isEmpty()){
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
		bool blocked = false;
		if (QAction *a = parentWidget()->findChild<QAction *>("Togg")){
			blocked = a->isChecked();
		}
		danmB->setText(blocked ? tr("弹幕·关") : tr("弹幕·开"));
	};
	connect(lApp->findObject<Shield>(), &Shield::shieldChanged, this, syncDanm);
	syncDanm();
	// 倍速按钮：文字实时跟随（包括长按右键 3 倍速）
	APlayer *ap = lApp->findObject<APlayer>();
	auto syncRate = [this](double r){ rateB->setText(QString("%1x").arg(r < 0.05 ? 1.0 : r, 0, 'f', 1)); };
	connect(ap, &APlayer::rateChanged, this, syncRate);
	syncRate(ap->getRate());
	connect(rateB, &QPushButton::clicked, [this](){
		QMenu menu(this);
		for (double r : { 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0 }){
			QAction *a = menu.addAction(QString("%1x").arg(r, 0, 'f', 1));
			a->setData(r);
		}
		QAction *picked = menu.exec(rateB->mapToGlobal(rateB->rect().bottomLeft()));
		if (picked){
			lApp->findObject<APlayer>()->setRate(picked->data().toDouble());
		}
	});
	duraT = new QLabel(this);
	duraT->setAlignment(Qt::AlignRight | Qt::AlignBottom);
	duraT->setText("00:00/00:00");
	danmV = new MTableView(this);
	danmV->setWordWrap(false);
	danmV->setSelectionBehavior(QAbstractItemView::SelectRows);
	danmV->setSelectionMode(QAbstractItemView::ExtendedSelection);
	danmV->verticalHeader()->hide();
	danmV->setAlternatingRowColors(true);
	danmV->setContextMenuPolicy(Qt::CustomContextMenu);
	danmV->setModel(lApp->findObject<Danmaku>());
	QHeaderView *header;
	header = danmV->horizontalHeader();
	header->setSectionResizeMode(0, QHeaderView::Fixed);
	header->setSectionResizeMode(1, QHeaderView::Stretch);
	header->setHighlightSections(false);
	resizeHeader();
	header = danmV->verticalHeader();
	header->setDefaultSectionSize(22.5*logicalDpiY() / 72);
	connect(lApp->findObject<Danmaku>(), &Danmaku::layoutChanged, this, &Info::resizeHeader);
	connect(danmV, &QTableView::doubleClicked, [this](QModelIndex index){
		lApp->findObject<APlayer>()->setTime(index.data(Danmaku::TimeRole).value<qint64>());
	});

	QAction *saveA = new QAction(tr("Save Danmaku to File"), this);
	saveA->setObjectName("Save");
	saveA->setShortcut(Config::getValue("/Shortcut/Save", QString()));
	saveA->setEnabled(false);
	connect(saveA, &QAction::triggered, [](){
		QFileDialog save(lApp->findObject<Interface>()->widget(), tr("Save File"));
		save.setAcceptMode(QFileDialog::AcceptSave);
		QFileInfo info(lApp->findObject<APlayer>()->getMedia());
		if (info.isFile()){
			save.setDirectory(info.absolutePath());
			save.selectFile(info.completeBaseName());
		}
		else{
			save.setDirectory(lApp->findObject<List>()->defaultPath(Utils::Danmaku));
		}
		save.setDefaultSuffix("json");
		QStringList type;
		type << tr("Bilibili Danmaku Format (*.xml)") << tr("AcFun Danmaku Format (*.json)");
		save.setNameFilters(type);
		save.connect(&save, &QFileDialog::filterSelected, [&](QString filter){
			save.setDefaultSuffix(filter.indexOf("xml") == -1 ? "json" : "xml");
		});
		if (save.exec() == QDialog::Accepted){
			QStringList file = save.selectedFiles();
			if (file.size() == 1){
				lApp->findObject<Danmaku>()->saveToFile(file.first());
			}
		}
	});
	danmV->addAction(saveA);

	connect(lApp->findObject<Danmaku>(), &Danmaku::modelReset, [=](){
		saveA->setEnabled(!lApp->findObject<Danmaku>()->getPool().isEmpty());
	});

	connect(danmV, &QTableView::customContextMenuRequested, [=](QPoint p){
		QMenu menu(this);
		auto selected = danmV->selectionModel()->selectedRows();
		if (!selected.isEmpty()){
			connect(menu.addAction(tr("Copy Danmaku")), &QAction::triggered, [=](){
				QStringList list;
				for (const QModelIndex &iter : selected) {
					QString string = iter.data(Danmaku::StringRole).toString();
					list.append(string);
				}
				qApp->clipboard()->setText(list.join('\n'));
			});
			
			QStringList ruleList;
			for (const QModelIndex &iter : selected) {
				QString sender = iter.data(Danmaku::SenderRole).toString();
				if (sender.isEmpty()) {
					continue;
				}
				ruleList.append("u=" + sender);
			}
			auto shield = lApp->findObject<Shield>();
			if (!ruleList.isEmpty()){
				connect(menu.addAction(tr("Eliminate The Sender")), &QAction::triggered, [=]() {
					for (const QString &iter : ruleList) {
						shield->insert(iter);
					}
					lApp->findObject<Danmaku>()->parse(Danmaku::Block);
				});
			}
			bool inList = false;
			for (const QString &iter : ruleList) {
				if (shield->contains(iter)) {
					inList = true;
					break;
				}
			}
			if (inList) {
				connect(menu.addAction(tr("Recover The Sender")), &QAction::triggered, [=]() {
					for (const QString &iter : ruleList) {
						shield->remove(iter);
					}
					lApp->findObject<Danmaku>()->parse(Danmaku::Block);
				});
			}
			menu.addSeparator();
		}
		connect(menu.addAction(tr("Edit Blocking List")), &QAction::triggered, [this](){
			Prefer::exec(lApp->findObject<Interface>()->widget(), 3);
		});
		connect(menu.addAction(tr("Edit Danmaku Pool")), &QAction::triggered, [this](){
			Editor::exec(lApp->findObject<Interface>()->widget(), 2);
		});
		connect(menu.addAction(tr("Clear Danmaku Pool")), &QAction::triggered, lApp->findObject<Danmaku>(), &Danmaku::clear);
		menu.addAction(saveA);
		isStay = 1;
		menu.exec(danmV->viewport()->mapToGlobal(p));
		isStay = 0;
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
	if (!isVisible()) {
		isPoped = false;
		return;
	}
	if (!preferStay() || force) {
		if (force) {
			isStay = false;
		}
		animation->stop();
		animation->setStartValue(pos());
		animation->setEndValue(restPos() + QPoint(0, height()));
		animation->start();
		isPoped = false;
	}
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
{	if (!playB || !timeT || !timeS || !volmS || !listB || !duraT || !volmT || !danmV){
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
	duraT->setGeometry(QRect(0, 0, 0, 0));
	volmT->setGeometry(QRect(0, 0, 0, 0));
	danmV->setGeometry(QRect(0, 0, 0, 0));
	QWidget::resizeEvent(e);
}

void Info::resizeHeader()
{
	Danmaku *d = lApp->findObject<Danmaku>();
	QStringList list;
	list.append(d->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString());
	int c = d->rowCount() - 1, i;
	for (i = 0; i <= c; ++i){
		if (d->data(d->index(i, 0), Qt::ForegroundRole).value<QColor>() != Qt::red){
			list.append(d->data(d->index(i, 0), Qt::DisplayRole).toString());
			break;
		}
	}
	for (i = c; i >= 0; --i){
		if (d->data(d->index(i, 0), Qt::ForegroundRole).value<QColor>() != Qt::red){
			list.append(d->data(d->index(i, 0), Qt::DisplayRole).toString());
			break;
		}
	}
	int m = 0;
	for (QString item : list){
		m = qMax(m, danmV->fontMetrics().width(item) + 8);
	}
	danmV->horizontalHeader()->resizeSection(0, m);
}

void Info::updateTimeText(qint64 _time)
{
	qint64 dur = duration > 0 ? duration : 0;
	int c = int(_time / 1000);
	int s = int(dur / 1000);
	auto to = [](int num){
		QString res = QString::number(num);
		res.prepend(QString(qMax(2 - res.length(), 0), '0'));
		return res;
	};
	timeT->setText(to(c / 60) + ':' + to(c % 60) + " / " + to(s / 60) + ':' + to(s % 60));
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
