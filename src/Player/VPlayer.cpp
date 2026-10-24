#include "Common.h"
#include "VPlayer.h"
#include "../Config.h"
#include "../Local.h"
#include "../Utils.h"
#include "../Render/ARender.h"
#include <functional>

namespace
{
	// 原名是 time。它位于匿名命名空间里，而下面四处都用 "::time." 做限定查找，
	// 限定查找看不到匿名命名空间的名字 → GCC 把它解析成 C 库的 time() 函数。
	// MSVC 容忍这种写法，MinGW 不容忍，改个名。
	// 递归锁：setTime 持锁期间会跑 processEvents，事件里若同步重入 setTime，
	// 非递归 QMutex 会当场死锁（实际调用图没踩到过，但属于地雷）。
	QRecursiveMutex timeMutex;

	// fmt/dsp 等回调跑在 VLC 解码线程上，原来每次都 lApp->findObject<T>()
	// （内部 qobject_cast），与 GUI 线程竞争，曾崩在 Qt5Core 的 QMetaObject::cast。
	// Local 构造完成后模块表不再增删（退出才销毁），首次解析后缓存裸指针，
	// 回调线程从此不做类型转换、不碰哈希表。
	APlayer *cbAPlayer()
	{
		static APlayer *p = Local::instance()->findObject<APlayer>();
		return p;
	}

	void sta(const libvlc_event_t *, void *)
	{
		QMetaObject::invokeMethod(cbAPlayer(), "event", Q_ARG(int, VPlayer::Init));
	}

	void mid(const libvlc_event_t *, void *)
	{
		if (timeMutex.tryLock()) {
			QMetaObject::invokeMethod(cbAPlayer(),
				"timeChanged",
				Q_ARG(qint64, cbAPlayer()->getTime()));
			timeMutex.unlock();
		}
	}

	void hal(const libvlc_event_t *, void *)
	{
		QMetaObject::invokeMethod(cbAPlayer(), "event", Q_ARG(int, VPlayer::Wait));
	}

	void end(const libvlc_event_t *, void *)
	{
		QMetaObject::invokeMethod(cbAPlayer(), "event", Q_ARG(int, VPlayer::Free));
	}

	void err(const libvlc_event_t *, void *)
	{
		QMetaObject::invokeMethod(cbAPlayer(), "event", Q_ARG(int, VPlayer::Fail));
	}
}

VPlayer::VPlayer(QObject *parent)
	: APlayer(parent)
{
	setObjectName("VPlayer");

	QList<QByteArray> args;
	for (QJsonValue arg : Config::getValue<QJsonArray>("/Player/Arguments")){
		args.append(arg.toString().toUtf8());
	}
	// Embed 直出：视频由 VLC 自己画到原生子窗口，固定开 Direct3D11 硬解；
	// Arguments 里已写 --avcodec-hw= / --vout= 的以配置为准（只有最后一个生效，
	// 自动追加会把手写参数顶掉）。
	if (!args.contains("--avcodec-hw=d3d11va")){
		args.append("--avcodec-hw=d3d11va");
	}
	if (!args.contains("--vout=direct3d11")){
		args.append("--vout=direct3d11");
	}
	QVector<const char *> argv(args.size());
	for (int i = 0; i < args.size(); ++i){
		argv[i] = args[i].data();
	}
	vlc = libvlc_new(argv.size(), argv.data());
	mp = nullptr;
	state = Stop;
	// 音量常驻成员：未加载媒体（mp 为空）时 getVolume 不能依赖 libvlc，
	// 否则滚轮调音量永远从 0 起步、向上只能到 +5
	volume = Config::getValue("/Player/Volume", 50);
}

VPlayer::~VPlayer()
{
	if (mp){
		if (state != Stop){
			// 先停掉解码线程再释放，避免析构期间 VLC 线程还在往已失效的回调里打
			libvlc_media_player_stop(mp);
		}
		libvlc_media_player_release(mp);
	}
	libvlc_release(vlc);
}

void VPlayer::init()
{
	if (mp){
		auto connection = QSharedPointer<QMetaObject::Connection>::create();
		*connection = connect(this, &VPlayer::timeChanged, this, [=](){
			int last = state;
			emit stateChanged(state = Play);
			disconnect(*connection);
			switch (last){
			case Stop:
			{
				if (!Config::getValue("/Player/Subtitle", true)){
					libvlc_video_set_spu(mp, -1);
				}
				parseTracks(Utils::Video);
				parseTracks(Utils::Audio);
				parseTracks(Utils::Subtitle);
				emit begin();
				break;
			}
			case Loop:
			{
				setTrack(Utils::Video, tracks[0].current);
				setTrack(Utils::Audio, tracks[1].current);
				setTrack(Utils::Subtitle, tracks[2].current);
				break;
			}
			default:
				return;
			}
			setVolume(Config::getValue("/Player/Volume", 50));
		});
	}
}

