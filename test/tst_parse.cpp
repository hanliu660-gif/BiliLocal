/*=======================================================================
*
*   Filename:    tst_parse.cpp
*   Time:        2026/10/24
*
*   最小单元测试：弹幕解析（Parse）与纯工具函数（Utils）。
*   覆盖不到的部分（List/Danmaku 等依赖整套服务定位器的模型类、渲染层）
*   验证走"脚本 + 日志/钩子 + 时序"流程：弹幕层的行为面 agent 均可自验
*   （交接文档 §3），只有"真实右键点到弹幕"这一个动作需要用户本人。
*
*   本文件同样受 GPL-3.0 约束（随主工程分发）。
*
=========================================================================*/

#include <QtTest>
#include <QTextCodec>
#include "../src/Access/Parse.h"
#include "../src/Utils.h"

using Parse::parseComment;

class tst_Parse : public QObject
{
	Q_OBJECT
private slots:
	// Bilibili XML：字段拆解（时间取整毫秒、模式、发送者=第 7 字段、实体解码）
	void bilibiliXml()
	{
		QByteArray xml =
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?><i>"
			"<d p=\"1.5,1,25,16777215,1234567890,0,ABC123,0\">hello &amp; world</d>"
			"<d p=\"2.5,4,25,16777215,1234567891,0,DEF456,0\">bottom</d>"
			"</i>";
		QVector<Comment> r = parseComment(xml, Utils::Bilibili);
		QCOMPARE(r.size(), 2);
		QCOMPARE(r[0].time, qint64(1500));
		QCOMPARE(r[0].mode, 1);
		QCOMPARE(r[0].sender, QLatin1String("ABC123"));
		QCOMPARE(r[0].string, QStringLiteral("hello & world"));
		QCOMPARE(r[1].mode, 4);
	}

	// 契约锁定：BOM 只影响 Load 的"格式探测"（剥 BOM 才认得出 <?xml 头），
	// 格式已知时 parseComment 本身不忌讳 BOM——全文匹配 <d p= 照常工作
	void bomOnlyMattersForSniffing()
	{
		QByteArray xml = "\xEF\xBB\xBF"
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?><i>"
			"<d p=\"1.0,1,25,16777215,1234567890,0,S,0\">x</d></i>";
		QCOMPARE(QVector<Comment>(parseComment(xml, Utils::Bilibili)).size(), 1);
		QCOMPARE(QVector<Comment>(parseComment(xml.mid(3), Utils::Bilibili)).size(), 1);
	}

	// 录播姬 2.x 写毫秒时间戳(13 位):不得被 32 位 strtol 饱和成
	// INT32_MAX(=2038-01-19),须归一为秒。旧格式秒级原样保留
	void bilibiliMillisecondDate()
	{
		QByteArray xml =
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?><i>"
			"<d p=\"1.5,1,25,16777215,1792731812763,0,UID,0\">ms</d>"
			"<d p=\"2.5,1,25,16777215,1792731812,0,UID,0\">sec</d>"
			"</i>";
		QVector<Comment> r = parseComment(xml, Utils::Bilibili);
		QCOMPARE(r.size(), 2);
		QCOMPARE(r[0].date, qint64(1792731812));
		QCOMPARE(r[1].date, qint64(1792731812));
	}

	// 录播姬 2.x:uid 脱敏为 0,但 user 属性带脱敏名、raw 带 user_hash。
	// sender 应被 user_hash 覆盖(它是录制内稳定标识,可屏蔽同人)
	void bilibiliRecorderUser()
	{
		QByteArray xml =
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?><i>"
			"<d p=\"39.909,1,25,16777215,1792834333995,0,0,0\" user=\"out**\" raw=\"[[0,0,0,0,0,0,0,&quot;x&quot;,0,0,0,&quot;&quot;,0,&quot;{}&quot;,&quot;{}&quot;,{&quot;extra&quot;:&quot;{\\&quot;user_hash\\&quot;:\\&quot;1504623549\\&quot;}&quot;}\"]\">yo</d>"
			"</i>";
		QVector<Comment> r = parseComment(xml, Utils::Bilibili);
		QCOMPARE(r.size(), 1);
		QCOMPARE(r[0].nick, QStringLiteral("out**"));
		QCOMPARE(r[0].sender, QStringLiteral("1504623549"));
	}

