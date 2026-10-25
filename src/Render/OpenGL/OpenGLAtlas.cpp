#include "Common.h"
#include "OpenGLAtlas.h"
#include "OpenGLRenderPrivate.h"
#include "../../Graphic/GraphicPrivate.h"

const int OpenGLAtlas::MaxSize = 2048;
const int OpenGLAtlas::Padding = 2;
const int OpenGLAtlas::Spacing = 0;

OpenGLAtlas::OpenGLAtlas(OpenGLRenderPrivate *render)
	: render(render)
	, refNum(0)
	, cached(0)
{
	render->glGenTextures(1, &cached);
	render->glBindTexture(GL_TEXTURE_2D, cached);
	GLenum format = render->pixelFormat(2, true);
	render->glTexImage2D(
		GL_TEXTURE_2D,
		0,
		format,
		MaxSize, MaxSize,
		0,
		format,
		GL_UNSIGNED_BYTE,
		nullptr);
	render->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	render->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	render->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	render->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

OpenGLAtlas::~OpenGLAtlas()
{
	// 刻意不删纹理：页的 GL 释放只走 OpenGLAtlasMgr::collect()（批量、且保证
	// 上下文当前）。走到这里还有纹理的情况只有进程退出——context 马上销毁，
	// 对象随 context 回收；Qt6 的退出序列里调 glDelete* 会长时间拖住 GUI 线程
	// 甚至空指针崩溃（dumps 24384/32884 挂起于 glDeleteFramebuffers，
	// 3148/36200 同析构链 ACCESS_VIOLATION——交接文档坑 55）。
}

void OpenGLAtlas::insert(QList<QImage> &source, int effect, QList<OpenGLSprite> &result)
{
	for (auto iter = source.begin(); iter != source.end();) {
		bool fitted = false;
		auto offset = QPoint(0, 0);
		for (QSize &size : usages) {
			if (size.height() >= iter->height() && size.width() + iter->width() <= MaxSize) {
				offset.rx() = size.width();
				size.rwidth() += iter->width();
				fitted = true;
				break;
			}
			offset.ry() += size.height();
		}
		if (!fitted && offset.y() + iter->height() <= MaxSize) {
			usages.append(iter->size());
			fitted = true;
		}
		if (fitted) {
			QRect data = QRect(offset, iter->size());
			QOpenGLShaderProgram *p = nullptr;
			switch (effect) {
			case 0:
				p = &render->resource->program[OpenGLRenderPrivate::Stro];
				break;
			case 1:
				p = &render->resource->program[OpenGLRenderPrivate::Proj];
				break;
			case 2:
				p = &render->resource->program[OpenGLRenderPrivate::Glow];
				break;
			default:
				return;
			}
			const int spc = OpenGLAtlas::Spacing;
			QRectF draw(data.adjusted(spc, spc, -spc, -spc));

			render->appendLoadCall(draw, p, data, iter->constBits());

			OpenGLSprite sprite(this);
			sprite.rect = draw;
			result.append(sprite);
			iter = source.erase(iter);
		}
		else {
			return;
		}
	}
}

OpenGLAtlasMgr::OpenGLAtlasMgr(OpenGLRenderPrivate *render)
	: render(render)
{
	render->glGenTextures(1, &upload);
	render->glBindTexture(GL_TEXTURE_2D, upload);
	render->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	render->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	render->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	render->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLenum format = render->pixelFormat(1);
	render->glTexImage2D(
		GL_TEXTURE_2D,
		0,
		format,
		OpenGLAtlas::MaxSize,
		OpenGLAtlas::MaxSize,
		0,
		format,
		GL_UNSIGNED_BYTE,
		nullptr);
	render->glGenFramebuffers(1, &buffer);
}

OpenGLAtlasMgr::~OpenGLAtlasMgr()
{
	// 退出路径零 GL 调用（同 ~OpenGLAtlas 的理由）：FBO 与纹理随 context 销毁
	// 回收。atlases/graveyard 里的页由 ~OpenGLAtlas 无 GL 地释放。
}

uint qHash(const OpenGLAtlasMgr::CreateInfo &i, uint seed = 0)
{
	uint h = ::qHash(i.effect, seed);
	h = (h << 1) ^ ::qHash(i.text, seed);
	h = (h << 1) ^ ::qHash(i.font, seed);
	return h;
}

bool operator ==(const OpenGLAtlasMgr::CreateInfo &f, const OpenGLAtlasMgr::CreateInfo &s)
{
	return f.text == s.text && f.font == s.font && f.effect == s.effect;
}

void OpenGLAtlasMgr::insert(CreateInfo info, QList<OpenGLSprite> &result, QSize &size)
{
	auto itr = cached.find(info);
	if (cached.end() != itr) {
		result = itr->first; size = itr->second;
		return;
	}

	const int exp = OpenGLAtlas::Padding + OpenGLAtlas::Spacing;
	QImage source(GraphicPrivate::getSize(info.text, info.font) + QSize(exp, exp) * 2, QImage::Format_Alpha8);
	source.fill(Qt::transparent);
	QPainter painter(&source);
	painter.setFont(info.font);
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.drawText(source.rect().adjusted(exp, exp, -exp, -exp), info.text);
	painter.end();

	QList<QImage> splited;
	size = QSize(0, 0);
	int sw = source.width(), sh = source.height();
	for (int y = 0; y < sh; y += OpenGLAtlas::MaxSize) {
		++size.rheight();
		for (int x = 0; x < sw; x += OpenGLAtlas::MaxSize) {
			int w = std::min<int>(sw - x, OpenGLAtlas::MaxSize);
			int h = std::min<int>(sh - y, OpenGLAtlas::MaxSize);
			if (w == sw && h == sh) {
				splited.append(source);
			}
			else {
				splited.append(source.copy(x, y, w, h));
			}
		}
	}
	size.rwidth() = size.height() == 0 ? 0 : splited.size() / size.height();

	result.clear();
	for (;;) {
		if (atlases.isEmpty() == false) {
			atlases.front()->insert(splited, info.effect, result);
		}
		if (splited.isEmpty() == false) {
			shuffle();
		}
		else {
			break;
		}
	}

	if (!result.isEmpty()) {
		cached.insert(info, qMakePair(result, size));
	}
}

void OpenGLAtlasMgr::shuffle()
{
	if (atlases.isEmpty()) {
		atlases.prepend(new OpenGLAtlas(render));
	}
	else {
		render->flushLoad();

		for (auto iter = atlases.begin() + 1; iter != atlases.end(); ++iter) {
			if ((*iter)->isEmpty()) {
				OpenGLAtlas *at = *iter;
				atlases.erase(iter);
				atlases.prepend(at);
				break;
			}
		}
		if (atlases.front()->isEmpty() == false) {
			atlases.prepend(new OpenGLAtlas(render));
		}
	}

	GLuint target = atlases.front()->getTexture();
	GLuint window = QOpenGLContext::currentContext()->defaultFramebufferObject();
	render->glBindFramebuffer(GL_FRAMEBUFFER, buffer);
	if (render->extensions.contains("clear_texture")) {
		static uchar bits[4];
		GLenum format = render->pixelFormat(1);

		typedef void (QOPENGLF_APIENTRYP ClearTexImageFunc) (
			GLuint texture,
			int level,
			GLenum format,
			GLenum type,
			const void *data);

		static ClearTexImageFunc glClearTexImage = (ClearTexImageFunc)QOpenGLContext::currentContext()->getProcAddress("glClearTexImage");
		glClearTexImage(
			upload,
			0,
			format,
			GL_UNSIGNED_BYTE,
			bits);
	}
	else if (render->extensions.contains("texture_rg")) {
		render->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, upload, 0);
		render->glViewport(0, 0, OpenGLAtlas::MaxSize, OpenGLAtlas::MaxSize);
		render->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		render->glClear(GL_COLOR_BUFFER_BIT);
	}
	else{
		static uchar bits[OpenGLAtlas::MaxSize * OpenGLAtlas::MaxSize];
		GLenum format = render->pixelFormat(1);
		render->glBindTexture(GL_TEXTURE_2D, upload);
		render->glTexImage2D(
			GL_TEXTURE_2D,
			0,
			format,
			OpenGLAtlas::MaxSize,
			OpenGLAtlas::MaxSize,
			0,
			format,
			GL_UNSIGNED_BYTE,
			bits);
	}
	render->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
	render->glBindFramebuffer(GL_FRAMEBUFFER, window);
	cached.clear();
}

