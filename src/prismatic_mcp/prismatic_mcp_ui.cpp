/*
 * This file is part of korczis's research fork of OpenTTD and is NOT part of upstream OpenTTD.
 * RESEARCH-ONLY / UNSTABLE.
 *
 * The user interface of a GUI process as data and as input, for an external controller that
 * plays the way a person does: it reads the windows and widgets on screen, finds a button by the
 * text a person would read, moves the visible cursor to it, presses, drags across tiles, and the
 * game reacts through its ordinary input path. Nothing here is a list of features: every window,
 * widget, string and rectangle is read from the window system as it stands.
 *
 * Observer effects: input tools change the local cursor and button state and call the entry
 * points the video driver calls; a click that a person's click would turn into a command does so
 * here too, and that command reaches the server through the network like any client's. Read
 * tools change nothing.
 */

/** @file prismatic_mcp_ui.cpp The user interface as data and as input (research-only). */

#include "../stdafx.h"

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "prismatic_mcp_ui.h"

#include "../company_func.h"
#include "../core/backup_type.hpp"
#include "../gfx_func.h"
#include "../landscape.h"
#include "../map_func.h"
#include "../network/network.h"
#include "../strings_func.h"
#include "../tile_map.h"
#include "../tilehighlight_func.h"
#include "../table/strings.h"
#include "prismatic_mcp_hooks.h"
#include "../vehicle_type.h"
#include "../viewport_func.h"
#include "../widget_type.h"
#include "../window_func.h"
#include "../window_gui.h"
#include "../zoom_func.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>

#include "../safeguards.h"

