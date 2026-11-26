/* SPDX-License-Identifier: MIT */
/*
 * SystemService: requests to lunar-settingsd, the root daemon behind the
 * Settings app, for platform apps. Each send() is one request on its own
 * connection; the answer comes back later as replied(), so a slow daemon
 * never blocks the QML thread.
 *
 *     SystemService {
 *         id: system
 *         onReplied: (request, ok, error, text) => console.log(request, ok, error, text)
 *     }
 *     system.send("bt power on")
 *
 * State to show comes from Constellation instead, as Blink objects under
 * /blinks/settings/. The protocol is lunar-settingsd's.
 */
#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class QSocketNotifier;
class QTimer;

class SdSystemService : public QObject {
	Q_OBJECT
	/* The daemon's socket; the default is lunar-settingsd's. */
	Q_PROPERTY(QString socket MEMBER socket_ NOTIFY socketChanged)
	/* Requests sent and not yet answered. */
	Q_PROPERTY(int pending READ pending NOTIFY pendingChanged)
	/* Seconds to wait for an answer before replied(..., "timeout", ...). */
	Q_PROPERTY(int timeout MEMBER timeout_ NOTIFY timeoutChanged)

public:
	explicit SdSystemService(QObject *parent = nullptr);
	~SdSystemService() override;
	int pending() const { return int(requests_.size()); }

	/* Sends one request line ("bt scan on"); returns its number, which
	 * replied() carries. Failures arrive through replied() as well. */
	Q_INVOKABLE int send(const QString &request);

signals:
	/* ok: the daemon said "ok"; error: its code ("denied", "invalid",
	 * "busy"... or "unavailable", "timeout" from here) when not; text: the
	 * rest of the answer, lines included. */
	void replied(int request, bool ok, const QString &error, const QString &text);
	void socketChanged();
	void pendingChanged();
	void timeoutChanged();

private:
	struct Request {
		int fd = -1;
		QSocketNotifier *notifier = nullptr;
		QTimer *timer = nullptr;
	};
	void finish(int id, bool ok, const QString &error, const QString &text);
	void receive(int id);

	QString socket_ = QStringLiteral("/run/lunar-settings.sock");
	int timeout_ = 10;
	int last_id_ = 0;
	QHash<int, Request> requests_;
};

void registerSystemTypes();
