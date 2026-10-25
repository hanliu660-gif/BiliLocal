/*=======================================================================
*
*   Filename:    tst_shield.cpp
*   Time:        2026/10/25
*
*   最小单元测试：弹幕屏蔽规则（Shield）。
*   对应历史 bug：空正则=匹配一切全屏屏蔽、非法正则静默失效（2f8a1d1），
*   t= 残留规则删不掉（1b10dc0），以及规则经 aboutToSave 钩子落盘的往返。
*   真链 Config.cpp 与 Shield.cpp；Local 与 Utils::localPath 用最小桩替代，
*   配置目录指到临时目录，不碰真实 %LOCALAPPDATA%。
*
*   注意：Shield 构造时连接 Config::aboutToSave 未带上下文对象，连接挂在
*   Config 上、不随 Shield 析构解除（产品中两者同生命周期，无此问题）；
*   测试重建 Shield 前必须手动 disconnect，否则旧 lambda 持悬空 d_ptr。
*
*   本文件同样受 GPL-3.0 约束（随主工程分发）。
*
=========================================================================*/

#include <QtTest>
#include "../src/Config.h"
#include "../src/Local.h"
#include "../src/Utils.h"
#include "../src/Model/Shield.h"
#include "../src/Model/Comment.h"

// ---- 桩：Local 给一个可注册服务的最小实现，配置目录指到临时目录 -----------
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
	// 故意泄漏：避免静态析构顺序与 QCoreApplication 纠缠
	static Local *ins = new Local;
	return ins;
}

Local::Local(QObject *parent)
	: QObject(parent)
{
}

Local::~Local()
{
}

void Local::tryLocal(QString)
{
}

// ---- 测试 ----------------------------------------------------------------
class tst_Shield : public QObject
{
	Q_OBJECT
	Config *config = nullptr;
	Shield *shield = nullptr;

	void rebuild(const QByteArray &json)
	{
		QFile f(gRoot + "/Config.txt");
		QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
		f.write(json);
		f.close();
		Config::load();
		// 见文件头注：Shield 的 aboutToSave 连接不带上下文，须手动断开
		if (shield){
			QObject::disconnect(config, &Config::aboutToSave, nullptr, nullptr);
			delete shield;
		}
		shield = new Shield(Local::instance());
	}

private slots:
	void initTestCase()
	{
		gRoot = QDir::tempPath() + "/bili-tst-shield-" +
			QString::number(QCoreApplication::applicationPid());
		QDir().mkpath(gRoot);
		config = new Config(Local::instance());
		Local::instance()->objects.insert("Config", config);
	}

	// 2f8a1d1：配置里的空串与非法正则要跳过——
	// 空正则匹配一切（所有弹幕被屏蔽，界面看着像死了），坏正则恒不匹配
	void loadSkipsEmptyAndInvalidRegexp()
	{
		rebuild("{\"Shield\": {\"Regexp\": [\"good.*\", \"\", \"(\"]}}");
		QCOMPARE(shield->getAllShields(), QStringList() << "t=good.*");
		QVERIFY(shield->contains("t=good.*"));
	}

	// m/u/t 三类规则的 insert/contains/remove 往返，getAllShields 有序
	void roundTrip()
	{
		rebuild("{}");
		shield->insert("u=alice");
		shield->insert("t=sp.*");
		shield->insert("m=3");
		QVERIFY(shield->contains("u=alice"));
		QVERIFY(shield->contains("t=sp.*"));
		QVERIFY(shield->contains("m=3"));
		QCOMPARE(shield->getAllShields(),
			QStringList() << "m=3" << "t=sp.*" << "u=alice");
		shield->remove("u=alice");
		shield->remove("t=sp.*");
		shield->remove("m=3");
		QVERIFY(!shield->contains("u=alice"));
		QVERIFY(!shield->contains("t=sp.*"));
		QVERIFY(!shield->contains("m=3"));
		// 1b10dc0：2 字节的 "t="（旧版残留形态）删除门槛从 <=2 改 <2——
		// 语义上只是"不崩、无副作用"，但这条路径以前根本走不进来
		shield->remove("t=");
		QVERIFY(shield->getAllShields().isEmpty());
	}

	// 非法 insert 一律忽略：坏正则、越界/非数字 mode、过短串
	void insertInvalidIgnored()
	{
		rebuild("{}");
		shield->insert("t=(");
		shield->insert("m=9");
		shield->insert("m=x");
		shield->insert("ab");
		QVERIFY(shield->getAllShields().isEmpty());
	}