namespace PrismaticMCP::UI {

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

namespace {

/** How long input stays paused after the real mouse moved: the owner has taken over. */
constexpr auto OWNER_GRACE = std::chrono::seconds(3);

/** Where the injected pointer was left, and until when the owner holds the mouse. */
struct InputState {
	Point last{-1, -1};           ///< The cursor position after our last move; -1 when unknown.
	Clock::time_point owner_until{}; ///< Input is refused until then: the real mouse moved.
	uint64_t moves = 0;           ///< Pointer moves injected so far.
	uint64_t presses = 0;         ///< Button presses injected so far.
	uint64_t keys = 0;            ///< Key presses injected so far.
};

InputState _state;

json Error(std::string message)
{
	return json{{"ok", false}, {"error", std::move(message)}};
}

/** Input is the owner's switch for their own process, given in its environment at start. */
bool Armed()
{
	static const bool armed = [] {
		const char *v = std::getenv("PRISMATIC_MCP_INPUT");
		return v != nullptr && std::string_view(v) == "1";
	}();
	return armed;
}

/**
 * Has the owner moved the real mouse since our last move? Then they hold it for a while.
 * The video driver writes _cursor.pos on every real mouse event; a position we did not leave
 * there can only come from the owner's hand.
 */
bool OwnerHasMouse()
{
	if (_state.last.x >= 0 && (_cursor.pos.x != _state.last.x || _cursor.pos.y != _state.last.y)) {
		_state.owner_until = Clock::now() + OWNER_GRACE;
		_state.last = {-1, -1};
	}
	return Clock::now() < _state.owner_until;
}

/** Run an input entry point the way a video driver does: as the local company. */
template <typename F>
void AsDriver(F &&f)
{
	Backup<CompanyID> cur_company(_current_company, _local_company);
	f();
	cur_company.Restore();
}

std::string_view TypeName(WidgetType type)
{
	switch (static_cast<WidgetType>(type & WWT_MASK)) {
		case WWT_EMPTY: return "empty";
		case WWT_PANEL: return "panel";
		case WWT_INSET: return "inset";
		case WWT_IMGBTN: return "image_button";
		case WWT_IMGBTN_2: return "image_button";
		case WWT_ARROWBTN: return "arrow_button";
		case WWT_TEXTBTN: return "text_button";
		case WWT_TEXTBTN_2: return "text_button";
		case WWT_BOOLBTN: return "bool_button";
		case WWT_IMGTEXTBTN: return "image_text_button";
		case WWT_LABEL: return "label";
		case WWT_TEXT: return "text";
		case WWT_MATRIX: return "matrix";
		case WWT_FRAME: return "frame";
		case WWT_CAPTION: return "caption";
		case WWT_DEBUGBOX: return "debug_box";
		case WWT_SHADEBOX: return "shade_box";
		case WWT_DEFSIZEBOX: return "default_size_box";
		case WWT_STICKYBOX: return "sticky_box";
		case WWT_RESIZEBOX: return "resize_box";
		case WWT_CLOSEBOX: return "close_box";
		case WWT_DROPDOWN: return "dropdown";
		case WWT_EDITBOX: return "edit_box";
		default: break;
	}
	switch (type) {
		case NWID_VIEWPORT: return "viewport";
		case NWID_BUTTON_DROPDOWN: return "button_dropdown";
		case NWID_HSCROLLBAR: return "hscrollbar";
		case NWID_VSCROLLBAR: return "vscrollbar";
		case NWID_CUSTOM: return "custom";
		default: return "other";
	}
}

/** Does this widget show its own string (rather than a sprite)? */
bool ShowsText(WidgetType type)
{
	switch (static_cast<WidgetType>(type & WWT_MASK)) {
		case WWT_TEXTBTN: case WWT_TEXTBTN_2: case WWT_LABEL: case WWT_TEXT: case WWT_FRAME:
		case WWT_CAPTION: case WWT_DROPDOWN: case WWT_IMGTEXTBTN:
			return true;
		default:
			return type == NWID_BUTTON_DROPDOWN;
	}
}

std::string Text(StringID id)
{
	if (id == STR_NULL || id == INVALID_STRING_ID) return {};
	return GetString(id);
}

json Rect(int left, int top, int width, int height)
{
	return json{{"x", left}, {"y", top}, {"w", width}, {"h", height},
		{"cx", left + width / 2}, {"cy", top + height / 2}};
}

json Widget(const Window *w, WidgetID index, const NWidgetBase *nwb)
{
	json out = {{"index", index}, {"type", TypeName(nwb->type)},
		{"rect", Rect(w->left + nwb->pos_x, w->top + nwb->pos_y, nwb->current_x, nwb->current_y)}};
	if (auto nwc = dynamic_cast<const NWidgetCore *>(nwb); nwc != nullptr) {
		if (ShowsText(nwb->type)) {
			std::string text = w->GetWidgetString(index, nwc->GetString());
			if (!text.empty()) out["text"] = std::move(text);
		}
		if (std::string tip = Text(nwc->GetToolTip()); !tip.empty()) out["tooltip"] = std::move(tip);
		/* A matrix draws rows of its own; a person clicks a row, so say how tall one is. */
		if ((nwb->type & WWT_MASK) == WWT_MATRIX && nwb->resize_y > 0) out["row_height"] = nwb->resize_y;
		out["disabled"] = nwc->IsDisabled();
		out["lowered"] = nwc->IsLowered();
	}
	return out;
}

json WindowJson(const Window *w, bool widgets)
{
	json out = {{"class", static_cast<int>(w->window_class)}, {"number", static_cast<int>(w->window_number)},
		{"key", std::string(w->window_desc.ini_key)}, {"rect", Rect(w->left, w->top, w->width, w->height)},
		{"viewport", w->viewport != nullptr}};
	json list = json::array();
	const NWidgetBase *caption = nullptr;
	for (const auto &[index, nwb] : w->widget_lookup) {
		if (index < 0 || nwb == nullptr) continue;
		json wj = Widget(w, index, nwb);
		if (wj["type"] == "caption") {
			caption = nwb;
			if (wj.contains("text")) out["caption"] = wj["text"];
		}
		if (widgets) list.push_back(std::move(wj));
	}
	/* The title bar's boxes (close, sticky, shade, default size, debug) carry no index, so the
	 * lookup above never sees them; find them as a person does, along the caption's row. */
	if (widgets && caption != nullptr && w->nested_root != nullptr) {
		const int y = caption->pos_y + static_cast<int>(caption->current_y) / 2;
		std::vector<const NWidgetCore *> seen;
		for (int x = 0; x < w->width; x += 2) {
			const NWidgetCore *nwc = w->nested_root->GetWidgetFromPos(x, y);
			if (nwc == nullptr || nwc->GetIndex() >= 0 || std::ranges::find(seen, nwc) != seen.end()) continue;
			switch (nwc->type) {
				case WWT_CLOSEBOX: case WWT_STICKYBOX: case WWT_SHADEBOX: case WWT_DEFSIZEBOX: case WWT_DEBUGBOX: break;
				default: continue;
			}
			seen.push_back(nwc);
			json wj = Widget(w, -1, nwc);
			wj["index"] = nullptr;
			list.push_back(std::move(wj));
		}
	}
	if (widgets) out["widgets"] = std::move(list);
	/* A dropdown's items are drawn, not widgets: report them as a person sees them. */
	if (auto items = Hooks::DropdownItems(w); !items.empty()) {
		json dd = json::array();
		for (const auto &it : items) {
			dd.push_back({{"result", it.result}, {"selectable", it.selectable}, {"selected", it.selected},
				{"rect", Rect(it.rect.left, it.rect.top, it.rect.Width(), it.rect.Height())}});
		}
		out["items"] = std::move(dd);
	}
	return out;
}

/** The main window, or nullptr in a process without one. */
Window *MainWindow()
{
	if (_network_dedicated) return nullptr;
	return FindWindowById(WindowClass::MainWindow, 0);
}

/** Screen pixel of a tile's centre in a viewport, at the tile's own height. */
Point TileToScreen(const Viewport &vp, TileIndex tile)
{
	int x = TileX(tile) * TILE_SIZE + TILE_SIZE / 2;
	int y = TileY(tile) * TILE_SIZE + TILE_SIZE / 2;
	Point p = RemapCoords(x, y, GetSlopePixelZ(x, y));
	return {vp.left + UnScaleByZoom(p.x - vp.virtual_left, vp.zoom), vp.top + UnScaleByZoom(p.y - vp.virtual_top, vp.zoom)};
}

std::optional<TileIndex> TileArg(const json &args, std::string &error)
{
	if (args.contains("tile") && args["tile"].is_number_unsigned()) {
		TileIndex t{args["tile"].get<uint32_t>()};
		if (t.base() >= Map::Size()) { error = "tile is outside the map"; return std::nullopt; }
		return t;
	}
	if (args.contains("x") && args.contains("y") && args["x"].is_number_unsigned() && args["y"].is_number_unsigned()) {
		uint x = args["x"].get<uint>(), y = args["y"].get<uint>();
		if (x >= Map::SizeX() || y >= Map::SizeY()) { error = "x/y is outside the map"; return std::nullopt; }
		return TileXY(x, y);
	}
	error = "give tile, or x and y";
	return std::nullopt;
}

json TileJson(TileIndex t)
{
	return json{{"tile", t.base()}, {"x", TileX(t)}, {"y", TileY(t)}};
}

json InputState()
{
	json out = {{"ok", true}, {"armed", Armed()}, {"owner_has_mouse", OwnerHasMouse()},
		{"cursor", {{"x", _cursor.pos.x}, {"y", _cursor.pos.y}, {"in_window", _cursor.in_window}, {"sprite", _cursor.sprites.empty() ? 0u : _cursor.sprites[0].image.sprite}}},
		{"buttons", {{"left", IsLeftButtonDown()}, {"right", IsRightButtonDown()}, {"ctrl", IsCtrlPressed()}, {"shift", IsShiftPressed()}}},
		{"screen", {{"w", _screen.width}, {"h", _screen.height}}},
		{"local_company", _local_company.base()},
		{"injected", {{"moves", _state.moves}, {"presses", _state.presses}, {"keys", _state.keys}}},
		{"tool", {{"place_mode", static_cast<int>(_thd.place_mode)}, {"window_class", static_cast<int>(_thd.window_class)},
			{"window_number", static_cast<int>(_thd.window_number)}}},
		{"selection", {{"x", _thd.pos.x / static_cast<int>(TILE_SIZE)}, {"y", _thd.pos.y / static_cast<int>(TILE_SIZE)},
			{"w", _thd.size.x / static_cast<int>(TILE_SIZE)}, {"h", _thd.size.y / static_cast<int>(TILE_SIZE)},
			{"start", {{"x", _thd.selstart.x / static_cast<int>(TILE_SIZE)}, {"y", _thd.selstart.y / static_cast<int>(TILE_SIZE)}}},
			{"end", {{"x", _thd.selend.x / static_cast<int>(TILE_SIZE)}, {"y", _thd.selend.y / static_cast<int>(TILE_SIZE)}}}}}};
	return out;
}

/** Refuse input unless armed and the owner does not hold the mouse; std::nullopt means go. */
std::optional<json> InputRefused()
{
	if (!Armed()) return Error("input is not armed in this process: start it with PRISMATIC_MCP_INPUT=1 (the owner's switch)");
	if (OwnerHasMouse()) return Error("the owner holds the mouse: input pauses until the real mouse rests for a few seconds");
	return std::nullopt;
}

void MovePointer(int x, int y)
{
	x = Clamp(x, 0, _screen.width - 1);
	y = Clamp(y, 0, _screen.height - 1);
	AsDriver([&] {
		_cursor.in_window = true;
		_cursor.UpdateCursorPosition(x, y);
		HandleMouseEvents();
	});
	_state.last = _cursor.pos;
	_state.moves++;
}

std::optional<ControlInput> ButtonArg(const json &args)
{
	std::string b = args.value("button", "left");
	if (b == "left") return ControlInput::Left;
	if (b == "right") return ControlInput::Right;
	if (b == "middle") return ControlInput::Middle;
	return std::nullopt;
}

void SetModifiers(const json &args)
{
	if (!args.contains("mods") || !args["mods"].is_array()) return;
	for (const auto &m : args["mods"]) {
		if (!m.is_string()) continue;
		if (m == "ctrl") _input_pressed.Set(ControlInput::Ctrl);
		if (m == "shift") _input_pressed.Set(ControlInput::Shift);
	}
}

void ClearModifiers()
{
	_input_pressed.Reset(ControlInput::Ctrl);
	_input_pressed.Reset(ControlInput::Shift);
}

json Schema(json properties, json required = json::array())
{
	json s = {{"type", "object"}, {"properties", std::move(properties)}};
	if (!required.empty()) s["required"] = std::move(required);
	return s;
}

/** Lower-case copy, for matching what a widget or window says. */
std::string Lower(std::string s)
{
	for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

/** The caption text of a window, if it has one. */
std::string CaptionOf(const Window *w)
{
	for (const auto &[index, nwb] : w->widget_lookup) {
		if (index < 0 || nwb == nullptr || (nwb->type & WWT_MASK) != WWT_CAPTION) continue;
		if (auto nwc = dynamic_cast<const NWidgetCore *>(nwb); nwc != nullptr) return w->GetWidgetString(index, nwc->GetString());
	}
	return {};
}

/** The front-most window matching {class, number, caption} (any subset; caption is a case-insensitive substring). */
Window *FindWindowBy(const json &sel)
{
	if (!sel.is_object()) return nullptr;
	for (Window *w : Window::IterateFromFront()) {
		if (sel.contains("class") && sel["class"].is_number_integer() && static_cast<int>(w->window_class) != sel["class"].get<int>()) continue;
		if (sel.contains("number") && sel["number"].is_number_integer() && static_cast<int>(w->window_number) != sel["number"].get<int>()) continue;
		if (sel.contains("caption") && sel["caption"].is_string() && Lower(CaptionOf(w)).find(Lower(sel["caption"].get<std::string>())) == std::string::npos) continue;
		return w;
	}
	return nullptr;
}

/** A screen rectangle (this file's Rect() builds JSON, so a plain struct of its own). */
struct Box {
	int left, top, right, bottom;
};

/** A widget's screen rectangle: by index, by what it says (text or tooltip), or a title-bar box by type. */
std::optional<Box> FindWidgetBy(Window *w, const json &sel, json &found)
{
	if (sel.contains("index") && sel["index"].is_number_integer()) {
		const NWidgetBase *nwb = w->GetWidget<NWidgetBase>(sel["index"].get<int>());
		if (nwb == nullptr) return std::nullopt;
		found = Widget(w, sel["index"].get<int>(), nwb);
	} else if (sel.contains("text") && sel["text"].is_string()) {
		const std::string needle = Lower(sel["text"].get<std::string>());
		for (const auto &[index, nwb] : w->widget_lookup) {
			if (index < 0 || nwb == nullptr) continue;
			json wj = Widget(w, index, nwb);
			if (Lower(wj.value("text", "") + " " + wj.value("tooltip", "")).find(needle) != std::string::npos) { found = std::move(wj); break; }
		}
	} else if (sel.contains("type") && sel["type"].is_string()) {
		json all = WindowJson(w, true);
		for (auto &wj : all["widgets"]) if (wj["type"] == sel["type"]) { found = wj; break; }
	}
	if (found.is_null()) return std::nullopt;
	const json &r = found["rect"];
	int x = r["x"].get<int>(), y = r["y"].get<int>();
	return Box{x, y, x + r["w"].get<int>() - 1, y + r["h"].get<int>() - 1};
}

/** Every window as {class, number, caption}, front first: what a press changed is the difference. */
json WindowList()
{
	json list = json::array();
	for (const Window *w : Window::IterateFromFront()) {
		list.push_back({{"class", static_cast<int>(w->window_class)}, {"number", static_cast<int>(w->window_number)}, {"caption", CaptionOf(w)}});
	}
	return list;
}

/** Windows in `after` not in `before`. */
json Difference(const json &after, const json &before)
{
	json out = json::array();
	for (const auto &w : after) {
		bool seen = false;
		for (const auto &b : before) if (b["class"] == w["class"] && b["number"] == w["number"]) { seen = true; break; }
		if (!seen) out.push_back(w);
	}
	return out;
}

/** Press and release a button at a screen point through the driver's path (the cursor moves there first). */
void Click(int x, int y, ControlInput button, const json &mods)
{
	MovePointer(x, y);
	AsDriver([&] {
		SetModifiers(json{{"mods", mods}});
		_input_pressed.Set(button);
		if (button != ControlInput::Left) _input_clicked.Set(button);
		HandleMouseEvents();
		_input_pressed.Reset(button);
		if (button == ControlInput::Left) _input_clicked.Reset(ControlInput::Left);
		HandleMouseEvents();
		ClearModifiers();
	});
	_state.last = _cursor.pos;
	_state.presses++;
}

} // namespace

json Tools()
{
	/* A process without an interface offers none of these: its catalogue is what it can do. */
	if (_network_dedicated) return json::array();
	json xy = {{"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}}};
	json tile = {{"tile", {{"type", "integer"}}}, {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}}};
	json tile_props_click = tile;
	tile_props_click["button"] = {{"type", "string"}, {"enum", {"left", "right"}}};
	json mods = {{"type", "array"}, {"items", {{"type", "string"}, {"enum", {"ctrl", "shift"}}}}};
	json arr = json::array();
	arr.push_back({{"name", "openttd.ui.windows"},
		{"description", "GUI only, read: every open window front to back with its class, number, key, caption and rectangle; widgets: true adds every widget with its type, screen rectangle, resolved text and tooltip, disabled and lowered state; class/number filter"},
		{"inputSchema", Schema({{"widgets", {{"type", "boolean"}}}, {"class", {{"type", "integer"}}}, {"number", {{"type", "integer"}}}})}});
	arr.push_back({{"name", "openttd.ui.at"},
		{"description", "GUI only, read: what is under a screen pixel: the window, the widget and, over a viewport, the map tile"},
		{"inputSchema", Schema(xy, json::array({"x", "y"}))}});
	arr.push_back({{"name", "openttd.ui.locate"},
		{"description", "GUI only, read: the screen pixel of a tile's centre in the main viewport (tile, or x/y in tiles) and whether it is on screen"},
		{"inputSchema", {{"type", "object"}, {"properties", tile},
			{"anyOf", json::array({json{{"required", json::array({"tile"})}}, json{{"required", json::array({"x", "y"})}}})}}}});
	arr.push_back({{"name", "openttd.input.state"},
		{"description", "GUI only, read: the cursor, buttons, armed switch, whether the owner holds the mouse, the active placement tool and the tile selection"},
		{"inputSchema", Schema(json::object())}});
	arr.push_back({{"name", "openttd.input.pointer"},
		{"description", "GUI only, armed: move the visible cursor to a screen pixel through the driver's input path (hover, drag and tile highlight follow)"},
		{"inputSchema", Schema(xy, json::array({"x", "y"}))}});
	arr.push_back({{"name", "openttd.input.button"},
		{"description", "GUI only, armed: press or release a mouse button at the cursor (state down|up, button left|right|middle, mods ctrl/shift held for this event)"},
		{"inputSchema", Schema({{"state", {{"type", "string"}, {"enum", {"down", "up"}}}}, {"button", {{"type", "string"}, {"enum", {"left", "right", "middle"}}}}, {"mods", mods}},
			json::array({"state"}))}});
	arr.push_back({{"name", "openttd.input.wheel"},
		{"description", "GUI only, armed: turn the mouse wheel at the cursor by delta notches (positive zooms out)"},
		{"inputSchema", Schema({{"delta", {{"type", "integer"}}}}, json::array({"delta"}))}});
	arr.push_back({{"name", "openttd.input.key"},
		{"description", "GUI only, armed: press a key as the keyboard driver does: keycode (OpenTTD WKC_* value, letters are their upper-case code) and the character it types"},
		{"inputSchema", Schema({{"keycode", {{"type", "integer"}}}, {"char", {{"type", "string"}}}}, json::array({"keycode"}))}});
	json window = {{"type", "object"}, {"description", "the front-most window matching any of: class, number, caption (case-insensitive substring)"},
		{"properties", {{"class", {{"type", "integer"}}}, {"number", {{"type", "integer"}}}, {"caption", {{"type", "string"}}}}}};
	json widget = {{"type", "object"}, {"description", "one of: index, text (matches the widget's text or tooltip, case-insensitive), type (e.g. close_box)"},
		{"properties", {{"index", {{"type", "integer"}}}, {"text", {{"type", "string"}}}, {"type", {{"type", "string"}}}}}};
	arr.push_back({{"name", "openttd.ui.press"},
		{"description", "GUI only, armed: press a widget of a window, found by what it is or says; raises the window when covered, clicks through the driver's input path, and answers what changed (the widget's state, windows opened and closed, the active tool)"},
		{"inputSchema", Schema({{"window", window}, {"widget", widget}, {"button", {{"type", "string"}, {"enum", {"left", "right"}}}}, {"mods", mods}}, json::array({"window", "widget"}))}});
	arr.push_back({{"name", "openttd.ui.choose"},
		{"description", "GUI only, armed: choose an item of the open dropdown, by its position among the selectable items (index) or by the value it returns (result)"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"index", {{"type", "integer"}}}, {"result", {{"type", "integer"}}}}},
			{"anyOf", json::array({json{{"required", json::array({"index"})}}, json{{"required", json::array({"result"})}}})}}}});
	arr.push_back({{"name", "openttd.ui.type"},
		{"description", "GUI only, armed: type text into the focused edit box, a key at a time (press the box first with openttd.ui.press)"},
		{"inputSchema", Schema({{"text", {{"type", "string"}}}}, json::array({"text"}))}});
	arr.push_back({{"name", "openttd.ui.close"},
		{"description", "GUI only, armed: close a window with its close box, as a person does"},
		{"inputSchema", Schema({{"window", window}}, json::array({"window"}))}});
	arr.push_back({{"name", "openttd.ui.click_tile"},
		{"description", "GUI only, armed: click a map tile (the active tool acts on it, or the tile's window opens) and answer what changed; a tile off screen makes the view jump there and answers scrolled: true, so call again"},
		{"inputSchema", {{"type", "object"}, {"properties", tile_props_click},
			{"anyOf", json::array({json{{"required", json::array({"tile"})}}, json{{"required", json::array({"x", "y"})}}})}}}});

	/* MCP tool annotations: readers are read-only; input changes local input state and, through the
	 * game's own handling, may lead to a command, so it is neither read-only nor idempotent. */
	for (auto &t : arr) {
		bool reader = t["name"].get<std::string>().rfind("openttd.ui.", 0) == 0 || t["name"] == "openttd.input.state";
		t["annotations"] = reader ? json{{"readOnlyHint", true}} : json{{"readOnlyHint", false}, {"idempotentHint", false}};
	}
	return arr;
}

