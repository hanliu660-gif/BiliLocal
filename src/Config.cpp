/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    Config.cpp
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

#include "Common.h"
#include "Config.h"
#include "Local.h"
#include "Utils.h"

QReadWriteLock Config::lock;

namespace
{
	// save() 内部会 emit aboutToSave()，而 List 等模块会在槽里继续 setValue()，
	// 这里挡住重入，否则「保存 -> 又变脏 -> 再保存」会无限循环。
	bool saving = false;
}

Config::Config(QObject *parent)
	: QObject(parent)
{
}

QJsonObject Config::config;

void Config::load()
{
	QString path = Utils::localPath(Utils::Config) + "Config.txt";
	QFile conf(path);
	if (conf.open(QIODevice::ReadOnly | QIODevice::Text)){
		const QByteArray data = conf.readAll();
		conf.close();
		const QJsonDocument doc = QJsonDocument::fromJson(data);
		// 手改坏的 Config.txt（漏逗号、截断）以前会被静默读成空对象，退出时
		// 再被 save() 用出厂值整体覆盖，原文件就没了。现在先留一份带时间戳的
		// 备份，再按空配置启动 —— 用户还能把它捞回来。
		if (!doc.isObject() && !data.trimmed().isEmpty()){
			const QString backup = path + QString(".broken-%1")
				.arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
			// 日志一律走英文：message handler 用 qPrintable 写盘，
			// 中文在非 UTF-8 控制台/日志里会变乱码
			if (QFile::copy(path, backup)){
				qWarning("Config: parse failed, original kept at %s", qPrintable(backup));
			}
			else{
				qWarning("Config: parse failed and backup also failed");
			}
		}
		config = doc.object();
	}
	else{
		// 老版本把 Config.txt 写在 exe 旁边，搬到用户目录后顺手迁移一次，
		// 免得升级之后所有设置看起来都丢了。
		QFile legacy(qApp->applicationDirPath() + "/Config.txt");
		if (legacy.exists() && legacy.open(QIODevice::ReadOnly | QIODevice::Text)){
			QByteArray data = legacy.readAll();
			legacy.close();
			config = QJsonDocument::fromJson(data).object();
			QDir().mkpath(QFileInfo(path).absolutePath());
			QFile file(path);
			if (file.open(QIODevice::WriteOnly | QIODevice::Text)){
				file.write(data);
				file.close();
			}
		}
	}
	migrate();
}

void Config::save()
{
	if (saving){
		return;
	}
	saving = true;
	if (lApp){
		if (Config *obj = lApp->findObject<Config>()){
			emit obj->aboutToSave();
		}
	}
	QString path = Utils::localPath(Utils::Config) + "Config.txt";
	QDir().mkpath(QFileInfo(path).absolutePath());
	QSaveFile conf(path);
	if (!conf.open(QIODevice::WriteOnly | QIODevice::Text)){
		qWarning("Config: %s", qPrintable(conf.errorString()));
		saving = false;
		return;
	}
	// 锁内只取一份快照，避免和工作线程的 setValue 竞争读到半更新状态
	QJsonObject snapshot;
	{
		QReadLocker locker(&lock);
		snapshot = config;
	}
	conf.write(QJsonDocument(snapshot).toJson());
	if (!conf.commit()){
		qWarning("Config: %s", qPrintable(conf.errorString()));
	}
	saving = false;
}

void Config::touch()
{
	if (saving || !lApp){
		return;
	}
	if (Config *obj = lApp->findObject<Config>()){
		// 跨线程调用 setValue 时也能安全地回到对象所属线程再起定时器
		QMetaObject::invokeMethod(obj, "autoSave", Qt::QueuedConnection);
	}
}

void Config::autoSave()
{
	if (!autoSaveTimer){
		autoSaveTimer = new QTimer(this);
		autoSaveTimer->setSingleShot(true);
		autoSaveTimer->setInterval(1000);
		connect(autoSaveTimer, &QTimer::timeout, this, [](){ Config::save(); });
	}
	autoSaveTimer->start();
}

void Config::migrate()
{
	int version = config.value("Meta").toObject().value("ConfigVersion").toInt();
	if (version >= 2){
		return;
	}
	if (version < 1){
		// 只改“用户从没动过”的键，自己调过参数的一律不碰
		QJsonObject danmaku = config.value("Danmaku").toObject();
		if (danmaku.value("Speed").toString() == "125+%{width}/5"){
			setValue<QString>("/Danmaku/Speed", "150");
		}
		if (danmaku.value("Alpha").toInt() == 100){
			setValue("/Danmaku/Alpha", 80);
		}
	}
	// v2: 侧边栏宽度 144pt ≈ 192px，150% 缩放下内容被挤成一团，加宽
	QJsonObject popup = config.value("Interface").toObject().value("Popup").toObject();
	if (popup.value("Width").toInt() < 220){
		setValue("/Interface/Popup/Width", 240);
	}
	setValue("/Meta/ConfigVersion", 2);
}

void     Config::setVariant(QString key, QVariant val)
{
	setValue(key, val);
}

QVariant Config::getVariant(QString key, QVariant val)
{
	return getValue(key, val);
}
