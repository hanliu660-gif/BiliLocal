/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    Menu.cpp
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
#include "Menu.h"
#include "WidgetInterfacePrivate.h"
#include "../Interface.h"
#include "../../Config.h"
#include "../../Local.h"
#include "../../Utils.h"
#include "../../Access/Load.h"
#include "../../Model/Danmaku.h"
#include "../../Model/List.h"
#include "../../Player/APlayer.h"
#include "../../Render/ARender.h"
#include <numeric>

using namespace UI;

namespace
{
	class ListProxyModel :public QSortFilterProxyModel
	{
	public:
		explicit ListProxyModel(QObject *parent = 0) :
			QSortFilterProxyModel(parent)
		{
			setSourceModel(lApp->findObject<List>());
			setSortRole(List::DateRole);
			sort(0, Qt::DescendingOrder);
		}

		QVariant data(const QModelIndex &index, int role) const
		{
			return role == Qt::DecorationRole ? QVariant() : QSortFilterProxyModel::data(index, role);
		}

	private:
		bool filterAcceptsRow(int row, const QModelIndex &parent) const
		{
			QModelIndex i = sourceModel()->index(row, 0, parent);
			if (!i.data(List::DateRole).toDateTime().isValid() || i.data(List::CodeRole).toInt() == List::Inherit){
				return false;
			}
			QStandardItem *c = lApp->findObject<List>()->getCurrent();
			return !c || c->index() != i;
		}
	};

	class FileEdit :public QLineEdit
	{
	public:
		explicit FileEdit(QCompleter *completer, QWidget *parent = 0) :
			QLineEdit(parent), select(false), completer(completer)
		{
			connect(this, &QLineEdit::selectionChanged, [this](){
				select = 0;
			});
		}

	private:
		bool select;
		QCompleter *completer;

		void mousePressEvent(QMouseEvent *e)
		{
			if (e->button() == Qt::LeftButton){
				select = 1;
			}
			QLineEdit::mousePressEvent(e);
		}

		void mouseReleaseEvent(QMouseEvent *e)
		{
			if (e->button() == Qt::LeftButton){
				if (select){
					completer->complete();
					completer->popup()->setCurrentIndex(completer->model()->index(0, 0));
				}
				select = 0;
			}
			QLineEdit::mouseReleaseEvent(e);
		}

		void focusInEvent(QFocusEvent *e)
		{
			//tab focus will cause whole selection
			if (e->reason() != Qt::TabFocusReason){
				QLineEdit::focusInEvent(e);
			}
		}
	};
}

