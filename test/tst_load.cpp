/*=======================================================================
*
*   Filename:    tst_load.cpp
*   Time:        2026/10/25
*
*   最小单元测试：弹幕装载（Load）——真实读文件 → 嗅探格式 → 同步解析 →
*   挂进池 的完整路径。历史上的坑集中在嗅探：BOM 让 xml 认不出（坑 2）、
*   前置空行整份被当 AcFun JSON 得 0 条弹幕（坑 35，1b10dc0 修）。
*   真链 Load/Parse/Config/Utils；Danmaku 用最小实现收结果，Local 用桩。
*   不链 APlayer.cpp（其 create 会引入 VPlayer/libvlc；测试不构造 APlayer，
*   只为 vtable 补齐非纯虚函数的空定义）。
*   本测试不调用 Config::load，内存配置保持空对象，不落盘、不碰用户配置。
*
*   本文件同样受 GPL-3.0 约束（随主工程分发）。
*
=========================================================================*/

#include <QtTest>
#include "../src/Access/Load.h"
#include "../src/Access/Parse.h"
#include "../src/Config.h"
#include "../src/Local.h"
#include "../src/Model/Comment.h"
#include "../src/Model/Danmaku.h"
#include "../src/Player/APlayer.h"
#include "../src/Utils.h"

// ---- Danmaku 最小实现：Load 只调 append/clear/getPool，其余按接口给空实现 -
static QList<Record> gPool;

QList<Record> &Danmaku::getPool()
{
	return gPool;
}

void Danmaku::append(Record &&record)
{
	gPool.append(std::move(record));
	emit modelAppend();
}

void Danmaku::clear()
{
	gPool.clear();
}

void Danmaku::remove(QString source)
{
	for (int i = gPool.size() - 1; i >= 0; --i){
		if (gPool[i].source == source){
			gPool.removeAt(i);
		}
	}
}

void Danmaku::parse(int) {}
void Danmaku::delayAll(qint64) {}
qint64 Danmaku::getDuration() const { return 0; }
int Danmaku::size() const { return gPool.size(); }
Comment *Danmaku::at(int) { return nullptr; }
QList<Comment *>::iterator Danmaku::begin() { return QList<Comment *>::iterator(); }
QList<Comment *>::iterator Danmaku::end() { return QList<Comment *>::iterator(); }

int Danmaku::rowCount(const QModelIndex &) const { return 0; }
int Danmaku::columnCount(const QModelIndex &) const { return 1; }
QVariant Danmaku::data(const QModelIndex &, int) const { return QVariant(); }
QModelIndex Danmaku::parent(const QModelIndex &) const { return QModelIndex(); }
QModelIndex Danmaku::index(int, int, const QModelIndex &) const { return QModelIndex(); }
QVariant Danmaku::headerData(int, Qt::Orientation, int) const { return QVariant(); }
QHash<int, QByteArray> Danmaku::roleNames() const { return QAbstractItemModel::roleNames(); }

Danmaku::Danmaku(QObject *parent)
	: QAbstractItemModel(parent), d_ptr(nullptr)
{
	setObjectName("Danmaku");
}

Danmaku::~Danmaku()
{
}

// ---- Local 最小实现 -------------------------------------------------------
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

// ---- APlayer 非纯虚函数空定义：不链 APlayer.cpp（会引到 libvlc），
//      但 Load.cpp 的 codeToTask 引用了 APlayer 类型，vtable 需要有着落 ------
void APlayer::setRate(double) {}
double APlayer::getRate() { return 1.0; }
qint64 APlayer::getDelay(int) { return 0; }
void APlayer::setDelay(int, qint64) {}
int APlayer::getTrack(int) { return 0; }
void APlayer::setTrack(int, int) {}
QStringList APlayer::getTracks(int) { return QStringList(); }
void APlayer::addSubtitle(QString) {}
void APlayer::event(APlayer::Event) {}

// ---- 测试 ----------------------------------------------------------------
class tst_Load : public QObject
{
	Q_OBJECT
	Danmaku danmaku;
	QString gRoot;

