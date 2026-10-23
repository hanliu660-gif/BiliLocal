#pragma once

#include "OpaquePrivate.h"
#include "../../Local.h"
#include <QWidget>
#include <QOpenGLWindow>

class OpenGLWindowRenderPrivate :public OpenGLOpaqueRenderPrivate
{
public:
	OpenGLWindowRenderPrivate();

	virtual bool isVisible() override
	{
		return widget->isVisible();
	}

	virtual QObject *getHandle() override
	{
		return window;
	}

	virtual void resize(QSize size) override
	{
		// createWindowContainer 建出来的容器是原生子窗口，全屏切换时它的
		// 尺寸同步会晚一拍；这里三层一起摆，避免只跟上外层、内层仍是旧尺寸。
		middle->setGeometry(0, 0, size.width(), size.height());
		widget->setGeometry(0, 0, size.width(), size.height());
		window->resize(size);
	}

	virtual QSize getActualSize() override
	{
		return widget->size();
	}

	virtual void draw(QRect rect) override
	{
		window->update(rect);
	}

private:
	QWidget *widget;
	QWidget *middle;
	QOpenGLWindow *window;
};
