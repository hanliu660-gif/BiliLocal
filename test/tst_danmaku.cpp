/*=======================================================================
*
*   Filename:    tst_danmaku.cpp
*   Time:        2026/11/07
*
*   最小单元测试：弹幕模型（Danmaku）——
*   测试 QAbstractItemModel 契约、角色映射、特殊弹幕模式上色（模式 4/5/6/7/8 双主题自适应）、
*   弹幕装载与去重、时间排序（is_sorted）、时长统计、时间偏移（delayAll）、
*   正则屏蔽与历史限制联动过滤、记录移除与清空。
*
*   本文件同样受 GPL-3.0 约束（随主工程分发）。
*
=========================================================================*/

#include <QtTest>
#include <QtGui>
#include "../src/Model/Danmaku.h"
#include "../src/Model/Comment.h"
#include "../src/Model/Shield.h"
#include "../src/Config.h"
#include "../src/Local.h"
#include "../src/Utils.h"

// ---- 桩：Local 与 Utils::localPath 最小实现（支持 findObject 查找 Shield/Config） ----
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

// ---- 测试套件 ----------------------------------------------------------------
class tst_Danmaku : public QObject
{
	Q_OBJECT

	Config *config = nullptr;
	Shield *shield = nullptr;
	Danmaku *danmaku = nullptr;

private slots:
	void initTestCase()
	{
		gRoot = QDir::tempPath() + "/bili-tst-danmaku-" +
			QString::number(QCoreApplication::applicationPid());
		QDir().mkpath(gRoot);

		config = new Config(Local::instance());
		Local::instance()->objects.insert("Config", config);

		shield = new Shield(Local::instance());
		Local::instance()->objects.insert("Shield", shield);

		danmaku = new Danmaku(Local::instance());
		Local::instance()->objects.insert("Danmaku", danmaku);
	}

	void emptyModelContract()
	{
		danmaku->clear();
		QCOMPARE(danmaku->rowCount(), 0);
		QCOMPARE(danmaku->columnCount(), 2);
		QCOMPARE(danmaku->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(), QStringLiteral("Time"));
		QCOMPARE(danmaku->headerData(1, Qt::Horizontal, Qt::DisplayRole).toString(), QStringLiteral("Comment"));
		QVERIFY(!danmaku->headerData(0, Qt::Vertical, Qt::DisplayRole).isValid());
		QVERIFY(!danmaku->hasIndex(0, 0));
		QVERIFY(!danmaku->index(0, 2).isValid());
		QVERIFY(!danmaku->parent(QModelIndex()).isValid());
		QCOMPARE(danmaku->getDuration(), (qint64)-1);
		QCOMPARE(danmaku->size(), 0);
		QVERIFY(danmaku->getPool().isEmpty());

		auto roles = danmaku->roleNames();
		QVERIFY(roles.contains(Danmaku::ModeRole));
		QVERIFY(roles.contains(Danmaku::FontRole));
		QVERIFY(roles.contains(Danmaku::ColorRole));
		QVERIFY(roles.contains(Danmaku::TimeRole));
		QVERIFY(roles.contains(Danmaku::DateRole));
		QVERIFY(roles.contains(Danmaku::SenderRole));
		QVERIFY(roles.contains(Danmaku::StringRole));
		QVERIFY(roles.contains(Danmaku::BlockRole));
	}

	void appendAndSorting()
	{
		danmaku->clear();

		Record r;
		r.source = QStringLiteral("video_1.xml");

		Comment c1;
		c1.time = 5000;
		c1.string = QStringLiteral("Danmaku at 5s");
		c1.mode = 1;

		Comment c2;
		c2.time = 1000;
		c2.string = QStringLiteral("Danmaku at 1s");
		c2.mode = 1;

		Comment c3;
		c3.time = 3000;
		c3.string = QStringLiteral("Danmaku at 3s");
		c3.mode = 1;

		// 乱序加入
		r.danmaku << c1 << c2 << c3;

		QSignalSpy spy(danmaku, &Danmaku::modelAppend);
		danmaku->append(std::move(r));

		QCOMPARE(spy.count(), 1);
		QCOMPARE(danmaku->rowCount(), 3);
		QCOMPARE(danmaku->size(), 3);

		// 验证严格按时间递增升序排列
		QCOMPARE(danmaku->at(0)->time, (qint64)1000);
		QCOMPARE(danmaku->at(1)->time, (qint64)3000);
		QCOMPARE(danmaku->at(2)->time, (qint64)5000);

		// 验证显示时间格式化
		QCOMPARE(danmaku->data(danmaku->index(0, 0), Qt::DisplayRole).toString(), QStringLiteral("00:01"));
		QCOMPARE(danmaku->data(danmaku->index(1, 0), Qt::DisplayRole).toString(), QStringLiteral("00:03"));
		QCOMPARE(danmaku->data(danmaku->index(2, 0), Qt::DisplayRole).toString(), QStringLiteral("00:05"));

		QCOMPARE(danmaku->data(danmaku->index(0, 1), Qt::DisplayRole).toString(), QStringLiteral("Danmaku at 1s"));
		QCOMPARE(danmaku->data(danmaku->index(1, 1), Qt::DisplayRole).toString(), QStringLiteral("Danmaku at 3s"));
		QCOMPARE(danmaku->data(danmaku->index(2, 1), Qt::DisplayRole).toString(), QStringLiteral("Danmaku at 5s"));

		// 验证 duration 正确更新为最大弹幕时间
		QCOMPARE(danmaku->getDuration(), (qint64)5000);
	}

