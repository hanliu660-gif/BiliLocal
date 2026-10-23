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

*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
=========================================================================*/

#pragma once

#include <QtCore>
#include <QtNetwork>
#include <functional>

class Comment;
class Record;
class LoadPrivate;

class Load : public QObject
{
	Q_OBJECT
public:
	enum State
	{
		None = 0,
		File = 384,
	};

	struct Proc
	{
		std::function<bool(QString &)> regular;
		int priority;
		std::function<void(QNetworkReply *)> process;
	};

	struct Task
	{
		QString code;
		QNetworkRequest request;
		int state;
		const Proc *processer;
		qint64 delay;
		Task() :state(None), processer(nullptr), delay(0){}
	};

	Task codeToTask(QString code);

	explicit Load(QObject *parent = nullptr);
	virtual ~Load();

private:
	LoadPrivate *const d_ptr;
	Q_DECLARE_PRIVATE(Load);

signals:
	void stateChanged(int state);
	void errorOccured(int state);
	void progressChanged(double progress);

public slots:
	void addProc(const Load::Proc *proc);
	const Load::Proc *getProc(QString code);

	void loadDanmaku(QString);

	void forward();
	void dequeue();
	bool enqueue(const Load::Task &);
	Load::Task *getHead();
};
