/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    List.cpp
*   Time:        2014/11/19
*   Author:      Lysine
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
#include "List.h"
#include "../Config.h"
#include "../Local.h"
#include "../Utils.h"
#include "../Access/Load.h"
#include "../Model/Danmaku.h"
#include "../Player/APlayer.h"
#include <algorithm>

namespace
{
	class IconEngine :public QIconEngine
	{
	public:
		explicit IconEngine(int state) :
			state(state)
		{
		}

		void paint(QPainter *p, const QRect &r, QIcon::Mode, QIcon::State)
		{
			switch (state){
			case 0:
				p->drawPolyline(QPolygon({ (r.topRight() + r.bottomRight()) / 2, r.center(), (r.bottomLeft() + r.bottomRight()) / 2 }));
				break;
			case 1:
				p->drawLine((r.topRight() + r.topLeft()) / 2, (r.bottomLeft() + r.bottomRight()) / 2);
				break;
			case 2:
				p->drawPolyline(QPolygon({ (r.topRight() + r.bottomRight()) / 2, r.center(), (r.topLeft() + r.topRight()) / 2 }));
				break;
			default:
				break;
			}
		}

		QIconEngine *clone() const
		{
			return new IconEngine(state);
		}

	private:
		int state;
	};

	QModelIndex getGroupHead(QModelIndex i)
	{
		QModelIndex f;
		for (int o = 0;; ++o){
			f = i.sibling(i.row() - o, 0);
			if (f.data(List::CodeRole).toInt() == List::Records || f.row() == 0){
				return f;
			}
		}
	}

	// 同目录同名弹幕（录播姬默认就是这么放的），xml 优先于其它格式。
	// Z: 是 WebDAV，每次目录列举都有可感延迟——mediaChanged 与 itemFromFile
	// 共用这一个实现，调用方尽量把多次需求合并成一次扫描。
	QString danmakuNearby(const QFileInfo &video)
	{
		QString fallback;
		QStringList accept = Utils::getSuffix(Utils::Danmaku);
		for (const QFileInfo &iter : video.dir().entryInfoList(QDir::Files, QDir::Name)){
			if (iter.completeBaseName() != video.completeBaseName()){
				continue;
			}
			QString suffix = iter.suffix().toLower();
			if (!accept.contains(suffix)){
				continue;
			}
			if (suffix == "xml"){
				return iter.absoluteFilePath();
			}
			if (fallback.isEmpty()){
				fallback = iter.absoluteFilePath();
			}
		}
		return fallback;
	}

	QModelIndex getGroupTail(QModelIndex i)
	{
		for (;;){
			QModelIndex j = i.sibling(i.row() + 1, 0);
			if (j.isValid() && j.data(List::CodeRole).toInt() != List::Records){
				i = j;
			}
			else{
				return i;
			}
		}
	}

	void clearItemGroup(int row)
	{
		QStandardItem *i = lApp->findObject<List>()->item(row);
		i->setIcon(QIcon());
		i->setData(List::Records, List::CodeRole);
	}
}

