/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    Prefer.cpp
*   Time:        2015/05/13
*   Author:      Lysine
*
*   Lysine is a student majoring in Software Engineering
*   from the School of Software, SUN YAT-SEN UNIVERSITY.
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
=========================================================================*/

#include "Common.h"
#include "Prefer.h"
#include "WidgetInterfacePrivate.h"
#include "../Interface.h"
#include "../../Config.h"
#include "../../Local.h"
#include "../../Utils.h"
#include "../../Model/Danmaku.h"
#include "../../Model/Shield.h"
#include "../../Player/APlayer.h"
#include "../../Render/ARender.h"
#include <vlc/libvlc.h>

using namespace UI;

void Prefer::exec(QWidget *parent, int index)
{
	static Prefer *executing;
	if (!executing){
		Prefer prefer(parent);
		prefer.nav->setCurrentRow(index);
		executing = &prefer;
		prefer.QDialog::exec();
		executing = nullptr;
	}
	else{
		executing->activateWindow();
	}
}

QHash<QString, QVariant> Prefer::getRestart()
{
	QStringList path;
	path <<
		"/Player/Arguments" <<
		"/Interface/Single" <<
		"/Interface/Locale";
	QHash<QString, QVariant> data;
	for (QString iter : path){
		data[iter] = Config::getValue<QVariant>(iter);
	}
	return data;
}

QHash<QString, QVariant> Prefer::getReparse()
{
	QHash<QString, QVariant> data;
	int g = 0;
	for (int i = 0; i < 8; ++i){
		g = (g << 1) + check[i]->isChecked();
	}
	data["/Shield/Group"] = g;
	data["/Shield/Regexp"] = rm->stringList();
	data["/Shield/Sender"] = sm->stringList();
	return data;
}

namespace
{
	class List :public QListView
	{
	public:
		List(QWidget *parent = 0) :
			QListView(parent)
		{
		}

		void currentChanged(const QModelIndex &c,
			const QModelIndex &p)
		{
			QListView::currentChanged(c, p);
			selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
		}
	};

	// Win11 式设置页骨架：无框滚动区 + 卡片列
	class Page :public QWidget
	{
	public:
		explicit Page(QWidget *parent = 0) :
			QWidget(parent)
		{
			auto outer = new QVBoxLayout(this);
			outer->setContentsMargins(0, 0, 0, 0);
			auto scroll = new QScrollArea(this);
			scroll->setWidgetResizable(true);
			scroll->setFrameShape(QFrame::NoFrame);
			auto body = new QWidget(scroll);
			column = new QVBoxLayout(body);
			column->setContentsMargins(16, 14, 16, 16);
			column->setSpacing(10);
			scroll->setWidget(body);
			outer->addWidget(scroll);
		}

		QVBoxLayout *column;
	};

	// 卡片：标题 + 可选副标题 + 内容列；返回内容列供调用方继续加行
	QVBoxLayout *card(QVBoxLayout *parent, const QString &title, const QString &note = QString())
	{
		auto frame = new QFrame;
		frame->setObjectName("Card");
		auto box = new QVBoxLayout(frame);
		box->setContentsMargins(14, 10, 14, 12);
		box->setSpacing(4);
		auto head = new QLabel(title, frame);
		head->setObjectName("CardTitle");
		box->addWidget(head);
		if (!note.isEmpty()){
			auto sub = new QLabel(note, frame);
			sub->setObjectName("CardNote");
			sub->setWordWrap(true);
			box->addWidget(sub);
		}
		parent->addWidget(frame);
		return box;
	}

	// 设置行：说明在左、控件靠右；说明与控件共用 tooltip
	void addRow(QVBoxLayout *parent, const QString &text, QWidget *control, const QString &tip = QString())
	{
		auto row = new QWidget;
		auto h = new QHBoxLayout(row);
		h->setContentsMargins(0, 4, 0, 4);
		h->setSpacing(10);
		auto label = new QLabel(text, row);
		if (!tip.isEmpty()){
			label->setToolTip(tip);
			control->setToolTip(tip);
		}
		h->addWidget(label);
		h->addStretch(1);
		control->setMinimumWidth(170);
		h->addWidget(control);
		parent->addWidget(row);
	}

	void addSep(QVBoxLayout *parent)
	{
		auto line = new QFrame;
		line->setObjectName("SepLine");
		line->setFrameShape(QFrame::NoFrame);
		line->setFixedHeight(1);
		parent->addWidget(line);
	}

