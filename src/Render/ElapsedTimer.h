#pragma once

#include <QElapsedTimer>

class ElapsedTimer
{
public:
	ElapsedTimer()
	{
		estimate = qSNaN();
		interval = overflow = 0;
	}

	void setInterval(double second)
	{
		interval = second;
	}

	void invalidate()
	{
		inner.invalidate();
	}

	bool swap()
	{
		overflow = inner.isValid() ? qMax(0.0, elapsed() - estimate) : 0.0;
		inner.start();
		estimate = qSNaN();
		return overflow > 0;
	}

	//离下一帧还差多少毫秒。原来的调用方在「还没到点」时用 QTimer::singleShot(1, ...)
	//空转重试，每秒上千次唤醒；按剩余时间定时可以把唤醒次数降到刷新率级别。
	int remainingMs() const
	{
		if (interval <= 0 || !inner.isValid()) {
			return 1;
		}
		const double left = inner.nsecsElapsed() / 1000000000.0;
		return qBound(1, int((interval - left) * 1000.0), int(interval * 1000.0) + 1);
	}

	double step()
	{
		if (inner.isValid() && qIsNaN(estimate)){
			estimate = qMax(interval, elapsed());
			return estimate + overflow;
		}
		else{
			return 0;
		}
	}

private:
	double interval;
	double estimate;
	double overflow;
	QElapsedTimer inner;

	double elapsed()
	{
		return inner.nsecsElapsed() / 1000000000.0;
	}
};
