/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/media/history_view_community_added.h"

#include "core/click_handler_types.h" // ClickHandlerContext
#include "data/data_channel.h"
#include "data/data_media_types.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/view/media/history_view_sticker_player_abstract.h"
#include "history/view/history_view_element.h"
#include "lang/lang_keys.h"
#include "ui/text/text_utilities.h"
#include "ui/dynamic_image.h"
#include "ui/dynamic_thumbnails.h"
#include "ui/painter.h"
#include "window/window_session_controller.h"
#include "styles/style_chat.h"
#include "styles/style_menu_icons.h"

namespace HistoryView {

CommunityAdded::CommunityAdded(
	not_null<Element*> parent,
	not_null<Data::MediaCommunityAdded*> media)
: _parent(parent)
, _community(media->community()) {
}

CommunityAdded::~CommunityAdded() {
	if (_subscribed) {
		unloadHeavyPart();
		_parent->checkHeavyPart();
	}
}

int CommunityAdded::top() {
	return st::msgServiceGiftBoxButtonMargins.top();
}

QSize CommunityAdded::size() {
	return { st::msgServicePhotoWidth, st::msgServicePhotoWidth };
}

TextWithEntities CommunityAdded::title() {
	return {};
}

TextWithEntities CommunityAdded::subtitle() {
	const auto from = _parent->data()->from();
	return tr::lng_action_community_added(
		tr::now,
		lt_from,
		tr::bold(from->shortName()),
		lt_community,
		tr::bold(_community->name()),
		tr::rich);
}

rpl::producer<QString> CommunityAdded::button() {
	return tr::lng_community_view();
}

void CommunityAdded::draw(
		Painter &p,
		const PaintContext &context,
		const QRect &geometry) {
	if (!_thumbnail) {
		_thumbnail = _community->hasUserpic()
			? Ui::MakeUserpicThumbnail(_community)
			: Ui::MakeIconThumbnail(st::menuIconGroups);
	}
	if (!_subscribed) {
		_subscribed = true;
		_parent->history()->owner().registerHeavyViewPart(_parent);
		const auto raw = _parent;
		_thumbnail->subscribeToUpdates([raw] {
			raw->history()->owner().requestViewRepaint(raw);
		});
	}
	p.drawImage(geometry.topLeft(), _thumbnail->image(geometry.width()));
}

ClickHandlerPtr CommunityAdded::createViewLink() {
	const auto community = _community;
	return std::make_shared<LambdaClickHandler>([=](ClickContext context) {
		const auto my = context.other.value<ClickHandlerContext>();
		if (const auto controller = my.sessionWindow.get()) {
			controller->showPeerInfo(community);
		}
	});
}

void CommunityAdded::stickerClearLoopPlayed() {
}

std::unique_ptr<StickerPlayer> CommunityAdded::stickerTakePlayer(
		not_null<DocumentData*> data,
		const Lottie::ColorReplacements *replacements) {
	return nullptr;
}

bool CommunityAdded::hasHeavyPart() {
	return _subscribed;
}

void CommunityAdded::unloadHeavyPart() {
	if (_subscribed) {
		_subscribed = false;
		if (_thumbnail) {
			_thumbnail->subscribeToUpdates(nullptr);
		}
	}
}

} // namespace HistoryView
