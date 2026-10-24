/*=======================================================================
*
*   Copyright (C) 2013-2016 Lysine.
*
*   Filename:    Running.cpp
*   Time:        2016/05/29
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
#include "Running.h"
#include "Danmaku.h"
#include "../Config.h"
#include "../Local.h"
#include "../Graphic/Graphic.h"
#include "../Player/APlayer.h"
#include "../Render/ARender.h"
#include <algorithm>

namespace
{
	class TextCounter : QHash<QString, int>
	{
	public:
		using QHash<QString, int>::value;

		void insert(const QString &txt)
		{
			++(*this)[txt];
		}

		void insert(const Comment *com)
		{
			insert(com->string);
		}

		void insert(const Graphic *gra)
		{
			const Comment *com = gra->getSource();
			if (com) {
				insert(com);
			}
		}

		void remove(const QString &txt)
		{
			auto itr = find(txt);
			int &num = itr.value();
			if (--num <= 0) {
				erase(itr);
			}
		}

		void remove(const Comment *com)
		{
			remove(com->string);
		}

		void remove(const Graphic *gra)
		{
			const Comment *com = gra->getSource();
			if (com) {
				remove(com);
			}
		}

	};
}

class RunningPrivate
{
public:
	qint32 curr;
	qint64 time;
	QList<Graphic *> draw;
	int wait;
	TextCounter text;
	mutable QReadWriteLock lock;
	// 悬停冻结态：只在 GUI 线程读写（setHover 与 draw 都在 GUI 线程），
	// 池线程不碰；指向的 Graphic 可能被池线程删除，使用处一律做"仍在
	// d->draw 列表里"的成员资格判断，比较裸指针值本身不会解引用
	Graphic *frozen = nullptr;
	bool pinned = false;
};

Running::Running(QObject *parent)
	: QObject(parent), d_ptr(new RunningPrivate())
{
	setObjectName("Running");
}

Running::~Running()
{
	Q_D(Running);
	qThreadPool->clear();
	qThreadPool->waitForDone();
	qDeleteAll(d->draw);
	delete d_ptr;
}

void Running::setup()
{
	Q_D(Running);
	d->curr = d->time = 0;

	connect(lApp->findObject<APlayer>(), &APlayer::jumped, this, &Running::jumpTime);
	connect(lApp->findObject<APlayer>(), &APlayer::timeChanged, this, &Running::moveTime);
	connect(lApp->findObject<APlayer>(), &APlayer::reach, this, [this]() {
		clear();
	});

	connect(lApp->findObject<Danmaku>(), &Danmaku::layoutChanged, this, [this]() {
		Q_D(Running);
		qThreadPool->clear();
		qThreadPool->waitForDone();
		d->lock.lockForWrite();
		for (auto iter = d->draw.begin(); iter != d->draw.end();) {
			const Comment *cur = (*iter)->getSource();
			if (cur && cur->blocked) {
				if (*iter == d->frozen) {
					d->frozen = nullptr;
				}
				d->text.remove(cur);
				delete *iter;
				iter = d->draw.erase(iter);
			}
			else {
				++iter;
			}
		}
		d->lock.unlock();
		lApp->findObject<ARender>()->draw();
	});
	connect(lApp->findObject<Danmaku>(), &Danmaku::modelAboutToBeReset, this, [this]() {
		Q_D(Running);
		d->curr = 0;
		clear();
	});
	connect(lApp->findObject<Danmaku>(), &Danmaku::modelReset, this, [this]() {
		Q_D(Running);
		jumpTime(d->time);
		lApp->findObject<ARender>()->draw();
	});

	// 悬停冻结的过期定时器：光标停止投喂 400ms 后自动解冻，
	// 对"光标离开窗口/弹出对话框/任何断供"天然自愈
	hoverTimer = new QTimer(this);
	hoverTimer->setSingleShot(true);
	hoverTimer->setInterval(400);
	connect(hoverTimer, &QTimer::timeout, this, [this]() {
		Q_D(Running);
		if (d->pinned || d->frozen == nullptr) {
			return;
		}
		qWarning("hover: release (timeout)");
		d->frozen = nullptr;
		lApp->findObject<ARender>()->draw();
	});
}

void Running::clear(bool soft)
{
	Q_D(Running);
	qThreadPool->clear();
	qThreadPool->waitForDone();
	d->lock.lockForWrite();
	for (auto iter = d->draw.begin(); iter != d->draw.end();) {
		Graphic *g = *iter;
		if (soft && g->stay()) {
			++iter;
		}
		else {
			if (g == d->frozen) {
				d->frozen = nullptr;
			}
			d->text.remove(g);
			delete g;
			iter = d->draw.erase(iter);
		}
	}
	d->lock.unlock();
	d->time = 0;
	d->curr = 0;
	lApp->findObject<ARender>()->draw();
}

void Running::insert(Graphic *graphic, int index)
{
	Q_D(Running);
	d->lock.lockForWrite();
	graphic->setIndex();
	int size = d->draw.size(), next;
	if (size == 0 || index == 0) {
		next = 0;
	}
	else {
		int ring = size + 1;
		next = index > 0 ? (index%ring) : (ring + index%ring);
		if (next == 0) {
			next = size;
		}
	}
	d->text.insert(graphic);
	d->draw.insert(next, graphic);
	d->lock.unlock();
}

namespace
{
	class CreateMeta
	{
	public:
		QSizeF size;
		int mode;
		QList<QRectF> rect;

		explicit CreateMeta(Graphic *graphic)
			: size(graphic->currentRect().size())
			, mode(graphic->getMode())
			, rect(graphic->locate())
		{
		}
	};

	bool operator == (const CreateMeta &f, const CreateMeta &s)
	{
		return f.size == s.size && f.mode == s.mode && f.rect == s.rect;
	}

	uint qHash(const CreateMeta &l, uint s = 0)
	{
		return (((::qHash(l.mode, s) << 1) + ::qHash((int)l.size.width(), s)) << 1) + ::qHash((int)l.size.height(), s);
	}

	//Floor太慢了
	inline int lower(double n)
	{
		return (int)n - (n < (int)n);
	}

	//Ceil 太慢了
	inline int upper(double n)
	{
		return (int)n + (n > (int)n);
	}

	class Process : public QRunnable
	{
	public:
		static const int Priority = 2;

		Process(RunningPrivate *p, const QList<Graphic *> &w, const CreateMeta &m)
			: priv(p)
			, meta(m)
			, wait(w)
		{
		}

		virtual void run() override
		{
			if (wait.isEmpty()) {
				return;
			}

			//子线程优先级需要低于主线程
			QThread::currentThread()->setPriority(QThread::LowPriority);

			switch (meta.rect.size()) {
			case 1:
				//图元指定位置
				for (Graphic *iter : wait) {
					iter->currentRect() = meta.rect[0];
				}
			case 0:
				//弹幕自行定位
				priv->lock.lockForWrite();
				for (Graphic *iter : wait) {
					priv->wait--;
					priv->draw.append(iter);
				}
				priv->lock.unlock();
				break;
			default:
			{
				//弹幕自动定位
				QVarLengthArray<int> thick(meta.rect.size());
				std::fill(thick.begin(), thick.end(), 0);
				//计算每个位置的拥挤程度
				auto calculate = [&](Graphic *against) {
					Graphic *compare = wait.front() == against ? wait.last() : wait.front();
					Q_ASSERT(compare->getMode() == meta.mode && against->getMode() == meta.mode);
					const QRectF &rect = against->currentRect();
					const QRectF &from = meta.rect.at(0);
					double stp = meta.rect.at(1).top() - from.top();
					double off = stp > 0 ? (rect.top() - from.top()) : (rect.bottom() - from.bottom());
					double len = rect.height() + from.height();
					int sta = qMax(lower(off / stp), 0);
					int end = qMin(upper(len / qAbs(stp) + sta), thick.size());
					QRectF &iter = compare->currentRect(), back = iter;
					for (; sta < end; ++sta) {
						iter = meta.rect.at(sta);
						thick[sta] += compare->intersects(against);
					}
					iter = back;
				};
				//获取读锁，计算现有弹幕的拥挤程度
				priv->lock.lockForRead();
				for (auto iter : priv->draw) {
					if (iter->getMode() == meta.mode) {
						calculate(iter);
					}
				}
				quint64 last = priv->draw.isEmpty() ? 0 : priv->draw.last()->getIndex();
				priv->lock.unlock();
				//获取写锁，计算两次锁之间的新弹幕
				priv->lock.lockForWrite();
				QListIterator<Graphic *> back(priv->draw);
				back.toBack();
				while (back.hasPrevious()) {
					Graphic *iter = back.previous();
					if (iter->getIndex() > last && iter->getMode() == meta.mode) {
						calculate(iter);
					}
					else break;
				}
				//挑选最空闲的位置
				for (Graphic *iter : wait) {
					int thin = std::min_element(thick.begin(), thick.end()) - thick.begin();
					iter->currentRect() = meta.rect[thin];
					iter->setIndex();
					iter->setEnabled(true);
					priv->wait--;
					priv->draw.append(iter);
					calculate(iter);
				}
				priv->lock.unlock();
			}
			}
		}

	private:
		RunningPrivate * const priv;
		CreateMeta meta;
		QList<Graphic *> wait;
	};

	class Prepare : public QRunnable
	{
	public:
		static const int Priority = 0;

		Prepare(RunningPrivate *p, const QList<const Comment *> &w)
			: priv(p)
			, wait(w)
		{
			createTime = QDateTime::currentMSecsSinceEpoch();
		}

		virtual void run() override
		{
			//跳过500毫秒以上未处理的弹幕
			if (wait.isEmpty() || createTime < QDateTime::currentMSecsSinceEpoch() - 500) {
				QWriteLocker locker(&priv->lock);
				for (const Comment *iter : wait) {
					priv->wait--;
					priv->text.remove(iter);
				}
				return;
			}

			//子线程优先级需要低于主线程
			QThread::currentThread()->setPriority(QThread::LowPriority);

			QHash<CreateMeta, QList<Graphic *>> slot;
			for (const Comment *iter : wait) {
				try {
					Graphic *graphic = Graphic::create(*iter);
					slot[CreateMeta(graphic)].append(graphic);
				}
				catch (const Graphic::format_unrecognized &) {
					//高级弹幕类型不支持：历史上交给插件兜底，插件机制移除后直接跳过
				}
				catch (const Graphic::args_prasing_error &) {
				}
			}
			for (auto iter = slot.begin(); iter != slot.end(); ++iter) {
				qThreadPool->start(new Process(priv, iter.value(), iter.key()), Process::Priority);
			}
		}

	private:
		RunningPrivate * const priv;
		qint64 createTime;
		QList<const Comment *> wait;
	};
}

void Running::moveTime(qint64 time)
{
	Q_D(Running);
	// 播放位置突然大幅跳动（起播就跳到 --start-time、拖动进度条、VLC 内部 seek）时，
	// 直接重定位 + 清屏；否则跳过的整段里的弹幕会被一次性全吐到屏幕上。
	if (time > d->time + 2000 || (d->time > 0 && time + 2000 < d->time)) {
		jumpTime(time);
		return;
	}
	d->time = time;
	int screen = Config::getValue("/Shield/Amount/Screen", 0);
	int repeat = Config::getValue("/Shield/Amount/Repeat", 5);
	QMap<qint64, QList<const Comment *>> splited;
	auto danm = lApp->findObject<Danmaku>();
	//整批只加一次写锁：原来每条弹幕都 lockForWrite/unlock 一次，
	//起播或密集段一次要进上千条，加解锁本身就吃掉可观的时间。
	{
		QWriteLocker locker(&d->lock);
		for (; d->curr < danm->size() && danm->at(d->curr)->time < time; ++d->curr) {
			const Comment *c = danm->at(d->curr);
			if (c->blocked) {
				continue;
			}
			if (screen > 0 && d->wait + d->draw.size() >= screen) {
				continue;
			}
			if (repeat > 0 && d->text.value(c->string) >= repeat) {
				continue;
			}
			d->wait++;
			d->text.insert(c);
			splited[c->time].append(c);
		}
	}
	for (const QList<const Comment *> &iter : splited) {
		qThreadPool->start(new Prepare(d, iter), Prepare::Priority);
	}
}

namespace
{
	class CommentComparer
	{
	public:
		//overloads for comparing with time
		inline bool operator ()(const Comment *c, qint64 time)
		{
			return c->time < time;
		}

		inline bool operator ()(qint64 time, const Comment *c)
		{
			return time < c->time;
		}
	};
}

void Running::jumpTime(qint64 time)
{
	Q_D(Running);
	clear(true);
	auto danm = lApp->findObject<Danmaku>();
	d->time = time;
	d->curr = std::lower_bound(danm->begin(), danm->end(), time, CommentComparer()) - danm->begin();
}

void Running::draw(QPainter *painter, double move)
{
	Q_D(Running);
	QVarLengthArray<Graphic *> dirty;
	d->lock.lockForWrite();
	dirty.reserve(d->draw.size());
	for (auto iter = d->draw.begin(); iter != d->draw.end();) {
		Graphic *g = *iter;
		if (g->move(g == d->frozen ? 0.0 : move)) {
			dirty.append(g);
			++iter;
		}
		else {
			d->text.remove(g);
			delete g;
			iter = d->draw.erase(iter);
		}
	}
	d->lock.unlock();
	for (Graphic *g : dirty) {
		g->draw(painter);
	}
}

const Comment *Running::commentAt(QPointF point) const
{
	Q_D(const Running);
	d->lock.lockForRead();
	for (Graphic *g : d->draw) {
		if (g->currentRect().contains(point)) {
			d->lock.unlock();
			return g->getSource();
		}
	}
	d->lock.unlock();
	return nullptr;
}

int Running::size() const
{
	Q_D(const Running);
	return d->draw.size();
}

void Running::setHover(QPointF point, bool active)
{
	Q_D(Running);
	Graphic *hit = nullptr;
	if (active) {
		d->lock.lockForRead();
		for (Graphic *g : d->draw) {
			if (g->currentRect().contains(point)) {
				hit = g;
				break;
			}
		}
		d->lock.unlock();
	}
	if (hit == d->frozen) {
		// 同一条：只续期，不刷日志
		if (hit != nullptr && !d->pinned) {
			hoverTimer->start();
		}
		return;
	}
	d->frozen = hit;
	if (hit != nullptr) {
		qWarning("hover: freeze \"%s\"", qPrintable(hit->getSource()->string));
		if (!d->pinned) {
			hoverTimer->start();
		}
	}
	else {
		qWarning("hover: release (left)");
	}
	lApp->findObject<ARender>()->draw();
}

void Running::pinHover(bool pin)
{
	Q_D(Running);
	d->pinned = pin;
	if (!pin && d->frozen != nullptr) {
		// 菜单关闭后仍给一个过期窗口，光标动了会自然续期
		hoverTimer->start();
	}
}