	void appendDeduplication()
	{
		danmaku->clear();

		// 同一 Record 内部去重
		Record r;
		r.source = QStringLiteral("dup.xml");

		Comment c1;
		c1.time = 1000;
		c1.string = QStringLiteral("duplicate text");
		c1.mode = 1;

		Comment c2 = c1; // 完全相同的副本

		Comment c3;
		c3.time = 2000;
		c3.string = QStringLiteral("unique text");
		c3.mode = 1;

		r.danmaku << c1 << c2 << c3;
		danmaku->append(std::move(r));

		QCOMPARE(danmaku->rowCount(), 2);
		QCOMPARE(danmaku->at(0)->string, QStringLiteral("duplicate text"));
		QCOMPARE(danmaku->at(1)->string, QStringLiteral("unique text"));

		// 同源（source 相同）第二份 xml 追加合并
		Record r_more;
		r_more.source = QStringLiteral("dup.xml");

		Comment c_dup = c1; // 已存在的旧弹幕
		Comment c_new;
		c_new.time = 1500;
		c_new.string = QStringLiteral("inserted middle");
		c_new.mode = 1;

		r_more.danmaku << c_dup << c_new;

		QSignalSpy spy(danmaku, &Danmaku::modelInsert);
		danmaku->append(std::move(r_more));

		// 同源追加应发出 modelInsert 信号
		QCOMPARE(spy.count(), 1);
		// 重复的不应增加，总行数 2 + 1 = 3
		QCOMPARE(danmaku->rowCount(), 3);
		// 验证仍有序：1000, 1500, 2000
		QCOMPARE(danmaku->at(0)->time, (qint64)1000);
		QCOMPARE(danmaku->at(1)->time, (qint64)1500);
		QCOMPARE(danmaku->at(2)->time, (qint64)2000);
		QCOMPARE(danmaku->at(1)->string, QStringLiteral("inserted middle"));
	}

	void dataRolesAndJsonFormat()
	{
		danmaku->clear();

		Record r;
		r.source = QStringLiteral("special.xml");

		Comment normal;
		normal.time = 1000;
		normal.string = QStringLiteral("Normal danmaku");
		normal.mode = 1;
		normal.font = 25;
		normal.color = 0x00FF00;
		normal.date = 1600000000;
		normal.sender = QStringLiteral("user123");

		Comment advArray;
		advArray.time = 2000;
		// 模式 7 数组 JSON 格式：[x, y, alpha, color, "text content", ...]
		advArray.string = QString::fromUtf8(R"(["0", "0", "1", "16777215", "Special Adv Array"])");
		advArray.mode = 7;

		Comment advObject;
		advObject.time = 3000;
		// 模式 7 对象 JSON 格式：{"n": "Special Adv Object"}
		advObject.string = QString::fromUtf8(R"({"n": "Special Adv Object"})");
		advObject.mode = 7;

		r.danmaku << normal << advArray << advObject;
		danmaku->append(std::move(r));

		// 普通弹幕直接呈现字符串
		QCOMPARE(danmaku->data(danmaku->index(0, 1), Qt::DisplayRole).toString(), QStringLiteral("Normal danmaku"));
		// 模式 7 数组 JSON 正确解析出第 5 项文本
		QCOMPARE(danmaku->data(danmaku->index(1, 1), Qt::DisplayRole).toString(), QStringLiteral("Special Adv Array"));
		// 模式 7 对象 JSON 正确解析出键 "n" 文本
		QCOMPARE(danmaku->data(danmaku->index(2, 1), Qt::DisplayRole).toString(), QStringLiteral("Special Adv Object"));

		// 验证自定义角色
		QModelIndex idx0 = danmaku->index(0, 1);
		QCOMPARE(danmaku->data(idx0, Danmaku::ModeRole).toInt(), 1);
		QCOMPARE(danmaku->data(idx0, Danmaku::FontRole).toInt(), 25);
		QCOMPARE(danmaku->data(idx0, Danmaku::ColorRole).value<QColor>(), QColor(0x00FF00));
		QCOMPARE(danmaku->data(idx0, Danmaku::TimeRole).toLongLong(), (qint64)1000);
		QCOMPARE(danmaku->data(idx0, Danmaku::DateRole).toLongLong(), (qint64)1600000000);
		QCOMPARE(danmaku->data(idx0, Danmaku::SenderRole).toString(), QStringLiteral("user123"));
		QCOMPARE(danmaku->data(idx0, Danmaku::StringRole).toString(), QStringLiteral("Normal danmaku"));
		QCOMPARE(danmaku->data(idx0, Danmaku::BlockRole).toBool(), false);

		// 验证对齐角色
		QCOMPARE(danmaku->data(danmaku->index(0, 0), Qt::TextAlignmentRole).toInt(), (int)Qt::AlignCenter);
		QVERIFY(!danmaku->data(danmaku->index(0, 1), Qt::TextAlignmentRole).isValid());
	}

