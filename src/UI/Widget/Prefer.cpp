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
#include "../../Plugin.h"
#include "../../Utils.h"
#include "../../Model/Danmaku.h"
#include "../../Model/Shield.h"
#include "../../Player/APlayer.h"
#include "../../Render/ARender.h"

using namespace UI;

void Prefer::exec(QWidget *parent, int index)
{
	static Prefer *executing;
	if (!executing){
		Prefer prefer(parent);
		prefer.tab->setCurrentIndex(index);
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
		"/Interface/Font" <<
		"/Interface/Frameless" <<
		"/Interface/Single" <<
		"/Interface/Version" <<
		"/Interface/Locale" <<
		"/Plugin";
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
}

Prefer::Prefer(QWidget *parent) :
QDialog(parent)
{
	setWindowTitle(tr("Config"));
	auto outer = new QGridLayout(this);
	tab = new QTabWidget(this);
	connect(tab, &QTabWidget::currentChanged, [this](int index){
		tab->widget(index)->setFocus();
	});
	outer->addWidget(tab);
	//Playing
	{
		widget[0] = new QWidget(this);
		auto list = new QVBoxLayout(widget[0]);
		auto c = new QGridLayout;
		load[0] = new QCheckBox(tr("clear when reloading"), widget[0]);
		load[0]->setChecked(Config::getValue("/Load/Clear", false));
		connect(load[0], &QCheckBox::stateChanged, [](int state){
			Config::setValue("/Load/Clear", state == Qt::Checked);
		});
		c->addWidget(load[0], 0, 0);
		load[1] = new QCheckBox(tr("auto delay after loaded"), widget[0]);
		load[1]->setChecked(Config::getValue("/Load/Delay", false));
		connect(load[1], &QCheckBox::stateChanged, [](int state){
			Config::setValue("/Load/Delay", state == Qt::Checked);
		});
		c->addWidget(load[1], 0, 1);
		load[2] = new QCheckBox(tr("load local subtitles"), widget[0]);
		load[2]->setChecked(Config::getValue("/Player/Subtitle", true));
		connect(load[2], &QCheckBox::stateChanged, [](int state){
			Config::setValue("/Player/Subtitle", state == Qt::Checked);
		});
		c->addWidget(load[2], 1, 0);
		load[3] = new QCheckBox(tr("auto play after loaded"), widget[0]);
		load[3]->setChecked(Config::getValue("/Player/Immediate", false));
		connect(load[3], &QCheckBox::stateChanged, [](int state){
			Config::setValue("/Player/Immediate", state == Qt::Checked);
		});
		c->addWidget(load[3], 1, 1);
		box[0] = new QGroupBox(tr("loading"), widget[0]);
		box[0]->setLayout(c);
		list->addWidget(box[0]);

		auto s = new QHBoxLayout;
		play[0] = new QLineEdit(widget[0]);
		play[0]->setText(Config::getValue("/Danmaku/Speed", QString("150")));
		connect(play[0], &QLineEdit::editingFinished, [=](){
			Config::setValue("/Danmaku/Speed", play[0]->text());
		});
		s->addWidget(play[0]);
		box[1] = new QGroupBox(tr("slide speed"), widget[0]);
		box[1]->setToolTip(tr("%{width} means the width of an danmaku"));
		box[1]->setLayout(s);
		list->addWidget(box[1]);

		auto l = new QHBoxLayout;
		play[1] = new QLineEdit(widget[0]);
		play[1]->setText(Config::getValue("/Danmaku/Life", QString("5")));
		connect(play[1], &QLineEdit::editingFinished, [=](){
			Config::setValue("/Danmaku/Life", play[1]->text());
		});
		l->addWidget(play[1]);
		box[2] = new QGroupBox(tr("life time"), widget[0]);
		box[2]->setToolTip(tr("%{width} means the width of an danmaku"));
		box[2]->setLayout(l);
		list->addWidget(box[2]);

		auto e = new QHBoxLayout;
		int state = Config::getValue("/Danmaku/Scale/Fitted", 0x1);
		fitted[0] = new QCheckBox(tr("ordinary"), widget[0]);
		fitted[0]->setChecked((state & 0x2) > 0);
		fitted[1] = new QCheckBox(tr("advanced"), widget[0]);
		fitted[1]->setChecked((state & 0x1) > 0);
		auto slot = [=](){
			int n = fitted[0]->checkState() == Qt::Checked;
			int a = fitted[1]->checkState() == Qt::Checked;
			Config::setValue("/Danmaku/Scale/Fitted", (n << 1) + a);
		};
		connect(fitted[0], &QCheckBox::stateChanged, slot);
		connect(fitted[1], &QCheckBox::stateChanged, slot);
		e->addWidget(fitted[0], 1);
		e->addWidget(fitted[1], 1);
		box[3] = new QGroupBox(tr("scale to fitted"), widget[0]);
		box[3]->setLayout(e);

		auto a = new QHBoxLayout;
		factor = new QLineEdit(widget[0]);
		factor->setText(QString::number(Config::getValue("/Danmaku/Scale/Factor", 1.0), 'f', 2));
		connect(factor, &QLineEdit::editingFinished, [=](){
			Config::setValue("/Danmaku/Scale/Factor", factor->text().toDouble());
		});
		a->addWidget(factor);
		box[4] = new QGroupBox(tr("scale by factor"), widget[0]);
		box[4]->setLayout(a);

		auto o = new QHBoxLayout;
		o->addWidget(box[3], 1);
		o->addWidget(box[4], 1);
		list->addLayout(o);

		auto g = new QHBoxLayout;
		int ef = Config::getValue("/Danmaku/Effect", 5);
		bold = new QCheckBox(tr("Bold"), widget[0]);
		bold->setChecked(ef & 1);
		connect(bold, &QCheckBox::stateChanged, [=](int s){
			Config::setValue("/Danmaku/Effect", (effect->currentIndex() << 1) | (int)(s == Qt::Checked));
		});
		g->addWidget(bold);
		effect = new QComboBox(widget[0]);
		effect->addItem(tr("Stroke"));
		effect->addItem(tr("Projection"));
		effect->addItem(tr("Shadow"));
		effect->setCurrentIndex(ef >> 1);
		connect<void (QComboBox::*)(int)>(effect, &QComboBox::currentIndexChanged, [=](int i){
			Config::setValue("/Danmaku/Effect", (i << 1) | (int)(bold->checkState() == Qt::Checked));
		});
		g->addWidget(effect);
		box[5] = new QGroupBox(tr("style"), widget[0]);
		box[5]->setLayout(g);

		auto f = new QHBoxLayout;
		dmfont = new QComboBox(widget[0]);
		dmfont->addItems(QFontDatabase().families());
		dmfont->setCurrentText(Config::getValue("/Danmaku/Font", QFont().family()));
		connect(dmfont, &QComboBox::currentTextChanged, [this](QString _font){
			Config::setValue("/Danmaku/Font", _font);
		});
		f->addWidget(dmfont);
		box[6] = new QGroupBox(tr("font"), widget[0]);
		box[6]->setLayout(f);

		auto v = new QHBoxLayout;
		v->addWidget(box[5], 1);
		v->addWidget(box[6], 1);
		list->addLayout(v);

		list->addStretch(10);
		tab->addTab(widget[0], tr("Playing"));
	}
	//Interface
	{
		widget[1] = new QWidget(this);
		auto list = new QVBoxLayout(widget[1]);

		auto s = new QHBoxLayout;
		size = new QLineEdit(widget[1]);
		size->setText(Config::getValue("/Interface/Size", QString("720,405")).trimmed());
		connect(size, &QLineEdit::editingFinished, [=](){
			QRegularExpression r("\\D");
			r.setPatternOptions(QRegularExpression::UseUnicodePropertiesOption);
			QString s = size->text().trimmed();
			s.replace(r, ",");
			size->setText(s);
			Config::setValue("/Interface/Size", s + " ");
		});
		s->addWidget(size);
		ui[0] = new QGroupBox(tr("initialize size"), widget[1]);
		ui[0]->setLayout(s);

		auto j = new QHBoxLayout;
		jump = new QLineEdit(widget[1]);
		jump->setText(QString::number(Config::getValue("/Interface/Interval", 10), 'f', 2));
		connect(jump, &QLineEdit::editingFinished, [=](){
			Config::setValue("/Interface/Interval", jump->text().toDouble());
		});
		j->addWidget(jump);
		ui[1] = new QGroupBox(tr("skip time"), widget[1]);
		ui[1]->setLayout(j);

		auto q = new QHBoxLayout;
		q->addWidget(ui[0]);
		q->addWidget(ui[1]);
		list->addLayout(q);

		auto t = new QGridLayout;
		less = new QCheckBox(tr("frameless"), widget[1]);
		less->setChecked(Config::getValue("/Interface/Frameless", false));
		connect(less, &QCheckBox::stateChanged, [](int state){
			Config::setValue("/Interface/Frameless", state == Qt::Checked);
		});
		t->addWidget(less, 0, 1);
		vers = new QCheckBox(tr("version information"), widget[1]);
		vers->setChecked(Config::getValue("/Interface/Version", true));
		connect(vers, &QCheckBox::stateChanged, [](int state){
			Config::setValue("/Interface/Version", state == Qt::Checked);
		});
		t->addWidget(vers, 0, 0);
		sens = new QCheckBox(tr("sensitive pausing"), widget[1]);
		sens->setChecked(Config::getValue("/Interface/Sensitive", false));
		connect(sens, &QCheckBox::stateChanged, [](int state){
			Config::setValue("/Interface/Sensitive", state == Qt::Checked);
		});
		t->addWidget(sens, 1, 0);
		ui[2] = new QGroupBox(tr("window flag"), widget[1]);
		ui[2]->setLayout(t);
		list->addWidget(ui[2]);

		auto f = new QHBoxLayout;
		font = new QComboBox(widget[1]);
		font->addItems(QFontDatabase().families());
		font->setCurrentText(Config::getValue("/Interface/Font/Family", QFont().family()));
		connect(font, &QComboBox::currentTextChanged, [=](QString _font){
			Config::setValue("/Interface/Font/Family", _font);
		});
		f->addWidget(font);
		ui[3] = new QGroupBox(tr("interface font"), widget[1]);
		ui[3]->setLayout(f);

		auto r = new QHBoxLayout;
		reop = new QComboBox(widget[1]);
		reop->addItems({ tr("open in new window"), tr("open in current window"), tr("append to playlist") });
		reop->setCurrentIndex(Config::getValue("/Interface/Single", 1));
		connect<void (QComboBox::*)(int)>(reop, &QComboBox::currentIndexChanged, [=](int i){
			Config::setValue("/Interface/Single", i);
		});
		r->addWidget(reop);
		ui[4] = new QGroupBox(tr("reopen action"), widget[1]);
		ui[4]->setLayout(r);

		auto v = new QHBoxLayout;
		v->addWidget(ui[3], 1);
		v->addWidget(ui[4], 1);
		list->addLayout(v);

		auto l = new QHBoxLayout;
		loca = new QComboBox(widget[1]);
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
		connect<void(QComboBox::*)(int)>(loca, &QComboBox::currentIndexChanged, [this](int i){
			Config::setValue("/Interface/Locale", loca->itemData(i).toString());
		});
		l->addWidget(loca);
		ui[5] = new QGroupBox(tr("locale"));
		ui[5]->setLayout(l);

		auto m = new QHBoxLayout;
		stay = new QComboBox(widget[1]);
		stay->addItems({ tr("never"), tr("playing"), tr("always") });
		stay->setCurrentIndex(Config::getValue("/Interface/Top", 0));
		connect<void (QComboBox::*)(int)>(stay, &QComboBox::currentIndexChanged, [this](int i){
			Config::setValue("/Interface/Top", i);
			QMetaObject::invokeMethod(lApp->findObject<Interface>()->widget(), "setWindowFlags");
		});
		m->addWidget(stay);
		ui[6] = new QGroupBox(tr("stay on top"), widget[1]);
		ui[6]->setLayout(m);

		auto th = new QHBoxLayout;
		theme = new QComboBox(widget[1]);
		theme->addItems({ tr("light"), tr("dark"), tr("follow system") });
		theme->setCurrentIndex(Config::getValue("/Interface/Theme", 0));
		connect<void (QComboBox::*)(int)>(theme, &QComboBox::currentIndexChanged, [](int i){
			Config::setValue("/Interface/Theme", i);
			Utils::applyTheme(i);
		});
		th->addWidget(theme);
		ui[7] = new QGroupBox(tr("theme"), widget[1]);
		ui[7]->setLayout(th);

		auto n = new QHBoxLayout;
		n->addWidget(ui[5]);
		n->addWidget(ui[6]);
		list->addLayout(n);
		auto n2 = new QHBoxLayout;
		n2->addWidget(ui[7]);
		list->addLayout(n2);

		list->addStretch(10);
		tab->addTab(widget[1], tr("Interface"));
	}
	//Shield
	{
		widget[2] = new QWidget(this);
		QStringList list;
		list << tr("Top") << tr("Bottom") <<
			tr("Slide") << tr("Reverse") <<
			tr("Guest") << tr("Advanced") <<
			tr("Color") << tr("Whole");
		auto grid = new QGridLayout(widget[2]);

		auto g = new QHBoxLayout;
		for (int i = 0; i < 8; ++i){
			check[i] = new QCheckBox(list[i], widget[2]);
			check[i]->setFixedHeight(40);
			check[i]->setChecked(false);
			g->addWidget(check[i]);
		}
		grid->addLayout(g, 0, 0, 1, 4);

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

		type = new QComboBox(widget[2]);
		type->addItem(tr("Text"));
		type->addItem(tr("User"));
		edit = new QLineEdit(widget[2]);
		regexp = new List(widget[2]);
		sender = new List(widget[2]);
		regexp->setSelectionMode(QListView::ExtendedSelection);
		sender->setSelectionMode(QListView::ExtendedSelection);
		regexp->setModel(rm = new QStringListModel(rl, regexp));
		sender->setModel(sm = new QStringListModel(sl, sender));
		action[0] = new QAction(tr("Add"), widget[2]);
		action[1] = new QAction(tr("Del"), widget[2]);
		action[2] = new QAction(tr("Import"), widget[2]);
		action[3] = new QAction(tr("Export"), widget[2]);
		action[1]->setShortcut(QKeySequence("Del"));
		action[2]->setShortcut(QKeySequence("Ctrl+I"));
		action[3]->setShortcut(QKeySequence("Ctrl+E"));
		connect(action[0], &QAction::triggered, [this](){
			if (!edit->text().isEmpty()){
				QStringListModel *m = type->currentIndex() == 0 ? rm : sm;
				m->insertRow(m->rowCount());
				m->setData(m->index(m->rowCount() - 1), edit->text());
				edit->clear();
			}
			else{
				edit->setFocus();
			}
		});
		connect(action[1], &QAction::triggered, [this](){
			auto remove = [this](QListView *v){
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
			if (regexp->hasFocus()){
				remove(regexp);
			}
			if (sender->hasFocus()){
				remove(sender);
			}
		});
		connect(action[2], &QAction::triggered, [this](){
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
		connect(action[3], &QAction::triggered, [this](){
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
			widget[2]->addAction(iter);
		}
		button[0] = new QPushButton(tr("Add"), widget[2]);
		button[1] = new QPushButton(tr("Del"), widget[2]);
		button[0]->setFixedWidth(60 * logicalDpiX() / 96);
		button[1]->setFixedWidth(60 * logicalDpiX() / 96);
		button[0]->setFocusPolicy(Qt::NoFocus);
		button[1]->setFocusPolicy(Qt::NoFocus);
		connect(button[0], &QPushButton::clicked, action[0], &QAction::trigger);
		connect(button[1], &QPushButton::clicked, action[1], &QAction::trigger);
		regexp->addActions(widget[2]->actions());
		sender->addActions(widget[2]->actions());
		regexp->setContextMenuPolicy(Qt::ActionsContextMenu);
		sender->setContextMenuPolicy(Qt::ActionsContextMenu);

		grid->addWidget(type, 1, 0);
		grid->addWidget(edit, 1, 1);
		grid->addWidget(button[0], 1, 2);
		grid->addWidget(button[1], 1, 3);
		grid->addWidget(regexp, 2, 0, 1, 2);
		grid->addWidget(sender, 2, 2, 1, 2);

		same = new QSlider(Qt::Horizontal, widget[2]);
		same->setRange(0, 40);
		same->setValue(Config::getValue("/Shield/Amount/Repeat", 5));
		connect(same, &QSlider::valueChanged, [=](int value){
			Config::setValue("/Shield/Amount/Repeat", value);
			QPoint p;
			p.setX(QCursor::pos().x());
			p.setY(same->mapToGlobal(same->rect().center()).y());
			QToolTip::showText(p, QString::number(value));
		});
		auto a = new QHBoxLayout;
		a->addWidget(same);
		label[0] = new QGroupBox(tr("limit of the same"), widget[2]);
		label[0]->setToolTip(tr("0 means disabled"));
		label[0]->setLayout(a);
		grid->addWidget(label[0], 3, 0, 1, 2);

		limit = new QLineEdit(widget[2]);
		limit->setText(QString::number(Config::getValue("/Shield/Amount/Screen", 0)));
		connect(limit, &QLineEdit::editingFinished, [=](){
			Config::setValue("/Shield/Amount/Screen", limit->text().toInt());
		});
		auto m = new QHBoxLayout;
		m->addWidget(limit);
		label[1] = new QGroupBox(tr("limit of density"), widget[2]);
		label[1]->setToolTip(tr("0 means disabled"));
		label[1]->setLayout(m);
		grid->addWidget(label[1], 3, 2, 1, 2);

		tab->addTab(widget[2], tr("Shield"));
	}
	//Shortcut
		{
			widget[3] = new QWidget(this);
			auto w = new QGridLayout(widget[3]);
			hotkey = new QTreeWidget(widget[3]);
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
			w->addWidget(hotkey);
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
			tab->addTab(widget[3], tr("Shortcut"));
		}
		//Thanks
		{
			widget[4] = new QWidget(this);
			auto w = new QGridLayout(widget[4]);
			QFile t(":/Text/THANKS");
			t.open(QIODevice::ReadOnly | QIODevice::Text);
			thanks = new QTextEdit(widget[4]);
			thanks->setReadOnly(true);
			thanks->setText(t.readAll());
			w->addWidget(thanks);
			tab->addTab(widget[4], tr("Thanks"));
		}
		//License
		{
			widget[5] = new QWidget(this);
			auto w = new QGridLayout(widget[5]);
			QFile l(":/Text/COPYING");
			l.open(QIODevice::ReadOnly | QIODevice::Text);
			license = new QTextEdit(widget[5]);
			license->setReadOnly(true);
			license->setText(l.readAll());
			w->addWidget(license);
			tab->addTab(widget[5], tr("License"));
		}
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
		int h = outer->minimumSize().height(), w = 1.15*h;
		setMinimumSize(w, h);
		Utils::setCenter(this);

		restart = getRestart();
		reparse = getReparse();
}