	// ASS：覆盖标签（\fs、\c&H…&、\pos）驱动字号/颜色/模式，正文剥标签；
	// 颜色是 &HBBGGRR& 字节序（原实现吞 '&' 得 0 丢弹幕，已修）
	void ass()
	{
		QByteArray ass =
			"[Script Info]\nPlayResY: 720\n\n"
			"[V4+ Styles]\n"
			"Format: Name, Fontname, Fontsize, PrimaryColour, Bold, Outline\n"
			"Style: Default,Arial,20,&H00FFFFFF,0,2\n\n"
			"[Events]\n"
			"Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
			"Dialogue: 0,0:00:01.00,0:00:05.00,Default,,0,0,0,,{\\fs25\\c&HFFFFFF&\\pos(100,200)}hello ass\n"
			"Dialogue: 0,0:00:02.00,0:00:05.00,Default,,0,0,0,,{\\c&H0000FF&\\pos(10,600)}red bottom\n";
		QVector<Comment> r = parseComment(ass, Utils::ASS);
		QCOMPARE(r.size(), 2);
		QCOMPARE(r[0].string, QStringLiteral("hello ass"));
		QCOMPARE(r[0].font, 25);
		QCOMPARE(r[0].color, 16777215);	// &HFFFFFF& → 白
		QCOMPARE(r[0].mode, 5);	// pos 在下半屏 → 底部固定
		QCOMPARE(r[0].time, qint64(1000));
		QCOMPARE(r[1].color, 16711680);	// &H0000FF& 是 &HBBGGRR&：RR=FF → 红 0xFF0000
		QCOMPARE(r[1].mode, 4);	// pos 在下半屏（y>PlayResY/2）→ 底部固定
	}

	// 时间/算术表达式（跳转框、同步框输入）
	void evaluate()
	{
		QCOMPARE(Utils::evaluate("1:30"), 90.0);
		QCOMPARE(Utils::evaluate("0:00:01.00"), 1.0);
		QCOMPARE(Utils::evaluate("1+2*3"), 7.0);
	}

	// HTML 实体快速解码（fast 路径，不进 QTextDocument）
	void decodeXml()
	{
		QCOMPARE(Utils::decodeXml("&amp;&lt;&gt;&nbsp;&quot;", true),
			QStringLiteral("&<> \""));
	}

	// CVE-2026-107479 缓解回归：decodeBytes 改成分块 + ConverterState 后，
	// 大输入必须与整份解码逐字等价，尤其跨 64 KiB 块边界的多字节字符不能断字。
	// 覆盖 AcfunLocalizer（走 decodeBytes 的整份路径）；Bilibili 主路径本来
	// 就是按标签片段解码，不吃整份输入。
	void largeChunkedDecode()
	{
		const int count = 5000;
		QByteArray xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><i>";
		for (int i = 0; i < count; ++i) {
			xml += "<l i=\"1.0,1,25,16777215,1234567890,S\"><![CDATA[弹幕测试";
			xml += QByteArray::number(i);
			xml += "]]></l>";
		}
		xml += "</i>";
		QVERIFY(xml.size() > 200 * 1024);	// 必须跨过多个块
		QVector<Comment> r = parseComment(xml, Utils::AcfunLocalizer);
		QCOMPARE(r.size(), count);
		QCOMPARE(r.first().string, QStringLiteral("弹幕测试0"));
		QCOMPARE(r.last().string, QStringLiteral("弹幕测试") + QString::number(count - 1));
		bool utf8All = true;
		for (int i = 0; i < count; ++i) {
			if (r[i].string != QStringLiteral("弹幕测试") + QString::number(i)) {
				utf8All = false;
				break;
			}
		}
		QVERIFY(utf8All);	// 每个块边界处的汉字都必须完整

		// GBK：双字节汉字跨块边界；编码名由 XML 声明经 codecForName 选取
		QTextCodec *gbk = QTextCodec::codecForName("GBK");
		if (!gbk) {
			QSKIP("GBK codec unavailable in this Qt build");
		}
		QString src = QStringLiteral("<?xml version=\"1.0\" encoding=\"GBK\"?><i>");
		for (int i = 0; i < count; ++i) {
			src += QStringLiteral("<l i=\"1.0,1,25,16777215,1234567890,S\"><![CDATA[中文弹幕");
			src += QString::number(i);
			src += QStringLiteral("]]></l>");
		}
		src += QStringLiteral("</i>");
		QByteArray data = gbk->fromUnicode(src);
		QVERIFY(data.size() > 200 * 1024);
		QVector<Comment> g = parseComment(data, Utils::AcfunLocalizer);
		QCOMPARE(g.size(), count);
		QCOMPARE(g.first().string, QStringLiteral("中文弹幕0"));
		QCOMPARE(g.last().string, QStringLiteral("中文弹幕") + QString::number(count - 1));
		bool gbkAll = true;
		for (int i = 0; i < count; ++i) {
			if (g[i].string != QStringLiteral("中文弹幕") + QString::number(i)) {
				gbkAll = false;
				break;
			}
		}
		QVERIFY(gbkAll);	// GBK 双字节汉字不能跨块断字
	}
};

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	tst_Parse t;
	return QTest::qExec(&t, argc, argv);
}

#include "tst_parse.moc"
