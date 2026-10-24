/*=======================================================================
*
*   Filename:    tst_parse.cpp
*   Time:        2026/10/24
*
*   最小单元测试：弹幕解析（Parse）与纯工具函数（Utils）。
*   覆盖不到的部分（Danmaku/Shield 等依赖 Local 服务定位器的模型类、
*   渲染层）仍走"脚本 + 截图 + 人眼弹幕"的既有验证流程。
*
*   本文件同样受 GPL-3.0 约束（随主工程分发）。
*
=========================================================================*/

#include <QtTest>
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
};

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	tst_Parse t;
	return QTest::qExec(&t, argc, argv);
}

#include "tst_parse.moc"