std::optional<json> Call(const std::string &name, const json &args)
{
	bool ui = name.rfind("openttd.ui.", 0) == 0;
	bool input = name.rfind("openttd.input.", 0) == 0;
	if (!ui && !input) return std::nullopt;

	Window *main = MainWindow();
	if (main == nullptr || main->viewport == nullptr) return Error("no interface in this process (a dedicated server has none)");

	if (name == "openttd.ui.windows") {
		bool widgets = args.value("widgets", false);
		json list = json::array();
		for (const Window *w : Window::IterateFromFront()) {
			if (args.contains("class") && args["class"].is_number_integer() && static_cast<int>(w->window_class) != args["class"].get<int>()) continue;
			if (args.contains("number") && args["number"].is_number_integer() && static_cast<int>(w->window_number) != args["number"].get<int>()) continue;
			list.push_back(WindowJson(w, widgets));
		}
		return json{{"ok", true}, {"screen", {{"w", _screen.width}, {"h", _screen.height}}}, {"windows", std::move(list)}};
	}

	if (name == "openttd.ui.at") {
		if (!args.contains("x") || !args.contains("y") || !args["x"].is_number_integer() || !args["y"].is_number_integer()) return Error("give x and y in screen pixels");
		int x = args["x"].get<int>(), y = args["y"].get<int>();
		json out = {{"ok", true}, {"x", x}, {"y", y}};
		Window *w = FindWindowFromPt(x, y);
		if (w == nullptr) return out;
		out["window"] = WindowJson(w, false);
		WidgetID wid = GetWidgetFromPos(w, x - w->left, y - w->top);
		if (wid >= 0) {
			if (const NWidgetBase *nwb = w->GetWidget<NWidgetBase>(wid); nwb != nullptr) out["widget"] = Widget(w, wid, nwb);
		}
		if (const Viewport *vp = IsPtInWindowViewport(w, x, y); vp != nullptr) {
			Point pt = TranslateXYToTileCoord(*vp, x, y);
			if (pt.x >= 0 && pt.y >= 0) out["tile"] = TileJson(TileVirtXY(pt.x, pt.y));
		}
		return out;
	}

	if (name == "openttd.ui.locate") {
		std::string error;
		auto t = TileArg(args, error);
		if (!t.has_value()) return Error(error);
		const Viewport &vp = *main->viewport;
		Point p = TileToScreen(vp, *t);
		bool visible = p.x >= vp.left && p.x < vp.left + vp.width && p.y >= vp.top && p.y < vp.top + vp.height;
		/* Covered by another window? Then a click there would reach that window, not the map. */
		Window *top = visible ? FindWindowFromPt(p.x, p.y) : nullptr;
		bool clickable = top == main;
		json out = TileJson(*t);
		out["ok"] = true;
		out["screen"] = {{"x", p.x}, {"y", p.y}};
		out["visible"] = visible;
		out["clickable"] = clickable;
		out["zoom"] = static_cast<int>(vp.zoom);
		return out;
	}

	if (name == "openttd.input.state") return InputState();

	if (auto refused = InputRefused(); refused.has_value()) return refused;

	if (name == "openttd.input.pointer") {
		if (!args.contains("x") || !args.contains("y") || !args["x"].is_number_integer() || !args["y"].is_number_integer()) return Error("give x and y in screen pixels");
		MovePointer(args["x"].get<int>(), args["y"].get<int>());
		return json{{"ok", true}, {"cursor", {{"x", _cursor.pos.x}, {"y", _cursor.pos.y}}}};
	}

	if (name == "openttd.input.button") {
		auto button = ButtonArg(args);
		if (!button.has_value()) return Error("button is left, right or middle");
		std::string state = args.value("state", "");
		if (state != "down" && state != "up") return Error("state is down or up");
		AsDriver([&] {
			SetModifiers(args);
			if (state == "down") {
				_input_pressed.Set(*button);
				/* The driver marks a right or middle press as a click on the way down; a left click is
				 * detected by HandleMouseEvents() itself from the pressed state. */
				if (*button != ControlInput::Left) _input_clicked.Set(*button);
			} else {
				_input_pressed.Reset(*button);
				if (*button == ControlInput::Left) _input_clicked.Reset(ControlInput::Left);
			}
			HandleMouseEvents();
			ClearModifiers();
		});
		_state.last = _cursor.pos;
		if (state == "down") _state.presses++;
		return InputState();
	}

	if (name == "openttd.input.wheel") {
		if (!args.contains("delta") || !args["delta"].is_number_integer()) return Error("give delta in notches");
		AsDriver([&] {
			_cursor.wheel += args["delta"].get<int>();
			HandleMouseEvents();
		});
		return InputState();
	}

	if (name == "openttd.input.key") {
		if (!args.contains("keycode") || !args["keycode"].is_number_unsigned()) return Error("give keycode (a WKC_* value)");
		uint keycode = args["keycode"].get<uint>();
		char32_t ch = 0;
		if (args.contains("char") && args["char"].is_string() && !args["char"].get<std::string>().empty()) {
			ch = static_cast<unsigned char>(args["char"].get<std::string>()[0]);
		}
		AsDriver([&] { HandleKeypress(keycode, ch); });
		_state.keys++;
		return InputState();
	}

	if (name == "openttd.ui.press") {
		Window *w = FindWindowBy(args.value("window", json::object()));
		if (w == nullptr) return Error("no window matches");
		json found;
		auto r = FindWidgetBy(w, args.value("widget", json::object()), found);
		if (!r.has_value()) return Error("the window has no such widget");
		const int x = (r->left + r->right) / 2, y = (r->top + r->bottom) / 2;
		if (FindWindowFromPt(x, y) != w) w = BringWindowToFrontById(w->window_class, w->window_number);
		if (w == nullptr || FindWindowFromPt(x, y) != w) return Error("the widget stays covered by another window");
		json before = WindowList();
		Click(x, y, args.value("button", "left") == "right" ? ControlInput::Right : ControlInput::Left, args.value("mods", json::array()));
		json after = WindowList();
		json out = InputState();
		out["pressed"] = found;
		out["opened"] = Difference(after, before);
		out["closed"] = Difference(before, after);
		return out;
	}

	if (name == "openttd.ui.choose") {
		for (Window *w : Window::IterateFromFront()) {
			auto items = Hooks::DropdownItems(w);
			if (items.empty()) continue;
			const Hooks::DropdownItem *pick = nullptr;
			int k = 0;
			for (const auto &it : items) {
				if (!it.selectable) continue;
				if ((args.contains("index") && args["index"].is_number_integer() && args["index"].get<int>() == k) ||
						(args.contains("result") && args["result"].is_number_integer() && args["result"].get<int>() == it.result)) {
					pick = &it;
					break;
				}
				k++;
			}
			if (pick == nullptr) return Error("the open dropdown has no such item");
			json before = WindowList();
			Click((pick->rect.left + pick->rect.right) / 2, (pick->rect.top + pick->rect.bottom) / 2, ControlInput::Left, json::array());
			json out = InputState();
			out["chosen"] = pick->result;
			out["opened"] = Difference(WindowList(), before);
			return out;
		}
		return Error("no dropdown is open");
	}

	if (name == "openttd.ui.type") {
		const std::string text = args.value("text", "");
		AsDriver([&] {
			for (unsigned char c : text) {
				uint keycode = std::isalpha(c) ? static_cast<uint>(std::toupper(c)) : static_cast<uint>(c);
				HandleKeypress(keycode, static_cast<char32_t>(c));
			}
		});
		_state.keys += text.size();
		return InputState();
	}

	if (name == "openttd.ui.close") {
		Window *w = FindWindowBy(args.value("window", json::object()));
		if (w == nullptr) return Error("no window matches");
		json found;
		auto r = FindWidgetBy(w, json{{"type", "close_box"}}, found);
		if (!r.has_value()) return Error("the window has no close box");
		const int x = (r->left + r->right) / 2, y = (r->top + r->bottom) / 2;
		if (FindWindowFromPt(x, y) != w) BringWindowToFrontById(w->window_class, w->window_number);
		json before = WindowList();
		Click(x, y, ControlInput::Left, json::array());
		json out = InputState();
		out["closed"] = Difference(before, WindowList());
		return out;
	}

	if (name == "openttd.ui.click_tile") {
		std::string error;
		auto t = TileArg(args, error);
		if (!t.has_value()) return Error(error);
		const Viewport &vp = *main->viewport;
		Point p = TileToScreen(vp, *t);
		const bool on_screen = p.x >= vp.left && p.x < vp.left + vp.width && p.y >= vp.top && p.y < vp.top + vp.height;
		if (!on_screen) {
			/* Jump there; the viewport takes the new position on its next frame. Applying it here would
			 * redraw on the game thread, where drawing is not allowed, so the caller clicks again. */
			main->viewport->follow_vehicle = VehicleID::Invalid();
			ScrollMainWindowToTile(*t, true);
			json out = Error("the tile was off screen: the view jumped there; call again to click it");
			out["scrolled"] = true;
			return out;
		}
		if (FindWindowFromPt(p.x, p.y) != main) return Error("a window covers that tile");
		json before = WindowList();
		Click(p.x, p.y, args.value("button", "left") == "right" ? ControlInput::Right : ControlInput::Left, json::array());
		json out = InputState();
		out["tile"] = TileJson(*t);
		out["opened"] = Difference(WindowList(), before);
		return out;
	}

	return Error("unknown UI tool");
}

} // namespace PrismaticMCP::UI

#endif /* OTTD_PRISMATIC_MCP_SERVER */
