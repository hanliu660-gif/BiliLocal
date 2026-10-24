#include "Common.h"
#include "Parse.h"
#include "../Local.h"
#include <QtConcurrent>
#include <cstdlib>

namespace
{
	QTextCodec *codeForData(const QByteArray &data)
	{
		QTextCodec *code = QTextCodec::codecForUtfText(data, nullptr);
		if (code) {
			return code;
		}
		QByteArray name;
		QByteArray head = data.left(512).toLower();
		if (head.startsWith("<?xml")) {
			int pos = head.indexOf("encoding=");
			if (pos >= 0) {
				pos += 9;
				if (pos < head.size()) {
					auto c = head.at(pos);
					if ('\"' == c || '\'' == c) {
						++pos;
						name = head.mid(pos, head.indexOf(c, pos) - pos);
					}
				}
			}
		}
		else {
			int pos = head.indexOf("charset=", head.indexOf("meta "));
			if (pos >= 0) {
				pos += 8;
				int end = pos;
				while (++end < head.size()) {
					auto c = head.at(end);
					if (c == '\"' || c == '\'' || c == '>') {
						name = head.mid(pos, end - pos);
						break;
					}
				}
			}
		}
		code = QTextCodec::codecForName(name);
		if (code) {
			return code;
		}
		return QTextCodec::codecForLocale();
	}

	QString decodeBytes(const QByteArray &data)
	{
		return codeForData(data)->toUnicode(data);
	}

	using namespace Parse;

	typedef ResultDelegate::Finish Finish;
	typedef ResultDelegate::Record Record;
	typedef ResultDelegate::Result Result;

	class VectorRecord : public Record
	{
	public:
		Result data;

		explicit VectorRecord(const Result &data)
			: data(data)
		{
		}

		virtual void onFinish(Finish cb) override
		{
			cb(Result(data));
		}

		virtual Result get() override
		{
			return data;
		}
	};
}

