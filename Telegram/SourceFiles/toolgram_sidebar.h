// ToolGram: custom left-bar (filters sidebar) background color setting.
#pragma once

#include <QColor>
#include <QString>

namespace ToolGram {

// Returns true if a custom sidebar color was saved; writes out to *out.
bool SidebarHasCustomColor(QColor &out);

// Resolve the effective sidebar color: custom if present, else falls back to
// the default passed in.
inline QColor SidebarEffectiveColor(QColor fallback) {
	QColor custom;
	return SidebarHasCustomColor(custom) ? custom : fallback;
}

// Save the custom color (writes the json file next to tdata).
void SidebarSetCustomColor(const QColor &color);

// Removes the custom setting (revert to default).
void SidebarResetColor();

// Convenience: full list of NiN preset colors (name, hex) // handled in box.

} // namespace ToolGram
