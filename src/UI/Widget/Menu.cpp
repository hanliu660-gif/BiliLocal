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

using namespace UI;

namespace UI
{
	// 最近播放：有播放日期、非 Inherit 项、按日期倒序
	class RecentProxy :public QSortFilterProxyModel
	{
	public:
		explicit RecentProxy(QObject *parent = 0) :
			QSortFilterProxyModel(parent)
		{
			setSourceModel(lApp->findObject<List>());
			setSortRole(List::DateRole);
			sort(0, Qt::DescendingOrder);
		}

		QVariant data(const QModelIndex &index, int role) const override
		{
			return role == Qt::DecorationRole ? QVariant() : QSortFilterProxyModel::data(index, role);
		}

	private:
		bool filterAcceptsRow(int row, const QModelIndex &parent) const override
		{
			QModelIndex i = sourceModel()->index(row, 0, parent);
			if (!i.data(List::DateRole).toDateTime().isValid() || i.data(List::CodeRole).toInt() == List::Inherit){
				return false;
			}
			QStandardItem *c = lApp->findObject<List>()->getCurrent();
			return !c || c->index() != i;
		}
	};

	// 搜索：按文件名模糊过滤最近播放
	class SearchProxy :public QSortFilterProxyModel
	{
	public:
		using QSortFilterProxyModel::QSortFilterProxyModel;

		void setNeedle(const QString &n)
		{
			needle = n;
			invalidateFilter();
		}

	private:
		QString needle;

		bool filterAcceptsRow(int row, const QModelIndex &parent) const override
		{
			if (needle.isEmpty()){
				return true;
			}
			const QModelIndex i = sourceModel()->index(row, 0, parent);
			const QString file = i.data(List::FileRole).toString();
			return file.contains(needle, Qt::CaseInsensitive);
		}
	};
}