void OpenGLAtlasMgr::squeeze(int timeout)
{
	// 只标记、不删除：squeeze() 由 APlayer::timeChanged 直连进来，跑在 paintGL
	// 之外，那时没有「这块 surface 的当前上下文」，~OpenGLAtlas 里的
	// glDeleteTextures 会落到别的上下文上或直接不生效，纹理就一直留着。
	for (auto iter = atlases.begin(); iter != atlases.end() && atlases.size() > 2;) {
		if ((*iter)->expired(timeout)) {
			graveyard.append(*iter);
			iter = atlases.erase(iter);
		}
		else {
			++iter;
		}
	}
}

void OpenGLAtlasMgr::collect()
{
	// 批量删除：把整批页的纹理并成一次 glDeleteTextures（一次驱动往返），
	// 替代过去逐页 delete——每页各打一发 glDeleteTextures，Qt6 下这类调用
	// 每次都可能被驱动拖住，播放中的小顿挫主要来自这里
	if (!graveyard.isEmpty() && render){
		QList<GLuint> ids;
		ids.reserve(graveyard.size());
		for (OpenGLAtlas *page : graveyard) {
			ids.append(page->getTexture());
		}
		render->glDeleteTextures(ids.size(), ids.constData());
	}
	qDeleteAll(graveyard);
	graveyard.clear();
}
