#pragma once

#include <QPoint>

namespace UI
{
	class Home;

	class HomeContextMenu
	{
	public:
		static void show(Home *home, const QPoint &p);
	};
}
