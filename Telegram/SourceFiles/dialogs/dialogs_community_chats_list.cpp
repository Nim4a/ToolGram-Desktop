/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "dialogs/dialogs_community_chats_list.h"

#include "data/data_channel.h"
#include "data/data_community.h"
#include "data/data_session.h"
#include "dialogs/ui/dialogs_layout.h"
#include "dialogs/dialogs_indexed_list.h"
#include "dialogs/dialogs_main_list.h"
#include "dialogs/dialogs_row.h"
#include "history/history.h"
#include "main/main_session.h"
#include "ui/painter.h"
#include "window/window_session_controller.h"
#include "styles/style_dialogs.h"

namespace Dialogs {

CommunityChatsList::CommunityChatsList(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<Data::CommunityInfo*> community,
	CommunityChatsKind kind)
: RpWidget(parent)
, _controller(controller)
, _community(community)
, _kind(kind)
, _st(&st::defaultDialogRow) {
	setMouseTracking(true);
	rebuild();

	_community->linkedPeersValue(
	) | rpl::on_next([=] {
		rebuild();
	}, lifetime());

	using Event = Data::Session::ChatListEntryRefresh;
	_controller->session().data().chatListEntryRefreshes(
	) | rpl::filter([=](const Event &event) {
		const auto history = event.key.history();
		return history && (history->communityListInfo() == _community);
	}) | rpl::on_next([=](const Event &event) {
		if (event.existenceChanged) {
			rebuild();
		} else {
			update();
		}
	}, lifetime());
}

CommunityChatsList::~CommunityChatsList() = default;

void CommunityChatsList::rebuild() {
	const auto wasCount = int(_rows.size());
	_rows.clear();
	setSelected(-1);
	setPressed(-1);
	const auto owner = &_controller->session().data();
	const auto add = [&](not_null<History*> history) {
		auto row = std::make_unique<Row>(Key(history), 0, 0);
		row->recountHeight(0., FilterId());
		_rows.push_back(std::move(row));
	};
	if (_kind == CommunityChatsKind::Joined) {
		for (const auto &row : *_community->chatsList()->indexed()) {
			if (const auto history = row->history()) {
				add(history);
			}
		}
	} else {
		for (const auto &linked : _community->linkedPeers()) {
			const auto channel = linked.peer->asChannel();
			if (channel && channel->amIn()) {
				continue;
			} else if (!Data::IsCommunityChatViewable(linked)) {
				continue;
			}
			add(owner->history(linked.peer));
		}
	}
	_count = int(_rows.size());
	if (int(_rows.size()) != wasCount) {
		resizeToWidth(width());
	}
	update();
}

int CommunityChatsList::rowTop(int index) const {
	auto result = 0;
	for (auto i = 0; i != index; ++i) {
		result += _rows[i]->height();
	}
	return result;
}

int CommunityChatsList::resizeGetHeight(int newWidth) {
	auto result = 0;
	for (const auto &row : _rows) {
		result += row->height();
	}
	return result;
}

void CommunityChatsList::paintEvent(QPaintEvent *e) {
	Painter p(this);

	const auto clip = e->rect();
	p.fillRect(clip, st::dialogsBg);
	if (_rows.empty()) {
		return;
	}
	const auto paused = _controller->isGifPausedAtLeastFor(
		Window::GifPauseReason::Any);
	auto context = Ui::PaintContext{
		.st = _st,
		.community = _community,
		.currentBg = st::dialogsBg,
		.now = crl::now(),
		.width = width(),
		.paused = paused,
		.insideCommunity = true,
	};
	const auto pressed = (_pressed >= 0);
	auto top = 0;
	for (auto i = 0, count = int(_rows.size()); i != count; ++i) {
		const auto height = _rows[i]->height();
		if (top + height <= clip.top()) {
			top += height;
			continue;
		} else if (top >= clip.top() + clip.height()) {
			break;
		}
		context.active = false;
		context.selected = pressed
			? (_pressed == i)
			: (_selected == i);
		p.translate(0, top);
		Ui::RowPainter::Paint(p, _rows[i].get(), nullptr, context);
		p.translate(0, -top);
		top += height;
	}
}

void CommunityChatsList::updateSelected(QPoint local) {
	auto selected = -1;
	if (local.y() >= 0) {
		auto top = 0;
		for (auto i = 0, count = int(_rows.size()); i != count; ++i) {
			const auto bottom = top + _rows[i]->height();
			if (local.y() < bottom) {
				selected = i;
				break;
			}
			top = bottom;
		}
	}
	setSelected(selected);
	setCursor((_selected >= 0) ? style::cur_pointer : style::cur_default);
}

void CommunityChatsList::setSelected(int selected) {
	if (_selected == selected) {
		return;
	}
	if (_selected >= 0 && _selected < int(_rows.size())) {
		update(0, rowTop(_selected), width(), _rows[_selected]->height());
	}
	_selected = selected;
	if (_selected >= 0 && _selected < int(_rows.size())) {
		update(0, rowTop(_selected), width(), _rows[_selected]->height());
	}
}

void CommunityChatsList::setPressed(int pressed) {
	if (_pressed == pressed) {
		return;
	}
	if (_pressed >= 0 && _pressed < int(_rows.size())) {
		_rows[_pressed]->stopLastRipple();
	}
	_pressed = pressed;
}

void CommunityChatsList::mouseMoveEvent(QMouseEvent *e) {
	updateSelected(e->pos());
}

void CommunityChatsList::mousePressEvent(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton) {
		return;
	}
	updateSelected(e->pos());
	setPressed(_selected);
	if (_pressed >= 0) {
		const auto top = rowTop(_pressed);
		const auto height = _rows[_pressed]->height();
		_rows[_pressed]->addRipple(
			e->pos() - QPoint(0, top),
			QSize(width(), height),
			[=] { update(0, top, width(), height); });
	}
}

void CommunityChatsList::mouseReleaseEvent(QMouseEvent *e) {
	const auto pressed = _pressed;
	setPressed(-1);
	if (pressed >= 0 && pressed == _selected && pressed < int(_rows.size())) {
		if (const auto history = _rows[pressed]->history()) {
			_chatChosen.fire_copy(history);
		}
	}
}

void CommunityChatsList::enterEventHook(QEnterEvent *e) {
	updateSelected(mapFromGlobal(QCursor::pos()));
}

void CommunityChatsList::leaveEventHook(QEvent *e) {
	setSelected(-1);
	setCursor(style::cur_default);
}

rpl::producer<not_null<History*>> CommunityChatsList::chatChosen() const {
	return _chatChosen.events();
}

rpl::producer<int> CommunityChatsList::countValue() const {
	return _count.value();
}

} // namespace Dialogs
