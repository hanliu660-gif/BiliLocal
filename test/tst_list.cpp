/*=======================================================================
*
*   Filename:    tst_list.cpp
*   Time:        2026/10/25
*
*   最小单元测试：播放列表（List）的 mediaChanged -> case Records 路径，
*   对应 7bf39d2（同一视频挂两条指向同一份弹幕的规则时整屏重复）。
*   真链 List/Load/Parse/Config/Utils；Danmaku 用最小实现收结果，
*   APlayer 用最小子类发 mediaChanged，Local 用桩。
*   刻意不注册 Config 实例：setValue 的 write-back 会走 touch() -> 自动保存
*   定时器，没有 Config 对象时 touch() 直接跳过，测试全程不落盘。
*
*   本文件同样受 GPL-3.0 约束（随主工程分发）。
*
=========================================================================*/

#include <QtTest>
#include "../src/Model/List.h"
#include "../src/Access/Load.h"
#include "../src/Config.h"
#include "../src/Local.h"
#include "../src/Model/Comment.h"
#include "../src/Model/Danmaku.h"
#include "../src/Player/APlayer.h"
#include "../src/Utils.h"

// ---- Danmaku 最小实现（与 tst_load 相同的桩面）---------------------------
static QList<Record> gPool;

QList<Record> &Danmaku::getPool() { return gPool; }

void Danmaku::append(Record &&record)
{
	// 产品行为：按 source 字符串全等去重（坑 21）——两条异形路径都能进来
	for (const Record &r : gPool){
		if (r.source == record.source){
			return;
		}
	}
	gPool.append(std::move(record));
	emit modelAppend();
}

void Danmaku::clear() { gPool.clear(); }

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

// ---- APlayer：可实例化的最小子类（vtable 需要 + 测试要发信号）-------------
class TestPlayer : public APlayer
{
public:
	explicit TestPlayer(QObject *parent = nullptr) : APlayer(parent){}
	void play() override {}
	void stop(bool) override {}
	int getState() override { return Stop; }
	void setTime(qint64) override {}
	qint64 getTime() override { return 0; }
	void setMedia(QString file) override { media = file; }
	QString getMedia() override { return media; }
	qint64 getDuration() override { return 0; }
	void setVolume(int) override {}
	int getVolume() override { return 0; }

	void changeMedia(QString file) { emit mediaChanged(file); }

private:
	QString media;
};

APlayer::APlayer(QObject *parent)
	: QObject(parent)
{
	setObjectName("APlayer");
}

void APlayer::setRate(double) {}
double APlayer::getRate() { return 1.0; }
qint64 APlayer::getDelay(int) { return 0; }
void APlayer::setDelay(int, qint64) {}
int APlayer::getTrack(int) { return 0; }
void APlayer::setTrack(int, int) {}
QStringList APlayer::getTracks(int) { return QStringList(); }
void APlayer::addSubtitle(QString) {}
void APlayer::event(APlayer::Event) {}

// ---- Local 最小实现 -------------------------------------------------------
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

// ---- 测试 ----------------------------------------------------------------
class tst_List : public QObject
{
	Q_OBJECT
	Danmaku danmaku;
	List *list = nullptr;
	QString gRoot;

	bool waitUntil(const std::function<bool()> &pred)
	{
		for (int i = 0; i < 500 && !pred(); ++i){
			QTest::qWait(10);
		}
		return pred();
	}

	void writeFixture(const QString &name, const QByteArray &data, QString &path)
	{
		path = gRoot + "/" + name;
		QFile f(path);
		QVERIFY(f.open(QIODevice::WriteOnly));
		f.write(data);
		f.close();
	}

