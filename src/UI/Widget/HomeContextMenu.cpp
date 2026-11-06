#include "Common.h"
#include "HomeContextMenu.h"
#include "Home.h"
#include "HomeShortcuts.h"
#include "Menu.h"
#include "Info.h"
#include "Editor.h"
#include "../../Config.h"
#include "../../Local.h"
#include "../../Utils.h"
#include "../../Model/Comment.h"
#include "../../Model/Danmaku.h"
#include "../../Model/Running.h"
#include "../../Model/Shield.h"
#include "../../Player/APlayer.h"
#include <QtWidgets>
#include <QtGui>
#include <QDateTime>

using namespace UI;

namespace
{
	void addTrack(QMenu *menu, int type)
	{
		QMenu *sub = new QMenu(Home::tr("Track"), menu);
		auto group = new QActionGroup(sub);
		auto track = lApp->findObject<APlayer>()->getTracks(type);
		for (int i = 0; i < track.size(); ++i) {
			auto action = group->addAction(track[i]);
			action->setCheckable(true);
			action->setChecked(lApp->findObject<APlayer>()->getTrack(type) == i);
			QObject::connect(action, &QAction::triggered, [=]() {
				lApp->findObject<APlayer>()->setTrack(type, i);
			});
		}
		sub->addActions(group->actions());
		sub->setEnabled(!sub->isEmpty());
		menu->addMenu(sub);
	}

	void addDelay(QMenu *menu, int type, qint64 step)
	{
		QMenu *sub = new QMenu(Home::tr("Sync"), menu);
		auto fake = sub->addAction(QString());
		fake->setEnabled(false);
		auto edit = new QLineEdit(sub);
		edit->setFrame(false);
		edit->setText(Home::tr("Delay: %1s").arg(lApp->findObject<APlayer>()->getDelay(type) / 1000.0));
		edit->setGeometry(sub->actionGeometry(fake).adjusted(24, 1, -1, -1));
		QObject::connect(edit, &QLineEdit::editingFinished, [=]() {
			QString e = QRegularExpression("[\\d\\.\\+\\-\\(][\\s\\d\\.\\:\\+\\-\\*\\/\\(\\)]*").match(edit->text()).captured();
			lApp->findObject<APlayer>()->setDelay(type, e.isEmpty() ? 0 : Utils::evaluate(e) * 1000);
		});
		QObject::connect(sub->addAction(Home::tr("%1s Faster").arg(step / 1000.0)), &QAction::triggered, [=]() {
			lApp->findObject<APlayer>()->setDelay(type, lApp->findObject<APlayer>()->getDelay(type) + step);
		});
		QObject::connect(sub->addAction(Home::tr("%1s Slower").arg(step / 1000.0)), &QAction::triggered, [=]() {
			lApp->findObject<APlayer>()->setDelay(type, lApp->findObject<APlayer>()->getDelay(type) - step);
		});
		sub->setEnabled(lApp->findObject<APlayer>()->getState() != APlayer::Stop);
		menu->addMenu(sub);
	}
}

