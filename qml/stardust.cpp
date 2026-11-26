/* SPDX-License-Identifier: MIT */
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMetaMethod>
#include <QMetaProperty>
#include <cstdio>
#include <algorithm>
#include <linux/input-event-codes.h>
#include "stardust.h"

using stardust::Mutation;
using stardust::Prop;
using stardust::Value;

namespace {

/* "#RRGGBB" or "#AARRGGBB"; anything else is transparent. */
stardust::Color parse_color(const QString &text) {
	QString hex = text.startsWith('#') ? text.mid(1) : text;
	bool ok = false;
	uint32_t value = hex.toUInt(&ok, 16);
	if (!ok || (hex.size() != 6 && hex.size() != 8)) {
		return {};
	}
	return stardust::Color::rgb(hex.size() == 6 ? 0xff000000 | value : value);
}

int parse_enum(const QString &text, std::initializer_list<const char *> names) {
	int index = 0;
	for (const char *name : names) {
		if (text == QLatin1String(name)) {
			return index;
		}
		index++;
	}
	return 0;
}

/* Maps a QML property to the render core's property and value. */
bool convert(const char *name, const QVariant &value, Prop &prop, Value &out) {
	if (qstrcmp(name, "points") == 0) {
		prop = Prop::Points;
		std::vector<float> points;
		const auto list = value.toList();
		const auto count = std::min(list.size(), qsizetype(16384));
		points.reserve(count);
		for (qsizetype i = 0; i < count; ++i) points.push_back(list[i].toFloat());
		out = std::move(points);
		return true;
	}
	struct Entry {
		const char *name;
		Prop prop;
		enum {
			Float, Int, Bool, Color, Text, LayoutKind, Direction, Alignment, SurfaceKind,
			ScrollKind,
		} type;
	};
	static const Entry entries[] = {
		{"background", Prop::Background, Entry::Color},
		{"radius", Prop::Radius, Entry::Float},
		{"opacity", Prop::Opacity, Entry::Float},
		{"scale", Prop::Scale, Entry::Float},
		{"translationX", Prop::TranslationX, Entry::Float},
		{"translationY", Prop::TranslationY, Entry::Float},
		{"layout", Prop::Layout, Entry::LayoutKind},
		{"orientation", Prop::Orientation, Entry::Direction},
		{"spacing", Prop::Spacing, Entry::Float},
		{"padding", Prop::Padding, Entry::Float},
		{"preferredWidth", Prop::PreferredWidth, Entry::Float},
		{"preferredHeight", Prop::PreferredHeight, Entry::Float},
		{"horizontalAlignment", Prop::HorizontalAlign, Entry::Alignment},
		{"verticalAlignment", Prop::VerticalAlign, Entry::Alignment},
		{"animated", Prop::Animated, Entry::Bool},
		{"duration", Prop::Duration, Entry::Int},
		{"text", Prop::Text, Entry::Text},
		{"textColor", Prop::TextColor, Entry::Color},
		{"fontSize", Prop::FontSize, Entry::Float},
		{"bold", Prop::Bold, Entry::Bool},
		{"backgroundEnd", Prop::BackgroundEnd, Entry::Color},
		{"gloss", Prop::Gloss, Entry::Float},
		{"borderWidth", Prop::BorderWidth, Entry::Float},
		{"borderColor", Prop::BorderColor, Entry::Color},
		{"bevel", Prop::Bevel, Entry::Float},
		{"innerShadow", Prop::InnerShadow, Entry::Float},
		{"shadow", Prop::Shadow, Entry::Float},
		{"shadowColor", Prop::ShadowColor, Entry::Color},
		{"glow", Prop::Glow, Entry::Float},
		{"innerGlow", Prop::InnerGlow, Entry::Float},
		{"glowColor", Prop::GlowColor, Entry::Color},
		{"material", Prop::Material, Entry::SurfaceKind},
		{"textShadow", Prop::TextShadowColor, Entry::Color},
		{"textShadowOffset", Prop::TextShadowOffset, Entry::Float},
		{"textGlow", Prop::TextGlow, Entry::Float},
		{"textGlowColor", Prop::TextGlowColor, Entry::Color},
		{"lightAngle", Prop::LightAngle, Entry::Float},
		{"focusable", Prop::Focusable, Entry::Bool},
		{"focusScale", Prop::FocusScale, Entry::Float},
		{"focusShadow", Prop::FocusShadow, Entry::Float},
		{"focusHighlight", Prop::FocusHighlight, Entry::Float},
		{"focusTilt", Prop::FocusTilt, Entry::Float},
		{"scroll", Prop::Scroll, Entry::ScrollKind},
	};
	for (const Entry &entry : entries) {
		if (qstrcmp(entry.name, name) != 0) {
			continue;
		}
		prop = entry.prop;
		switch (entry.type) {
		case Entry::Float: out = float(value.toDouble()); break;
		case Entry::Int: out = float(value.toInt()); break;
		case Entry::Bool: out = value.toBool() ? 1 : 0; break;
		case Entry::Color: out = parse_color(value.toString()); break;
		case Entry::Text: out = value.toString().toStdString(); break;
		case Entry::LayoutKind: out = parse_enum(value.toString(), {"stack", "dock"}); break;
		case Entry::Direction: out = parse_enum(value.toString(), {"vertical", "horizontal"}); break;
		case Entry::Alignment:
			out = parse_enum(value.toString(), {"fill", "start", "center", "end"});
			break;
		case Entry::SurfaceKind:
			out = parse_enum(value.toString(), {"plain", "brushed", "chrome"});
			break;
		case Entry::ScrollKind:
			out = parse_enum(value.toString(), {"none", "horizontal", "vertical"});
			break;
		}
		return true;
	}
	return false;
}

/* The name QML sees for a remote or keyboard key. */
QString key_name(uint32_t code) {
	static const struct { uint32_t code; const char *name; } names[] = {
		{KEY_UP, "up"}, {KEY_DOWN, "down"}, {KEY_LEFT, "left"}, {KEY_RIGHT, "right"},
		{KEY_OK, "ok"}, {KEY_SELECT, "ok"}, {KEY_ENTER, "ok"}, {KEY_KPENTER, "ok"},
		{KEY_HOMEPAGE, "home"}, {KEY_MENU, "menu"}, {KEY_SETUP, "setup"},
		{KEY_PLAYPAUSE, "playpause"}, {KEY_PLAY, "play"}, {KEY_PAUSE, "pause"},
		{KEY_STOP, "stop"}, {KEY_STOPCD, "stop"}, {KEY_REWIND, "rewind"},
		{KEY_FASTFORWARD, "fastforward"}, {KEY_NEXTSONG, "next"}, {KEY_PREVIOUSSONG, "previous"},
		{KEY_VOLUMEUP, "volumeup"}, {KEY_VOLUMEDOWN, "volumedown"}, {KEY_MUTE, "mute"},
		{KEY_SEARCH, "search"}, {KEY_INFO, "info"}, {KEY_EPG, "epg"}, {KEY_TV, "tv"},
		{KEY_VOD, "vod"}, {KEY_CHANNELUP, "channelup"}, {KEY_CHANNELDOWN, "channeldown"},
		{KEY_PROG1, "prog1"}, {KEY_PROG2, "prog2"}, {KEY_PROG3, "prog3"}, {KEY_PROG4, "prog4"},
		{KEY_RED, "red"}, {KEY_GREEN, "green"}, {KEY_YELLOW, "yellow"}, {KEY_BLUE, "blue"},
		{KEY_0, "0"}, {KEY_1, "1"}, {KEY_2, "2"}, {KEY_3, "3"}, {KEY_4, "4"},
		{KEY_5, "5"}, {KEY_6, "6"}, {KEY_7, "7"}, {KEY_8, "8"}, {KEY_9, "9"},
		{KEY_NUMERIC_0, "0"}, {KEY_NUMERIC_1, "1"}, {KEY_NUMERIC_2, "2"},
		{KEY_NUMERIC_3, "3"}, {KEY_NUMERIC_4, "4"}, {KEY_NUMERIC_5, "5"},
		{KEY_NUMERIC_6, "6"}, {KEY_NUMERIC_7, "7"}, {KEY_NUMERIC_8, "8"},
		{KEY_NUMERIC_9, "9"},
	};
	for (const auto &entry : names) {
		if (entry.code == code) {
			return QString::fromLatin1(entry.name);
		}
	}
	return QStringLiteral("key%1").arg(code);
}

} // namespace

