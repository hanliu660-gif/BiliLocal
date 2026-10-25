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
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
=========================================================================*/

#include "Common.h"
#include "Load.h"
#include "Parse.h"
#include "../Config.h"
#include "../Local.h"
#include "../Utils.h"
#include "../Model/Danmaku.h"
#include "../Player/APlayer.h"

Load::Load(QObject *parent)
	: QObject(parent)
{
	setObjectName("Load");
}

Load::Task Load::codeToTask(QString code)
{
	Task task;
	task.code = code;
	APlayer *aplayer = lApp->findObject<APlayer>();
	task.delay = aplayer->getState() != APlayer::Stop&&Config::getValue("/Load/Delay", false) ? aplayer->getTime() : 0;
	return task;
}

void Load::loadDanmaku(QString code)
{
	Task task = codeToTask(code);
	if (enqueue(task) && Config::getValue("/Load/Clear", false)){
		lApp->findObject<Danmaku>()->clear();
	}
}

bool Load::enqueue(const Load::Task &task)
{
	queue.enqueue(task);
	if (!processing){
		processing = true;
		// 让步事件循环：当前调用方（如 mediaChanged 处理器）先走完
		QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
	}
	return true;
}

void Load::processNext()
{
	if (queue.isEmpty()){
		processing = false;
		return;
	}
	const Task task = queue.dequeue();

	QFile file(task.code);
	if (!file.open(QIODevice::ReadOnly)){
		qWarning("Load: cannot open %s", qPrintable(task.code));
		emit errorOccured(1);
		QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
		return;
	}
	QByteArray data = file.readAll();
	file.close();
	// 录播姬等工具写出的 xml 带 UTF-8 BOM，不剥掉的话下面的 startsWith("<?xml") 会失配，
	// 整个文件会被当成 AcFun 的 JSON 去解析，结果是 0 条弹幕。
	if (data.startsWith("\xEF\xBB\xBF")) {
		data.remove(0, 3);
	}

	Parse::ResultDelegate result;
	// 前面有空行/空白的 xml 以前会被当成 AcFun JSON（结果是 0 条弹幕），
	// 所以先 trim 再判类型；判不出来的仍走 AcFun 兜底，与原来的行为一致。
	const QByteArray head = data.left(512).trimmed();
	if (head.startsWith("[Script Info]")){
		result = Parse::parseComment(data, Utils::ASS);
	}
	else if (head.startsWith('<')){
		if (head.indexOf("<packet>") != -1){
			result = Parse::parseComment(data, Utils::Niconico);
		}
		else if (head.indexOf("<i>") != -1){
			result = Parse::parseComment(data, Utils::Bilibili);
		}
		else if (head.indexOf("<c>") != -1){
			result = Parse::parseComment(data, Utils::AcfunLocalizer);
		}
	}
	else {
		result = Parse::parseComment(data, Utils::AcFun);
	}

	Record load;
	load.source = task.code;
	load.access = task.code;
	load.string = QFileInfo(task.code).fileName();
	result.onFinish([&load, &task](QVector<Comment> &&r) mutable {
		load.danmaku.swap(r);
		if (task.delay != 0) {
			load.delay = task.delay;
			for (Comment &iter : load.danmaku) {
				iter.time += load.delay;
			}
		}
		lApp->findObject<Danmaku>()->append(std::move(load));
	});
	qWarning("Load: processed %s, pool=%d", qPrintable(task.code), lApp->findObject<Danmaku>()->getPool().size());
	QMetaObject::invokeMethod(this, "processNext", Qt::QueuedConnection);
}