	// QVERIFY 宏展开成 return;，只能用在 void 函数里，路径用出参带出
	void writeFixture(const QString &name, const QByteArray &data, QString &path)
	{
		path = gRoot + "/" + name;
		QFile f(path);
		QVERIFY(f.open(QIODevice::WriteOnly));
		f.write(data);
		f.close();
	}

	bool waitUntil(const std::function<bool()> &pred)
	{
		for (int i = 0; i < 500 && !pred(); ++i){
			QTest::qWait(10);
		}
		return pred();
	}

	QByteArray bilibiliXml() const
	{
		return QByteArray(
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?><i>"
			"<d p=\"1.5,1,25,16777215,1234567890,0,ABC123,0\">hello &amp; world</d>"
			"<d p=\"2.5,4,25,16777215,1234567891,0,DEF456,0\">bottom</d>"
			"</i>");
	}

private slots:
	void initTestCase()
	{
		gRoot = QDir::tempPath() + "/bili-tst-load-" +
			QString::number(QCoreApplication::applicationPid());
		QDir().mkpath(gRoot);
		Local *l = Local::instance();
		l->objects.insert("Config", new Config(l));
		l->objects.insert("Danmaku", &danmaku);
	}

	void init()
	{
		danmaku.clear();
	}

	// Bilibili xml 端到端：Record 的路径字段与解析结果都对上
	void bilibiliEndToEnd()
	{
		Load load;
		Load::Task task;
		writeFixture("bilibili.xml", bilibiliXml(), task.code);
		QVERIFY(load.enqueue(task));
		QVERIFY(waitUntil([&]{ return danmaku.getPool().size() == 1; }));

		const Record &record = danmaku.getPool().first();
		QCOMPARE(record.source, task.code);
		QCOMPARE(record.access, task.code);
		QCOMPARE(record.string, QStringLiteral("bilibili.xml"));
		QCOMPARE(record.danmaku.size(), 2);
		QCOMPARE(record.danmaku[0].time, qint64(1500));
		QCOMPARE(record.danmaku[0].sender, QLatin1String("ABC123"));
		QCOMPARE(record.danmaku[1].mode, 4);
	}

	// 坑 2：录播姬 xml 带 UTF-8 BOM，剥掉后照常识别为 xml
	void bomStripped()
	{
		Load load;
		Load::Task task;
		writeFixture("bom.xml", QByteArray("\xEF\xBB\xBF") + bilibiliXml(), task.code);
		QVERIFY(load.enqueue(task));
		QVERIFY(waitUntil([&]{ return danmaku.getPool().size() == 1; }));
		QCOMPARE(danmaku.getPool().first().danmaku.size(), 2);
	}

	// 坑 35：xml 前面有空行，trim 后才认得出 <?xml 头（旧行为=0 条弹幕）
	void blankLineBeforeXml()
	{
		Load load;
		Load::Task task;
		writeFixture("blank.xml", QByteArray("\n\n") + bilibiliXml(), task.code);
		QVERIFY(load.enqueue(task));
		QVERIFY(waitUntil([&]{ return danmaku.getPool().size() == 1; }));
		QCOMPARE(danmaku.getPool().first().danmaku.size(), 2);
	}

	// <packet> → Niconico 分支
	void niconicoPacket()
	{
		Load load;
		Load::Task task;
		writeFixture("nico.xml",
			QByteArray("<packet><chat vpos=\"100\" user_id=\"u1\">hello nico</chat></packet>"), task.code);
		QVERIFY(load.enqueue(task));
		QVERIFY(waitUntil([&]{ return danmaku.getPool().size() == 1; }));
		QCOMPARE(danmaku.getPool().first().danmaku.size(), 1);
		QCOMPARE(danmaku.getPool().first().danmaku[0].string, QStringLiteral("hello nico"));
	}