	QByteArray bilibiliXml() const
	{
		return QByteArray(
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?><i>"
			"<d p=\"1.5,1,25,16777215,1234567890,0,ABC123,0\">hello</d>"
			"<d p=\"2.5,4,25,16777215,1234567891,0,DEF456,0\">bottom</d>"
			"</i>");
	}

private slots:
	void initTestCase()
	{
		gRoot = QDir::tempPath() + "/bili-tst-list-" +
			QString::number(QCoreApplication::applicationPid());
		QDir().mkpath(gRoot);
		Local *l = Local::instance();
		l->objects.insert("APlayer", new TestPlayer(l));
		l->objects.insert("Danmaku", &danmaku);
		l->objects.insert("Load", new Load(l));
		// 刻意不注册 Config：见文件头注
		// List 全程一份（成员）：它的 mediaChanged 等连接不带上下文对象，
		// 挂在长命的 TestPlayer 上，per-case 栈实例会在析构后仍被回调（坑同 Shield）
		list = new List(l);
	}

	void init() { danmaku.clear(); }

	// 7bf39d2 的现场：模板子项（正斜杠，itemFromFile 的目录扫描自动建）
	// + 命令行带 xml 打开登记的绝对路径（反斜杠）。两者指向同一份 xml，
	// 按 source 全等去重分辨不出，canonicalFilePath 才认得出是同一份。
	void dedupRealFile()
	{
		QString xmlPath;
		writeFixture("video.xml", bilibiliXml(), xmlPath);
		QString videoPath;
		writeFixture("video.mp4", QByteArray("dummy"), videoPath);

		QStandardItem *item = list->itemFromFile(videoPath, true);
		QVERIFY(item != nullptr);
		// 目录扫描自动挂了模板子项，且 group() 把条目置成 Records 头
		QCOMPARE(item->rowCount(), 1);
		QCOMPARE(item->data(List::CodeRole).toInt(), (int)List::Records);

		// 手工补第二条：命令行打开时存的反斜杠绝对路径（不经 QFileInfo 归一）
		QStandardItem *b = new QStandardItem;
		QString raw = gRoot + "\\video.xml";
		b->setData(raw, List::CodeRole);
		b->setData((qint64)0, List::TimeRole);
		item->appendRow(b);
		QCOMPARE(item->rowCount(), 2);

		static_cast<TestPlayer *>(Local::instance()->findObject<APlayer>())->changeMedia(videoPath);
		QVERIFY(waitUntil([&]{ return danmaku.getPool().size() >= 1; }));
		QCOMPARE(danmaku.getPool().size(), 1);
		QCOMPARE(danmaku.getPool()[0].danmaku.size(), 2);
	}

	// 兜底分支：文件不存在时 canonicalFilePath 为空，
	// 退回 cleanPath(fromNativeSeparators) 做去重——只该入队一次，
	// 表现为 Load 的 errorOccured 只发一次
	void dedupMissingFileFallback()
	{
		QString videoPath;
		writeFixture("ghost.mp4", QByteArray("dummy"), videoPath);
		// 不放 ghost.xml：两条规则都指向不存在的文件。
		// 不走 itemFromFile 的目录扫描（无 xml 兄弟时不在本用例范围），
		// 手工构造等价的 Records 结构挂上共享列表。
		QFileInfo vi(videoPath);
		QStandardItem *item = new QStandardItem;
		item->setData(videoPath, List::FileRole);
		item->setData(List::Records, List::CodeRole);

		QStandardItem *a = new QStandardItem;
		a->setData(QString("%{Path}/%{File}.xml"), List::CodeRole);
		a->setData((qint64)0, List::TimeRole);
		item->appendRow(a);
		QStandardItem *b = new QStandardItem;
		b->setData(vi.absolutePath() + "\\ghost.xml", List::CodeRole);
		b->setData((qint64)0, List::TimeRole);
		item->appendRow(b);
		list->appendRow(item);

		Load *load = Local::instance()->findObject<Load>();
		QSignalSpy spy(load, &Load::errorOccured);

		static_cast<TestPlayer *>(Local::instance()->findObject<APlayer>())->changeMedia(videoPath);
		QVERIFY(waitUntil([&]{ return spy.count() >= 1; }));
		QCOMPARE(spy.count(), 1);
		QCOMPARE(danmaku.getPool().size(), 0);
	}

	void cleanupTestCase()
	{
		QDir(gRoot).removeRecursively();
	}
};

QTEST_GUILESS_MAIN(tst_List)
#include "tst_list.moc"