	void backgroundTintingRolesAndThemes()
	{
		danmaku->clear();

		Record r;
		r.source = QStringLiteral("modes.xml");

		int modes[] = { 1, 4, 5, 6, 7, 8 };
		for (int m : modes) {
			Comment c;
			c.time = m * 1000;
			c.string = QStringLiteral("Mode %1").arg(m);
			c.mode = m;
			r.danmaku.append(c);
		}
		danmaku->append(std::move(r));
		QCOMPARE(danmaku->rowCount(), 6);

		// 1. 浅色主题验证（Base lightness >= 128）
		QPalette lightPal = qApp->palette();
		lightPal.setColor(QPalette::Base, Qt::white);
		qApp->setPalette(lightPal);

		// 模式 1 滚动弹幕：透明/默认无底色（避免穿透到 ModeRole）
		QVERIFY(!danmaku->data(danmaku->index(0, 1), Qt::BackgroundRole).isValid());
		// 模式 4 底端弹幕：淡蓝
		QCOMPARE(danmaku->data(danmaku->index(1, 1), Qt::BackgroundRole).value<QColor>(), QColor(215, 235, 255));
		// 模式 5 顶端弹幕：淡橙
		QCOMPARE(danmaku->data(danmaku->index(2, 1), Qt::BackgroundRole).value<QColor>(), QColor(255, 230, 210));
		// 模式 6 逆向弹幕：淡紫
		QCOMPARE(danmaku->data(danmaku->index(3, 1), Qt::BackgroundRole).value<QColor>(), QColor(245, 220, 255));
		// 模式 7 高级弹幕：淡绿
		QCOMPARE(danmaku->data(danmaku->index(4, 1), Qt::BackgroundRole).value<QColor>(), QColor(200, 255, 200));
		// 模式 8 代码弹幕：淡黄
		QCOMPARE(danmaku->data(danmaku->index(5, 1), Qt::BackgroundRole).value<QColor>(), QColor(255, 255, 160));

		// 2. 深色主题验证（Base lightness < 128）
		QPalette darkPal = qApp->palette();
		darkPal.setColor(QPalette::Base, QColor(43, 43, 43));
		qApp->setPalette(darkPal);

		// 模式 1 仍为透明
		QVERIFY(!danmaku->data(danmaku->index(0, 1), Qt::BackgroundRole).isValid());
		// 模式 4 深色柔和暗蓝
		QCOMPARE(danmaku->data(danmaku->index(1, 1), Qt::BackgroundRole).value<QColor>(), QColor(40, 75, 120));
		// 模式 5 深色柔和暗橙
		QCOMPARE(danmaku->data(danmaku->index(2, 1), Qt::BackgroundRole).value<QColor>(), QColor(110, 70, 40));
		// 模式 6 深色柔和暗紫
		QCOMPARE(danmaku->data(danmaku->index(3, 1), Qt::BackgroundRole).value<QColor>(), QColor(85, 55, 115));
		// 模式 7 深色柔和墨绿
		QCOMPARE(danmaku->data(danmaku->index(4, 1), Qt::BackgroundRole).value<QColor>(), QColor(40, 95, 55));
		// 模式 8 深色柔和暗黄
		QCOMPARE(danmaku->data(danmaku->index(5, 1), Qt::BackgroundRole).value<QColor>(), QColor(95, 95, 40));

		// 恢复调色板
		qApp->setPalette(lightPal);
	}

	void delayAll()
	{
		danmaku->clear();

		Record r;
		r.source = QStringLiteral("delay.xml");
		Comment c;
		c.time = 2000;
		c.string = QStringLiteral("test delay");
		r.danmaku << c;
		danmaku->append(std::move(r));

		QCOMPARE(danmaku->at(0)->time, (qint64)2000);
		QCOMPARE(danmaku->getPool().first().delay, (qint64)0);

		// 平移 +1500ms
		danmaku->delayAll(1500);
		QCOMPARE(danmaku->at(0)->time, (qint64)3500);
		QCOMPARE(danmaku->getPool().first().delay, (qint64)1500);

		// 逆向平移 -1500ms 恢复
		danmaku->delayAll(-1500);
		QCOMPARE(danmaku->at(0)->time, (qint64)2000);
		QCOMPARE(danmaku->getPool().first().delay, (qint64)0);
	}