Parse::ResultDelegate Parse::parseComment(const QByteArray &data, Utils::Site site)
{
	switch (site) {
	case Utils::Bilibili:
	case Utils::TuCao:
	{
		Result result = [data]() {
			typedef QPair<const char *, const char *> RawChar;
			QVector<RawChar> raws;
			const char *dat = data.constData();
			QByteArrayMatcher match("<d p=");
			int sta = match.indexIn(data);
			if (sta == -1) {
				return QVector<Comment>();
			}
			for (;;) {
				auto sub = dat + sta;
				sta = match.indexIn(data, sta + 5);
				if (sta == -1) {
					raws.append(qMakePair(sub, dat + data.size()));
					break;
				}
				else {
					raws.append(qMakePair(sub, dat + sta));
				}
			}
			auto codec = codeForData(data);
			auto map = [codec](const RawChar &raw) {
				Comment comment;
				//strtof/strtoi need char * ?!
				char *arg = const_cast<char *>(raw.first) + 6;
				int time = std::strtod(arg, &arg) * 1000 + 0.5;
				if (*(arg++) != ',') return comment;
				int mode = std::strtol(arg, &arg, 10);
				if (*(arg++) != ',') return comment;
				int font = std::strtol(arg, &arg, 10);
				if (*(arg++) != ',') return comment;
				int colo = std::strtol(arg, &arg, 10);
				if (*(arg++) != ',') return comment;
				// 录播姬 2.x 写的是毫秒时间戳(13 位),旧 B 站格式是秒(10 位)。
				// 用 32 位 strtol 读毫秒会饱和到 INT32_MAX(=2038-01-19)。
				long long date = std::strtoll(arg, &arg, 10);
				if (date > 10000000000LL) {
					date /= 1000;	// 阈值 1e11 秒:秒级时间戳永达不到,毫秒级必超过
				}
				comment.mode = mode;
				comment.font = font;
				comment.color = colo;
				comment.time = time;
				comment.date = date;
				auto num = 4;
				auto end = *(raw.first + 5);
				auto lst = arg;
				for (; arg < raw.second && *arg != end; ++arg) {
					if (',' == *arg) {
						if (7 == (++num)) {
							comment.sender = QLatin1String(lst, arg - lst);
						}
						lst = arg + 1;
					}
				}
				for (; arg < raw.second && *arg != '>'; ++arg);
				lst = arg + 1;
				for (; arg < raw.second && *arg != '<'; ++arg);
				comment.string = Utils::decodeXml(codec->toUnicode(lst, arg - lst), true);
				return comment;
			};
			auto reduce = [](QVector<Comment> &list, const Comment &comment) {
				if (comment.isEmpty() == false) {
					list.append(comment);
				}
			};
			qThreadPool->reserveThread();
			const auto &result = QtConcurrent::blockingMappedReduced<
				QVector<Comment>,
				QVector<RawChar>,
				std::function<Comment(const RawChar &)>,
				std::function<void(QVector<Comment> &, const Comment &)>>
				(raws, map, reduce, QtConcurrent::OrderedReduce | QtConcurrent::SequentialReduce);
			qThreadPool->releaseThread();
			return result;
		}();
		return ResultDelegate(new VectorRecord(result));

	}
	case Utils::AcFun:
	{
		Result result = [data]() {
			QQueue <QJsonArray> wait;
			wait.append(QJsonDocument::fromJson(data).array());
			QVector<QJsonValue> raws;
			while (!wait.isEmpty()) {
				const QJsonArray &head = wait.takeFirst();
				raws.reserve(raws.size() + head.size());
				for (const QJsonValue &item : head) {
					if (item.isArray()) {
						wait.append(item.toArray());
					}
					else {
						raws.append(item);
					}
				}
			}
			auto map = [](const QJsonValue &item) {
				QJsonObject o = item.toObject();
				const QString &c = o["c"].toString();
				const QStringList args = c.split(',');
				Comment comment;
				if (args.size() > 5) {
					comment.time = args[0].toDouble() * 1000 + 0.5;
					comment.color = args[1].toInt();
					comment.mode = args[2].toInt();
					comment.font = args[3].toInt();
					comment.sender = args[4];
					comment.date = args[5].toInt();
					comment.string = o["m"].toString();
				}
				return comment;
			};
			auto reduce = [](QVector<Comment> &list, const Comment &comment) {
				if (!comment.isEmpty()) {
					list.append(comment);
				}
			};
			qThreadPool->reserveThread();
			const auto &result = QtConcurrent::blockingMappedReduced<
				QVector<Comment>,
				QVector<QJsonValue>,
				std::function<Comment(const QJsonValue &)>,
				std::function<void(QVector<Comment> &, const Comment &)>>
				(raws, map, reduce, QtConcurrent::OrderedReduce | QtConcurrent::SequentialReduce);
			qThreadPool->releaseThread();
			return result;
		}();
		return ResultDelegate(new VectorRecord(result));
	}
	case Utils::AcfunLocalizer:
	{
		QStringList items = decodeBytes(data).split("<l i=\"");
		if (items.isEmpty()) {
			return ResultDelegate();
		}
		items.removeFirst();
		QVector<Comment> list;
		list.reserve(items.size());
		for (const QString &item : items){
			const QStringList args = item.left(item.indexOf("\"")).split(',');
			if (args.size() < 6){
				continue;
			}
			Comment comment;
			comment.time = args[0].toDouble() * 1000 + 0.5;
			comment.date = args[5].toInt();
			comment.mode = 1;
			comment.font = 25;
			comment.color = args[2].toInt();
			comment.sender = args[4];
			int sta = item.indexOf("<![CDATA[") + 9;
			int len = item.indexOf("]]>", sta) - sta;
			comment.string = Utils::decodeXml(item.mid(sta, len), true);
			list.append(comment);
		}
		return ResultDelegate(new VectorRecord(list));
	}
	case Utils::Niconico:
	{
		QStringList items = decodeBytes(data).split("<chat ");
		if (items.isEmpty()) {
			return ResultDelegate();
		}
		items.removeFirst();
		QVector<Comment> list;
		list.reserve(items.size());
		for (const QString &item : items){
			Comment comment;
			QString key, val;
			/* 0 wait for key
			 * 1 wait for left quot
			 * 2 wait for value
			 * 3 wait for comment
			 * 4 finsihed */
			int state = 0;
			QMap<QString, QString> args;
			for (const QChar &c : item){
				switch (state){
				case 0:
					if (c == '='){
						state = 1;
					}
					else if (c == '>'){
						state = 3;
					}
					else if (c != ' '){
						key.append(c);
					}
					break;
				case 1:
					if (c == '\"'){
						state = 2;
					}
					break;
				case 2:
					if (c == '\"'){
						state = 0;
						args.insert(key, val);
						key = val = QString();
					}
					else{
						val.append(c);
					}
					break;
				case 3:
					if (c == '<'){
						state = 4;
					}
					else{
						comment.string.append(c);
					}
					break;
				}
			}
			if (state != 4){
				continue;
			}
			comment.time = args["vpos"].toLongLong() * 10;
			comment.date = args["date"].toLongLong();
			QStringList ctrl = args["mail"].split(' ', Qt::SkipEmptyParts);
			comment.mode = ctrl.contains("shita") ? 4 : (ctrl.contains("ue") ? 5 : 1);
			comment.font = ctrl.contains("small") ? 15 : (ctrl.contains("big") ? 36 : 25);
			comment.color = 0xFFFFFF;
			for (const QString &name : ctrl){
				QColor color(name);
				if (color.isValid()){
					comment.color = color.rgb();
					break;
				}
			}
			comment.sender = args["user_id"];
			list.append(comment);
		}
		return ResultDelegate(new VectorRecord(list));
	}
	case Utils::ASS:
	{
		QString xml = decodeBytes(data);
		int pos = 0, len;
		pos = xml.indexOf("PlayResY:") + 9;
		len = xml.indexOf('\n', pos) + 1 - pos;
		int vertical = xml.mid(pos, len).trimmed().toInt();
		QStringList ref;
		pos = xml.indexOf("Format:", pos) + 7;
		len = xml.indexOf('\n', pos) + 1 - pos;
		ref = xml.mid(pos, len).split(',');
		int name = -1, size = -1;
		for (int i = 0; i < ref.size(); ++i){
			const QString item = ref[i].trimmed();
			if (item == "Name"){
				name = i;
			}
			else if (item == "Fontsize"){
				size = i;
			}
		}
		if (name < 0 || size < 0){
			return ResultDelegate();
		}
		pos += len;
		len = xml.indexOf("Format:", pos) - pos;
		ref = xml.mid(pos, len).split("Style:", Qt::SkipEmptyParts);
		QMap<QString, int> style;
		for (const QString &item : ref){
			const auto &args = item.split(',');
			style[args[name].trimmed()] = args[size].toInt();
		}
		pos += len;
		pos = xml.indexOf("Format:", pos) + 7;
		len = xml.indexOf("\n", pos) + 1 - pos;
		ref = xml.mid(pos, len).split(',');
		int text = -1, font = -1, time = -1;
		for (int i = 0; i < ref.size(); ++i){
			const QString item = ref[i].trimmed();
			if (item == "Text"){
				text = i;
			}
			else if (item == "Start"){
				time = i;
			}
			else if (item == "Style"){
				font = i;
			}
		}
		if (text < 0 || font < 0 || time < 0){
			return ResultDelegate();
		}
		qint64 dat = QDateTime::currentDateTime().toSecsSinceEpoch();
		pos += len;
		ref = xml.mid(pos).split("Dialogue:", Qt::SkipEmptyParts);
		QVector<Comment> list;
		list.reserve(ref.size());
		for (const QString &item : ref){
			const auto &args = item.split(',');
			Comment comment;
			comment.date = dat;
			QString t;
			t = args[time].trimmed();
			comment.time = 1000 * Utils::evaluate(t);
			t = args[font].trimmed();
			comment.font = style[t];
			// ASS 的 Text 是最后一个字段、可含逗号：从第 text 个逗号之后截到结尾
			int cut = 0;
			for (int i = 0; i < text; ++i){
				cut = item.indexOf(',', cut) + 1;
			}
			t = item.mid(cut).trimmed();
			int split = t.indexOf("}") + 1;
			comment.string = t.mid(split).trimmed();
			const QStringList m = t.mid(1, split - 2).split('\\', Qt::SkipEmptyParts);
			for (const QString &i : m){
				if (i.startsWith("fs")){
					comment.font = i.mid(2).toInt();
				}
				else if (i.startsWith("c&H", Qt::CaseSensitive)){
					// ASS 颜色写作 &HBBGGRR&：原实现 toInt 直接吃进尾部 '&' 得 0，
					// 带颜色标签的 ASS 弹幕因此全部被"color==0"过滤丢弃（单测逮住的上游 bug）
					QString hex = i.mid(3);
					if (hex.endsWith('&')){
						hex.chop(1);
					}
					bool ok = false;
					const uint bgr = hex.toUInt(&ok, 16);
					if (ok){
						comment.color = ((bgr & 0xFF) << 16) | (bgr & 0xFF00) | ((bgr >> 16) & 0xFF);
					}
				}
				else if (i.startsWith("c")){
					comment.color = i.mid(1).toInt();
				}
				else if (i.startsWith("move")){
					const auto &p = i.mid(5, i.length() - 6).split(',');
					if (p.size() == 4){
						comment.mode = p[0].toInt() > p[2].toInt() ? 1 : 6;
					}
				}
				else if (i.startsWith("pos")){
					const auto &p = i.mid(4, i.length() - 5).split(',');
					if (p.size() == 2){
						comment.mode = p[1].toInt() > vertical / 2 ? 4 : 5;
					}
				}
				else{
					comment.mode = 0;
					break;
				}
			}
			if (comment.mode != 0 && comment.color != 0){
				list.append(comment);
			}
		}
		return ResultDelegate(new VectorRecord(list));
	}
	default:
		return ResultDelegate();
	}
}
