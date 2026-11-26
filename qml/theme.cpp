/* SPDX-License-Identifier: MIT */
#include "theme.h"

#include <QRegularExpression>
#include <QtQml>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include "constellation-qml.h"

static const char *const appearance_path = "/blinks/system/appearance";

/* The phone at 3x; the TV at 1x on 1920x1080, read from about three metres:
 * a 5% safe area for overscan, body text no smaller than 24 px, cards with
 * room around them for a 10% lift. Solid fills only: no long gradients. */
static const SdTheme::Variant phone_variant = {
	"phone", 16, 16, 96, 96, 18, 16,
	30, 20, 16, 13,
	1.04, 10, 0.3, 2,
	"#101425", "#1e2438", "#2a3150", "#ffffff", "#b3ffffff", "#cc000000",
};
static const SdTheme::Variant tv_variant = {
	"tv", 96, 54, 300, 180, 20, 44,
	56, 38, 28, 24,
	1.1, 36, 0.5, 6,
	"#0b0d12", "#1c212c", "#2a3140", "#f2f4f8", "#9aa3b2", "#b3000000",
};

static bool valid_color(const QString &color) {
	static const QRegularExpression rgb(QStringLiteral("^#[0-9a-fA-F]{6}$"));
	return rgb.match(color).hasMatch();
}

SdTheme *SdTheme::instance() {
	static SdTheme *theme = new SdTheme;
	return theme;
}

SdTheme::SdTheme() {
	const char *mode = getenv("STARRY_MODE");
	variant_ = mode && strcmp(mode, "tv") == 0 ? &tv_variant : &phone_variant;
	appearance_ = new CnBlink(this);
	appearance_->setPath(QString::fromLatin1(appearance_path));
	connect(appearance_, &CnBlink::updated, this, &SdTheme::update);
	appearance_->componentComplete(); /* starts watching, as QML would */
}

void SdTheme::update() {
	QString accent = appearance_->values().value(QStringLiteral("accent")).toString();
	accent = valid_color(accent) ? accent.toLower() : QString::fromLatin1(default_accent);
	QString style = appearance_->values().value(QStringLiteral("icons")).toString();
	static const QStringList styles = { "default", "dark", "tinted", "clear" };
	style = styles.contains(style) ? style : QStringLiteral("default");
	if (accent != accent_ || style != icon_style_) {
		accent_ = accent;
		icon_style_ = style;
		emit changed();
	}
}

/* The accent moved toward white, then scaled, as "#AARRGGBB". */
QString SdTheme::mix(float toward_white, float brightness, int alpha) const {
	bool ok = false;
	uint rgb = accent_.mid(1).toUInt(&ok, 16);
	int channels[3] = { int(rgb >> 16 & 0xff), int(rgb >> 8 & 0xff), int(rgb & 0xff) };
	for (int &c : channels) {
		float value = (c + (255 - c) * toward_white) * brightness;
		c = std::clamp(int(value + 0.5f), 0, 255);
	}
	return QStringLiteral("#%1%2%3%4")
		.arg(alpha, 2, 16, QLatin1Char('0'))
		.arg(channels[0], 2, 16, QLatin1Char('0'))
		.arg(channels[1], 2, 16, QLatin1Char('0'))
		.arg(channels[2], 2, 16, QLatin1Char('0'));
}

bool SdTheme::tv() const {
	return variant_ == &tv_variant;
}

QString SdTheme::accentLit() const { return mix(0.62f, 1, 255); }
QString SdTheme::accentGlow() const { return mix(0, 1, 0x8c); }
QString SdTheme::accentShade() const { return mix(0, 0.30f, 255); }
QString SdTheme::accentDeep() const { return mix(0, 0.17f, 255); }

bool SdTheme::setAccent(const QString &color) {
	if (!valid_color(color)) {
		qWarning("Theme.setAccent: %s is not #RRGGBB", qPrintable(color));
		return false;
	}
	return appearance_->publish({{QStringLiteral("accent"), color.toLower()}});
}

bool SdTheme::setIconStyle(const QString &style) {
	static const QStringList styles = { "default", "dark", "tinted", "clear" };
	if (!styles.contains(style)) {
		qWarning("Theme.setIconStyle: %s is not default, dark, tinted or clear", qPrintable(style));
		return false;
	}
	return appearance_->publish({{QStringLiteral("icons"), style}});
}

void registerThemeTypes() {
	qmlRegisterSingletonInstance("Stardust", 1, 0, "Theme", SdTheme::instance());
}
