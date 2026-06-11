/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/components/ephemeral_messages.h"

#include "api/api_common.h"
#include "api/api_text_entities.h"
#include "apiwrap.h"
#include "base/random.h"
#include "base/unixtime.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_peer_bot_command.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_edition.h"
#include "main/main_session.h"

namespace Data {
namespace {

constexpr auto kKeepDuration = TimeId(2 * 86400);
constexpr auto kPruneEach = 3600 * crl::time(1000);
constexpr auto kPendingFlushDelay = 2 * crl::time(1000);

struct ParsedCommand {
	QString command;
	QString username;
};

[[nodiscard]] std::optional<ParsedCommand> ParseCommand(
		const QString &text) {
	if (!text.startsWith(QChar('/'))) {
		return std::nullopt;
	}
	const auto size = int(text.size());
	const auto good = [](QChar ch) {
		return ch.isLetterOrNumber() || (ch == QChar('_'));
	};
	auto i = 1;
	while (i < size && good(text[i])) {
		++i;
	}
	auto result = ParsedCommand{ .command = text.mid(1, i - 1) };
	if (result.command.isEmpty()) {
		return std::nullopt;
	}
	if (i < size && text[i] == QChar('@')) {
		const auto from = ++i;
		while (i < size && good(text[i])) {
			++i;
		}
		result.username = text.mid(from, i - from);
		if (result.username.isEmpty()) {
			return std::nullopt;
		}
	}
	if (i < size && !text[i].isSpace()) {
		return std::nullopt;
	}
	return result;
}

} // namespace

EphemeralMessages::EphemeralMessages(not_null<Main::Session*> session)
: _session(session)
, _pruneTimer([=] { pruneOld(); })
, _pendingTimer([=] { drainPending(true); }) {
	_session->data().itemRemoved(
	) | rpl::on_next([=](not_null<const HistoryItem*> item) {
		if (item->isEphemeral()) {
			itemRemoved(item);
		}
	}, _lifetime);

	_pruneTimer.callEach(kPruneEach);
}

EphemeralMessages::~EphemeralMessages() = default;

void EphemeralMessages::apply(const MTPDupdateNewEphemeralMessage &update) {
	applyOrDefer(update.vmessage());
}

void EphemeralMessages::applyOrDefer(const MTPEphemeralMessage &message) {
	if (replyTargetMissing(message.data())) {
		_pending.push_back(message);
		_pendingTimer.callOnce(kPendingFlushDelay);
		return;
	}
	applyNew(message.data());
	drainPending();
}

bool EphemeralMessages::replyTargetMissing(
		const MTPDephemeralMessage &data) const {
	const auto header = data.vreply_to();
	if (!header) {
		return false;
	}
	return header->match([&](const MTPDmessageReplyHeader &reply) {
		const auto replyToId = reply.vreply_to_msg_id();
		if (!reply.is_reply_to_ephemeral() || !replyToId) {
			return false;
		}
		const auto history = _session->data().history(
			peerFromMTP(data.vpeer_id()));
		return !lookupItem(history->peer, replyToId->v);
	}, [](const MTPDmessageReplyStoryHeader &) {
		return false;
	});
}

void EphemeralMessages::drainPending(bool force) {
	while (!_pending.empty()) {
		const auto i = ranges::find_if(_pending, [&](
				const MTPEphemeralMessage &message) {
			return force || !replyTargetMissing(message.data());
		});
		if (i == end(_pending)) {
			return;
		}
		const auto message = *i;
		_pending.erase(i);
		applyNew(message.data());
	}
}

void EphemeralMessages::apply(const MTPDupdateEditEphemeralMessage &update) {
	const auto &data = update.vmessage().data();
	const auto history = _session->data().history(
		peerFromMTP(data.vpeer_id()));
	const auto item = lookupItem(history->peer, data.vid().v);
	if (!item) {
		applyNew(data);
		return;
	}
	auto edition = HistoryMessageEdition();
	edition.isEditHide = true;
	edition.editDate = -1;
	edition.useSameViews = true;
	edition.useSameForwards = true;
	edition.useSameReplies = true;
	edition.useSameReactions = true;
	edition.useSameSuggest = true;
	edition.textWithEntities = {
		qs(data.vmessage()),
		Api::EntitiesFromMTP(
			_session,
			data.ventities().value_or_empty()),
	};
	edition.replyMarkup = HistoryMessageMarkupData(data.vreply_markup());
	edition.mtpMedia = data.vmedia();
	item->applyEdition(std::move(edition));
}

void EphemeralMessages::apply(
		const MTPDupdateDeleteEphemeralMessages &update) {
	const auto history = _session->data().historyLoaded(
		peerFromMTP(update.vpeer()));
	if (!history) {
		return;
	}
	for (const auto &id : update.vids().v) {
		if (const auto item = lookupItem(history->peer, id.v)) {
			item->destroy();
		}
	}
}

HistoryItem *EphemeralMessages::applyNew(const MTPDephemeralMessage &data) {
	const auto history = _session->data().history(
		peerFromMTP(data.vpeer_id()));
	const auto ephemeralId = data.vid().v;
	if (const auto existing = lookupItem(history->peer, ephemeralId)) {
		return existing;
	}
	const auto fromId = peerFromMTP(data.vfrom_id());
	auto replyTo = FullReplyTo();
	if (const auto topMsgId = data.vtop_msg_id()) {
		replyTo.topicRootId = topMsgId->v;
	}
	if (const auto header = data.vreply_to()) {
		header->match([&](const MTPDmessageReplyHeader &reply) {
			const auto replyToId = reply.vreply_to_msg_id();
			if (reply.is_reply_to_ephemeral() && replyToId) {
				const auto target = lookupItem(history->peer, replyToId->v);
				if (target) {
					replyTo.messageId = target->fullId();
				}
			}
		}, [](const MTPDmessageReplyStoryHeader &) {
		});
	}
	const auto item = history->addNewLocalMessage(
		{
			.id = _session->data().nextLocalMessageId(),
			.flags = (MessageFlag::HistoryEntry
				| MessageFlag::Ephemeral
				| (data.is_out() ? MessageFlag::Outgoing : MessageFlag())
				| (fromId ? MessageFlag::HasFromId : MessageFlag())
				| (replyTo.messageId
					? MessageFlag::HasReplyInfo
					: MessageFlag())
				| (data.vreply_markup()
					? MessageFlag::HasReplyMarkup
					: MessageFlag())),
			.from = fromId,
			.replyTo = replyTo,
			.date = data.vdate().v,
			.markup = HistoryMessageMarkupData(data.vreply_markup()),
		},
		TextWithEntities {
			qs(data.vmessage()),
			Api::EntitiesFromMTP(
				_session,
				data.ventities().value_or_empty()),
		},
		data.vmedia()
			? *data.vmedia()
			: MTPMessageMedia(MTP_messageMediaEmpty()));
	_data[history].push_back({
		.ephemeralId = ephemeralId,
		.receiverId = UserId(data.vreceiver_id()),
		.item = item,
	});
	_session->data().requestItemResize(item);
	return item;
}

HistoryItem *EphemeralMessages::lookupItem(
		not_null<PeerData*> peer,
		int32 ephemeralId) const {
	const auto history = _session->data().historyLoaded(peer);
	if (!history) {
		return nullptr;
	}
	const auto i = _data.find(history);
	if (i == end(_data)) {
		return nullptr;
	}
	const auto j = ranges::find(
		i->second,
		ephemeralId,
		&Entry::ephemeralId);
	return (j != end(i->second)) ? j->item.get() : nullptr;
}

int32 EphemeralMessages::lookupId(not_null<const HistoryItem*> item) const {
	const auto entry = findByItem(item);
	return entry ? entry->ephemeralId : 0;
}

UserData *EphemeralMessages::replyReceiver(
		not_null<const HistoryItem*> item) const {
	if (const auto entry = findByItem(item)) {
		return botForSending(*entry);
	}
	if (item->out() && item->isEphemeral()) {
		const auto target = _session->data().message(
			item->replyTo().messageId);
		if (target && target->isEphemeral() && !target->out()) {
			const auto from = target->from();
			return from ? from->asUser() : nullptr;
		}
	}
	return nullptr;
}

bool EphemeralMessages::wouldSend(const Api::MessageToSend &message) const {
	const auto history = message.action.history;
	const auto peer = history->peer;
	if (!peer->isChat() && !peer->isMegagroup()) {
		return false;
	}
	const auto replyTo = _session->data().message(
		message.action.replyTo.messageId);
	if (replyTo && replyTo->isEphemeral()) {
		return true;
	} else if (message.action.replyTo) {
		return false;
	}
	return findCommandBot(peer, message.textWithTags.text.trimmed())
		!= nullptr;
}

bool EphemeralMessages::trySend(const Api::MessageToSend &message) {
	const auto history = message.action.history;
	const auto peer = history->peer;
	if (!peer->isChat() && !peer->isMegagroup()) {
		return false;
	} else if (message.action.options.scheduled
		|| message.action.options.shortcutId) {
		if (wouldSend(message)) {
			LOG(("API Error: "
				"Dropping a scheduled ephemeral message send."));
			return true;
		}
		return false;
	}
	auto text = TextWithEntities{
		message.textWithTags.text,
		TextUtilities::ConvertTextTagsToEntities(message.textWithTags.tags),
	};
	TextUtilities::Trim(text);
	if (text.text.isEmpty()) {
		return false;
	}
	const auto replyToId = message.action.replyTo.messageId;
	if (const auto replyTo = _session->data().message(replyToId)) {
		if (replyTo->isEphemeral()) {
			const auto entry = findByItem(replyTo);
			const auto bot = entry ? botForSending(*entry) : nullptr;
			if (bot) {
				send(history, bot, std::move(text), entry->ephemeralId);
			}
			return true;
		}
	}
	if (message.action.replyTo) {
		return false;
	}
	const auto bot = findCommandBot(peer, text.text);
	if (!bot) {
		return false;
	}
	send(history, bot, std::move(text));
	return true;
}

UserData *EphemeralMessages::findCommandBot(
		not_null<PeerData*> peer,
		const QString &text) const {
	const auto parsed = ParseCommand(text);
	if (!parsed) {
		return nullptr;
	}
	const auto &commands = peer->isChat()
		? peer->asChat()->botCommands()
		: peer->asMegagroup()->mgInfo->botCommands();
	for (const auto &[userId, list] : commands) {
		const auto user = _session->data().userLoaded(userId);
		if (!user
			|| (!parsed->username.isEmpty()
				&& parsed->username.compare(
					user->username(),
					Qt::CaseInsensitive) != 0)) {
			continue;
		}
		for (const auto &command : list) {
			if (command.ephemeral
				&& !command.command.compare(
					parsed->command,
					Qt::CaseInsensitive)) {
				return user;
			}
		}
	}
	return nullptr;
}

void EphemeralMessages::send(
		not_null<History*> history,
		not_null<UserData*> bot,
		TextWithEntities text,
		int32 replyToEphemeralId) {
	const auto session = _session;
	const auto entities = Api::EntitiesToMTP(
		session,
		text.entities,
		Api::ConvertOption::SkipLocal);
	using Flag = MTPephemeral_SendMessage::Flag;
	const auto flags = Flag(0)
		| (entities.v.isEmpty() ? Flag(0) : Flag::f_entities)
		| (replyToEphemeralId ? Flag::f_reply_to : Flag(0));
	session->api().request(MTPephemeral_SendMessage(
		MTP_flags(flags),
		history->peer->input(),
		bot->inputUser(),
		MTPlong(), // query_id
		MTP_string(text.text),
		entities,
		MTPInputMedia(),
		MTPReplyMarkup(),
		MTPInputRichMessage(),
		MTP_long(base::RandomValue<uint64>()),
		MTP_inputReplyToEphemeralMessage(MTP_int(replyToEphemeralId))
	)).done([=](const MTPUpdates &result) {
		session->api().applyUpdates(result);
	}).fail([=](const MTP::Error &error) {
		LOG(("API Error: send ephemeral message - %1").arg(error.type()));
	}).send();
}

void EphemeralMessages::deleteMessage(not_null<HistoryItem*> item) {
	if (const auto entry = findByItem(item)) {
		const auto receiver = _session->data().user(entry->receiverId);
		_session->api().request(MTPephemeral_DeleteMessage(
			item->history()->peer->input(),
			receiver->inputUser(),
			MTP_int(entry->ephemeralId)
		)).send();
	}
	item->destroy();
}

const EphemeralMessages::Entry *EphemeralMessages::findByItem(
		not_null<const HistoryItem*> item) const {
	const auto i = _data.find(item->history());
	if (i == end(_data)) {
		return nullptr;
	}
	const auto j = ranges::find(i->second, item.get(), &Entry::item);
	return (j != end(i->second)) ? &*j : nullptr;
}

UserData *EphemeralMessages::botForSending(const Entry &entry) const {
	if (entry.item->out()) {
		return _session->data().userLoaded(entry.receiverId);
	}
	const auto from = entry.item->from();
	return from ? from->asUser() : nullptr;
}

void EphemeralMessages::itemRemoved(not_null<const HistoryItem*> item) {
	const auto i = _data.find(item->history());
	if (i == end(_data)) {
		return;
	}
	const auto j = ranges::find(i->second, item.get(), &Entry::item);
	if (j == end(i->second)) {
		return;
	}
	i->second.erase(j);
	if (i->second.empty()) {
		_data.erase(i);
	}
}

void EphemeralMessages::pruneOld() {
	const auto till = base::unixtime::now() - kKeepDuration;
	auto old = std::vector<not_null<HistoryItem*>>();
	for (const auto &[history, list] : _data) {
		for (const auto &entry : list) {
			if (entry.item->date() <= till) {
				old.push_back(entry.item);
			}
		}
	}
	for (const auto &item : old) {
		item->destroy();
	}
}

} // namespace Data