/* ---- SdNode ---- */

SdNode::SdNode(QObject *parent) : QObject(parent) {}

QQmlListProperty<SdNode> SdNode::nodes() {
	return QQmlListProperty<SdNode>(this, nullptr, &SdNode::append, &SdNode::count,
		&SdNode::at, nullptr);
}

void SdNode::append(QQmlListProperty<SdNode> *list, SdNode *node) {
	auto *self = static_cast<SdNode *>(list->object);
	node->parent_node_ = self;
	self->children_.append(node);
}

qsizetype SdNode::count(QQmlListProperty<SdNode> *list) {
	return static_cast<SdNode *>(list->object)->children_.size();
}

SdNode *SdNode::at(QQmlListProperty<SdNode> *list, qsizetype index) {
	return static_cast<SdNode *>(list->object)->children_.at(index);
}

/* Every property with a change signal reports to one slot. */
void SdNode::classBegin() {
	const QMetaObject *meta = metaObject();
	const QMetaMethod slot = meta->method(meta->indexOfSlot("propertyChanged()"));
	for (int i = QObject::staticMetaObject.propertyCount(); i < meta->propertyCount(); i++) {
		QMetaProperty property = meta->property(i);
		if (property.hasNotifySignal()) {
			connect(this, property.notifySignal(), this, slot);
		}
	}
}

