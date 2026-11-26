/* SPDX-License-Identifier: MIT */
#include "system-qml.h"

#include <QSocketNotifier>
#include <QTimer>
#include <QtQml>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static constexpr int max_request = 512;
static constexpr int max_reply = 8192;

SdSystemService::SdSystemService(QObject *parent) : QObject(parent) {}

SdSystemService::~SdSystemService() {
	for (const Request &request : std::as_const(requests_)) {
		delete request.notifier;
		delete request.timer;
		close(request.fd);
	}
}

int SdSystemService::send(const QString &text) {
	int id = ++last_id_;
	QByteArray line = text.trimmed().toUtf8();
	QByteArray path = socket_.toLocal8Bit();
	auto fail = [this, id](const QString &error, const QString &detail) {
		/* Later, like any answer, so handlers see the request number first. */
		QTimer::singleShot(0, this, [this, id, error, detail] { emit replied(id, false, error, detail); });
		return id;
	};
	if (line.isEmpty() || line.size() > max_request || line.contains('\n') || line.contains('\0')) {
		return fail(QStringLiteral("invalid"), QStringLiteral("one line of at most 512 bytes"));
	}
	sockaddr_un address = {};
	address.sun_family = AF_UNIX;
	if (path.size() >= qsizetype(sizeof(address.sun_path))) {
		return fail(QStringLiteral("unavailable"), QStringLiteral("socket path too long"));
	}
	memcpy(address.sun_path, path.constData(), size_t(path.size()));
	int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (fd < 0 || ::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0 ||
			::send(fd, line.constData(), size_t(line.size()), MSG_NOSIGNAL) != line.size()) {
		QString detail = QString::fromLocal8Bit(strerror(errno));
		if (fd >= 0) {
			close(fd);
		}
		return fail(QStringLiteral("unavailable"), detail);
	}
	Request request;
	request.fd = fd;
	request.notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
	connect(request.notifier, &QSocketNotifier::activated, this, [this, id] { receive(id); });
	request.timer = new QTimer(this);
	request.timer->setSingleShot(true);
	connect(request.timer, &QTimer::timeout, this, [this, id] {
		finish(id, false, QStringLiteral("timeout"), QString());
	});
	request.timer->start(timeout_ * 1000);
	requests_.insert(id, request);
	emit pendingChanged();
	return id;
}

/* "ok [TEXT]" or "err CODE [DETAIL]"; TEXT may run over several lines. */
void SdSystemService::receive(int id) {
	auto found = requests_.find(id);
	if (found == requests_.end()) {
		return;
	}
	char buffer[max_reply + 1];
	ssize_t length = recv(found->fd, buffer, max_reply, 0);
	if (length < 0 && (errno == EAGAIN || errno == EINTR)) {
		return;
	}
	if (length <= 0) {
		finish(id, false, QStringLiteral("unavailable"), QStringLiteral("no answer"));
		return;
	}
	QString answer = QString::fromUtf8(buffer, length);
	if (answer == QLatin1String("ok") || answer.startsWith(QLatin1String("ok ")) ||
			answer.startsWith(QLatin1String("ok\n"))) {
		finish(id, true, QString(), answer.mid(3));
		return;
	}
	QString rest = answer.startsWith(QLatin1String("err ")) ? answer.mid(4) : answer;
	qsizetype space = rest.indexOf(QLatin1Char(' '));
	finish(id, false, space < 0 ? rest : rest.left(space), space < 0 ? QString() : rest.mid(space + 1));
}

void SdSystemService::finish(int id, bool ok, const QString &error, const QString &text) {
	auto found = requests_.find(id);
	if (found == requests_.end()) {
		return;
	}
	Request request = *found;
	requests_.erase(found);
	request.notifier->setEnabled(false);
	request.notifier->deleteLater();
	request.timer->stop();
	request.timer->deleteLater();
	close(request.fd);
	emit pendingChanged();
	emit replied(id, ok, error, text);
}

void registerSystemTypes() {
	qmlRegisterType<SdSystemService>("Stardust", 1, 0, "SystemService");
}
