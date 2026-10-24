/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    Config.h
*   Time:        2013/06/17
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
#include <exception>

namespace{
	template<class TypeName>
	TypeName fromJsonValue(QJsonValue v)
	{
		QVariant t = v.toVariant();
		if (!t.canConvert<TypeName>()){
			throw std::runtime_error("type missmatch");
		}
		return t.value<TypeName>();
	}

	template<>
	QVariant fromJsonValue(QJsonValue v)
	{
		return v.toVariant();
	}

	// 模板特化并非每个编译单元都会实例化，[[maybe_unused]] 压掉 MinGW 的
	// "defined but not used" 告警（每 TU 刷屏的那四条）
	template<>
	[[maybe_unused]]
	QJsonArray fromJsonValue(QJsonValue v)
	{
		if (QJsonValue::Array != v.type()){
			throw std::runtime_error("type missmatch");
		}
		return v.toArray();
	}

	template<>
	[[maybe_unused]]
	QJsonObject fromJsonValue(QJsonValue v)
	{
		if (QJsonValue::Object != v.type()){
			throw std::runtime_error("type missmatch");
		}
		return v.toObject();
	}

	template<class TypeName>
	QJsonValue toJsonValue(TypeName v)
	{
		return QJsonValue(v);
	}

	template<>
	[[maybe_unused]]
	QJsonValue toJsonValue(QVariant v)
	{
		return QJsonValue::fromVariant(v);
	}
}

class Config :public QObject
{
	Q_OBJECT
public:
	explicit Config(QObject *parent = 0);

	// 标记“配置有改动”，稍后统一落盘（1s 去抖）
	static void touch();
	// 老配置的一次性升级，只动仍停留在出厂值的那几个键
	static void migrate();

	template<class T>
	static T getValue(QString key, T def = T())
	{
		QStringList tree = key.split('/', Qt::SkipEmptyParts);
		QString last = tree.takeLast();
		lock.lockForRead();
		QJsonObject cur = config;
		lock.unlock();
		QList<QJsonObject> path;
		for (const QString &k : tree){
			path.append(cur);
			cur = cur.value(k).toObject();
		}
		if (cur.contains(last)){
			try{
				return fromJsonValue<T>(cur.value(last));
			}
			catch (...){}
		}
		QJsonValue val = toJsonValue(def);
		if (!val.isNull()){
			cur[last] = val;
			while (!path.isEmpty()){
				QJsonObject pre = path.takeLast();
				pre[tree.takeLast()] = cur;
				cur = pre;
			}
			lock.lockForWrite();
			config = cur;
			lock.unlock();
		}
		return def;
	}

	template<class T>
	static void setValue(QString key, T set)
	{
		QStringList tree = key.split('/', Qt::SkipEmptyParts);
		QString last = tree.takeLast();
		QJsonObject cur = config;
		QList<QJsonObject> path;
		for (const QString &k : tree){
			path.append(cur);
			cur = cur.value(k).toObject();
		}
		cur[last] = toJsonValue(set);
		while (!path.isEmpty()){
			QJsonObject pre = path.takeLast();
			pre[tree.takeLast()] = cur;
			cur = pre;
		}
		lock.lockForWrite();
		config = cur;
		lock.unlock();
		touch();
	}

private:
	static QJsonObject config;
	static QReadWriteLock lock;
	QTimer *autoSaveTimer = 0;

signals:
	void aboutToSave();

public slots:
	static void load();
	static void save();
	void     setVariant(QString key, QVariant val);
	QVariant getVariant(QString key, QVariant val = QVariant());

private slots:
	void     autoSave();
};