void SdNode::propertyChanged() {
	SdApp *app = SdApp::instance();
	if (!app->live() || !id) {
		return; // the initial values go out with the node's creation
	}
	int signal = senderSignalIndex();
	const QMetaObject *meta = metaObject();
	for (int i = QObject::staticMetaObject.propertyCount(); i < meta->propertyCount(); i++) {
		QMetaProperty property = meta->property(i);
		if (property.notifySignalIndex() != signal) {
			continue;
		}
		Prop prop;
		Value value;
		if (convert(property.name(), property.read(this), prop, value)) {
			app->post({Mutation::Set, id, 0, stardust::Kind::Container, prop, std::move(value)});
		}
	}
}

bool SdNode::handlesClicked() const {
	return isSignalConnected(QMetaMethod::fromSignal(&SdNode::clicked));
}

bool SdNode::handlesActivated() const {
	return isSignalConnected(QMetaMethod::fromSignal(&SdNode::activated));
}

bool SdNode::handlesBack() const {
	return isSignalConnected(QMetaMethod::fromSignal(&SdNode::back));
}

bool SdNode::handlesKeys() const {
	return isSignalConnected(QMetaMethod::fromSignal(&SdNode::keyPressed)) ||
		isSignalConnected(QMetaMethod::fromSignal(&SdNode::keyReleased));
}

void SdNode::setFocused(bool focused) {
	if (focused_ != focused) {
		focused_ = focused;
		emit focusedChanged();
	}
}

void SdNode::forceFocus() {
	SdApp *app = SdApp::instance();
	if (app->live() && id) {
		app->post({Mutation::Set, id, 0, stardust::Kind::Container, Prop::Focus, 1});
	} else {
		wants_focus_ = true; // sent with the node's creation
	}
}

void SdNode::setPressed(bool pressed) {
	if (pressed_ != pressed) {
		pressed_ = pressed;
		emit pressedChanged();
	}
}

/* ---- SdTimer ---- */

SdTimer::SdTimer(QObject *parent) : QObject(parent) {
	timer_.setInterval(1000);
	timer_.setSingleShot(true);
	connect(&timer_, &QTimer::timeout, this, [this] {
		emit triggered();
		if (timer_.isSingleShot()) {
			emit runningChanged();
		}
	});
}

void SdTimer::setInterval(int interval) {
	if (interval != timer_.interval()) {
		timer_.setInterval(interval);
		emit intervalChanged();
	}
}

void SdTimer::setRunning(bool running) {
	if (running == timer_.isActive()) {
		return;
	}
	running ? timer_.start() : timer_.stop();
	emit runningChanged();
}

void SdTimer::setRepeat(bool repeat) {
	if (repeat == !timer_.isSingleShot()) {
		return;
	}
	timer_.setSingleShot(!repeat);
	emit repeatChanged();
}

void SdTimer::restart() {
	timer_.start();
	emit runningChanged();
}

/* ---- SdApp ---- */

SdApp *SdApp::instance() {
	static SdApp app;
	return &app;
}