void HomeContextMenu::show(Home *home, const QPoint &p)
{
	if (!home) return;
	auto *menu = home->menuWidget();
	auto *info = home->infoWidget();
	auto *shortcuts = home->shortcutsController();
	auto *running = lApp->findObject<Running>();
	auto *aplayer = lApp->findObject<APlayer>();
	auto *danmaku = lApp->findObject<Danmaku>();

	bool flag = true;
	flag = flag && !(menu && menu->isVisible() && menu->geometry().contains(p));
	flag = flag && !(info && info->isVisible() && info->geometry().contains(p));
	if (!flag) return;

	QMenu top(home);
	const Comment *cur = running ? running->commentAt(p) : nullptr;
	// 子菜单声明必须在函数作用域(exec 在 if 外),否则 if 闭合时
	// 栈对象析构,exec 时子菜单已销毁、菜单项凭空消失
	QMenu vid(Home::tr("Video"), home);
	QMenu aud(Home::tr("Audio"), home);
	QMenu sub(Home::tr("Subtitle"), home);
	QMenu dmk(Home::tr("Danmaku"), home);
	if (cur) {
		// 弹幕全文：QMenu 自带省略号看不全,换 QLabel 换行展示 + 可选中复制
		QLabel *title = new QLabel(cur->string, &top);
		title->setWordWrap(true);
		// 左缘对齐 QMenu::item 文字起点(QSS padding 18px + margin 4px)
		title->setContentsMargins(22, 2, 10, 2);
		title->setMaximumWidth(340 * home->logicalDpiX() / 96);
		title->setTextInteractionFlags(Qt::TextSelectableByMouse);
		QWidgetAction *text = new QWidgetAction(&top);
		text->setDefaultWidget(title);
		top.addAction(text);
		// 发送者数据块：禁用态菜单项，只读展示。
		// 录播姬 2.x 与新 B 站弹幕的 uid 已脱敏（一律 0）,按"匿名"展示
		const QString sender = cur->sender;
		const bool noId = sender.isEmpty() || sender == QLatin1String("0");
		const QDateTime sent = cur->date > 0 ? QDateTime::fromSecsSinceEpoch(cur->date) : QDateTime();
		auto infoItem = [&top](const QString &text) {
			QAction *a = top.addAction(text);
			a->setEnabled(false);
		};
		const bool hasHash = !cur->nick.isEmpty() && !cur->sender.isEmpty() && cur->sender != QLatin1String("0");
		infoItem(Home::tr("User: %1").arg(cur->nick.isEmpty() ? (noId ? Home::tr("anonymous") : sender) : cur->nick));
		if (hasHash) {
			infoItem(Home::tr("Hash: %1").arg(cur->sender));
		}
		if (sent.isValid()) {
			infoItem(Home::tr("Sent at: %1").arg(sent.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
		}
		{
			const qint64 sec = cur->time / 1000;
			const QString pos = sec >= 3600
				? QString("%1:%2:%3").arg(sec / 3600).arg(sec % 3600 / 60, 2, 10, QChar('0')).arg(sec % 60, 2, 10, QChar('0'))
				: QString("%1:%2").arg(sec / 60, 2, 10, QChar('0')).arg(sec % 60, 2, 10, QChar('0'));
			infoItem(Home::tr("Position: %1").arg(pos));
		}
		const bool hasId = hasHash || (!sender.isEmpty() && sender != QLatin1String("0"));
		int same = 0;
		if (hasId && danmaku) {
			for (const Record &r : danmaku->getPool()) {
				for (const Comment &c : r.danmaku) {
					if (c.sender == cur->sender) {
						++same;
					}
				}
			}
		}
		if (same > 1) {
			infoItem(Home::tr("%1 danmaku from this sender").arg(same));
		}
		top.addSeparator();
		QObject::connect(top.addAction(Home::tr("Copy")), &QAction::triggered, [cur]() {
			qApp->clipboard()->setText(cur->string);
		});
		QAction *elim = top.addAction(Home::tr("Eliminate The Sender"));
		elim->setEnabled(hasId);	// 屏蔽 u=0 等于清空整个池,禁用防手滑
		QObject::connect(elim, &QAction::triggered, [cur, danmaku]() {
			QString sender = cur->sender;
			if (!sender.isEmpty()) {
				lApp->findObject<Shield>()->insert("u=" + sender);
				if (danmaku) {
					danmaku->parse(Danmaku::Block);
				}
			}
		});
	}
	// 点在弹幕上:这是弹幕专属菜单,只保留信息/复制/屏蔽;通用功能只在空白处右键出现
	if (cur == nullptr) {
		top.addSeparator();
		if (info) {
			top.addActions(info->actions());
		}
		if (menu) {
			top.addAction(menu->findChild<QAction *>("File"));
		}
		if (shortcuts) {
			top.addAction(shortcuts->listAction());
		}
		addTrack(&vid, Utils::Video);
		if (shortcuts) {
			vid.addMenu(shortcuts->speedMenu());
		}
		vid.addAction(home->findChild<QAction *>("Rest"));
		top.addMenu(&vid);
		addTrack(&aud, Utils::Audio);
		addDelay(&aud, Utils::Audio, 100);
		top.addMenu(&aud);
		addTrack(&sub, Utils::Subtitle);
		addDelay(&sub, Utils::Subtitle, 500);
		QObject::connect(sub.addAction(Home::tr("Load Subtitle")), &QAction::triggered, [home, aplayer]() {
			if (!aplayer) return;
			QFileInfo mediaInfo(aplayer->getMedia());
			QString file = QFileDialog::getOpenFileName(home,
				Home::tr("Open File"),
				mediaInfo.absolutePath(),
				Home::tr("Subtitle files (%1);;All files (*.*)").arg(Utils::getSuffix(Utils::Subtitle, "*.%1").join(' ')));
			if (!file.isEmpty()) {
				aplayer->addSubtitle(file);
			}
		});
		top.addMenu(&sub);
		QMenu *dts = new QMenu(Home::tr("Track"), &dmk);	// 堆分配:同作用域陷阱
		if (danmaku) {
			for (const Record r : danmaku->getPool()) {
				dts->addAction(r.string);
			}
		}
		dts->setEnabled(!dts->isEmpty());
		dmk.addMenu(dts);
		QObject::connect(dmk.addAction(Home::tr("Sync")), &QAction::triggered, [home]() {
			Editor::exec(home, 2);
		});
		QAction *loadA = menu ? menu->findChild<QAction *>("Danm") : nullptr;
		// 开关行显示为普通动作,文案随状态翻转(打开弹幕/关闭弹幕),
		// 点击走 Togg 的完整逻辑——不用勾选行:QSS 下勾选列会把文字
		// 顶出一条与其他行错位的怪缩进
		const bool blockedNow = lApp->findObject<Shield>()->contains(QString("m=%1").arg(Shield::Whole));
		if (shortcuts) {
			dmk.addAction(blockedNow ? Home::tr("Open Danmaku") : Home::tr("Close Danmaku"),
				shortcuts->toggAction(), SLOT(trigger()), shortcuts->toggAction()->shortcut());
		}
		if (loadA) {
			dmk.addAction(Home::tr("Load"), loadA, SLOT(trigger()), loadA->shortcut())->setEnabled(loadA->isEnabled());
		}
		top.addMenu(&dmk);

		// 快捷键列宽度补偿:QSS 给 QMenu::item 的左右大内边距会把
		// 右对齐的快捷键压进长文字(播放列表/Ctrl+L 重叠),统一把
		// 主菜单与四个子菜单加宽到 容纳最长快捷键 为止
		int shortcutW = 0;
		QList<const QMenu *> menus{ &top, &vid, &aud, &sub, &dmk };
		for (const QMenu *m : menus){
			for (const QAction *a : m->actions()){
				if (!a->shortcut().isEmpty()){
					shortcutW = qMax(shortcutW,
						m->fontMetrics().horizontalAdvance(a->shortcut().toString(QKeySequence::NativeText)));
				}
			}
		}
		const int menuW = top.sizeHint().width() + shortcutW + 6;
		top.setMinimumWidth(menuW);
		for (QMenu *m : { &vid, &aud, &sub, &dmk }){
			m->setMinimumWidth(menuW);
		}
	}
	if (cur == nullptr && shortcuts) {
		top.addAction(shortcuts->confAction());
		top.addAction(shortcuts->quitAction());
	}
	// 菜单开着时钉住冻结：嵌套事件循环里过期定时器还会跑，
	// 不钉住的话菜单开着、弹幕却飞了
	if (cur && running) {
		running->pinHover(true);
	}
	top.exec(home->mapToGlobal(p));
	if (cur && running) {
		running->pinHover(false);
	}
}
