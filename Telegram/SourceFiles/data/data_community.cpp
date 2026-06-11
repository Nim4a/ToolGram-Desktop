/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_community.h"

#include "data/data_channel.h"
#include "data/data_peer.h"
#include "data/data_session.h"

namespace Data {

CommunityInfo::CommunityInfo(not_null<ChannelData*> channel)
: _channel(channel) {
}

void CommunityInfo::applyLinkedPeers(const QVector<MTPCommunityPeer> &list) {
	auto now = std::vector<CommunityLinkedPeer>();
	now.reserve(list.size());
	auto &owner = _channel->owner();
	for (const auto &entry : list) {
		const auto &data = entry.data();
		auto visible = std::optional<bool>();
		if (const auto value = data.vvisible()) {
			visible = mtpIsTrue(*value);
		}
		now.push_back({
			.peer = owner.peer(peerFromMTP(data.vpeer())),
			.visible = visible,
		});
	}
	if (_linkedPeers != now) {
		_linkedPeers = std::move(now);
		_linkedPeersChanges.fire({});
	}
}

rpl::producer<> CommunityInfo::linkedPeersValue() const {
	return rpl::single(
		rpl::empty
	) | rpl::then(_linkedPeersChanges.events());
}

} // namespace Data
