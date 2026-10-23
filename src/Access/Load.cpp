/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    Load.cpp
*   Time:        2014/04/22
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
#include "Load.h"
#include "AccessPrivate.h"
#include "Parse.h"
#include "../Config.h"
#include "../Local.h"
#include "../Utils.h"
#include "../Model/Danmaku.h"
#include "../Player/APlayer.h"
#include <algorithm>

class LoadPrivate : public AccessPrivate<Load, Load::Proc, Load::Task>
{
public:

	explicit LoadPrivate(Load *load) :
		AccessPrivate<Load, Load::Proc, Load::Task>(load)
	{
		Q_Q(Load);
	}
};

namespace
{
	std::function<bool(QString &)> getRegular(QRegularExpression ex)
	{
		return [ex](QString &code){
			auto iter = ex.globalMatch(code);
			code.clear();
			while (iter.hasNext()){
				code = iter.next().captured();
			}
			return code.length() > 2;
		};
	}
}

Load::Load(QObject *parent)
	: QObject(parent), d_ptr(new LoadPrivate(this))
{
	Q_D(Load);
	setObjectName("Load");

	d->pool.append(Proc());
	Proc *directProc = &d->pool.last();
	directProc->process = [this](QNetworkReply *reply){
		Q_D(Load);
		Task &task = d->queue.head();
		switch (task.state){
		case None:
		{
			QUrl url = QUrl::fromUserInput(task.code);
			task.request.setUrl(url);
			task.state = File;
			forward();
			break;
		}
		case File:
		{
			const QUrl &url = reply->url();

			Record load;
			load.source = url.url();
			load.access = url.isLocalFile() ? url.toLocalFile() : load.source;
			load.string = QFileInfo(task.code).fileName();

			Parse::ResultDelegate result;
			QByteArray data(reply->readAll());
			// 录播姬等工具写出的 xml 带 UTF-8 BOM，不剥掉的话下面的 startsWith("<?xml") 会失配，
			// 整个文件会被当成 AcFun 的 JSON 去解析，结果是 0 条弹幕。
			if (data.startsWith("\xEF\xBB\xBF")) {
				data.remove(0, 3);
			}
			QByteArray head = data.left(512);
			if (head.startsWith("[Script Info]")){
				result = Parse::parseComment(data, Utils::ASS);
			}
			else if (!head.startsWith("<?xml")){
				result = Parse::parseComment(data, Utils::AcFun);
			}
			else if (head.indexOf("<packet>") != -1){
				result = Parse::parseComment(data, Utils::Niconico);
			}
			else if (head.indexOf("<i>") != -1){
				result = Parse::parseComment(data, Utils::Bilibili);
			}
			else if (head.indexOf("<c>") != -1){
				result = Parse::parseComment(data, Utils::AcfunLocalizer);
			}
			result.onFinish([this, load, &task](QVector<Comment> &&r) mutable {
				load.danmaku.swap(r);
				if (task.delay != 0) {
					load.delay = task.delay;
					for (Comment &iter : load.danmaku) {
						iter.time += load.delay;
					}
				}
				lApp->findObject<Danmaku>()->append(std::move(load));
				emit stateChanged(task.state = None);
				dequeue();
			});
			break;
		}
		}
	};
	directProc->priority = -100;
	directProc->regular = [](QString &code){
		QUrl u = QUrl::fromUserInput(code);
		if (u.isLocalFile() && QFileInfo(u.toLocalFile()).exists()){
			return true;
		}
		code.clear();
		return false;
	};

	connect(this, &Load::stateChanged, [this](int code){
		switch (code){
		case None:
		case File:
			break;
		default:
		{
			Q_D(Load);
			if (!d->tryNext()){
				emit errorOccured(code);
			}
			break;
		}
		}
	});
}

Load::~Load()
{
	delete d_ptr;
}

Load::Task Load::codeToTask(QString code)
{
	Task task;
	task.processer = getProc(code);
	task.code = code;
	APlayer *aplayer = lApp->findObject<APlayer>();
	task.delay = aplayer->getState() != APlayer::Stop&&Config::getValue("/Load/Delay", false) ? aplayer->getTime() : 0;
	return task;
}


void Load::addProc(const Load::Proc *proc)
{
	Q_D(Load);
	d->addProc(proc);
}

const Load::Proc *Load::getProc(QString code)
{
	Q_D(Load);
	return d->getProc(code);
}






void Load::loadDanmaku(QString code)
{
	const Task &task = codeToTask(code);
	if (enqueue(task) && Config::getValue("/Load/Clear", false)){
		lApp->findObject<Danmaku>()->clear();
	}
}






void Load::forward()
{
	Q_D(Load);
	d->forward();
}



void Load::dequeue()
{
	Q_D(Load);
	d->dequeue();
}

bool Load::enqueue(const Load::Task &task)
{
	Q_D(Load);
	return d->enqueue(task);
}

Load::Task *Load::getHead()
{
	Q_D(Load);
	return d->getHead();
}
