/* SPDX-License-Identifier: MIT */
/*
 * Theme: system-wide appearance for QML, as the Stardust singleton "Theme".
 *
 *     Label { textColor: Theme.accentLit; textGlowColor: Theme.accent }
 *     onClicked: Theme.setAccent("#4fc3ff")
 *
 * The accent and the icon style live in Constellation
 * (/blinks/system/appearance, "accent" and "icons"),
 * so every application recolours at once when it changes. The shades are
 * derived from it: a pale core for lit text, a halo, and dark tints for
 * lit recesses.
 */
#pragma once

#include <QObject>
#include <QString>

class CnBlink;

class SdTheme : public QObject {
	Q_OBJECT
	Q_PROPERTY(QString accent READ accent NOTIFY changed)
	Q_PROPERTY(QString accentLit READ accentLit NOTIFY changed)
	Q_PROPERTY(QString accentGlow READ accentGlow NOTIFY changed)
	Q_PROPERTY(QString accentShade READ accentShade NOTIFY changed)
	Q_PROPERTY(QString accentDeep READ accentDeep NOTIFY changed)
	/* Home screen icons: "default", "dark", "tinted" (in the accent) or "clear". */
	Q_PROPERTY(QString iconStyle READ iconStyle NOTIFY changed)

public:
	static constexpr const char *default_accent = "#ffb340";
	static SdTheme *instance();

	QString accent() const { return accent_; }
	QString accentLit() const; // mixed most of the way to white
	QString accentGlow() const; // translucent, for halos
	QString accentShade() const; // 30% brightness
	QString accentDeep() const; // 17% brightness
	QString iconStyle() const { return icon_style_; }

	/* Publishes a new accent ("#RRGGBB") for the whole system. */
	Q_INVOKABLE bool setAccent(const QString &color);
	Q_INVOKABLE bool setIconStyle(const QString &style);

signals:
	void changed();

private:
	SdTheme();
	void update();
	QString mix(float toward_white, float brightness, int alpha) const;

	CnBlink *appearance_ = nullptr;
	QString accent_ = default_accent;
	QString icon_style_ = QStringLiteral("default");
};

void registerThemeTypes();