List::List(QObject *parent)
	: QStandardItemModel(parent)
{
	setObjectName("List");
	cur = nullptr;
	time = 0;
	resumed = false;
	for (int i = 0; i < 3; ++i){
		icons.append(QIcon(new IconEngine(i)));
	}
	QStandardItem *lastE = nullptr;
	QStandardItem *lastD = nullptr;
	int lastC = 0;
	auto conbine = [&](){
		if (lastE&&lastD){
			QModelIndexList indexes;
			for (int i = lastE->row(); i <= lastD->row(); ++i){
				indexes.append(index(i, 0));
			}
			setRelated(indexes, lastC);
		}
	};
	for (const QJsonValue &i : Config::getValue<QJsonArray>("/List")){
		QStandardItem *item = new QStandardItem;
		QJsonObject data = i.toObject();
		QFileInfo info(data["File"].toString());
		item->setData(QColor(info.isFile() ? Qt::black : Qt::gray), Qt::ForegroundRole);
		item->setText(info.completeBaseName());
		item->setData(info.absoluteFilePath(), FileRole);
		item->setData(data["Time"].toDouble(), TimeRole);
		item->setData(data["Date"].toString(), DateRole);
		item->setEditable(false); item->setDropEnabled(false);
		QJsonValue danm = data["Danm"];
		if (danm.isArray()){
			for (const QJsonValue &j : danm.toArray()){
				QStandardItem *c = new QStandardItem;
				QJsonObject d = j.toObject();
				c->setData(d["Code"].toString(), CodeRole);
				c->setData(d["Time"].toDouble(), TimeRole);
				item->appendRow(c);
			}
			conbine();
			lastE = item;
		}
		else{
			lastC = data["Danm"].toString() == "Inherit" ? Inherit : Surmise;
			lastD = item;
		}
		appendRow(item);
	}
	conbine();

	connect(lApp->findObject<APlayer>(), &APlayer::timeChanged, [this](qint64 _time){
		time = _time;
	});
	connect(lApp->findObject<APlayer>(), &APlayer::mediaChanged, [this](QString file){
		resumed = false;
		updateCurrent();
		QStandardItem *old = cur;
		cur = itemFromFile(file, true);
		if (old){
			old->setData(QColor(Qt::black), Qt::ForegroundRole);
		}
		cur->setData(QColor(90, 115, 210), Qt::ForegroundRole);
		cur->setData(QDateTime::currentDateTime(), DateRole);
		bool success = false;
		switch (cur->data(CodeRole).toInt()){
		case Inherit:
			lApp->findObject<Danmaku>()->delayAll(-time);
			success = true;
			break;;
		case Surmise:
		{
			// 向上找所属的 Records 头。老实现是 for (int i = 1;; i++)：既没有
			// 上界，也不判 item() 是否为空。一旦第 0 行本身就是个孤立的 Surmise
			// 子项（外部改过 Config.txt、换机迁移、旧版遗留数据），row() - i 变负
			// 后 item() 返回 nullptr，下一行解引用就崩。
			QStandardItem *head = nullptr;
			int offset = 0;
			for (int i = 1; cur->row() - i >= 0; ++i){
				QStandardItem *up = item(cur->row() - i);
				if (up && up->data(CodeRole).toInt() == Records){
					head = up;
					offset = i;
					break;
				}
			}
			if (head){
				for (int j = 0; j < head->rowCount(); ++j){
					QString code = head->child(j)->data(CodeRole).toString();
					int sharp = code.indexOf(QRegularExpression("[#_]"));
					if (sharp != -1 && !QFile::exists(code)){
						QString id = code.mid(0, sharp);
						QString pt = code.mid(sharp + 1);
						lApp->findObject<Load>()->loadDanmaku((id + "#%1").arg(pt.toInt() + offset));
						success = true;
					}
				}
			}
			// 找不到头就不再冒认组内成员：success 保持 false，
			// 留给下面的“同目录同名弹幕”兜底。
			lApp->findObject<Danmaku>()->clear();
			break;
		}
		case Records:
		{
			QFileInfo info(cur->data(FileRole).toString());
			for (int i = 0; i < cur->rowCount(); ++i){
				Load *load = lApp->findObject<Load>();
				QStandardItem *d = cur->child(i);
				QString danmaku = d->data(CodeRole).toString();
				danmaku.replace("%{File}", info.completeBaseName()).replace("%{Path}", info.absolutePath());
				Load::Task task = load->codeToTask(danmaku);
				task.delay = d->data(List::TimeRole).value<qint64>();
				load->enqueue(task);
				success = true;
			}
			if (old || success){
				lApp->findObject<Danmaku>()->clear();
			}
			break;
		}
		}
		// 没登记弹幕的视频：找同目录同名的 xml/json。
		// 原来的条件还要求"弹幕池为空"，于是上一条视频残留的弹幕既挡住这次查找、
		// 又不会被清掉 —— 表现就是串台。改成无条件查找，找不到就清空，宁可没弹幕。
		if (!success){
			QFileInfo info(cur->data(FileRole).toString());
			QString nearby = danmakuNearby(info);
			if (!nearby.isEmpty()){
				lApp->findObject<Load>()->loadDanmaku(nearby);
				success = true;
			}
			if (!success){
				lApp->findObject<Danmaku>()->clear();
			}
		}
	});
	connect(lApp->findObject<APlayer>(), &APlayer::begin, this, [this](){
		// 断点续播：上次看到 10 秒以上且没看到结尾的，自动跳回存档点。
		// 每个媒体只回跳一次——循环播放重开时不能反复跳回。
		if (resumed || !cur || !Config::getValue("/Player/Resume", true)){
			return;
		}
		const qint64 saved = cur->data(TimeRole).value<qint64>();
		APlayer *ap = lApp->findObject<APlayer>();
		const qint64 dur = ap->getDuration();
		if (saved > 10000 && (dur <= 0 || saved < dur - 10000)){
			resumed = true;
			ap->setTime(saved);
		}
	});
	connect(lApp->findObject<APlayer>(), &APlayer::reach, this, [this](bool m){
		if (!m&&!finished()){
			QModelIndex i = indexFromItem(cur);
			if (jumpToIndex(index(i.row() + 1, 0, i.parent()), false) &&
				lApp->findObject<APlayer>()->getState() != APlayer::Play){
				lApp->findObject<APlayer>()->play();
				return;
			}
		}
		updateCurrent();
	});
	connect(lApp->findObject<Config>(), &Config::aboutToSave, this, &List::save);
}