void SdApp::create(SdNode *node, stardust::NodeId parent) {
	node->id = ++last_id_;
	nodes_.insert(node->id, node);
	batch_.push_back({Mutation::Create, node->id, 0, node->kind()});
	const QMetaObject *meta = node->metaObject();
	for (int i = QObject::staticMetaObject.propertyCount(); i < meta->propertyCount(); i++) {
		QMetaProperty property = meta->property(i);
		Prop prop;
		Value value;
		if (convert(property.name(), property.read(node), prop, value)) {
			batch_.push_back({Mutation::Set, node->id, 0, stardust::Kind::Container, prop,
				std::move(value)});
		}
	}
	batch_.push_back({Mutation::Append, node->id, parent});
	if (node->wantsFocus()) {
		batch_.push_back({Mutation::Set, node->id, 0, stardust::Kind::Container, Prop::Focus, 1});
	}
	for (SdNode *child : node->children()) {
		create(child, node->id);
	}
}

bool SdApp::start(SdWindow *window) {
	stardust::CoreOptions options;
	options.title = window->title_.toStdString();
	options.app_id = window->app_id_.toStdString();
	window_ = window;
	core_ = std::make_unique<stardust::Core>(bridge_, options);
	bridge_.on_event = [this] {
		QMetaObject::invokeMethod(this, &SdApp::drainEvents, Qt::QueuedConnection);
	};
	if (!core_->start()) {
		return false;
	}
	create(window, 0);
	live_ = true;
	flush();
	return true;
}

void SdApp::post(Mutation mutation) {
	/* Straight to the render core: a handler that then blocks (or runs long)
	 * must not hold back what it already changed. The core picks up every
	 * mutation waiting at the start of a frame, so a binding cascade still
	 * lands in one frame. */
	bridge_.post(std::move(mutation));
}

void SdApp::stop() {
	live_ = false;
	if (core_) core_->stop();
	bridge_.on_event = {};
	bridge_.wake_core = {};
	core_.reset();
}

void SdApp::flush() {
	flush_scheduled_ = false;
	if (!batch_.empty()) {
		bridge_.post(std::exchange(batch_, {}));
	}
}

void SdApp::tapFirstClickable() {
	for (stardust::NodeId id = 1; id <= last_id_; id++) {
		SdNode *node = nodes_.value(id);
		if (node && node->handlesClicked()) {
			QElapsedTimer timer;
			timer.start();
			emit node->clicked();
			fprintf(stderr, "stardust trace: node %u onClicked ran %lld ms on the QML thread\n",
				id, (long long)timer.elapsed());
			return;
		}
	}
}

void SdApp::drainEvents() {
	for (const stardust::Event &event : bridge_.take_events()) {
		if (event.type == stardust::Event::Closed) {
			QCoreApplication::quit();
			return;
		}
		using stardust::Event;
		SdNode *node = nodes_.value(event.node);
		switch (event.type) {
		case Event::Pressed:
		case Event::Released:
			/* pressed holds for the touched node and all its ancestors. */
			for (SdNode *n = node; n; n = n->parentNode()) {
				n->setPressed(event.type == Event::Pressed);
			}
			break;
		case Event::Clicked:
		case Event::Activated: {
			/* The nearest ancestor that handles it gets it; a tap and OK
			 * stand in for each other where only one is handled. */
			bool click = event.type == Event::Clicked;
			for (SdNode *n = node; n; n = n->parentNode()) {
				bool primary = click ? n->handlesClicked() : n->handlesActivated();
				bool other = click ? n->handlesActivated() : n->handlesClicked();
				if (primary || other) {
					if (click == primary) {
						emit n->clicked();
					} else {
						emit n->activated();
					}
					break;
				}
			}
			break;
		}
		case Event::Focused:
		case Event::Unfocused:
			if (node) {
				node->setFocused(event.type == Event::Focused);
			}
			break;
		case Event::Back: {
			SdNode *n = node ? node : window_;
			while (n && !n->handlesBack()) {
				n = n->parentNode();
			}
			if (!n) {
				QCoreApplication::quit(); /* nothing to go back to: the shell takes over */
				return;
			}
			emit n->back();
			break;
		}
		case Event::Key: {
			const QString name = key_name(event.key);
			for (SdNode *n = node ? node : window_; n; n = n->parentNode()) {
				if (n->handlesKeys()) {
					event.down ? emit n->keyPressed(name) : emit n->keyReleased(name);
					break;
				}
			}
			break;
		}
		case Event::Closed:
			break;
		}
	}
}
