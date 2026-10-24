#pragma once

#include "Editor.h"
#include "Info.h"
#include "Menu.h"
#include "Prefer.h"
#include "../Interface.h"
#include "../../Config.h"
#include "../../Local.h"
#include "../../Access/Load.h"
#include "../../Utils.h"
#include "../../Model/Danmaku.h"
#include "../../Model/Running.h"
#include "../../Model/List.h"
#include "../../Model/Shield.h"
#include "../../Player/APlayer.h"
#include "../../Render/ARender.h"
#include <QtCore>
#include <QtWidgets>

namespace UI
{
	class Home : public QWidget
	{
		Q_OBJECT
	public:
		explicit Home(QWidget *parent = nullptr);

		void warning(QString title, QString text);

	private:
		QTimer *timer;
		QTimer *delay;
		QTimer *holdTimer;
		bool speedHold;
		double prevRate;
		int prevVolume;

		QAction *quitA;
		QAction *fullA;
		QAction *confA;
		QAction *toggA;
		QAction *listA;
		QMenu *spd;
		QPointer<QDialog> msg;

		UI::Menu *menu;
		UI::Info *info;
		List *list;
		Load *load;
		APlayer *aplayer;
		Danmaku *danmaku;
		Running *running;
		ARender *arender;

		QPoint sta;
		QPoint wgd;

		bool seekdrag;
		qint64 lastMove;
		bool wasMaximized;

		int  infoHeight() const;
		void seekTo(QPoint p, bool commit);

		virtual void closeEvent(QCloseEvent *e) override;
		virtual void dragEnterEvent(QDragEnterEvent *e) override;
		virtual void dropEvent(QDropEvent *e) override;
		virtual void mouseDoubleClickEvent(QMouseEvent *e) override;
		virtual void mouseMoveEvent(QMouseEvent *e) override;
		virtual void mousePressEvent(QMouseEvent *e) override;
		virtual void mouseReleaseEvent(QMouseEvent *e) override;
		virtual void wheelEvent(QWheelEvent *e) override;
		virtual void resizeEvent(QResizeEvent *e) override;

		QSize parseSize(QString string);
		void setGeometry(QSize size, bool center);
		void showContextMenu(QPoint p);

	private slots:
		// 私有非槽时 Prefer 里 QMetaObject::invokeMethod(widget,"setWindowFlags")
		// 找不到方法会静默失败——置顶设置改完不生效，要等播放状态变化才应用
		void setWindowFlags();
	};
}
