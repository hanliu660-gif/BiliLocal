#pragma once

#include "APlayer.h"
#include "Utils.h"
#include <functional>

extern "C"
{
#include <vlc/vlc.h>
}

class VPlayer :public APlayer
{
public:
	explicit VPlayer(QObject *parent = 0);
	virtual ~VPlayer();

private:
	struct Track
	{
		QString name;
		std::function<void()> set;
	};

	struct TrackSlot
	{
		QList<Track> list;
		int current;
	};

	int state;
	int volume;
	libvlc_instance_t *vlc;
	libvlc_media_player_t *mp;
	TrackSlot tracks[3];

	void    init();
	void    wait();
	void    free();
	void    parseTracks(Utils::Type type);

public slots:
	void    play() override;
	void    stop(bool manually = true) override;
	int     getState() override { return state; }

	void    setTime(qint64 time) override;
	qint64  getTime() override;

	void    setMedia(QString file) override;
	QString getMedia() override;

	qint64  getDuration() override;

	void    setVolume(int volume) override;
	int     getVolume() override;

	void    setRate(double rate) override;
	double  getRate() override;

	qint64  getDelay(int type) override;
	void    setDelay(int type, qint64 delay) override;

	int     getTrack(int type) override;
	void    setTrack(int type, int index) override;
	QStringList getTracks(int type) override;

	void    addSubtitle(QString file) override;

	void    event(APlayer::Event type) override;
};
