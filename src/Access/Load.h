/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    Load.h
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

#pragma once

#include <QtCore>

class Comment;
class Record;

/*	弹幕装载器（2026-10-25 收敛为纯本地文件加载）。
	原版继承在线网络队列（QNetworkAccessManager + Proc 插槽机制），
	联网功能移除后全部不可达；插件注册入口（addProc）随之消亡。
	现在只有一个语义：队列里的每个 code 都是本地文件路径，
	读入 → 嗅探格式 → 同步解析 → 挂进 Danmaku 池。
	文件间让步事件循环（processNext 走 QueuedConnection），
	大批量加载时栈不递增、界面不冻结。 */
class Load : public QObject
{
	Q_OBJECT
public:
	struct Task
	{
		QString code;
		qint64 delay;
		Task() : delay(0){}
	};

	Task codeToTask(QString code);

	explicit Load(QObject *parent = nullptr);

signals:
	void errorOccured(int state);

public slots:
	void loadDanmaku(QString code);
	bool enqueue(const Load::Task &task);
	// 是否还有未开工的文件。在途文件不可见（处理全程同步，期间排队信号
	// 不会插进来），语义与旧版 getHead() != nullptr 一致：最后一个文件
	// append 完即视为空闲，Home 的 modelAppend 检查据此弹 Editor
	bool busy() const { return !queue.isEmpty(); }

private slots:
	// 必须是槽：文件间让步用 invokeMethod(QueuedConnection) 按名投递
	void processNext();

private:
	QQueue<Task> queue;
	bool processing = false;	// 已有 processNext 在队列里待跑，防重复投递
};
