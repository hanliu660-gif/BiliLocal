#pragma once

#include <QtCore>
#include <QtGui>

class OpenGLSprite;
class OpenGLRenderPrivate;

class OpenGLAtlas
{
public:
	static const int MaxSize;
	static const int Padding;
	static const int Spacing;

	explicit OpenGLAtlas(OpenGLRenderPrivate *render);

	~OpenGLAtlas();

	void insert(QList<QImage> &source, int effect, QList<OpenGLSprite> &result);

	inline void AddRef()
	{
		++refNum;
	}

	inline void DecRef()
	{
		if (refNum == 0) {
			return;
		}

		--refNum;

		if (refNum == 0) {
			expiry.start();
			usages.clear();
		}
	}

	inline bool isEmpty() const
	{
		return refNum == 0;
	}

	inline bool expired(int timeout) const
	{
		return isEmpty() && expiry.hasExpired(timeout);
	}

	inline GLuint getTexture() const
	{
		return cached;
	}

private:
	OpenGLRenderPrivate *render;

	size_t refNum;
	GLuint cached;
	QVector<QSize> usages;
	QElapsedTimer expiry;
};

class OpenGLSprite
{
public:
	QRectF rect;

	explicit OpenGLSprite(OpenGLAtlas *parent)
		: parent(parent)
	{
		parent->AddRef();
	}

	OpenGLSprite(const OpenGLSprite &other)
		: rect(other.rect), parent(other.parent)
	{
		parent->AddRef();
	}

	~OpenGLSprite()
	{
		parent->DecRef();
	}

	GLuint getTexture() const
	{
		return parent->getTexture();
	}

private:
	OpenGLAtlas *parent;
};

class OpenGLAtlasMgr
{
public:
	struct CreateInfo
	{
		QString text;
		QFont font;
		int effect;
	};

	explicit OpenGLAtlasMgr(OpenGLRenderPrivate *render);
	~OpenGLAtlasMgr();

	void insert(CreateInfo info, QList<OpenGLSprite> &result, QSize &size);

	inline const QList<OpenGLAtlas *> &getAtlases()
	{
		return atlases;
	}

	inline GLuint getUpload() const
	{
		return upload;
	}

	inline GLuint getBuffer() const
	{
		return buffer;
	}

	void shuffle();
	void squeeze(int timeout);

	// 真正释放 squeeze() 标记掉的图集。必须在 paintGL 里（上下文当前、且绑的
	// 是这块 surface）调用；删除是批量的——一次 glDeleteTextures 顶每页一次
	// 驱动往返（Qt6 下逐页删会拖住 GUI 线程，见交接文档坑 55）。
	void collect();

private:
	OpenGLRenderPrivate *render;

	QList<OpenGLAtlas *> atlases;
	QList<OpenGLAtlas *> graveyard;
	GLuint upload;
	GLuint buffer;
	QHash<CreateInfo, QPair<QList<OpenGLSprite>, QSize>> cached;
};
