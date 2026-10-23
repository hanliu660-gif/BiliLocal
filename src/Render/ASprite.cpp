#include "Common.h"
#include "ASprite.h"
#include "ARender.h"
#include "../Graphic/GraphicPrivate.h"
#include "../Local.h"
#include "../Utils.h"

void ASprite::setAuto(const Comment & comment)
{
	color = QColor::fromRgb(comment.color);
	// 100 是纯不透明，压在画面上太糊；80 接近 B 站客户端的观感
	color.setAlphaF(Config::getValue("/Danmaku/Alpha", 80) / 100.0);
	effect = Config::getValue("/Danmaku/Effect", 5) / 2;
	const QSize & size = lApp->findObject<ARender>()->getActualSize();
	font = GraphicPrivate::getFont(comment.font * GraphicPrivate::getScale(comment.mode, comment.date, size));
	frame = comment.isLocal();
	text = Utils::decodeTxt(QString(comment.string));
}

void ASprite::setPosition(QPointF position)
{
	transform.reset();
	transform.translate(position.x(), position.y());
}
