#pragma once

#include "OpenGLRenderPrivate.h"
#include "../../Local.h"
#include <QOpenGLWindow>
#include <QWidget>

/*
	Embed output: libvlc renders the video itself into a native child widget
	(hardware decoding straight to the screen), while the danmaku are painted
	by a separate transparent overlay window that follows the main window
	geometry. This is the only output backend since the single-platform
	consolidation (2026-10-24).
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

	virtual QObject *getHandle() override
	{
		return surface;
	}

	virtual void resize(QSize size) override;

	virtual QSize getActualSize() override
	{
		return surface->size();
	}

	virtual void draw(QRect rect) override;

	void syncOverlay();
	void showOverlay();
	void hideOverlay();
	void nudge();

	QWidget *hostWidget() const
	{
		return host;
	}

private:
	QWidget *host;
	QWidget *surface;
	QOpenGLWindow *overlay;
};
