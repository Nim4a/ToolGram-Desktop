/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/userpic_view.h"

#include "ui/empty_userpic.h"
#include "ui/painter.h"
#include "ui/image/image_prepare.h"

#include <cmath>

namespace Ui {
namespace {

constexpr auto kPeek = 0.26; // Tunable. Strip width to the left of the userpic.
constexpr auto kCard1Scale = 0.94; // Tunable.
constexpr auto kCard1Dx = 0.12; // Tunable.
constexpr auto kCard1Angle = -7.; // Tunable.
constexpr auto kCard1Radius = 0.25; // Tunable. Less rounded than the userpic.
constexpr auto kCard1Opacity = 0.5; // Tunable.
constexpr auto kCard2Scale = 0.86; // Tunable.
constexpr auto kCard2Dx = 0.22; // Tunable.
constexpr auto kCard2Angle = -14.; // Tunable.
constexpr auto kCard2Radius = 0.18; // Tunable. Even less rounded.
constexpr auto kCard2Opacity = 0.3; // Tunable.
constexpr auto kGap = 0.02; // Tunable.

} // namespace

float64 ForumUserpicRadiusMultiplier() {
	return 0.3;
}

void PaintCommunityUserpicEffect(
		QPainter &p,
		CommunityUserpicEffect &cache,
		int x,
		int y,
		int size,
		QColor color) {
	if (size <= 0) {
		return;
	}
	const auto dpr = style::DevicePixelRatio();
	const auto version = style::PaletteVersion();
	const auto rgba = color.rgba();
	const auto peek = size * kPeek;
	const auto regenerate = cache.image.isNull()
		|| (cache.size != size)
		|| (cache.color != rgba)
		|| (cache.paletteVersion != version)
		|| (cache.dpr != dpr);
	if (regenerate) {
		cache.size = size;
		cache.color = rgba;
		cache.paletteVersion = version;
		cache.dpr = dpr;

		const auto imageW = int(std::ceil(peek * dpr));
		const auto imageH = int(std::ceil(size * dpr));
		if (cache.image.size() != QSize(imageW, imageH)) {
			cache.image = QImage(
				QSize(imageW, imageH),
				QImage::Format_ARGB32_Premultiplied);
		}
		cache.image.setDevicePixelRatio(dpr);
		cache.image.fill(Qt::transparent);

		auto q = QPainter(&cache.image);
		auto hq = PainterHighQualityEnabler(q);
		const auto Cu = QPointF(peek + size / 2., size / 2.);
		const auto gap = size * kGap;

		const auto card = [&](
				QPointF center,
				float64 side,
				float64 angle,
				float64 rounding) {
			const auto half = side / 2.;
			const auto radius = side * rounding;
			q.save();
			q.translate(center);
			q.rotate(angle);
			q.drawRoundedRect(
				QRectF(-half, -half, side, side),
				radius,
				radius);
			q.restore();
		};

		const auto s1 = size * kCard1Scale;
		const auto C1 = Cu - QPointF(size * kCard1Dx, 0.);
		const auto s2 = size * kCard2Scale;
		const auto C2 = Cu - QPointF(size * kCard2Dx, 0.);

		q.setPen(Qt::NoPen);

		auto color2 = color;
		color2.setAlphaF(color.alphaF() * kCard2Opacity);
		q.setBrush(color2);
		card(C2, s2, kCard2Angle, kCard2Radius);

		// Carve a transparent gap, then draw card1 on top.
		q.setCompositionMode(QPainter::CompositionMode_Source);
		q.setBrush(Qt::transparent);
		card(C1, s1 + 2 * gap, kCard1Angle, kCard1Radius);
		q.setCompositionMode(QPainter::CompositionMode_SourceOver);
		auto color1 = color;
		color1.setAlphaF(color.alphaF() * kCard1Opacity);
		q.setBrush(color1);
		card(C1, s1, kCard1Angle, kCard1Radius);

		// Carve the userpic gap; the userpic is drawn by the caller.
		q.setCompositionMode(QPainter::CompositionMode_Source);
		q.setBrush(Qt::transparent);
		card(Cu, size + 2 * gap, 0., Ui::ForumUserpicRadiusMultiplier());
	}
	p.drawImage(QPointF(x - peek, y), cache.image);
}

bool PeerUserpicLoading(const PeerUserpicView &view) {
	return view.cloud && view.cloud->isNull();
}

void ValidateUserpicCache(
		PeerUserpicView &view,
		const QImage *cloud,
		const EmptyUserpic *empty,
		int size,
		PeerUserpicShape shape) {
	Expects(cloud != nullptr || empty != nullptr);

	const auto full = QSize(size, size);
	const auto version = style::PaletteVersion();
	const auto shapeValue = static_cast<uint32>(shape) & 3;
	const auto regenerate = (view.cached.size() != QSize(size, size))
		|| (view.shape != shapeValue)
		|| (cloud && !view.empty.null())
		|| (empty && empty != view.empty.get())
		|| (empty && view.paletteVersion != version);
	if (!regenerate) {
		return;
	}
	view.empty = empty;
	view.shape = shapeValue;
	view.paletteVersion = version;

	if (cloud) {
		view.cached = cloud->scaled(
			full,
			Qt::IgnoreAspectRatio,
			Qt::SmoothTransformation);
		if (shape == PeerUserpicShape::Monoforum) {
			view.cached = Ui::ApplyMonoforumShape(std::move(view.cached));
		} else if (shape == PeerUserpicShape::Forum) {
			view.cached = Images::Round(
				std::move(view.cached),
				Images::CornersMask(size
					* Ui::ForumUserpicRadiusMultiplier()
					/ style::DevicePixelRatio()));
		} else {
			view.cached = Images::Circle(std::move(view.cached));
		}
	} else {
		if (view.cached.size() != full) {
			view.cached = QImage(full, QImage::Format_ARGB32_Premultiplied);
		}
		view.cached.fill(Qt::transparent);

		auto p = QPainter(&view.cached);
		if (shape == PeerUserpicShape::Monoforum) {
			empty->paintMonoforum(p, 0, 0, size, size);
		} else if (shape == PeerUserpicShape::Forum) {
			empty->paintRounded(
				p,
				0,
				0,
				size,
				size,
				size * Ui::ForumUserpicRadiusMultiplier());
		} else {
			empty->paintCircle(p, 0, 0, size, size);
		}
	}
}

} // namespace Ui
