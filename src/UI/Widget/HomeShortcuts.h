#pragma once

#include <QObject>
#include <QAction>
#include <QMenu>

namespace UI
{
	class Home;

	class HomeShortcuts : public QObject
	{
		Q_OBJECT
	public:
		explicit HomeShortcuts(Home *parent);

		QAction *quitAction() const { return quitA; }
		QAction *fullAction() const { return fullA; }
		QAction *confAction() const { return confA; }
		QAction *toggAction() const { return toggA; }
		QAction *listAction() const { return listA; }
		QAction *snapAction() const { return snapA; }
		QMenu   *speedMenu()  const { return spd; }

	private:
		Home    *home;
		QAction *quitA;
		QAction *fullA;
		QAction *confA;
		QAction *toggA;
		QAction *listA;
		QAction *snapA;
		QMenu   *spd;
		int      prevVolume;

		void initActions();
	};
}
