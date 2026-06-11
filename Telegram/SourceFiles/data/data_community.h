/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class ChannelData;
class PeerData;

namespace Data {

struct CommunityLinkedPeer {
	not_null<PeerData*> peer;
	std::optional<bool> visible;

	friend inline bool operator==(
		const CommunityLinkedPeer &,
		const CommunityLinkedPeer &) = default;
};

class CommunityInfo final {
public:
	explicit CommunityInfo(not_null<ChannelData*> channel);

	[[nodiscard]] not_null<ChannelData*> channel() const {
		return _channel;
	}

	void applyLinkedPeers(const QVector<MTPCommunityPeer> &list);
	[[nodiscard]] auto linkedPeers() const
	-> const std::vector<CommunityLinkedPeer> & {
		return _linkedPeers;
	}
	[[nodiscard]] rpl::producer<> linkedPeersValue() const;

private:
	const not_null<ChannelData*> _channel;
	std::vector<CommunityLinkedPeer> _linkedPeers;
	rpl::event_stream<> _linkedPeersChanges;

};

} // namespace Data
