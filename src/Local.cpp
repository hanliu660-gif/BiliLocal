/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    Local.cpp
*   Time:        2013/03/18
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
#include "Local.h"
#ifdef Q_OS_WIN
#	include <windows.h>
#	include <shellapi.h>
#endif
#include "Config.h"
#include "Utils.h"
#include <QtNetwork>	// QLocalSocket/QLocalServer：单实例机制（Qt5 里属 QtNetwork）
#include "Access/Load.h"
#include "Model/Danmaku.h"
#include "Model/Running.h"
#include "Model/List.h"
#include "Model/Shield.h"
#include "Player/APlayer.h"
#include "Render/ARender.h"
#include "UI/Interface.h"
#include <type_traits>

Local *Local::ins = nullptr;

Local *Local::instance()
{
	return ins ? ins : new Local(qApp);
}

Local::Local(QObject *parent)
	: QObject(parent)
{
	ins = this;
	setObjectName("Local");
#define lIns(ModuleType) (this->objects[#ModuleType] = new ModuleType(this))
	lIns(Config);
	lIns(Shield);
	lIns(Danmaku);
	lIns(Running);
	lIns(Interface);
	objects["APlayer"] = APlayer::create(this);
	objects["ARender"] = ARender::create(this);
	lIns(Load);
	lIns(List);
#define lSet(ModuleType) static_cast<ModuleType *>(this->objects[#ModuleType])->setup()
	lSet(Interface);
	lSet(ARender);
	lSet(Running);
#undef lIns
#undef lSet
	connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
		delete this;
	});
}

Local::~Local()
{
	Config::save();
	if (ins == this){
		ins = nullptr;
	}
	delete findObject<APlayer>();
	delete findObject<Running>();
	delete findObject<ARender>();
}

void Local::tryLocal(QString path)
{
	QFileInfo info(path);
	QString suffix = info.suffix().toLower();
	if (info.exists() == false) {
		return;
	}
	else if (Utils::getSuffix(Utils::Danmaku).contains(suffix)) {
		findObject<Load>()->loadDanmaku(path);
	}
	else if (findObject<APlayer>()->getState() != APlayer::Stop
		&& Utils::getSuffix(Utils::Subtitle).contains(suffix)) {
		findObject<APlayer>()->addSubtitle(path);
	}
	else {
		switch (Config::getValue("/Interface/Single", 1)) {
		case 2:
			findObject<List>()->appendMedia(path);
			break;
		default:
			findObject<APlayer>()->stop();
			findObject<APlayer>()->setMedia(path);
			break;
		}
	}
}

namespace
{
	void setDefaultFont()
	{
		QString def = Utils::defaultFont();
		QFontInfo i(qApp->font());
		if (!Utils::fontFamilies().contains(def)){
			def = i.family();
		}
		double p = i.pointSizeF();
		QFont f;
		f.setFamily(Config::getValue("/Interface/Font/Family", def));
		f.setPointSizeF(Config::getValue("/Interface/Font/Size", p));
		qApp->setFont(f);
	}

	void loadTranslator()
	{
		QString path = Utils::localPath(Utils::Locale);
		QString name = Config::getValue("/Interface/Locale", QLocale::system().name());
		QFileInfoList list;
		list += QDir(path + name).entryInfoList();
		list += QFileInfo(path + name + ".qm");
		name.resize(2);
		list += QDir(path + name).entryInfoList();
		list += QFileInfo(path + name + ".qm");
		for (QFileInfo info : list){
			if (!info.isFile()){
				continue;
			}
			QTranslator *trans = new QTranslator(qApp);
			if (trans->load(info.absoluteFilePath())){
				qApp->installTranslator(trans);
			}
			else{
				delete trans;
			}
		}
	}
}

namespace
{
	// Windows 下 QCoreApplication::arguments() 会把 UTF-8 的 argv 当成本地编码（GBK）解，
	// 中文/全角文件名的路径会变成乱码（实测「！.flv」→「???flv」）。直接从宽字符命令行取。
	QStringList commandLineArguments()
	{
#ifdef Q_OS_WIN
		int count = 0;
		LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &count);
		QStringList res;
		for (int i = 1; i < count; ++i) {
			res.append(QString::fromWCharArray(argv[i]));
		}
		if (argv) {
			LocalFree(argv);
		}
		return res;
#else
		return QCoreApplication::arguments().mid(1);
#endif
	}
}