	// 导航小图标：QPainter 矢量，中性灰，深浅主题通用（2x pixmap 供高分屏取用）
	QIcon navIcon(int kind)
	{
		QPixmap pm(32, 32);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		QPen pen(QColor(130, 130, 136), 1.7);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.scale(2, 2);
		switch (kind){
		case 0:	// Playing：圆角框里的播放三角
			p.drawRoundedRect(QRectF(1.6, 3.2, 12.8, 9.6), 1.6, 1.6);
			p.drawPolyline(QPolygonF() << QPointF(6.2, 5.9) << QPointF(6.2, 10.1) << QPointF(10.2, 8.0) << QPointF(6.2, 5.9));
			break;
		case 1:	// Interface：显示器
			p.drawRoundedRect(QRectF(1.6, 2.6, 12.8, 9.0), 1.5, 1.5);
			p.drawLine(QPointF(5.0, 13.7), QPointF(11.0, 13.7));
			p.drawLine(QPointF(8.0, 11.6), QPointF(8.0, 13.7));
			break;
		case 2:	// Shield：盾牌
		{
			QPolygonF shield;
			shield << QPointF(8.0, 2.0) << QPointF(13.4, 4.2) << QPointF(13.4, 8.2)
				<< QPointF(8.0, 12.6) << QPointF(2.6, 8.2) << QPointF(2.6, 4.2);
			p.drawPolygon(shield);
			break;
		}
		case 3:	// Shortcut：键盘
			p.drawRoundedRect(QRectF(1.2, 4.0, 13.6, 8.0), 1.5, 1.5);
			for (int r = 0; r < 2; ++r){
				for (int c = 0; c < 4; ++c){
					p.drawPoint(4.0 + c * 2.2, 6.4 + r * 2.6);
				}
			}
			p.drawLine(QPointF(5.4, 10.4), QPointF(10.6, 10.4));
			break;
		default:	// About：i 圆
			p.drawEllipse(QRectF(2.4, 2.4, 11.2, 11.2));
			p.drawPoint(8.0, 6.4);
			p.drawLine(QPointF(8.0, 8.2), QPointF(8.0, 11.4));
			break;
		}
		return QIcon(pm);
	}
}

