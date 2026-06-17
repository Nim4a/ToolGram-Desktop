/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "history/view/media/history_view_service_box.h"

class ChannelData;

namespace Data {
class MediaCommunityAdded;
} // namespace Data

namespace Ui {
class DynamicImage;
} // namespace Ui

namespace HistoryView {

class CommunityAdded final
	: public ServiceBoxContent
	, public base::has_weak_ptr {
public:
	CommunityAdded(
		not_null<Element*> parent,
		not_null<Data::MediaCommunityAdded*> media);
	~CommunityAdded();

	int top() override;
	QSize size() override;
	TextWithEntities title() override;
	TextWithEntities subtitle() override;
	rpl::producer<QString> button() override;
	void draw(
		Painter &p,
		const PaintContext &context,
		const QRect &geometry) override;
	ClickHandlerPtr createViewLink() override;

	bool hideServiceText() override {
		return true;
	}

	void stickerClearLoopPlayed() override;
	std::unique_ptr<StickerPlayer> stickerTakePlayer(
		not_null<DocumentData*> data,
		const Lottie::ColorReplacements *replacements) override;

	bool hasHeavyPart() override;
	void unloadHeavyPart() override;

private:
	const not_null<Element*> _parent;
	const not_null<ChannelData*> _community;
	std::shared_ptr<Ui::DynamicImage> _thumbnail;
	bool _subscribed = false;

};

} // namespace HistoryView