QStringList List::mimeTypes() const
{
	return{ "application/x-bililocallistdata" };
}

namespace
{
	QList<QStandardItem *> getItems(const QModelIndexList &indexes)
	{
		QList<QStandardItem *> items;
		for (const QModelIndex&i : indexes){
			auto model = qobject_cast<const QStandardItemModel *>(i.model());
			items.append(model->itemFromIndex(i));
		}
		std::sort(items.begin(), items.end(), [](QStandardItem *f, QStandardItem *s){return f->row() < s->row(); });
		return items;
	}
}

QMimeData *List::mimeData(const QModelIndexList &indexes) const
{
	QByteArray byte;
	QDataStream s(&byte, QIODevice::WriteOnly);
	for (QStandardItem *item : getItems(indexes)){
		s << item->row();
	}
	QMimeData *data = new QMimeData;
	data->setData("application/x-bililocallistdata", byte);
	return data;
}

bool List::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
	if (action != Qt::MoveAction || column != 0 || parent.isValid()){
		return false;
	}
	if (item(row) && item(row)->data(CodeRole).toInt() != Records){
		return false;
	}
	QDataStream s(data->data("application/x-bililocallistdata"));
	QList<QStandardItem *> items;
	while (!s.atEnd()){
		int source;
		s >> source;
		items.append(item(source));
	}
	if (items.isEmpty()){
		return false;
	}
	std::sort(items.begin(), items.end(), [](QStandardItem *f, QStandardItem *s){return f->row() < s->row(); });
	for (QStandardItem *item : items){
		split(item->index());
		if (item->row() < row){
			--row;
		}
		insertRow(row, takeRow(item->row()));
		++row;
	}
	return true;
}

void List::setRelated(const QModelIndexList &indexes, int reason)
{
	if (indexes.size() < 2){
		return;
	}
	int i = -1;
	auto items = getItems(indexes);
	split(indexes);
	for (QStandardItem *item : items){
		if (i < 0){
			i = item->row();
			insertRow(i, takeRow(item->row()));
			item->setIcon(icons[0]);
		}
		else{
			++i;
			insertRow(i, takeRow(item->row()));
			item->setIcon(icons[1]);
			item->setData(reason, CodeRole);
			item->setRowCount(0);
		}
	}
	item(i)->setIcon(icons[2]);
}

QString List::resumeCandidate()
{
	// 播放日期倒序：优先「有断点(>10s)且文件存在」；全无断点则最新一条存在的
	QModelIndexList items;
	for (int i = 0; i < rowCount(); ++i){
		if (item(i)->data(DateRole).toDateTime().isValid()){
			items.append(index(i, 0));
		}
	}
	std::sort(items.begin(), items.end(), [this](const QModelIndex &f, const QModelIndex &s){
		return f.data(DateRole).toDateTime() > s.data(DateRole).toDateTime();
	});
	const QModelIndex *fallback = nullptr;
	for (const QModelIndex &i : items){
		const QString file = i.data(FileRole).toString();
		if (!QFile::exists(file)){
			continue;
		}
		if (i.data(TimeRole).toLongLong() > 10000){
			return file;
		}
		if (!fallback){
			fallback = &i;
		}
	}
	return fallback ? fallback->data(FileRole).toString() : QString();
}

