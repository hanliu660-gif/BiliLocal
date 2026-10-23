#pragma once

#include "OpaquePrivate.h"
#include "../PFormat.h"
#include "../../Local.h"
#include <QOpenGLWindow>
#include <QWidget>

/*
	Embed output: libvlc renders the video itself into a native child widget
	(hardware decoding straight to the screen), while the danmaku are painted
	by a separate transparent, click-through, always-on-top overlay window
	that follows the main window geometry.
*/

class OpenGLEmbedRenderPrivate :public OpenGLRenderPrivate
{
public:
	OpenGLEmbedRenderPrivate();

	virtual ~OpenGLEmbedRenderPrivate();

	virtual bool isVisible() override
	{
		return surface->isVisible();
	}

	virtual void drawData(QPainter *, QRect) override
	{
	}

	virtual void setFormat(PFormat *format) override
	{
		format->chroma = "NONE";
	}

	virtual void setBuffer(ABuffer *) override
	{
	}

	virtual QObject *getHandle() override
	{
		return surface;
	}

	virtual void resize(QSize size) override;

	virtual QSize getActualSize() override
	{
		return surface->size();
	}

	virtual QSize getBufferSize() override
	{
		return QSize();
	}

	virtual void draw(QRect rect) override;

	void syncOverlay();
	void showOverlay();
	void hideOverlay();
	void nudge();

private:
	QWidget *host;
	QWidget *surface;
	QOpenGLWindow *overlay;
};
