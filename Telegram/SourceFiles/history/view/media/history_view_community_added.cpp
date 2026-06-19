/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/media/history_view_community_added.h"

#include "core/click_handler_types.h" // ClickHandlerContext
#include "data/data_channel.h"
#include "history/history_item.h"
#include "history/view/history_view_element.h"
#include "history/view/media/history_view_media_generic.h"
#include "history/view/media/history_view_unique_gift.h" // MakeGenericButtonPart
#include "lang/lang_keys.h"
#include "ui/dynamic_thumbnails.h"
#include "ui/text/text_utilities.h"
#include "window/window_session_controller.h"
#include "styles/style_chat.h"
#include "styles/style_menu_icons.h"
#include "styles/style_premium.h"

namespace HistoryView {

auto GenerateCommunityAddedMedia(
	not_null<Element*> parent,
	not_null<ChannelData*> community)
-> Fn<void(
		not_null<MediaGeneric*>,
		Fn<void(std::unique_ptr<MediaGenericPart>)>)> {
	return [=](
			not_null<MediaGeneric*> media,
			Fn<void(std::unique_ptr<MediaGenericPart>)> push) {
		const auto from = parent->data()->from();

		const auto open = std::make_shared<LambdaClickHandler>([=](
				ClickContext context) {
			const auto my = context.other.value<ClickHandlerContext>();
			if (const auto controller = my.sessionWindow.get()) {
				controller->showPeerInfo(community);
			}
		});

		auto image = community->hasUserpic()
			? Ui::MakeUserpicThumbnail(community)
			: Ui::MakeIconThumbnail(st::menuIconGroups);
		push(std::make_unique<DynamicImagePart>(
			parent,
			std::move(image),
			st::msgServiceCommunityAddedPhoto,
			QMargins(
				0,
				st::msgServiceGiftBoxButtonMargins.top(),
				0,
				st::msgServiceGiftBoxTitlePadding.top()),
			open,
			true)); // Paint the community stacked-cards effect behind it.

		push(std::make_unique<MediaGenericTextPart>(
			tr::lng_action_community_added(
				tr::now,
				lt_from,
				tr::bold(from->shortName()),
				lt_community,
				tr::bold(community->name()),
				tr::rich),
			QMargins(
				st::msgPadding.left(),
				0,
				st::msgPadding.right(),
				st::msgServiceGiftBoxTitlePadding.bottom()),
			st::premiumPreviewAbout.style));

		push(MakeGenericButtonPart(
			tr::lng_community_view(tr::now),
			QMargins(
				0,
				st::msgServiceGiftBoxButtonMargins.top(),
				0,
				st::msgServiceGiftBoxButtonMargins.bottom()),
			[=] { parent->repaint(); },
			open));
	};
}

} // namespace HistoryView