QString List::defaultPath(int type)
{
	QStandardItem *item = cur;
	if (hasChildren() && !item){
		item = this->item(0);
		for (int i = 1; i < rowCount(); ++i){
			QStandardItem *iter = this->item(i);
			if (iter->data(DateRole).toDateTime() > item->data(DateRole).toDateTime()){
				item = iter;
			}
		}
	}
	if (item){
		if (type == Utils::Danmaku){
			for (int i = 0; i < item->rowCount(); ++i){
				QString code = item->child(i)->data(CodeRole).toString();
				if (QFile::exists(code)){
					return code;
				}
			}
		}
		return item->data(FileRole).toString();
	}
	QStringList paths = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation);
	paths.append(QDir::homePath());
	return paths.front();
}

namespace
{
	int diffAtNum(QString f, QString s)
	{
		for (int i = 0; i < f.size() && i < s.size(); ++i){
			int d = s[i].unicode() - f[i].unicode();
			if (d != 0){
				return f[i].isNumber() && s[i].isNumber() ? d : 0;
			}
		}
		return 0;
	}
}

QStandardItem *List::itemFromFile(QString file, bool create)
{
	int c = rowCount(), i;
	for (i = 0; i < c; ++i){
		if (item(i)->data(FileRole).toString() == file){
			return item(i);
		}
	}
	if (!create){
		return nullptr;
	}
	else{
		QFileInfo info(file);
		QString name = info.completeBaseName();
		QString path = info.absoluteFilePath();
		QStandardItem *item = new QStandardItem;
		item->setText(name);
		item->setData(path, FileRole);
		item->setEditable(false); item->setDropEnabled(false);
		appendRow(item);
		// 单次目录列举同时收集两样东西（Z: 是 WebDAV，每次列举都有可感延迟）：
		// 1) 同名 xml/json → 挂成子项：进列表时就绑定好，播放时
		//    （mediaChanged -> case Records）会随视频一起加载，不用再手动拖一次。
		//    存成 %{Path}/%{File}.<后缀> 模板（与 updateCurrent()/save() 的写法一致），
		//    这样整个录制目录搬家后不会失效。
		// 2) 同后缀、同系列编号的视频 → 直接建项并分组。
		QStringList accept = Utils::getSuffix(Utils::Danmaku);
		QString danmaku, fallback;
		QModelIndexList indexes;
		indexes.append(item->index());
		for (const QFileInfo &iter : info.dir().entryInfoList(QDir::Files, QDir::Name)){
			const QString o = iter.completeBaseName();
			if (o == name){
				const QString suffix = iter.suffix().toLower();
				if (accept.contains(suffix)){
					if (suffix == "xml"){
						danmaku = iter.absoluteFilePath();
					}
					else if (fallback.isEmpty()){
						fallback = iter.absoluteFilePath();
					}
				}
				continue;
			}
			const QString p = iter.absoluteFilePath();
			if (!itemFromFile(p) && info.suffix() == iter.suffix() && diffAtNum(name, o) > 0){
				QStandardItem *i = new QStandardItem;
				i->setText(o);
				i->setData(p, FileRole);
				i->setEditable(false); i->setDropEnabled(false);
				appendRow(i);
				indexes.append(i->index());
			}
		}
		if (danmaku.isEmpty()){
			danmaku = fallback;
		}
		if (!danmaku.isEmpty()){
			QStandardItem *c = new QStandardItem;
			c->setData(QString("%{Path}/%{File}.%1").arg(QFileInfo(danmaku).suffix().toLower()), CodeRole);
			c->setData(0, TimeRole);
			item->appendRow(c);
		}
		group(indexes);
		return item;
	}
}

bool List::finished()
{
	QStandardItem *n = item(cur ? cur->row() + 1 : 0);
	return !n || (n->data(CodeRole).toInt() == Records&&!Config::getValue("/Player/Continue", true));
}

void List::appendMedia(QString file)
{
	itemFromFile(file, true);
}

