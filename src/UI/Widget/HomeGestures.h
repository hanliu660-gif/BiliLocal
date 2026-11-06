#pragma once

#include <QObject>
#include <QPoint>

class QMouseEvent;
class QWheelEvent;
class QTimer;

namespace UI
{
	class Home;

	class HomeGestures : public QObject
	{
		Q_OBJECT
	public:
		explicit HomeGestures(Home *parent);

		void handleDoubleClick(QMouseEvent *e);
		void handleMouseMove(QMouseEvent *e);
		void handleMousePress(QMouseEvent *e);
		void handleMouseRelease(QMouseEvent *e);
		void handleWheel(QWheelEvent *e);

		int  infoHeight() const;
		void seekTo(QPoint p, bool commit);

	private:
		Home   *home;
		QTimer *timer;
		QTimer *delay;
		QTimer *holdTimer;
		QTimer *clickTimer;

		bool    speedHold;
		double  prevRate;
		bool    seekdrag;
		qint64  lastMove;
		QPoint  sta;
		QPoint  wgd;

		void initTimers();
	};
}