Menu::Menu(QWidget *parent) :
QWidget(parent)
{
	setObjectName("Menu");
	isPoped = false;

	recent = new RecentProxy(this);
	filtered = new SearchProxy(this);
	filtered->setSourceModel(recent);

	auto root = new QVBoxLayout(this);
	auto top = new QHBoxLayout;
	search = new QLineEdit(this);
	search->setPlaceholderText(tr("Search recent"));
	search->setClearButtonEnabled(true);
	connect(search, &QLineEdit::textChanged, [this](const QString &t){
		filtered->setNeedle(t);
	});
	// 回车直接播放当前过滤结果的第一条
	connect(search, &QLineEdit::returnPressed, [this](){
		const QModelIndex first = filtered->index(0, 0);
		if (first.isValid()){
			lApp->findObject<List>()->jumpToIndex(recent->mapToSource(filtered->mapToSource(first)));
		}
	});
	top->addWidget(search, 1);

	// 打开 / 载入弹幕：沿用原 QAction（Home 右键菜单按 objectName 引用它们）
	fileA = new QAction(tr("Open File"), this);
	fileA->setObjectName("File");
	fileA->setShortcut(Config::getValue("/Shortcut/File", QString()));
	danmA = new QAction(tr("Load Danmaku"), this);
	danmA->setObjectName("Danm");
	danmA->setShortcut(Config::getValue("/Shortcut/Danm", QString()));
	connect(fileA, &QAction::triggered, [this](){
		QString file = QFileDialog::getOpenFileName(
			lApp->findObject<Interface>()->widget(),
			tr("Open File"),
			lApp->findObject<List>()->defaultPath(Utils::Video | Utils::Audio),
			tr("Media files (%1);;All files (*.*)").arg(Utils::getSuffix(Utils::Video | Utils::Audio, "*.%1").join(' ')));
		if (!file.isEmpty()){
			lApp->findObject<APlayer>()->stop();
			lApp->findObject<APlayer>()->setMedia(file);
		}
	});
	// 在线弹幕（av 号/番剧/搜索）已随服务器下线移除，这里只剩本地弹幕文件加载
	connect(danmA, &QAction::triggered, [this](){
		QString file = QFileDialog::getOpenFileName(
			lApp->findObject<Interface>()->widget(),
			tr("Open File"),
			lApp->findObject<List>()->defaultPath(Utils::Danmaku),
			tr("Danmaku files (%1);;All files (*.*)").arg(Utils::getSuffix(Utils::Danmaku, "*.%1").join(' ')));
		if (!file.isEmpty()){
			lApp->findObject<Load>()->loadDanmaku(file);
		}
	});
	addAction(fileA);
	addAction(danmA);

	openB = new QPushButton(tr("Open"), this);
	danmB = new QPushButton(tr("Danmaku"), this);
	openB->setFocusPolicy(Qt::NoFocus);
	danmB->setFocusPolicy(Qt::NoFocus);
	openB->setToolTip(tr("Open File"));
	danmB->setToolTip(tr("Load a local danmaku file"));
	connect(openB, &QPushButton::clicked, fileA, &QAction::trigger);
	connect(danmB, &QPushButton::clicked, danmA, &QAction::trigger);
	top->addWidget(openB);
	top->addWidget(danmB);
	root->addLayout(top);

	view = new QListView(this);
	view->setModel(filtered);
	view->setAlternatingRowColors(true);
	view->setEditTriggers(QAbstractItemView::NoEditTriggers);
	view->setSelectionMode(QAbstractItemView::SingleSelection);
	view->setUniformItemSizes(true);
	// 双击播放
	connect(view, &QAbstractItemView::doubleClicked, [this](const QModelIndex &i){
		lApp->findObject<List>()->jumpToIndex(recent->mapToSource(filtered->mapToSource(i)));
	});
	// Del：从最近列表移除（清掉播放日期，不动文件与记录本身）
	QAction *rmA = new QAction(this);
	rmA->setShortcut(QKeySequence(Qt::Key_Delete));
	connect(rmA, &QAction::triggered, [this](){
		const QModelIndex i = view->currentIndex();
		if (i.isValid()){
			lApp->findObject<List>()->itemFromIndex(recent->mapToSource(filtered->mapToSource(i)))
				->setData(QVariant(), List::DateRole);
		}
	});
	view->addAction(rmA);
	root->addWidget(view, 1);

	// 底部紧凑行：弹幕不透明度 / 防挡字幕 / 循环
	auto bottom = new QHBoxLayout;
	alphaT = new QLabel(tr("Alpha"), this);
	alphaS = new QSlider(this);
	alphaS->setOrientation(Qt::Horizontal);
	alphaS->setRange(0, 100);
	alphaS->setValue(Config::getValue("/Danmaku/Alpha", 80));
	connect(alphaS, &QSlider::valueChanged, [this](int alpha){
		Config::setValue("/Danmaku/Alpha", alpha);
		if (alphaS->isVisible()){
			QPoint p;
			p.setX(QCursor::pos().x());
			p.setY(alphaS->mapToGlobal(alphaS->rect().center()).y());
			QToolTip::showText(p, QString::number(alpha));
		}
	});
	subC = new QCheckBox(tr("Protect"), this);
	subC->setToolTip(tr("Keep the bottom subtitle area danmaku-free"));
	subC->setChecked(Config::getValue("/Danmaku/Protect", false));
	connect(subC, &QCheckBox::stateChanged, [this](int state){
		Config::setValue("/Danmaku/Protect", state == Qt::Checked);
	});
	loopC = new QCheckBox(tr("Loop"), this);
	loopC->setChecked(Config::getValue("/Player/Loop", false));
	connect(loopC, &QCheckBox::stateChanged, [this](int state){
		Config::setValue("/Player/Loop", state == Qt::Checked);
	});
	bottom->addWidget(alphaT);
	bottom->addWidget(alphaS, 1);
	bottom->addWidget(subC);
	bottom->addWidget(loopC);
	root->addLayout(bottom);

	hide();
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
		hide();
		isPoped = false;
	}
}

void Menu::terminate()
{
}
