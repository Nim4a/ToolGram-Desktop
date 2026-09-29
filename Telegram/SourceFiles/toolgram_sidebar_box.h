// ToolGram: settings box to pick the left sidebar (folder bar) color.
#pragma once

#include "ui/layers/generic_box.h"

namespace Window {
class SessionController;
} // namespace Window

void ToolGramSidebarColorBox(
	not_null<Ui::GenericBox *> box,
	not_null<Window::SessionController *> controller);