	// <c> 包裹的 <l i="..."> → AcFunLocalizer 分支（Load 嗅探认 <c>，
	// Parse 按 <l i=" 切；6 个逗号字段 + CDATA 正文）
	void acfunLocalizer()
	{
		Load load;
		Load::Task task;
		writeFixture("loc.xml",
			QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?><c>"
				"<l i=\"1.5,25,16777215,0,sender1,1234567\"><![CDATA[localized]]></l>"
				"</c>"), task.code);
		QVERIFY(load.enqueue(task));
		QVERIFY(waitUntil([&]{ return danmaku.getPool().size() == 1; }));
		QCOMPARE(danmaku.getPool().first().danmaku.size(), 1);
		QCOMPARE(danmaku.getPool().first().danmaku[0].sender, QLatin1String("sender1"));
		QCOMPARE(danmaku.getPool().first().danmaku[0].date, qint64(1234567));
	}

	// ASS 文件按 [Script Info] 识别；解析器只收带 \pos 且颜色非 0 的行
	// （mode=0 或 color=0 被丢弃，样本沿用 tst_parse 里验证过的形态）
	void assScript()
	{
		Load load;
		Load::Task task;
		writeFixture("clip.ass",
			QByteArray("[Script Info]\nPlayResY: 720\n\n"
				"[V4+ Styles]\n"
				"Format: Name, Fontname, Fontsize, PrimaryColour, Bold, Outline\n"
				"Style: Default,Arial,20,&H00FFFFFF,0,2\n\n"
				"[Events]\n"
				"Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
				"Dialogue: 0,0:00:01.00,0:00:05.00,Default,,0,0,0,,{\\fs25\\c&HFFFFFF&\\pos(100,200)}hello ass\n"), task.code);
		QVERIFY(load.enqueue(task));
		QVERIFY(waitUntil([&]{ return danmaku.getPool().size() == 1; }));
		QCOMPARE(danmaku.getPool().first().danmaku.size(), 1);
		QCOMPARE(danmaku.getPool().first().danmaku[0].string, QStringLiteral("hello ass"));
	}

	// 认不出的内容走 AcFun 兜底：0 条弹幕、无 error、不崩
	void garbageFallsBackToAcFun()
	{
		Load load;
		QSignalSpy spy(&load, &Load::errorOccured);
		Load::Task task;
		writeFixture("garbage.txt", "not xml and not json", task.code);
		QVERIFY(load.enqueue(task));
		QVERIFY(waitUntil([&]{ return danmaku.getPool().size() == 1; }));
		QCOMPARE(danmaku.getPool().first().danmaku.size(), 0);
		QCOMPARE(spy.count(), 0);
	}

	// 文件打不开：errorOccured(1)，随后队列清空
	void missingFileEmitsError()
	{
		Load load;
		QSignalSpy spy(&load, &Load::errorOccured);
		Load::Task task;
		task.code = gRoot + "/no-such-file.xml";
		QVERIFY(load.enqueue(task));
		QVERIFY(waitUntil([&]{ return spy.count() == 1; }));
		QCOMPARE(spy.first().first().toInt(), 1);
		QVERIFY(!load.busy());
	}

	// 队列先入先出：两个文件按序进池
	void sequentialTasks()
	{
		Load load;
		Load::Task a;
		writeFixture("first.xml", bilibiliXml(), a.code);
		Load::Task b;
		writeFixture("second.xml", bilibiliXml(), b.code);
		QVERIFY(load.enqueue(a));
		QVERIFY(load.enqueue(b));
		QVERIFY(waitUntil([&]{ return danmaku.getPool().size() == 2; }));
		QCOMPARE(danmaku.getPool()[0].string, QStringLiteral("first.xml"));
		QCOMPARE(danmaku.getPool()[1].string, QStringLiteral("second.xml"));
	}

	// delay != 0：全部弹幕时间平移，Record 记下延迟值
	void delayShiftsTimes()
	{
		Load load;
		Load::Task task;
		writeFixture("delay.xml", bilibiliXml(), task.code);
		task.delay = 5000;
		QVERIFY(load.enqueue(task));
		QVERIFY(waitUntil([&]{ return danmaku.getPool().size() == 1; }));
		QCOMPARE(danmaku.getPool().first().delay, qint64(5000));
		QCOMPARE(danmaku.getPool().first().danmaku[0].time, qint64(6500));
	}

	void cleanupTestCase()
	{
		QDir(gRoot).removeRecursively();
	}
};

QTEST_GUILESS_MAIN(tst_Load)
#include "tst_load.moc"