Menu::Menu(QWidget *parent) :
QWidget(parent)
{
	setObjectName("Menu");
	isStay = isPoped = false;
	Utils::setGround(this, Qt::white);
	ListProxyModel *fileM = new ListProxyModel(this);
	fileC = new QCompleter(fileM, this);
	fileL = new FileEdit(fileC, this);
	fileL->installEventFilter(this);
	fileL->setReadOnly(true);
	fileL->setPlaceholderText(tr("choose a local media"));
	fileC->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
	fileC->setWidget(fileL);
	QAbstractItemView *popup;
	popup = fileC->popup();
	popup->setMouseTracking(true);
	QAction *hdelA = new QAction(popup);
	hdelA->setShortcut(QKeySequence(Qt::Key_Delete));
	connect(hdelA, &QAction::triggered, [=](){
		QModelIndex index = popup->currentIndex();
		index = qobject_cast<QAbstractProxyModel *>(fileC->completionModel())->mapToSource(index);
		index = fileM->mapToSource(index);
		lApp->findObject<List>()->itemFromIndex(index)->setData(QVariant(), List::DateRole);
	});
	popup->addAction(hdelA);
	connect(popup, SIGNAL(entered(QModelIndex)), popup, SLOT(setCurrentIndex(QModelIndex)));
	connect<void (QCompleter::*)(const QModelIndex &)>(fileC, &QCompleter::activated, [=](const QModelIndex &index){
		setFocus();
		lApp->findObject<List>()->jumpToIndex(fileM->mapToSource(qobject_cast<QAbstractProxyModel *>(fileC->completionModel())->mapToSource(index)));
	});
	fileB = new QPushButton(this);
	danmB = new QPushButton(this);
	fileB->setText(tr("Open"));
	danmB->setText(tr("Load Danmaku"));
	fileA = new QAction(tr("Open File"), this);
	fileA->setObjectName("File");
	fileA->setShortcut(Config::getValue("/Shortcut/File", QString()));
	danmA = new QAction(tr("Load Danmaku"), this);
	danmA->setObjectName("Danm");
	danmA->setShortcut(Config::getValue("/Shortcut/Danm", QString()));
	connect(fileA, &QAction::triggered, [this](){
		QString _file = QFileDialog::getOpenFileName(
			lApp->findObject<Interface>()->widget(),
			tr("Open File"),
			lApp->findObject<List>()->defaultPath(Utils::Video | Utils::Audio),
			tr("Media files (%1);;All files (*.*)").arg(Utils::getSuffix(Utils::Video | Utils::Audio, "*.%1").join(' ')));
		if (!_file.isEmpty()){
			lApp->findObject<APlayer>()->stop();
			lApp->findObject<APlayer>()->setMedia(_file);
		}
	});
	// 在线弹幕（av 号/番剧/搜索）已随服务器下线移除，这里只剩本地弹幕文件加载
	connect(danmA, &QAction::triggered, [this](){
		QString _file = QFileDialog::getOpenFileName(
			lApp->findObject<Interface>()->widget(),
			tr("Open File"),
			lApp->findObject<List>()->defaultPath(Utils::Danmaku),
			tr("Danmaku files (%1);;All files (*.*)").arg(Utils::getSuffix(Utils::Danmaku, "*.%1").join(' ')));
		if (!_file.isEmpty()){
			lApp->findObject<Load>()->loadDanmaku(_file);
		}
	});
	addAction(fileA);
	addAction(danmA);
	connect(fileB, &QPushButton::clicked, fileA, &QAction::trigger);
	connect(danmB, &QPushButton::clicked, danmA, &QAction::trigger);
	connect(lApp->findObject<APlayer>(), &APlayer::mediaChanged, [this](QString _file){
		fileL->setText(QFileInfo(_file).fileName());
		fileL->setCursorPosition(0);
	});
	alphaT = new QLabel(this);
	alphaT->setText(tr("Danmaku Alpha"));
	alphaS = new QSlider(this);
	alphaS->setOrientation(Qt::Horizontal);
	alphaS->setRange(0, 100);
	alphaS->setValue(Config::getValue("/Danmaku/Alpha", 80));
	connect(alphaS, &QSlider::valueChanged, [this](int _alpha){
		Config::setValue("/Danmaku/Alpha", _alpha);
		if (alphaS->isVisible()){
			QPoint p;
			p.setX(QCursor::pos().x());
			p.setY(alphaS->mapToGlobal(alphaS->rect().center()).y());
			QToolTip::showText(p, QString::number(_alpha));
		}
	});
	subT = new QLabel(this);
	subT->setText(tr("Protect Sub"));
	subC = new QCheckBox(this);
	subC->setChecked(Config::getValue("/Danmaku/Protect", false));
	connect(subC, &QCheckBox::stateChanged, [this](int state){
		Config::setValue("/Danmaku/Protect", state == Qt::Checked);
	});
	loopT = new QLabel(this);
	loopT->setText(tr("Loop Playback"));
	loopC = new QCheckBox(this);
	loopC->setChecked(Config::getValue("/Player/Loop", false));
	connect(loopC, &QCheckBox::stateChanged, [this](int state){
		Config::setValue("/Player/Loop", state == Qt::Checked);
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
	hide();
}

void Menu::resizeEvent(QResizeEvent *e)
{
	double f = font().pointSizeF();
	int x = logicalDpiX()*f / 72, y = logicalDpiY()*f / 72, w = e->size().width();
	fileL->setGeometry(0.83*x, 1.2*y, w - 5.65*x, 2.08*y);
	fileB->setGeometry(w - 4.44*x, 1.2*y, 3.60*x, 2.08*y);
	danmB->setGeometry(0.83*x, 3.9*y, w - 1.67*x, 2.08*y);
	alphaT->setGeometry(0.83*x, 6.8*y, w - 1.67*x, 1.7*y);
	alphaS->setGeometry(0.83*x, 8.5*y, w - 1.67*x, 1.25*y);
	int l = 15 * logicalDpiX() / 96;
	subT->setGeometry(0.83*x, 10.4*y, w * 0.45, 1.7*y);
	subC->setGeometry(int(w * 0.42), 10.55*y, l, 1.7*y);
	loopT->setGeometry(int(w * 0.52), 10.4*y, w * 0.4, 1.7*y);
	loopC->setGeometry(w - int(2.2*x), 10.55*y, l, 1.7*y);
}

bool Menu::eventFilter(QObject *o, QEvent *e)
{
	switch (e->type()){
	case QEvent::ContextMenu:
	{
		QLineEdit *l = qobject_cast<QLineEdit *>(o);
		if (!l){
			return 1;
		}
		isStay = 1;
		QMenu *m = l->createStandardContextMenu();
		m->exec(((QContextMenuEvent *)e)->globalPos());
		delete m;
		isStay = 0;
		return 1;
	}
	case QEvent::FocusIn:
		isStay = 1;
		return 0;
	case QEvent::FocusOut:
		isStay = 0;
		return 0;
	default:
		return 0;
	}
}

void Menu::pop()
{
	if (!isPoped){
		// 不激活式悬浮面板：弹出时不抢主窗口的键盘焦点，
		// 空格/方向键等快捷键在列表开着时照样可用
		show();
		raise();
		isPoped = true;
	}
}

void Menu::push(bool force)
{
	if (force&&isPoped){
		isStay = false;
		hide();
		isPoped = false;
	}
}

void Menu::terminate()
{
}
