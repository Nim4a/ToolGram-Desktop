/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "window/themes/window_themes_chat.h"

#include "base/crc32hash.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "ui/chat/chat_theme.h"
#include "window/themes/window_theme.h"
#include "window/themes/window_theme_editor.h"
#include "window/themes/window_theme_preview.h"
#include "window/themes/window_themes_embedded.h"
#include "window/window_session_controller.h"

namespace Window::Theme {
namespace {

[[nodiscard]] QByteArray GeneratePaletteContent(
		const style::palette &palette) {
	auto result = QByteArray();
	const auto rows = style::main_palette::data();
	result.reserve(rows.size() * 32);
	for (const auto &row : rows) {
		const auto index = style::internal::GetPaletteIndex(row.name);
		Assert(index >= 0);
		result.append(row.name.data(), row.name.size());
		result.append(": ");
		result.append(ColorHexString(palette.colorAtIndex(index)->c));
		result.append(";\n");
	}
	return result;
}

} // namespace

std::optional<Data::CloudThemeType> ChatThemeVariant(
		const Data::CloudTheme &theme,
		bool dark) {
	const auto type = dark
		? Data::CloudThemeType::Dark
		: Data::CloudThemeType::Light;
	const auto fallback = dark
		? Data::CloudThemeType::Light
		: Data::CloudThemeType::Dark;
	return theme.settings.contains(type)
		? std::make_optional(type)
		: theme.settings.contains(fallback)
		? std::make_optional(fallback)
		: std::nullopt;
}

Ui::ChatThemeBubblesData PrepareBubblesData(
		const Data::CloudTheme &theme,
		Data::CloudThemeType type) {
	const auto i = theme.settings.find(type);
	return {
		.colors = (i != end(theme.settings)
			? i->second.outgoingMessagesColors
			: std::vector<QColor>()),
		.accent = (i != end(theme.settings)
			? i->second.outgoingAccentColor
			: std::optional<QColor>()),
	};
}

std::unique_ptr<Preview> PreviewFromChatTheme(
		const Data::CloudTheme &theme,
		bool dark) {
	const auto used = ChatThemeVariant(theme, dark);
	if (!used) {
		return nullptr;
	}
	const auto &settings = theme.settings.find(*used)->second;
	auto descriptor = Ui::ChatThemeDescriptor{
		.key = { theme.id, dark },
		.preparePalette = PreparePaletteCallback(
			dark,
			settings.accentColor),
		.bubblesData = PrepareBubblesData(theme, *used),
		.basedOnDark = dark,
	};
	auto result = std::make_unique<Preview>();
	result->object.cloud = theme;
	result->object.pathRelative
		= result->object.pathAbsolute
		= CachedThemePath(theme.id);
	{
		const auto built = std::make_unique<Ui::ChatTheme>(
			std::move(descriptor));
		result->instance.palette.finalize();
		result->instance.palette = *built->palette();
		result->object.content = GeneratePaletteContent(
			*built->palette());
	}
	auto &cache = result->instance.cached;
	cache.colors = result->instance.palette.save();
	cache.paletteChecksum = style::palette::Checksum();
	cache.contentChecksum = base::crc32(
		result->object.content.constData(),
		result->object.content.size());
	return result;
}

void ApplyChatTheme(
		not_null<SessionController*> controller,
		const Data::CloudTheme &theme,
		bool dark) {
	const auto used = ChatThemeVariant(theme, dark);
	if (!used) {
		return;
	}
	const auto paper = theme.settings.find(*used)->second.paper;
	const auto weak = base::make_weak(controller);
	crl::async([=] {
		auto result = PreviewFromChatTheme(theme, dark);
		if (!result) {
			return;
		}
		crl::on_main(weak, [=, result = std::move(result)]() mutable {
			Apply(std::move(result));
			KeepApplied();
			if (paper) {
				controller->content()->setChatBackground(*paper);
			}
		});
	});
}

void CheckChatThemeWallPaper(not_null<SessionController*> controller) {
	if (!controller->widget()->sessionContent()) {
		return;
	}
	const auto background = Background();
	auto cloud = background->themeObject().cloud;
	if (cloud.emoticon.isEmpty() || cloud.settings.empty()) {
		return;
	}
	auto used = ChatThemeVariant(cloud, IsNightMode());
	if (!used) {
		return;
	}
	auto paper = cloud.settings.find(*used)->second.paper;
	if (!paper) {
		return;
	}
	if (!paper->document() && paper->isPattern()) {
		const auto &themes = controller->session().data().cloudThemes();
		const auto fresh = themes.themeForToken(cloud.emoticon);
		if (fresh) {
			auto updated = background->themeObject();
			updated.cloud = *fresh;
			background->setThemeObject(updated);
			cloud = *fresh;
			used = ChatThemeVariant(cloud, IsNightMode());
			if (!used) {
				return;
			}
			const auto &live = cloud.settings.find(*used)->second.paper;
			if (live) {
				paper = live;
			}
		}
	}
	const auto &current = background->paper();
	const auto degraded = current.isPattern()
		&& !current.document()
		&& background->prepared().isNull()
		&& (paper->document() != nullptr);
	if (!degraded && current.key() == paper->key()) {
		return;
	}
	const auto owned = Data::IsDefaultWallPaper(current)
		|| Data::IsThemeWallPaper(current)
		|| ranges::any_of(cloud.settings, [&](const auto &entry) {
			return entry.second.paper
				&& (entry.second.paper->key() == current.key());
		});
	if (owned) {
		controller->content()->setChatBackground(*paper);
	}
}

} // namespace Window::Theme