Prefer::Prefer(QWidget *parent) :
QDialog(parent)
{
	setWindowTitle(tr("Config"));
	auto outer = new QHBoxLayout(this);
	outer->setContentsMargins(10, 10, 10, 10);
	outer->setSpacing(10);

	nav = new QListWidget(this);
	nav->setObjectName("NavList");
	nav->setIconSize(QSize(18 * logicalDpiX() / 96, 18 * logicalDpiY() / 96));
	nav->setFixedWidth(176 * logicalDpiX() / 96);
	stack = new QStackedWidget(this);
	outer->addWidget(nav);
	outer->addWidget(stack, 1);

	//Playing
	{
		stack->addWidget(page[0] = new Page(this));
		auto col = static_cast<Page *>(page[0])->column;

		auto playing = card(col, tr("Playback"));
		{
			QCheckBox *resume = new QCheckBox(tr("resume where you left off"), page[0]);
			resume->setChecked(Config::getValue("/Player/Resume", true));
			connect(resume, &QCheckBox::stateChanged, [](int state){
				Config::setValue("/Player/Resume", state == Qt::Checked);
			});
			playing->addWidget(resume);
			QCheckBox *cont = new QCheckBox(tr("auto play next"), page[0]);
			cont->setChecked(Config::getValue("/Player/Continue", true));
			connect(cont, &QCheckBox::stateChanged, [](int state){
				Config::setValue("/Player/Continue", state == Qt::Checked);
			});
			playing->addWidget(cont);
			QCheckBox *loop = new QCheckBox(tr("loop playback"), page[0]);
			loop->setChecked(Config::getValue("/Player/Loop", false));
			connect(loop, &QCheckBox::stateChanged, [](int state){
				Config::setValue("/Player/Loop", state == Qt::Checked);
			});
			playing->addWidget(loop);
			addSep(playing);
			jump = new QDoubleSpinBox(page[0]);
			jump->setRange(1.0, 600.0);
			jump->setSingleStep(1.0);
			jump->setDecimals(1);
			jump->setSuffix(QStringLiteral(" s"));
			jump->setValue(Config::getValue("/Interface/Interval", 10));
			addRow(playing, tr("skip time"), jump);
			connect(jump, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [](double value){
				Config::setValue("/Interface/Interval", value);
			});
		}

		auto loading = card(col, tr("Loading"));
		load[0] = new QCheckBox(tr("clear when reloading"), page[0]);
		load[0]->setChecked(Config::getValue("/Load/Clear", false));
		connect(load[0], &QCheckBox::stateChanged, [](int state){
			Config::setValue("/Load/Clear", state == Qt::Checked);
		});
		loading->addWidget(load[0]);
		load[1] = new QCheckBox(tr("auto delay after loaded"), page[0]);
		load[1]->setChecked(Config::getValue("/Load/Delay", false));
		connect(load[1], &QCheckBox::stateChanged, [](int state){
			Config::setValue("/Load/Delay", state == Qt::Checked);
		});
		loading->addWidget(load[1]);
		load[2] = new QCheckBox(tr("load local subtitles"), page[0]);
		load[2]->setChecked(Config::getValue("/Player/Subtitle", true));
		connect(load[2], &QCheckBox::stateChanged, [](int state){
			Config::setValue("/Player/Subtitle", state == Qt::Checked);
		});
		loading->addWidget(load[2]);
		load[3] = new QCheckBox(tr("auto play after loaded"), page[0]);
		load[3]->setChecked(Config::getValue("/Player/Immediate", false));
		connect(load[3], &QCheckBox::stateChanged, [](int state){
			Config::setValue("/Player/Immediate", state == Qt::Checked);
		});
		loading->addWidget(load[3]);
		// 启动即续播：免去找目录，双击 exe 直接接着最近一条没看完的播
		load[4] = new QCheckBox(tr("resume on startup"), page[0]);
		load[4]->setChecked(Config::getValue("/Player/AutoResume", true));
		connect(load[4], &QCheckBox::stateChanged, [](int state){
			Config::setValue("/Player/AutoResume", state == Qt::Checked);
		});
		loading->addWidget(load[4]);

		const QString widthTip = tr("%{width} means the width of an danmaku");
		auto look = card(col, tr("Danmaku appearance"));

		// 滑动速度/存活时间支持 %{width} 表达式：可编辑下拉给常用预设，值仍以字符串存储
		play[0] = new QComboBox(page[0]);
		play[0]->setEditable(true);
		play[0]->addItems({ QStringLiteral("150"), QStringLiteral("125+%{width}/5"), QStringLiteral("200"), QStringLiteral("100") });
		play[0]->setCurrentText(Config::getValue("/Danmaku/Speed", QString("150")));
		addRow(look, tr("slide speed"), play[0], widthTip);
		connect(play[0], &QComboBox::currentTextChanged, [](const QString &text){
			Config::setValue("/Danmaku/Speed", text);
		});
		addSep(look);

		play[1] = new QComboBox(page[0]);
		play[1]->setEditable(true);
		play[1]->addItems({ QStringLiteral("5"), QStringLiteral("8"), QStringLiteral("12") });
		play[1]->setCurrentText(Config::getValue("/Danmaku/Life", QString("5")));
		addRow(look, tr("life time"), play[1], widthTip);
		connect(play[1], &QComboBox::currentTextChanged, [](const QString &text){
			Config::setValue("/Danmaku/Life", text);
		});
		addSep(look);

		int state = Config::getValue("/Danmaku/Scale/Fitted", 0x1);
		fitted[0] = new QCheckBox(tr("ordinary"), page[0]);
		fitted[0]->setChecked((state & 0x2) > 0);
		fitted[1] = new QCheckBox(tr("advanced"), page[0]);
		fitted[1]->setChecked((state & 0x1) > 0);
		auto slot = [=](){
			int n = fitted[0]->checkState() == Qt::Checked;
			int a = fitted[1]->checkState() == Qt::Checked;
			Config::setValue("/Danmaku/Scale/Fitted", (n << 1) + a);
		};
		connect(fitted[0], &QCheckBox::stateChanged, slot);
		connect(fitted[1], &QCheckBox::stateChanged, slot);
		{
			auto box = new QWidget(page[0]);
			auto h = new QHBoxLayout(box);
			h->setContentsMargins(0, 0, 0, 0);
			h->setSpacing(12);
			h->addWidget(fitted[0]);
			h->addWidget(fitted[1]);
			h->addStretch(1);
			addRow(look, tr("scale to fitted"), box);
		}
		addSep(look);

		factor = new QDoubleSpinBox(page[0]);
		factor->setRange(0.1, 4.0);
		factor->setSingleStep(0.05);
		factor->setDecimals(2);
		factor->setSuffix(QStringLiteral(" ") + QChar(0xD7));
		factor->setValue(Config::getValue("/Danmaku/Scale/Factor", 1.0));
		addRow(look, tr("scale by factor"), factor);
		connect(factor, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [](double value){
			Config::setValue("/Danmaku/Scale/Factor", value);
		});
		addSep(look);

		int ef = Config::getValue("/Danmaku/Effect", 5);
		bold = new QCheckBox(tr("Bold"), page[0]);
		bold->setChecked(ef & 1);
		connect(bold, &QCheckBox::stateChanged, [=](int s){
			Config::setValue("/Danmaku/Effect", (effect->currentIndex() << 1) | (int)(s == Qt::Checked));
		});
		effect = new QComboBox(page[0]);
		effect->addItem(tr("Stroke"));
		effect->addItem(tr("Projection"));
		effect->addItem(tr("Shadow"));
		effect->setCurrentIndex(ef >> 1);
		connect<void (QComboBox::*)(int)>(effect, &QComboBox::currentIndexChanged, [=](int i){
			Config::setValue("/Danmaku/Effect", (i << 1) | (int)(bold->checkState() == Qt::Checked));
		});
		{
			auto box = new QWidget(page[0]);
			auto h = new QHBoxLayout(box);
			h->setContentsMargins(0, 0, 0, 0);
			h->setSpacing(10);
			h->addWidget(effect);
			h->addWidget(bold);
			h->addStretch(1);
			addRow(look, tr("style"), box);
		}
		addSep(look);

		dmfont = new QComboBox(page[0]);
		dmfont->addItems(Utils::fontFamilies());
		dmfont->setCurrentText(Config::getValue("/Danmaku/Font", QFont().family()));
		connect(dmfont, &QComboBox::currentTextChanged, [](QString _font){
			Config::setValue("/Danmaku/Font", _font);
		});
		dmfont->setMinimumWidth(220);
		addRow(look, tr("font"), dmfont);
		addSep(look);
		QSpinBox *grating = new QSpinBox(page[0]);
		grating->setRange(4, 100);
		grating->setSuffix(QStringLiteral(" px"));
		grating->setValue(Config::getValue("/Danmaku/Grating", 10));
		addRow(look, tr("lane spacing"), grating, tr("Smaller = more danmaku lanes"));
		connect(grating, QOverload<int>::of(&QSpinBox::valueChanged), [](int value){
			Config::setValue("/Danmaku/Grating", value);
		});

		col->addStretch(10);
	}
	//Interface
	{
		stack->addWidget(page[1] = new Page(this));
		auto col = static_cast<Page *>(page[1])->column;

		auto window = card(col, tr("Window"));

		// 初始窗口尺寸：显式"锁定"复选框取代旧的"配置里加尾随空格"暗约定；
		// Home::closeEvent 仍按尾随空格判定锁定，这里负责显式写入该标记
		{
			QString sizeConf = Config::getValue("/Interface/Size", QString("720,405"));
			const bool locked = sizeConf.endsWith(' ');
			QStringList nums = sizeConf.trimmed().split(',', Qt::SkipEmptyParts);
			sizeW = new QSpinBox(page[1]);
			sizeH = new QSpinBox(page[1]);
			sizeW->setRange(240, 7680);
			sizeH->setRange(135, 4320);
			sizeW->setValue(nums.size() > 0 ? qBound(240, nums[0].toInt(), 7680) : 720);
			sizeH->setValue(nums.size() > 1 ? qBound(135, nums[1].toInt(), 4320) : 405);
			sizeLock = new QCheckBox(tr("lock"), page[1]);
			sizeLock->setChecked(locked);
			sizeLock->setToolTip(tr("Locked size survives window close; otherwise the size on close is remembered instead"));
			auto writeSize = [this](){
				QString s = QString("%1,%2").arg(sizeW->value()).arg(sizeH->value());
				if (sizeLock->isChecked()){
					s += ' ';
				}
				Config::setValue("/Interface/Size", s);
			};
			connect(sizeW, QOverload<int>::of(&QSpinBox::valueChanged), writeSize);
			connect(sizeH, QOverload<int>::of(&QSpinBox::valueChanged), writeSize);
			connect(sizeLock, &QCheckBox::toggled, writeSize);
			auto box = new QWidget(page[1]);
			auto h = new QHBoxLayout(box);
			h->setContentsMargins(0, 0, 0, 0);
			h->setSpacing(8);
			h->addWidget(sizeW);
			h->addWidget(new QLabel(QString(QChar(0xD7)), page[1]));
			h->addWidget(sizeH);
			h->addSpacing(8);
			h->addWidget(sizeLock);
			h->addStretch(1);
			addRow(window, tr("initialize size"), box);
		}

		sens = new QCheckBox(tr("toggle pause on video click"), page[1]);
		sens->setChecked(Config::getValue("/Interface/Sensitive", true));
		connect(sens, &QCheckBox::stateChanged, [](int state){
			Config::setValue("/Interface/Sensitive", state == Qt::Checked);
		});
		window->addWidget(sens);

		auto look = card(col, tr("Appearance and language"));

		theme = new QComboBox(page[1]);
		theme->addItems({ tr("light"), tr("dark"), tr("follow system") });
		theme->setCurrentIndex(Config::getValue("/Interface/Theme", 0));
		addRow(look, tr("theme"), theme);
		connect<void (QComboBox::*)(int)>(theme, &QComboBox::currentIndexChanged, [](int i){
			Config::setValue("/Interface/Theme", i);
			Utils::applyTheme(i);
		});
		addSep(look);

		loca = new QComboBox(page[1]);
		loca->addItem("English", QString());
		for (const QFileInfo &info : QDir(Utils::localPath(Utils::Locale)).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)){
			QLocale l(info.baseName());
			QString name = l.name();
			QString text = l.nativeLanguageName();
			loca->addItem(text, name);
			if (name == Config::getValue("/Interface/Locale", QVariant())){
				loca->setCurrentIndex(loca->count() - 1);
			}
		}
		addRow(look, tr("locale"), loca);
		connect<void(QComboBox::*)(int)>(loca, &QComboBox::currentIndexChanged, [this](int i){
			Config::setValue("/Interface/Locale", loca->itemData(i).toString());
		});
		addSep(look);

		font = new QComboBox(page[1]);
		font->addItems(Utils::fontFamilies());
		font->setCurrentText(Config::getValue("/Interface/Font/Family", QFont().family()));
		connect(font, &QComboBox::currentTextChanged, [=](QString _font){
			Config::setValue("/Interface/Font/Family", _font);
		});
		font->setMinimumWidth(220);
		addRow(look, tr("interface font"), font);
		addSep(look);

		reop = new QComboBox(page[1]);
		reop->addItems({ tr("open in new window"), tr("open in current window"), tr("append to playlist") });
		reop->setCurrentIndex(Config::getValue("/Interface/Single", 1));
		addRow(look, tr("reopen action"), reop);
		connect<void (QComboBox::*)(int)>(reop, &QComboBox::currentIndexChanged, [=](int i){
			Config::setValue("/Interface/Single", i);
		});
		addSep(look);

		stay = new QComboBox(page[1]);
		stay->addItems({ tr("never"), tr("playing"), tr("always") });
		stay->setCurrentIndex(Config::getValue("/Interface/Top", 0));
		addRow(look, tr("stay on top"), stay);
		connect<void (QComboBox::*)(int)>(stay, &QComboBox::currentIndexChanged, [=](int i){
			Config::setValue("/Interface/Top", i);
			QMetaObject::invokeMethod(lApp->findObject<Interface>()->widget(), "setWindowFlags");
		});

		col->addStretch(10);
	}
	//Shield
	{
		stack->addWidget(page[2] = new Page(this));
		auto col = static_cast<Page *>(page[2])->column;

		auto kinds = card(col, tr("Block by type"));
		QStringList list;
		list << tr("Top") << tr("Bottom") <<
			tr("Slide") << tr("Reverse") <<
			tr("Guest") << tr("Advanced") <<
			tr("Color") << tr("Whole");
		auto grid = new QGridLayout;
		grid->setContentsMargins(0, 4, 0, 4);
		grid->setHorizontalSpacing(18);
		const QStringList typeTips = {
			tr("Danmaku pinned to the top"), tr("Danmaku pinned to the bottom"),
			tr("Scrolling danmaku"), tr("Reverse scrolling danmaku"),
			tr("Guest danmaku"), tr("Advanced danmaku"),
			tr("Colored danmaku"), tr("All danmaku")
		};
		for (int i = 0; i < 8; ++i){
			check[i] = new QCheckBox(list[i], page[2]);
			check[i]->setChecked(false);
			check[i]->setToolTip(typeTips[i]);
			grid->addWidget(check[i], i / 4, i % 4);
		}
		grid->setColumnStretch(4, 1);
		kinds->addLayout(grid);

		QStringList rl, sl;
		for (const QString &shield : lApp->findObject<Shield>()->getAllShields()){
			if (shield.length() <= 2){
				continue;
			}
			QString content = shield.mid(2);
			switch (shield[0].unicode()){
			case 'm':
			{
				bool ok;
				int i = content.toInt(&ok);
				if (ok&&i >= 0 && i < 8){
					check[i]->setChecked(true);
				}
				break;
			}
			case 't':
				rl.append(content);
				break;
			case 'u':
				sl.append(content);
				break;
			default:
				break;
			}
		}

		auto rules = card(col, tr("Text and user rules"));
		regexp = new List(page[2]);
		sender = new List(page[2]);
		regexp->setSelectionMode(QListView::ExtendedSelection);
		sender->setSelectionMode(QListView::ExtendedSelection);
		regexp->setModel(rm = new QStringListModel(rl, regexp));
		sender->setModel(sm = new QStringListModel(sl, sender));
		// 删除选中行(列内删除按钮与 Del 快捷键共用)
		auto removeSelected = [this](QListView *v){
			QList<int> rows;
			for (const QModelIndex &i : v->selectionModel()->selectedRows()){
				rows.append(i.row());
			}
			std::sort(rows.begin(), rows.end());
			while (!rows.isEmpty()){
				int r = rows.takeFirst();
				v->model()->removeRow(r);
				for (int &i : rows){
					if (i > r){
						--i;
					}
				}
			}
		};
		action[0] = new QAction(tr("Del"), page[2]);
		action[1] = new QAction(tr("Import"), page[2]);
		action[2] = new QAction(tr("Export"), page[2]);
		action[0]->setShortcut(QKeySequence("Del"));
		action[1]->setShortcut(QKeySequence("Ctrl+I"));
		action[2]->setShortcut(QKeySequence("Ctrl+E"));
		connect(action[0], &QAction::triggered, [this, removeSelected](){
			// Del 键作用于焦点所在的列表
			if (regexp->hasFocus()){
				removeSelected(regexp);
			}
			if (sender->hasFocus()){
				removeSelected(sender);
			}
		});
		connect(action[1], &QAction::triggered, [this](){
			QFile file(QFileDialog::getOpenFileName(this, tr("Import File"), QDir::homePath(), tr("Shield files (*.xml *.sol)")));
			if (file.open(QIODevice::ReadOnly | QIODevice::Text)){
				QStringList rl = rm->stringList();
				QStringList sl = sm->stringList();
				{
					QTextStream read(&file);
					read.setCodec("UTF-8");
					QStringList list;
					QString type = QFileInfo(file).suffix();
					QRegularExpression rule;
					rule.setPatternOptions(QRegularExpression::UseUnicodePropertiesOption);
					if (type == "xml"){
						rule.setPattern("[tu]\\=[^\\s<]*");
						QRegularExpressionMatchIterator iter = rule.globalMatch(read.readAll());
						while (iter.hasNext()){
							list.append(Utils::decodeXml(iter.next().captured()));
						}
					}
					if (type == "sol"){
						rule.setPattern("[tu]\\=\\S*");
						QRegularExpressionMatchIterator iter = rule.globalMatch(read.readAll().replace('\x3', ' '));
						while (iter.hasNext()){
							list.append(iter.next().captured());
						}
					}
					for (QString item : list){
						(item[0] == 't' ? rl : sl).append(item.mid(2));
					}
				}
				rl.removeDuplicates();
				sl.removeDuplicates();
				rm->setStringList(rl);
				sm->setStringList(sl);
				regexp->setCurrentIndex(QModelIndex());
				sender->setCurrentIndex(QModelIndex());
			}
			file.close();
		});
		connect(action[2], &QAction::triggered, [this](){
			QString path = QFileDialog::getSaveFileName(this, tr("Export File"), QDir::homePath() + "/shield.bililocal.xml", tr("Shield files (*.xml *.sol)"));
			if (!path.isEmpty()){
				QFile file(path);
				if (!path.endsWith(".sol")){
					file.open(QIODevice::WriteOnly | QIODevice::Text);
					QTextStream stream(&file);
					stream.setCodec("UTF-8");
					stream << "<filters>" << endl;
					for (const QString &iter : rm->stringList()){
						stream << "  <item enabled=\"true\">t=" << iter << "</item>" << endl;
					}
					for (const QString &iter : sm->stringList()){
						stream << "  <item enabled=\"true\">u=" << iter << "</item>" << endl;
					}
					stream << "</filters>";
				}
				else if (file.open(QIODevice::ReadOnly)){
					QByteArray data = file.readAll();
					file.close();
					QString back = path + ".bak";
					QFile::remove(back);
					QFile::rename(path, back);
					file.open(QIODevice::WriteOnly);
					QStringList list;
					for (const QString &iter : rm->stringList()){
						list.append("t=" + iter);
					}
					for (const QString &iter : sm->stringList()){
						list.append("u=" + iter);
					}
					auto encode = [](quint64 n){
						QByteArray data;
						while (n >= 128){
							data.append((n & 0x127) | 128);
							n = n >> 7;
						}
						data.append(n);
						return data;
					};
					QByteArray buff;
					buff += "\x09list\x09" + encode(list.size() * 2 + 1) + "\x01";
					for (const QString &iter : list){
						QByteArray text = iter.toUtf8();
						buff += "\x09\x05\x01\x06" + encode(text.length() * 2 + 1) + text + "\x03";
					}
					buff += QByteArray("\x01\x00", 2);
					int pos = data.indexOf("\x09list\x09");
					int len = data.indexOf('\x00', pos) - pos + 1;
					data.replace(pos, len, buff);
					int all = data.length() - 6;
					QByteArray size;
					for (int i = 0; i < 4; ++i){
						size.prepend(all & 0xFF);
						all = all >> 8;
					}
					data.replace(2, 4, size);
					file.write(data);
				}
				else{
					QMessageBox::warning(this, tr("Warning"), tr("Please select an existing sol file."));
				}
			}
		});
		for (QAction *iter : action){
			page[2]->addAction(iter);
		}
		// 双列各自编辑器:输入框/添加/删除/列表同列,目标一目了然——
		// 不再绕"下拉选类型 + 共享输入框 + 焦点决定删除对象"的三段式
		auto makeColumn = [&](const QString &title, QStringListModel *model, QListView *list){
			QLineEdit *edit = new QLineEdit(page[2]);
			auto box = new QWidget(page[2]);
			auto v = new QVBoxLayout(box);
			v->setContentsMargins(0, 0, 0, 0);
			v->setSpacing(6);
			v->addWidget(new QLabel(title, page[2]));
			auto bar = new QWidget(box);
			auto h = new QHBoxLayout(bar);
			h->setContentsMargins(0, 0, 0, 0);
			h->setSpacing(6);
			h->addWidget(edit, 1);
			auto addBtn = new QPushButton(tr("Add"), page[2]);
			auto delBtn = new QPushButton(tr("Del"), page[2]);
			addBtn->setFocusPolicy(Qt::NoFocus);
			delBtn->setFocusPolicy(Qt::NoFocus);
			h->addWidget(addBtn);
			h->addWidget(delBtn);
			v->addWidget(bar);
			v->addWidget(list, 1);
			auto addEntry = [edit, model](){
				if (!edit->text().isEmpty()){
					model->insertRow(model->rowCount());
					model->setData(model->index(model->rowCount() - 1), edit->text());
					edit->clear();
				}
				else{
					edit->setFocus();
				}
			};
			connect(addBtn, &QPushButton::clicked, box, addEntry);
			connect(edit, &QLineEdit::returnPressed, box, addEntry);
			connect(delBtn, &QPushButton::clicked, box, [list, removeSelected](){
				removeSelected(list);
			});
			return box;
		};
		auto columns = new QWidget(page[2]);
		auto g = new QHBoxLayout(columns);
		g->setContentsMargins(0, 4, 0, 4);
		g->setSpacing(10);
		g->addWidget(makeColumn(tr("Text"), rm, regexp), 1);
		g->addWidget(makeColumn(tr("User"), sm, sender), 1);
		rules->addWidget(columns, 1);
		auto ioBar = new QWidget(page[2]);
		auto io = new QHBoxLayout(ioBar);
		io->setContentsMargins(0, 4, 0, 0);
		io->addStretch(1);
		auto importBtn = new QPushButton(tr("Import"), page[2]);
		auto exportBtn = new QPushButton(tr("Export"), page[2]);
		importBtn->setFocusPolicy(Qt::NoFocus);
		exportBtn->setFocusPolicy(Qt::NoFocus);
		connect(importBtn, &QPushButton::clicked, action[1], &QAction::trigger);
		connect(exportBtn, &QPushButton::clicked, action[2], &QAction::trigger);
		io->addWidget(importBtn);
		io->addWidget(exportBtn);
		io->addStretch(1);
		rules->addWidget(ioBar);
		// 列表右键只挂导入/导出(列表无关);删除走列内按钮或 Del 键
		regexp->addActions({action[1], action[2]});
		sender->addActions({action[1], action[2]});
		regexp->setContextMenuPolicy(Qt::ActionsContextMenu);
		sender->setContextMenuPolicy(Qt::ActionsContextMenu);

		auto density = card(col, tr("Density limits"));
		same = new QSlider(Qt::Horizontal, page[2]);
		same->setRange(0, 40);
		same->setValue(Config::getValue("/Shield/Amount/Repeat", 5));
		same->setMinimumWidth(190);
		addRow(density, tr("limit of the same"), same, tr("0 means disabled"));
		connect(same, &QSlider::valueChanged, [=](int value){
			Config::setValue("/Shield/Amount/Repeat", value);
			QPoint p;
			p.setX(QCursor::pos().x());
			p.setY(same->mapToGlobal(same->rect().center()).y());
			QToolTip::showText(p, QString::number(value));
		});
		addSep(density);

		limit = new QSpinBox(page[2]);
		limit->setRange(0, 2000);
		limit->setValue(Config::getValue("/Shield/Amount/Screen", 0));
		limit->setSpecialValueText(tr("unlimited"));
		addRow(density, tr("limit of density"), limit, tr("0 means disabled"));
		connect(limit, QOverload<int>::of(&QSpinBox::valueChanged), [](int value){
			Config::setValue("/Shield/Amount/Screen", value);
		});

		col->addStretch(10);
	}
	//Shortcut
	{
		stack->addWidget(page[3] = new Page(this));
		auto col = static_cast<Page *>(page[3])->column;

		auto keys = card(col, tr("Shortcuts"), tr("Click the key column to record a new shortcut"));
		hotkey = new QTreeWidget(page[3]);
		hotkey->setSelectionMode(QAbstractItemView::NoSelection);
		hotkey->setColumnCount(2);
		hotkey->header()->hide();
		hotkey->header()->setStretchLastSection(false);
		hotkey->header()->setSectionResizeMode(0, QHeaderView::Stretch);
		hotkey->setColumnWidth(1, 1.2*logicalDpiX());
		hotkey->setEditTriggers(QAbstractItemView::NoEditTriggers);
		for (QAction *iter : lApp->findObject<Interface>()->widget()->findChildren<QAction *>(QRegularExpression(".{4}"))){
			QTreeWidgetItem *item = new QTreeWidgetItem;
			item->setData(0, Qt::DisplayRole, iter->text());
			item->setData(1, Qt::DisplayRole, iter->shortcut().toString());
			item->setData(1, Qt::UserRole, iter->objectName());
			item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled);
			item->setSizeHint(0, QSize(60, 25 * logicalDpiY() / 72));
			hotkey->addTopLevelItem(item);
		}
		keys->addWidget(hotkey);
		connect(hotkey, &QTreeWidget::currentItemChanged, [this](){
			hotkey->setCurrentItem(nullptr);
		});
		// 点击快捷键列弹出按键捕获框：按下即录（QKeySequenceEdit），
		// 代替旧版"手打 Ctrl+X 文本再回车"
		connect(hotkey, &QTreeWidget::itemClicked, [=](QTreeWidgetItem *item, int column){
			if (column != 1){
				return;
			}
			const QVariant v = item->data(1, Qt::UserRole);
			if (!v.isValid()){
				return;
			}
			QAction *a = lApp->findObject<Interface>()->widget()->findChild<QAction *>(v.toString());
			if (!a){
				return;
			}
			QDialog dlg(this);
			dlg.setWindowTitle(tr("Shortcut: %1").arg(a->text()));
			auto box = new QVBoxLayout(&dlg);
			auto edit = new QKeySequenceEdit(&dlg);
			edit->setKeySequence(a->shortcut());
			box->addWidget(edit);
			auto btn = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, &dlg);
			box->addWidget(btn);
			connect(btn, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
			connect(btn, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
			connect(btn->button(QDialogButtonBox::Reset), &QPushButton::clicked, edit, &QKeySequenceEdit::clear);
			dlg.exec();
			const QKeySequence ns = edit->keySequence();
			if (ns != a->shortcut()){
				const QString text = ns.toString(QKeySequence::NativeText);
				a->setShortcut(text);
				item->setText(1, text);
				Config::setValue("/Shortcut/" + a->objectName(), text);
			}
		});

		col->addStretch(10);
	}
	//About（Thanks + License 合并成一页）
	{
		stack->addWidget(page[4] = new Page(this));
		auto col = static_cast<Page *>(page[4])->column;

		auto thanksCard = card(col, tr("Thanks"));
		QFile t(":/Text/THANKS");
		t.open(QIODevice::ReadOnly | QIODevice::Text);
		thanks = new QTextEdit(page[4]);
		thanks->setReadOnly(true);
		thanks->setText(t.readAll());
		thanks->setMinimumHeight(160 * logicalDpiY() / 96);
		thanksCard->addWidget(thanks);

		auto licenseCard = card(col, tr("License"));
		QFile l(":/Text/COPYING");
		l.open(QIODevice::ReadOnly | QIODevice::Text);
		license = new QTextEdit(page[4]);
		license->setReadOnly(true);
		license->setText(l.readAll());
		licenseCard->addWidget(license);

		auto buildCard = card(col, tr("This build"));
		QLabel *buildInfo = new QLabel(page[4]);
		buildInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
		buildInfo->setText(tr("Built: %1\nQt %2\nlibVLC %3")
			.arg(QStringLiteral(__DATE__))
			.arg(QT_VERSION_STR)
			.arg(QString::fromLatin1(libvlc_get_version())));
		buildCard->addWidget(buildInfo);

		col->addStretch(10);
	}

	const QString titles[5] = { tr("Playing"), tr("Interface"), tr("Shield"), tr("Shortcut"), tr("About") };
	for (int i = 0; i < 5; ++i){
		auto item = new QListWidgetItem(navIcon(i), titles[i], nav);
		item->setSizeHint(QSize(-1, 38 * logicalDpiY() / 96));
	}
	connect(nav, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);
	nav->setCurrentRow(0);

	connect(this, &QDialog::finished, [this](){
		Danmaku *d = lApp->findObject<Danmaku>();
		APlayer *p = lApp->findObject<APlayer>();
		if (reparse != getReparse()){
			QStringList shields;
			for (int i = 0; i < 8; ++i){
				if (check[i]->isChecked()){
					shields.append(QString("m=%1").arg(i));
				}
			}
			for (const QString &regexp : rm->stringList()){
				shields.append("t=" + regexp);
			}
			for (const QString &sender : sm->stringList()){
				shields.append("u=" + sender);
			}
			lApp->findObject<Shield>()->setAllShields(shields);
			d->parse(Danmaku::Block);
		}
		if (restart != getRestart() && ((p->getState() == APlayer::Stop && d->rowCount() == 0) || QMessageBox::warning(
			this,
			tr("Warning"),
			tr("Restart to apply changes?"),
			QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)){
			qApp->exit(12450);
			return;
		}
		Config::save();
	});

	setMinimumSize(560 * logicalDpiX() / 96, 460 * logicalDpiY() / 96);
	resize(680 * logicalDpiX() / 96, 520 * logicalDpiY() / 96);
	Utils::setCenter(this);

	restart = getRestart();
	reparse = getReparse();

#ifdef BILILOCAL_EMBED_TEST
	// 自动化验证钩子（与 Home/EWindow 的钩子同一编译开关）：BILILOCAL_UI_TEST
	// 触发逐页巡览，页码写进窗口标题，外部脚本按标题抓帧——外部投递的鼠标
	// 点击会被 Qt 按真实光标位置路由，对模态对话框不可靠。正常使用零影响。
	if (qEnvironmentVariableIsSet("BILILOCAL_UI_TEST")){
		auto tour = new QTimer(this);
		tour->setInterval(1200);
		connect(tour, &QTimer::timeout, this, [this, tour]() mutable {
			static int step = 0;	// ctor 局部变量按引用捕获会悬空，用 static
			if (step > 4){
				tour->stop();
				accept();
				return;
			}
			nav->setCurrentRow(step);
			setWindowTitle(tr("Config") + QStringLiteral(" - %1").arg(step));
			++step;
		});
		tour->start();
	}
#endif
}
