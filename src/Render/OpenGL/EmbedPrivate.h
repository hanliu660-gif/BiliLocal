#pragma once

#include "OpenGLRenderPrivate.h"
#include "../../Local.h"
#include <QOpenGLWindow>
#include <QSharedPointer>
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
	// 析构要按"挂上去的反顺序"拆干净：事件过滤器不摘，Home 上会留着指向
	// 已销毁渲染层的槽；延迟 nudge 的 singleShot 靠 alive 标记失效，别碰 this。
	QObject *filter;
	QMetaObject::Connection beginConn;
	QMetaObject::Connection beginNudgeConn;
	QSharedPointer<bool> alive;
};