int main(int argc, char *argv[])
{
	QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	// Qt5 默认把缩放按整数倍取整：150% 的系统缩放会被抬到 200%，整个界面大 1/3；
	// 改成跟随系统的小数缩放，和资源管理器/浏览器一致
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
	// 位图图标（/Picture 下的 png、窗口图标）在高 DPI 下按 @2x 取图，避免被拉伸发糊
	QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
	// 老显卡的 ANGLE（D3D9→GLES）兜底：必须在 QGuiApplication 构造之前设，
	// 否则是空操作——旧代码把 a.setAttribute(AA_UseOpenGLES) 写在构造之后，从没生效过。
	// 默认不开：弹幕层是透明 QOpenGLWindow 叠加，ANGLE 下没实测过；需要时临时试
	// BILILOCAL_GL=angle 启动。
	if (qEnvironmentVariable("BILILOCAL_GL") == QLatin1String("angle")) {
		QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
	}
	std::remove_pointer<decltype(qApp)>::type a(argc, argv);	// 配置/缓存的位置依赖这两个名字，必须在 Config::load() 之前定好
	QCoreApplication::setOrganizationName("BiliLocal");
	QCoreApplication::setApplicationName("BiliLocal");
	qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &msg) {
		const QString path = Utils::localPath(Utils::Cache) + "msg.log";
		// 无轮转的日志会一直变大：超过 10MB 时把旧的挪成 .old（覆盖上一份）
		QFile f(path);
		if (f.size() > 10 * 1024 * 1024) {
			QFile::remove(path + ".old");
			QFile::rename(path, path + ".old");
		}
		if (f.open(QIODevice::WriteOnly | QIODevice::Append)) {
			f.write((msg + "\r\n").toUtf8());
		}
	});
	Config::load();
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
	int single = Config::getValue("/Interface/Single", 1);
	if (single){
		QLocalSocket socket;
		socket.connectToServer("BiliLocalInstance");
		if (socket.waitForConnected()){
			QDataStream s(&socket);
			s << commandLineArguments();
			socket.waitForBytesWritten();
			return 0;
		}
	}
#endif
	qThreadPool->setMaxThreadCount(Config::getValue("/Danmaku/Thread", QThread::idealThreadCount()));
	loadTranslator();
	setDefaultFont();
	new Local(&a);
	lApp->findObject<Interface>()->show();
	const QStringList &args = commandLineArguments();
	for (const QString iter : args) {
		lApp->tryLocal(iter);
	}
	// 无参数启动 + 开启自动续播：直接接着最近一条没看完的播，免去每次找目录
	if (args.isEmpty() && Config::getValue("/Player/AutoResume", true)){
		QString target = lApp->findObject<List>()->resumeCandidate();
		if (!target.isEmpty()){
			lApp->tryLocal(target);
		}
	}
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
	QLocalServer *server = nullptr;
	if (single){
		server = new QLocalServer(&a);
		// 程序崩溃后残留的 socket 文件会让 listen 静默失败，单实例从此失效；
		// 失败时清掉残留再试一次
		if (!server->listen("BiliLocalInstance")){
			QLocalServer::removeServer("BiliLocalInstance");
			server->listen("BiliLocalInstance");
		}
		QObject::connect(server, &QLocalServer::newConnection, [=](){
			QLocalSocket *r = server->nextPendingConnection();
			// 显式短超时：对端异常时不让主线程白等默认的 30 秒
			// 只等一次可能只收到一半：多文件/长路径转发会被截断，反序列化静默得到空列表
			while (r->waitForReadyRead(500)){
			}
			QDataStream s(r);
			QStringList args;
			s >> args;
			delete r;
			for (const QString iter : args) {
				lApp->tryLocal(iter);
			}
		});
	}
	int r = a.exec();
	if (r == 12450){
		if (server){
			delete server;
		}
		QProcess::startDetached(a.applicationFilePath(), QStringList());
		return 0;
	}
	else{
		return r;
	}
#else
	return a.exec();
#endif
}
