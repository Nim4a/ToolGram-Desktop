// ToolGram: settings box to pick the left sidebar color (NiN presets).
#include "toolgram_sidebar_box.h"
#include "toolgram_sidebar.h"

#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "mainwidget.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"

namespace {

struct NiNColor {
	const char *name;
	const char *hex;
};

const NiNColor kNiNColors[] = {
	{ "NiN Olive",      "#97A87A" },
	{ "NiN Teal Blue",  "#2D7495" },
	{ "NiN Steel Blue", "#455B8A" },
	{ "NiN Deep Green", "#2A6B5C" },
	{ "NiN Wine",       "#972828" },
	{ "NiN Mauve",      "#A290B7" },
	{ "NiN Crimson",    "#CB2957" },
};

} // namespace

void ToolGramSidebarColorBox(
		not_null<Ui::GenericBox *> box,
		not_null<Window::SessionController *> controller) {
	box->setTitle(u"رنگ نوار سمت چپ"_q);
	box->setWidth(st::boxWidth);

	const auto content = box->verticalLayout();

	const auto refresh = [=] {
		controller->content()->update();
	};

	const auto apply = [=](const QString &hex) {
		if (hex.isEmpty()) {
			ToolGram::SidebarResetColor();
		} else {
			ToolGram::SidebarSetCustomColor(QColor(hex));
		}
		refresh();
	};

	const auto makeRow = [&](const QString &name, const QString &hex) {
		const auto button = content->add(
			object_ptr<Ui::RoundButton>(
				content.get(),
				rpl::single(name),
				st::defaultActiveButton));
		button->setClickedCallback([=] {
			apply(hex);
			box->closeBox();
		});
		const auto skip = content->add(object_ptr<Ui::RpWidget>(content));
		skip->setFixedHeight(st::settingsCheckboxesSkip);
		return button;
	};

	content->add(object_ptr<Ui::FlatLabel>(
		content.get(),
		u"یک رنگ برای نوار سمت چپ انتخاب کن:"_q,
		st::boxLabel));
	{
		const auto skip = content->add(object_ptr<Ui::RpWidget>(content));
		skip->setFixedHeight(st::settingsCheckboxesSkip);
	}

	for (const auto &entry : kNiNColors) {
		makeRow(QString::fromUtf8(entry.name), QString::fromUtf8(entry.hex));
	}

	makeRow(u"پیش‌فرض (آبی تیره)"_q, QString());

	box->addButton(rpl::single(u"بستن"_q), [=] { box->closeBox(); });
}