void VPlayer::wait()
{
	emit stateChanged(state = Pause);
}

void VPlayer::free()
{
	if (state == Play&&Config::getValue("/Player/Loop", false)){
		libvlc_media_player_stop(mp);
		emit stateChanged(state = Loop);
		libvlc_media_player_play(mp);
		emit jumped(0);
	}
	else{
		stop(false);
	}
}

void VPlayer::parseTracks(Utils::Type type)
{
	int curr = -1;
	libvlc_track_description_t *head = nullptr;
	TrackSlot *slot = nullptr;
	switch (type) {
	case Utils::Video:
		curr = libvlc_video_get_track(mp);
		head = libvlc_video_get_track_description(mp);
		slot = &tracks[0];
		break;
	case Utils::Audio:
		curr = libvlc_audio_get_track(mp);
		head = libvlc_audio_get_track_description(mp);
		slot = &tracks[1];
		break;
	case Utils::Subtitle:
		curr = libvlc_video_get_spu(mp);
		head = libvlc_video_get_spu_description(mp);
		slot = &tracks[2];
		break;
	default:
		return;
	}
	slot->list.clear();
	libvlc_track_description_t *iter = head;
	while (iter) {
		auto fake = slot->list.size();
		auto real = iter->i_id;
		auto name = iter->psz_name;
		iter = iter->p_next;
		std::function<void()> func;
		switch (type) {
		case Utils::Video:
			func = [=]() {
				libvlc_video_set_track(mp, real);
				slot->current = fake;
				emit trackChanged(type, fake);
			};
			break;
		case Utils::Audio:
			func = [=]() {
				libvlc_audio_set_track(mp, real);
				slot->current = fake;
				emit trackChanged(type, fake);
			};
			break;
		case Utils::Subtitle:
			func = [=]() {
				libvlc_video_set_spu(mp, real);
				slot->current = fake;
				emit trackChanged(type, fake);
			};
			break;
		default:
			continue;
		}
		if (curr == real) {
			slot->current = fake;
		}
		slot->list.append({ name , func });
	}
	libvlc_track_description_list_release(head);
}

void VPlayer::play()
{
	if (mp){
		if (state == Stop){
			// Embed 直出：视频画进原生子窗口，没有内存帧回调这条路
			QWidget *surface = qobject_cast<QWidget *>(lApp->findObject<ARender>()->getHandle());
			if (surface){
				surface->setAttribute(Qt::WA_NativeWindow, true);
				libvlc_media_player_set_hwnd(mp, (void *)surface->winId());
			}
			libvlc_media_player_play(mp);
		}
		else{
			libvlc_media_player_pause(mp);
		}
	}
}

void VPlayer::stop(bool manually)
{
	if (mp&&state != Stop){
		libvlc_media_player_stop(mp);
		emit stateChanged(state = Stop);
		emit reach(manually);
	}
}

void VPlayer::setTime(qint64 _time)
{
	if (mp&&state != Stop){
		if (getDuration() == _time){
			if (Config::getValue("/Player/Loop", false)){
				setTime(0);
			}
			else{
				stop();
			}
		}
		else{
			timeMutex.lock();
			qApp->processEvents();
			emit jumped(_time);
			libvlc_media_player_set_time(mp, qBound<qint64>(0, _time, getDuration()));
			timeMutex.unlock();
		}
	}
}

qint64 VPlayer::getTime()
{
	return state == Stop ? -1 : libvlc_media_player_get_time(mp);
}