void List::updateCurrent()
{
	if (!cur){
		return;
	}
	cur->setRowCount(0);
	cur->setData(time, TimeRole);
	if (cur->data(CodeRole).toInt() != Records){
		return;
	}
	QFileInfo info(cur->data(FileRole).toString());
	for (const Record &r : lApp->findObject<Danmaku>()->getPool()){
		QStandardItem *d = new QStandardItem;
		QString danmaku = r.access;
		danmaku.replace(info.completeBaseName(), "%{File}").replace(info.absolutePath(), "%{Path}");
		d->setData(danmaku, CodeRole);
		d->setData(r.delay, TimeRole);
		cur->appendRow(d);
	}
}

void List::waste(const QModelIndex &index)
{
	if (itemFromIndex(index) == cur){
		return;
	}
	int self = index.row();
	int head = getGroupHead(index).row();
	int tail = getGroupTail(index).row();
	removeRow(self);
	switch (tail - head){
	case 0:
		return;
	case 1:
		clearItemGroup(head);
		return;
	default:
		--tail;
		item(tail)->setIcon(icons[2]);
		item(head)->setIcon(icons[0]);
		item(head)->setData(Records, CodeRole);
		return;
	}
}

void List::waste(const QModelIndexList &indexes)
{
	for (QStandardItem *item : getItems(indexes)){
		waste(item->index());
	}
}

void List::split(const QModelIndex &index)
{
	int self = index.row();
	int head = getGroupHead(index).row();
	int tail = getGroupTail(index).row();
	switch (tail - head){
	case 0:
		return;
	case 1:
		clearItemGroup(head);
		clearItemGroup(tail);
		return;
	default:
		insertRow(tail, takeRow(self));
		clearItemGroup(tail);
		--tail;
		item(tail)->setIcon(icons[2]);
		item(head)->setIcon(icons[0]);
		item(head)->setData(Records, CodeRole);
		return;
	}
}

void List::split(const QModelIndexList &indexes)
{
	QList<QStandardItem *> items = getItems(indexes);
	while (!items.isEmpty()){
		split(items.takeLast()->index());
	}
}

void List::merge(const QModelIndexList &indexes)
{
	setRelated(indexes, Inherit);
}

void List::group(const QModelIndexList &indexes)
{
	setRelated(indexes, Surmise);
}

void List::jumpToLast()
{
	int rc = rowCount();
	// 空列表时 PgUp/PgDn 仍能触发（这两个是窗口级 QAction），
	// 必须先挡住：否则下面的 (i + rc - 1) % rc 在 rc==0 时是整数
	// 除零，Windows 上直接 EXCEPTION_INT_DIVIDE_BY_ZERO 崩掉。
	if (rc <= 0){
		return;
	}
	int i = cur ? cur->row() : rc;
	jumpToIndex(index((i + rc - 1) % rc, 0));
}

void List::jumpToNext()
{
	int rc = rowCount();
	if (rc <= 0){
		return;
	}
	int i = cur ? cur->row() : -1;
	jumpToIndex(index((i + rc + 1) % rc, 0));
}

bool List::jumpToIndex(const QModelIndex &index, bool manually)
{
	QStandardItem *head = itemFromIndex(index);
	if (nullptr == head || (manually && head->data(CodeRole).toInt() == Inherit)){
		return false;
	}
	else {
		lApp->findObject<APlayer>()->stop(manually);
		lApp->findObject<APlayer>()->setMedia(head->data(FileRole).toString());
		return true;
	}
}

void List::save()
{
	QJsonArray list;
	updateCurrent();
	for (int i = 0; i < rowCount(); ++i) {
		QStandardItem *item = this->item(i);
		QJsonObject data;
		data["File"] = item->data(FileRole).toString();
		data["Time"] = item->data(TimeRole).toDouble();
		data["Date"] = item->data(DateRole).toString();
		switch (item->data(CodeRole).toInt()) {
		case Records:
		{
			QJsonArray danm;
			for (int j = 0; j < item->rowCount(); ++j) {
				QStandardItem *c = item->child(j);
				QJsonObject d;
				d["Code"] = c->data(CodeRole).toString();
				d["Time"] = c->data(TimeRole).toDouble();
				danm.append(d);
			}
			data["Danm"] = danm;
			break;
		}
		case Inherit:
			data["Danm"] = "Inherit";
			break;
		case Surmise:
			data["Danm"] = "Surmise";
			break;
		}
		list.append(data);
	}
	Config::setValue("/List", list);
}
