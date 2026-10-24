/*=======================================================================
*
*   Filename:    tst_config.cpp
*   Time:        2026/10/25
*
*   最小单元测试：配置读写（Config）。
*   对应审计报告 A3（setValue 读-改-写非原子）与 A4（Config.txt 改坏后被
*   静默读成空对象、退出时再被覆盖）。
*
*   只链 Config.cpp：lApp（Local 服务定位器）与 Utils::localPath 在这里用
*   最小桩替代，所以不需要 Local/APlayer/Load/Danmaku 那一整套。
*
*   本文件同样受 GPL-3.0 约束（随主工程分发）。
*
=========================================================================*/

#include <QtTest>
#include <thread>
#include "../src/Config.h"
#include "../src/Utils.h"
#include "../src/Local.h"

// ---- 桩：把配置目录指到临时目录，且不提供任何 Local 服务 ----------------
static QString gRoot;

namespace Utils
{
	QString localPath(Path)
	{
		return gRoot + QLatin1Char('/');
	}
}

Local *Local::instance()
{
	return nullptr;
}

// ---- 测试 ----------------------------------------------------------------
class tst_Config : public QObject
{
	Q_OBJECT
private slots:
	void initTestCase()
	{
		gRoot = QDir::tempPath() + "/bili-tst-config-" +
			QString::number(QCoreApplication::applicationPid());
		QDir().mkpath(gRoot);
	}

	void init()
	{
		// 每个用例都从空配置开始（Config 是静态的，会跨用例残留）
		QFile f(gRoot + "/Config.txt");
		QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
		f.write("{}");
		f.close();
		Config::load();
	}

	// 基本读写与嵌套路径
	void roundTrip()
	{
		Config::setValue("/Test/A", 42);
		QCOMPARE(Config::getValue("/Test/A", -1), 42);

		Config::setValue("/Test/Nest/B", QStringLiteral("x"));
		QCOMPARE(Config::getValue("/Test/Nest/B", QString()), QStringLiteral("x"));
		QCOMPARE(Config::getValue("/Test/Nest/B", QString()) == QStringLiteral("x"), true);
	}

	// A3 的一半：缺键时写回出厂值，不能把别的键一起冲掉
	void missingKeyKeepsOtherKeys()
	{
		Config::setValue("/Test/Marker", 1);
		QCOMPARE(Config::getValue("/Test/Missing", 7), 7);
		QCOMPARE(Config::getValue("/Test/Missing", 7), 7);
		QCOMPARE(Config::getValue("/Test/Marker", -1), 1);
	}

	// A3 主体：两个线程同时改不同键，谁的写入都不能丢
	void concurrentWriters()
	{
		Config::setValue("/Test/Keep", QStringLiteral("must-survive"));
		const int rounds = 500;
		std::thread a([&]() {
			for (int i = 1; i <= rounds; ++i){
				Config::setValue("/Test/A", i);
			}
		});
		std::thread b([&]() {
			for (int i = 1; i <= rounds; ++i){
				Config::setValue("/Test/B", i);
			}
		});
		a.join();
		b.join();
		QCOMPARE(Config::getValue("/Test/A", -1), rounds);
		QCOMPARE(Config::getValue("/Test/B", -1), rounds);
		QCOMPARE(Config::getValue("/Test/Keep", QString()), QStringLiteral("must-survive"));
	}

	// A4：坏掉的 Config.txt 要先备份，再按空配置启动
	void brokenFileIsBackedUp()
	{
		const QByteArray broken = "{\"Test\": {\"A\": 1,}}";
		QFile f(gRoot + "/Config.txt");
		QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
		f.write(broken);
		f.close();

		Config::load();

		// 备份存在且内容与会坏掉的原文件一致
		QDir dir(gRoot);
		const QStringList backups = dir.entryList(QStringList() << "Config.txt.broken-*", QDir::Files);
		QCOMPARE(backups.size(), 1);
		QFile b(gRoot + "/" + backups.first());
		QVERIFY(b.open(QIODevice::ReadOnly));
		QCOMPARE(b.readAll(), broken);
		// 配置本身退回空对象，取值拿默认值
		QCOMPARE(Config::getValue("/Test/A", -1), -1);
	}

	// 落盘的结果必须是可解析的 JSON 对象
	void saveWritesValidJson()
	{
		Config::setValue("/Test/Save", 9);
		Config::save();
		QFile f(gRoot + "/Config.txt");
		QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
		const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
		f.close();
		QVERIFY(doc.isObject());
		QCOMPARE(doc.object().value("Test").toObject().value("Save").toInt(), 9);
	}

	void cleanupTestCase()
	{
		QDir(gRoot).removeRecursively();
	}
};

QTEST_GUILESS_MAIN(tst_Config)
#include "tst_config.moc"