	void shieldFilteringAndFormatting()
	{
		danmaku->clear();

		Record r;
		r.source = QStringLiteral("shield.xml");

		Comment normal;
		normal.time = 1000;
		normal.string = QStringLiteral("Good comment");
		normal.mode = 1;

		Comment spam;
		spam.time = 2000;
		spam.string = QStringLiteral("Spam advertisement text");
		spam.mode = 1;

		r.danmaku << normal << spam;
		danmaku->append(std::move(r));

		QCOMPARE(danmaku->at(0)->blocked, false);
		QCOMPARE(danmaku->at(1)->blocked, false);

		// 插入过滤规则：匹配 Spam.*
		shield->insert(QStringLiteral("t=Spam.*"));
		danmaku->parse(Danmaku::Block);

		QCOMPARE(danmaku->at(0)->blocked, false);
		QCOMPARE(danmaku->at(1)->blocked, true);

		// 被屏蔽弹幕第一列显示 "Blocked"，文字颜色标红
		QCOMPARE(danmaku->data(danmaku->index(1, 0), Qt::DisplayRole).toString(), QStringLiteral("Blocked"));
		QCOMPARE(danmaku->data(danmaku->index(1, 0), Qt::ForegroundRole).value<QColor>(), QColor(Qt::red));
		// 第二列被屏蔽弹幕变灰
		QCOMPARE(danmaku->data(danmaku->index(1, 1), Qt::ForegroundRole).value<QColor>(), QColor(Qt::gray));

		// 清除规则
		shield->remove(QStringLiteral("t=Spam.*"));
		// 重新解析解除屏蔽
		for (Comment &c : danmaku->getPool().first().danmaku) {
			c.blocked = false;
		}
		danmaku->parse(Danmaku::Block);
		QCOMPARE(danmaku->at(1)->blocked, false);
	}

	void recordLimit()
	{
		danmaku->clear();

		Record r;
		r.source = QStringLiteral("limit.xml");
		r.limit = 1600000000; // 时间戳上限

		Comment cEarly;
		cEarly.time = 1000;
		cEarly.date = 1500000000; // 早于上限
		cEarly.string = QStringLiteral("early");

		Comment cLate;
		cLate.time = 2000;
		cLate.date = 1700000000; // 晚于上限，应被屏蔽
		cLate.string = QStringLiteral("late");

		r.danmaku << cEarly << cLate;
		danmaku->append(std::move(r));

		QCOMPARE(danmaku->at(0)->blocked, false);
		QCOMPARE(danmaku->at(1)->blocked, true);
	}

	void removeAndClear()
	{
		danmaku->clear();

		Record r1;
		r1.source = QStringLiteral("file1.xml");
		Comment c1;
		c1.time = 1000;
		c1.string = QStringLiteral("c1");
		r1.danmaku << c1;
		danmaku->append(std::move(r1));

		Record r2;
		r2.source = QStringLiteral("file2.xml");
		Comment c2;
		c2.time = 2000;
		c2.string = QStringLiteral("c2");
		r2.danmaku << c2;
		danmaku->append(std::move(r2));

		QCOMPARE(danmaku->rowCount(), 2);
		QCOMPARE(danmaku->getPool().size(), 2);

		// 移除 file1
		danmaku->remove(QStringLiteral("file1.xml"));
		QCOMPARE(danmaku->rowCount(), 1);
		QCOMPARE(danmaku->getPool().size(), 1);
		QCOMPARE(danmaku->getPool().first().source, QStringLiteral("file2.xml"));
		QCOMPARE(danmaku->at(0)->string, QStringLiteral("c2"));

		// 清空
		danmaku->clear();
		QCOMPARE(danmaku->rowCount(), 0);
		QCOMPARE(danmaku->getPool().size(), 0);
	}

	void cleanupTestCase()
	{
		if (shield) {
			QObject::disconnect(config, &Config::aboutToSave, nullptr, nullptr);
			delete shield;
			shield = nullptr;
		}
		if (danmaku) {
			delete danmaku;
			danmaku = nullptr;
		}
		if (config) {
			delete config;
			config = nullptr;
		}
		QDir(gRoot).removeRecursively();
	}
};

int main(int argc, char **argv)
{
	QGuiApplication app(argc, argv);
	tst_Danmaku t;
	return QTest::qExec(&t, argc, argv);
}

#include "tst_danmaku.moc"
