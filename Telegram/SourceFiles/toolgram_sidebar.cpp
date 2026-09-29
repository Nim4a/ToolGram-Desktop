// ToolGram: custom left-bar (filters sidebar) background color setting.
#include "toolgram_sidebar.h"

#include "core/application.h"
#include "core/launcher.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace ToolGram {
namespace {

QString SidebarSettingsPath() {
	return cWorkingDir() + u"tdata/toolgram_sidebar.json"_q;
}

QColor ReadSavedColor() {
	QFile f(SidebarSettingsPath());
	if (!f.open(QIODevice::ReadOnly)) {
		return QColor();
	}
	const auto doc = QJsonDocument::fromJson(f.readAll());
	if (!doc.isObject()) {
		return QColor();
	}
	const auto obj = doc.object();
	const auto hex = obj.value(u"color"_q).toString();
	if (hex.isEmpty()) {
		return QColor();
	}
	const auto color = QColor(hex);
	return color.isValid() ? color : QColor();
}

} // namespace

bool SidebarHasCustomColor(QColor &out) {
	const auto color = ReadSavedColor();
	if (!color.isValid() || !color.alpha()) {
		return false;
	}
	out = color;
	return true;
}

void SidebarSetCustomColor(const QColor &color) {
	QFile f(SidebarSettingsPath());
	if (!f.open(QIODevice::WriteOnly)) {
		return;
	}
	QJsonObject obj;
	obj.insert(u"color"_q, color.name(QColor::HexArgb));
	f.write(QJsonDocument(obj).toJson());
	f.close();
}

void SidebarResetColor() {
	QFile::remove(SidebarSettingsPath());
}

} // namespace ToolGram