	// isBlocked 的模式组映射（Shield.h 的 Group 位序：Top0 Bottom1 Slide2
	// Reverse3 Guest4 Advanced5 Color6 Whole7）：mode 1→Slide、4→Bottom、
	// 5→Top、6→Reverse、7/8→Advanced、Whole 拦一切
	void blockedByMode()
	{
		rebuild("{}");
		Comment c;
		c.mode = 4;
		c.color = 0xFFFFFF;
		c.sender = QStringLiteral("S");
		c.string = QStringLiteral("text");
		shield->setAllShields(QStringList() << "m=1");	// Bottom
		QVERIFY(shield->isBlocked(c));
		shield->setAllShields(QStringList() << "m=0");	// Top → mode 5
		c.mode = 5;
		QVERIFY(shield->isBlocked(c));
		c.mode = 4;
		QVERIFY(!shield->isBlocked(c));
		shield->setAllShields(QStringList() << "m=7");	// Whole
		c.mode = 1;
		QVERIFY(shield->isBlocked(c));
		shield->setAllShields(QStringList() << "m=2");	// Slide → mode 1
		QVERIFY(shield->isBlocked(c));
		shield->setAllShields(QStringList() << "m=3");	// Reverse → mode 6
		c.mode = 6;
		QVERIFY(shield->isBlocked(c));
		shield->setAllShields(QStringList() << "m=5");	// Advanced → mode 7/8
		c.mode = 7;
		QVERIFY(shield->isBlocked(c));
		c.mode = 8;
		QVERIFY(shield->isBlocked(c));
	}

	// 颜色组拦非白弹幕；Guest 组拦 D 开头、"0"、14 字符且第 4 位是 k 的 user_hash
	void blockedByColorAndGuest()
	{
		rebuild("{}");
		Comment c;
		c.mode = 1;
		c.color = 0xFF0000;
		c.sender = QStringLiteral("u1");
		c.string = QStringLiteral("x");
		shield->setAllShields(QStringList() << "m=6");	// Color
		QVERIFY(shield->isBlocked(c));
		c.color = 0xFFFFFF;
		QVERIFY(!shield->isBlocked(c));

		shield->setAllShields(QStringList() << "m=4");	// Guest
		c.sender = QStringLiteral("0");
		QVERIFY(shield->isBlocked(c));
		c.sender = QStringLiteral("D1234567890abc");
		QVERIFY(shield->isBlocked(c));
		QString fourteen = QStringLiteral("xxxkxxxxxxxxxx");
		QCOMPARE(fourteen.size(), 14);
		c.sender = fourteen;
		QVERIFY(shield->isBlocked(c));
		c.sender = QStringLiteral("normal");
		QVERIFY(!shield->isBlocked(c));
	}

	// 发送者集合精确匹配；正文走正则匹配
	void blockedBySenderAndRegexp()
	{
		rebuild("{}");
		Comment c;
		c.mode = 1;
		c.color = 0xFFFFFF;
		c.sender = QStringLiteral("alice");
		c.string = QStringLiteral("hello");
		shield->setAllShields(QStringList() << "u=alice" << "t=wor");
		QVERIFY(shield->isBlocked(c));
		c.sender = QStringLiteral("bob");
		c.string = QStringLiteral("hello");
		QVERIFY(!shield->isBlocked(c));
		c.string = QStringLiteral("world");
		QVERIFY(shield->isBlocked(c));
	}

	// 往返：内存规则经 aboutToSave 落盘、重读配置重建后原样恢复
	void saveAndReloadRoundTrip()
	{
		rebuild("{}");
		shield->setAllShields(QStringList() << "u=bob" << "t=x.+" << "m=3");
		Config::save();
		Config::load();
		QObject::disconnect(config, &Config::aboutToSave, nullptr, nullptr);
		delete shield;
		shield = new Shield(Local::instance());
		QCOMPARE(shield->getAllShields(),
			QStringList() << "m=3" << "t=x.+" << "u=bob");
	}

	void cleanupTestCase()
	{
		if (shield){
			QObject::disconnect(config, &Config::aboutToSave, nullptr, nullptr);
			delete shield;
		}
		QDir(gRoot).removeRecursively();
	}
};

QTEST_GUILESS_MAIN(tst_Shield)
#include "tst_shield.moc"
