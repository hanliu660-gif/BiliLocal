/*=======================================================================
*
*   Filename:    tst_layout.cpp
*   Time:        2026/11/07
*
*   Minimal unit tests for Layout::chooseLane (HANDOVER 6-45 regression):
*   sparse danmaku must spread over the full screen height instead of
*   piling up in the top lanes, while a unique least-crowded lane still
*   wins and ties rotate deterministically.
*
*   GPL-3.0, see COPYING.
*
=========================================================================*/

#include <QtTest>
#include <QSet>
#include <algorithm>
#include "../src/Model/Layout.h"

class tst_Layout : public QObject
{
	Q_OBJECT

private slots:
	void sparseLanesSpreadAcrossScreen();
	void singleLeastCrowdedLaneUnchanged();
	void tiesRotateDeterministically();
	void emptyInputIsSafe();
};

void tst_Layout::sparseLanesSpreadAcrossScreen()
{
	// Mode1::locate() yields ~100 lanes at 1080p with Grating=10. Sparse
	// danmaku never collide, so calculate() leaves every thick[] at zero.
	const int lanes = 100;
	int thick[lanes];
	std::fill(thick, thick + lanes, 0);
	int cursor = 0;

	QSet<int> picked;
	for (int i = 0; i < lanes; ++i) {
		picked.insert(Layout::chooseLane(thick, lanes, cursor));
	}
	QCOMPARE(picked.size(), lanes);   // full screen height, not just the top
	QVERIFY(picked.contains(32));     // y ~= 320, the hover anchor in 6-45

	// Control: the old "first minimum" behaviour fails this test.
	QSet<int> legacy;
	for (int i = 0; i < lanes; ++i) {
		legacy.insert(static_cast<int>(std::min_element(thick, thick + lanes) - thick));
	}
	QCOMPARE(legacy.size(), 1);
	QVERIFY(legacy.size() < picked.size());
}

void tst_Layout::singleLeastCrowdedLaneUnchanged()
{
	int thick[] = {3, 1, 2, 4};
	const int n = 4;
	for (int start = 0; start < n; ++start) {
		int cursor = start;
		QCOMPARE(Layout::chooseLane(thick, n, cursor), 1);
	}
	QCOMPARE(static_cast<int>(std::min_element(thick, thick + n) - thick), 1);
}

void tst_Layout::tiesRotateDeterministically()
{
	int thick[] = {1, 0, 1, 0, 1};
	const int n = 5;
	int cursor = 0;
	QCOMPARE(Layout::chooseLane(thick, n, cursor), 1);
	QCOMPARE(Layout::chooseLane(thick, n, cursor), 3);
	QCOMPARE(Layout::chooseLane(thick, n, cursor), 1);
	QCOMPARE(Layout::chooseLane(thick, n, cursor), 3);

	int other = 2;
	QCOMPARE(Layout::chooseLane(thick, n, other), 3);
}

void tst_Layout::emptyInputIsSafe()
{
	int cursor = 7;
	QCOMPARE(Layout::chooseLane(nullptr, 0, cursor), 0);
	QCOMPARE(cursor, 7);
}

int main(int argc, char **argv)
{
	QGuiApplication app(argc, argv);
	tst_Layout t;
	return QTest::qExec(&t, argc, argv);
}

#include "tst_layout.moc"