void VPlayer::setMedia(QString file)
{
	file = QUrl::fromUserInput(file).toLocalFile();
	file = QDir::toNativeSeparators(file);
	libvlc_media_t *m = libvlc_media_new_path(vlc, file.toUtf8());
	if (!m){
		return;
	}
	if (mp){
		libvlc_media_player_release(mp);
	}
	mp = libvlc_media_player_new_from_media(m);
	libvlc_media_release(m);
	if (!mp){
		return;
	}
	libvlc_event_manager_t *man = libvlc_media_player_event_manager(mp);
	libvlc_event_attach(man,
		libvlc_MediaPlayerPlaying,
		sta, nullptr);
	libvlc_event_attach(man,
		libvlc_MediaPlayerTimeChanged,
		mid, nullptr);
	libvlc_event_attach(man,
		libvlc_MediaPlayerPaused,
		hal, nullptr);
	libvlc_event_attach(man,
		libvlc_MediaPlayerEndReached,
		end, nullptr);
	libvlc_event_attach(man,
		libvlc_MediaPlayerEncounteredError,
		err, nullptr);
	emit mediaChanged(getMedia());
	if (Config::getValue("/Player/Immediate", false)){
		play();
	}
}

QString VPlayer::getMedia()
{
	if (mp){
		libvlc_media_t *m = libvlc_media_player_get_media(mp);
		char *s = libvlc_media_get_mrl(m);
		QUrl u(s);
		libvlc_free(s);
		libvlc_media_release(m);
		return u.isLocalFile() ? u.toLocalFile() : u.url();
	}
	return QString();
}

qint64 VPlayer::getDuration()
{
	return mp ? libvlc_media_player_get_length(mp) : -1;
}

void VPlayer::setVolume(int volume)
{
	volume = qBound(0, volume, 100);
	this->volume = volume;
	Config::setValue("/Player/Volume", volume);
	if (mp){
		libvlc_audio_set_volume(mp, volume);
	}
	emit volumeChanged(volume);
}

int VPlayer::getVolume()
{
	return volume;
}

void VPlayer::setRate(double rate)
{
	if (mp){
		libvlc_media_player_set_rate(mp, rate);
		emit rateChanged(rate);
	}
}

double VPlayer::getRate()
{
	return mp ? libvlc_media_player_get_rate(mp) : 0;
}

qint64 VPlayer::getDelay(int type)
{
	if (mp){
		switch (type){
		case Utils::Audio:
			return libvlc_audio_get_delay(mp);
		case Utils::Subtitle:
			return libvlc_video_get_spu_delay(mp);
		}
	}
	return 0;
}

void VPlayer::setDelay(int type, qint64 delay)
{
	if (mp){
		int code = -1;
		switch (type){
		case Utils::Audio:
			code = libvlc_audio_set_delay(mp, delay);
			break;
		case Utils::Subtitle:
			code = libvlc_video_set_spu_delay(mp, delay);
			break;
		}
		if (code == 0){
			emit delayChanged(type, delay);
		}
	}
}

int VPlayer::getTrack(int type)
{
	switch (type) {
	case Utils::Video:
		return tracks[0].current;
	case Utils::Audio:
		return tracks[1].current;
	case Utils::Subtitle:
		return tracks[2].current;
	default:
		return -1;
	}
}

void VPlayer::setTrack(int type, int index)
{
	QList<Track> *list = nullptr;
	switch (type) {
	case Utils::Video:
		list = &tracks[0].list;
		break;
	case Utils::Audio:
		list = &tracks[1].list;
		break;
	case Utils::Subtitle:
		list = &tracks[2].list;
		break;
	default:
		return;
	}
	if (index < 0 || index >= list->size()) {
		index = 0;
	}
	(*list)[index].set();
}

QStringList VPlayer::getTracks(int type)
{
	QStringList result;
	QList<Track> *list = nullptr;
	switch (type) {
	case Utils::Video:
		list = &tracks[0].list;
		break;
	case Utils::Audio:
		list = &tracks[1].list;
		break;
	case Utils::Subtitle:
		list = &tracks[2].list;
		break;
	default:
		return result;
	}
	for (const Track &iter : *list) {
		result.append(iter.name);
	}
	return result;
}

void VPlayer::addSubtitle(QString file)
{
	if (mp){
		QList<Track> &list = tracks[2].list;
		if (list.isEmpty()) {
			list.prepend({
				APlayer::tr("Disable"),
				[this]() { libvlc_video_set_spu(mp, -1); }
			});
		}
		QFileInfo info(file);
		auto n = info.fileName();
		auto f = QDir::toNativeSeparators(info.absoluteFilePath()).toUtf8();
		std::function<void()> s = [=]() {
			libvlc_video_set_subtitle_file(mp, f);
		};
		list.append({ n, s });
		s();
	}
}

void VPlayer::event(int type)
{
	switch (type){
	case Init:
		init();
		break;
	case Wait:
		wait();
		break;
	case Free:
		free();
		break;
	case Fail:
		emit errorOccurred(UnknownError);
		break;
	}
}
