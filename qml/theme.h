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
 *
 * Form factor: STARRY_MODE=tv selects the TV variant, sized for 1920x1080
 * seen from a sofa: a safe area inside the overscan, large cards and type,
 * and the focus lift. Its surfaces are solid colours with soft shadows, which
 * Mali-400/450 class GPUs (mediump only) draw without banding.
 *
 *     Container { focusable: true; focusScale: Theme.focusScale
 *                 preferredWidth: Theme.cardWidth; background: Theme.surface }
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

	/* "phone" or "tv", fixed for the life of the app. */
	Q_PROPERTY(QString formFactor READ formFactor CONSTANT)
	Q_PROPERTY(bool tv READ tv CONSTANT)
	/* Sizes, logical px: the safe area from the screen edges, app cards, type. */
	Q_PROPERTY(qreal safeMarginX READ safeMarginX CONSTANT)
	Q_PROPERTY(qreal safeMarginY READ safeMarginY CONSTANT)
	Q_PROPERTY(qreal cardWidth READ cardWidth CONSTANT)
	Q_PROPERTY(qreal cardHeight READ cardHeight CONSTANT)
	Q_PROPERTY(qreal cardRadius READ cardRadius CONSTANT)
	Q_PROPERTY(qreal cardSpacing READ cardSpacing CONSTANT)
	Q_PROPERTY(qreal fontTitle READ fontTitle CONSTANT)
	Q_PROPERTY(qreal fontHeading READ fontHeading CONSTANT)
	Q_PROPERTY(qreal fontBody READ fontBody CONSTANT)
	Q_PROPERTY(qreal fontCaption READ fontCaption CONSTANT)
	/* The focus lift, for focusScale, focusShadow, focusHighlight, focusTilt. */
	Q_PROPERTY(qreal focusScale READ focusScale CONSTANT)
	Q_PROPERTY(qreal focusShadow READ focusShadow CONSTANT)
	Q_PROPERTY(qreal focusHighlight READ focusHighlight CONSTANT)
	Q_PROPERTY(qreal focusTilt READ focusTilt CONSTANT)
	/* Colours: page, cards, raised cards, text, and the lift's shadow. */
	Q_PROPERTY(QString background READ background CONSTANT)
	Q_PROPERTY(QString surface READ surface CONSTANT)
	Q_PROPERTY(QString surfaceRaised READ surfaceRaised CONSTANT)
	Q_PROPERTY(QString textPrimary READ textPrimary CONSTANT)
	Q_PROPERTY(QString textSecondary READ textSecondary CONSTANT)
	Q_PROPERTY(QString shadowColor READ shadowColor CONSTANT)

public:
	static constexpr const char *default_accent = "#ffb340";
	static SdTheme *instance();

	QString accent() const { return accent_; }
	QString accentLit() const; // mixed most of the way to white
	QString accentGlow() const; // translucent, for halos
	QString accentShade() const; // 30% brightness
	QString accentDeep() const; // 17% brightness
	QString iconStyle() const { return icon_style_; }

	struct Variant {
		const char *form_factor;
		qreal safe_x, safe_y, card_width, card_height, card_radius, card_spacing;
		qreal title, heading, body, caption;
		qreal focus_scale, focus_shadow, focus_highlight, focus_tilt;
		const char *background, *surface, *surface_raised, *text, *text_secondary, *shadow;
	};
	QString formFactor() const { return QString::fromLatin1(variant_->form_factor); }
	bool tv() const;
	qreal safeMarginX() const { return variant_->safe_x; }
	qreal safeMarginY() const { return variant_->safe_y; }
	qreal cardWidth() const { return variant_->card_width; }
	qreal cardHeight() const { return variant_->card_height; }
	qreal cardRadius() const { return variant_->card_radius; }
	qreal cardSpacing() const { return variant_->card_spacing; }
	qreal fontTitle() const { return variant_->title; }
	qreal fontHeading() const { return variant_->heading; }
	qreal fontBody() const { return variant_->body; }
	qreal fontCaption() const { return variant_->caption; }
	qreal focusScale() const { return variant_->focus_scale; }
	qreal focusShadow() const { return variant_->focus_shadow; }
	qreal focusHighlight() const { return variant_->focus_highlight; }
	qreal focusTilt() const { return variant_->focus_tilt; }
	QString background() const { return QString::fromLatin1(variant_->background); }
	QString surface() const { return QString::fromLatin1(variant_->surface); }
	QString surfaceRaised() const { return QString::fromLatin1(variant_->surface_raised); }
	QString textPrimary() const { return QString::fromLatin1(variant_->text); }
	QString textSecondary() const { return QString::fromLatin1(variant_->text_secondary); }
	QString shadowColor() const { return QString::fromLatin1(variant_->shadow); }

	/* Publishes a new accent ("#RRGGBB") for the whole system. */
	Q_INVOKABLE bool setAccent(const QString &color);
	Q_INVOKABLE bool setIconStyle(const QString &style);

signals:
	void changed();

private:
	SdTheme();
	void update();
	QString mix(float toward_white, float brightness, int alpha) const;

	const Variant *variant_;
	CnBlink *appearance_ = nullptr;
	QString accent_ = default_accent;
	QString icon_style_ = QStringLiteral("default");
};

void registerThemeTypes();
