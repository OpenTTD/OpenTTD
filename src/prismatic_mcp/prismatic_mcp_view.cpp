/*
 * This file is part of korczis's PRIVATE RESEARCH FORK of OpenTTD and is NOT
 * part of upstream OpenTTD. RESEARCH-ONLY / UNSTABLE.
 *
 * View tools of the embedded MCP server: an external controller moves the main viewport of the
 * GUI process the MCP server runs in (a player's client, or a GUI-hosted game), so the player
 * sees what their bot does. Only the local view changes: no command, no game state, nothing that
 * reaches the network, which is why these tools need no approval.
 */

/** @file prismatic_mcp_view.cpp View tools of the embedded MCP server (research-only). */

#include "../stdafx.h"
#include <algorithm>
#include <deque>
#include <optional>

#ifdef OTTD_PRISMATIC_MCP_SERVER

#include "prismatic_mcp_view.h"
#include "prismatic_mcp_text.h"

#include "../map_func.h"
#include "../network/network.h"
#include "../settings_type.h"
#include "../town.h"
#include "../vehicle_base.h"
#include "../viewport_func.h"
#include "../window_func.h"
#include "../window_gui.h"
#include "../zoom_func.h"
#include "../screenshot.h"
#include "../console_func.h"
#include "../console_type.h"
#include "../news_func.h"
#include "../company_base.h"
#include "../textbuf_gui.h"
#include "../news_gui.h"
#include "../strings_func.h"
#include "../table/strings.h"
#include "../timer/timer_game_calendar.h"
#include "../timer/timer.h"
#include "../timer/timer_window.h"
#include "../fileio_func.h"
#include "../gfx_func.h"
#include "../palette_func.h"
#include "../widget_type.h"

#include "../safeguards.h"

namespace PrismaticMCP::View {

using json = nlohmann::json;

namespace {

/** A question asked through openttd.view.ask, and the player's answer once given. */
struct Ask {
	uint32_t id;
	std::string question;
	std::optional<std::string> answer; ///< "yes", "no" (also a closed dialog), or "replaced" by a newer question.
};
std::deque<Ask> _asks; ///< Newest first; GUI state of this client only.
uint32_t _ask_next = 0; ///< Last id handed out.
uint32_t _ask_open = 0; ///< The question the dialog shows now, 0 if none.
std::optional<std::string> _ask_auto; ///< Owner switch: the answer given for the player (instantly, or when the countdown ends).
uint _ask_countdown = 0; ///< Owner switch: seconds the dialog shows its countdown before giving #_ask_auto; 0 answers at once with no dialog.
static constexpr uint ASK_COUNTDOWN_MAX = 600; ///< The longest countdown the switch accepts, in seconds.

/** Record an answer to question @p id unless it already has one. */
void RecordAnswer(uint32_t id, std::string_view answer)
{
	for (Ask &a : _asks) {
		if (a.id == id && !a.answer.has_value()) a.answer = std::string(answer);
	}
	if (_ask_open == id) _ask_open = 0;
}

/** Widgets of the bot's question window. */
enum BotAskWidgets : WidgetID {
	WID_BA_CAPTION, ///< The caption ("Your bot asks").
	WID_BA_TEXT, ///< The question.
	WID_BA_COUNTDOWN_SEL, ///< Shows the countdown bar, or nothing when the dialog waits for the player.
	WID_BA_COUNTDOWN, ///< The countdown: what will be answered, in how long, and a bar that empties.
	WID_BA_NO, ///< The No button.
	WID_BA_YES, ///< The Yes button.
};

/**
 * The bot's yes/no question, laid out like the game's own query dialog. With the owner's
 * auto-confirm switch on it shows a countdown bar under the question; when the bar runs out the
 * switch's answer is given for the player and the game goes on. Clicking the bar stops the
 * countdown, and the dialog then waits for a click on No or Yes. Closing it answers no.
 * Real time, not game time: the countdown runs while the game is paused, too.
 */
struct BotAskWindow : public Window {
	static constexpr auto TICK = std::chrono::milliseconds(100); ///< How often the countdown advances and redraws.

	uint32_t id; ///< The question's id in #_asks.
	std::string caption; ///< Window caption.
	std::string question; ///< The question.
	std::string on_timeout; ///< The answer given when the countdown runs out.
	std::chrono::milliseconds total{0}; ///< The whole countdown; 0 when there is none.
	std::chrono::milliseconds left{0}; ///< What is left of it.
	bool counting = false; ///< The countdown runs (not stopped by the player, not off).

	BotAskWindow(WindowDesc &desc, uint32_t id, std::string caption, std::string question, std::optional<std::string> on_timeout, uint seconds)
		: Window(desc), id(id), caption(std::move(caption)), question(std::move(question)), on_timeout(on_timeout.value_or("yes")),
		total(std::chrono::seconds(seconds)), left(std::chrono::seconds(seconds)), counting(on_timeout.has_value() && seconds > 0)
	{
		this->CreateNestedTree();
		this->GetWidget<NWidgetStacked>(WID_BA_COUNTDOWN_SEL)->SetDisplayedPlane(this->counting ? 0 : SZSP_HORIZONTAL);
		this->FinishInitNested(0);
	}

	/** The line over the countdown bar, with @p left of it to go. */
	std::string CountdownText(std::chrono::milliseconds left) const
	{
		return fmt::format("Answering '{}' in {} s - click here to wait", this->on_timeout, (left.count() + 999) / 1000);
	}

	/** Give @p answer and close: the one way a click or the countdown answers. */
	void Answer(std::string_view answer)
	{
		RecordAnswer(this->id, answer);
		this->Close();
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		RecordAnswer(this->id, "no"); // closed without an answer (close box): no, as with the game's query dialog
		this->Window::Close();
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_BA_CAPTION) return this->caption;
		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_BA_TEXT:
				size = GetStringMultiLineBoundingBox(this->question, size);
				break;
			case WID_BA_COUNTDOWN:
				/* As wide as its longest line (the whole countdown to go), so it is never cut off. */
				size.width = std::max(size.width, GetStringBoundingBox(this->CountdownText(this->total)).width);
				size.height = GetCharacterHeight(FontSize::Normal) + WidgetDimensions::scaled.vsep_normal + ScaleGUITrad(6);
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_BA_TEXT:
				DrawStringMultiLine(r, this->question, TextColour::FromString, {AlignmentH::Centre, AlignmentV::Middle});
				break;

			case WID_BA_COUNTDOWN: {
				DrawString(r.left, r.right, r.top, this->CountdownText(this->left), TextColour::White, AlignmentH::Centre);
				Rect bar = r.WithHeight(ScaleGUITrad(6), true);
				GfxFillRect(bar, PC_BLACK);
				const Rect inner = bar.Shrink(WidgetDimensions::scaled.bevel);
				const int width = static_cast<int>(inner.Width() * this->left.count() / std::max<int64_t>(this->total.count(), 1));
				if (width > 0) GfxFillRect(inner.WithWidth(width, _current_text_dir == TD_RTL), PC_YELLOW);
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BA_YES: this->Answer("yes"); break;
			case WID_BA_NO: this->Answer("no"); break;
			case WID_BA_COUNTDOWN:
				/* The player is here: stop the countdown and wait for their answer. */
				this->counting = false;
				this->GetWidget<NWidgetStacked>(WID_BA_COUNTDOWN_SEL)->SetDisplayedPlane(SZSP_HORIZONTAL);
				this->ReInit();
				break;
		}
	}

	const IntervalTimer<TimerWindow> countdown_tick = {TICK, [this](uint count) {
		if (!this->counting) return;
		const auto step = TICK * count;
		if (this->left <= step) {
			this->Answer(this->on_timeout);
			return;
		}
		this->left -= step;
		this->SetWidgetDirty(WID_BA_COUNTDOWN);
	}};
};

static constexpr std::initializer_list<NWidgetPart> _nested_bot_ask_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Red),
		NWidget(WWT_CAPTION, Colours::Red, WID_BA_CAPTION),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Red),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.modalpopup),
			NWidget(WWT_TEXT, Colours::Invalid, WID_BA_TEXT), SetMinimalSize(200, 12),
			NWidget(NWID_SELECTION, Colours::Invalid, WID_BA_COUNTDOWN_SEL),
				NWidget(WWT_EMPTY, Colours::Invalid, WID_BA_COUNTDOWN), SetMinimalSize(200, 12), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPIP(WidgetDimensions::unscaled.hsep_indent, WidgetDimensions::unscaled.hsep_indent, WidgetDimensions::unscaled.hsep_indent),
				NWidget(WWT_PUSHTXTBTN, Colours::Yellow, WID_BA_NO), SetMinimalSize(71, 12), SetFill(1, 1), SetStringTip(STR_QUIT_NO),
				NWidget(WWT_PUSHTXTBTN, Colours::Yellow, WID_BA_YES), SetMinimalSize(71, 12), SetFill(1, 1), SetStringTip(STR_QUIT_YES),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

/** The bot's question window: centred, modal like the game's query dialog, never focused (a stray Enter must not answer). */
static WindowDesc _bot_ask_desc(
	WindowPosition::Center, {}, 0, 0,
	WindowClass::PrismaticAsk, WindowClass::None,
	WindowDefaultFlag::Modal,
	_nested_bot_ask_widgets
);

json Error(std::string message)
{
	return json{{"ok", false}, {"error", std::move(message)}};
}

/** The main window, or nullptr in a process without one (a dedicated server). */
Window *MainWindow()
{
	if (_network_dedicated) return nullptr;
	return FindWindowById(WindowClass::MainWindow, 0);
}

/** Resolve the target of scroll_to: a tile, x/y, a town or a vehicle. */
std::optional<TileIndex> Target(const json &args, std::string &error)
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
	if (args.contains("town") && args["town"].is_number_unsigned()) {
		const Town *t = Town::GetIfValid(args["town"].get<uint32_t>());
		if (t == nullptr) { error = "no such town"; return std::nullopt; }
		return t->xy;
	}
	if (args.contains("vehicle") && args["vehicle"].is_number_unsigned()) {
		const Vehicle *v = Vehicle::GetIfValid(args["vehicle"].get<uint32_t>());
		if (v == nullptr) { error = "no such vehicle"; return std::nullopt; }
		return v->tile;
	}
	error = "give one of tile, x and y, town or vehicle";
	return std::nullopt;
}

json State(Window *w)
{
	const ViewportData &vp = *w->viewport;
	return json{{"ok", true}, {"zoom", static_cast<int>(vp.zoom)},
		{"zoom_min", static_cast<int>(_settings_client.gui.zoom_min)}, {"zoom_max", static_cast<int>(_settings_client.gui.zoom_max)},
		{"following_vehicle", vp.follow_vehicle == VehicleID::Invalid() ? json(nullptr) : json(vp.follow_vehicle.base())}};
}

} // namespace

json Tools()
{
	json obj = {{"type", "object"}};
	json arr = json::array();
	/* A dedicated server has no window to view: it lists no view tool, so each instance's catalogue is
	 * what it can actually do. */
	if (_network_dedicated) return arr;
	arr.push_back({{"name", "openttd.view.scroll_to"},
		{"description", "GUI only, no approval: centre the main viewport on a tile, x/y, a town or a vehicle"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"tile", {{"type", "integer"}}}, {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
			{"town", {{"type", "integer"}}}, {"vehicle", {{"type", "integer"}}}, {"instant", {{"type", "boolean"}}}}}}}});
	arr.push_back({{"name", "openttd.view.follow"},
		{"description", "GUI only, no approval: the main viewport follows a vehicle; vehicle null stops following"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"vehicle", {{"type", {"integer", "null"}}}}}}, {"required", json::array({"vehicle"})}}}});
	arr.push_back({{"name", "openttd.view.zoom"},
		{"description", "GUI only, no approval: set the main viewport's zoom level within the client's zoom limits"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"level", {{"type", "integer"}}}}}, {"required", json::array({"level"})}}}});
	arr.push_back({{"name", "openttd.view.mark"},
		{"description", "GUI only, no approval: the bot's cursor, a marked tile (the red tile square) at a tile, x/y, a town or a vehicle; clear removes it"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"tile", {{"type", "integer"}}}, {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
			{"town", {{"type", "integer"}}}, {"vehicle", {{"type", "integer"}}}, {"clear", {{"type", "boolean"}}}}}}}});
	arr.push_back({{"name", "openttd.view.screenshot"},
		{"description", "GUI only, no approval: save what this window shows right now (the whole screen buffer, UI included) and answer the file path"},
		{"inputSchema", obj}});
	arr.push_back({{"name", "openttd.view.news.list"},
		{"description", "GUI only, no approval: the news this client has shown (newest first): text, type, style, date"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"limit", {{"type", "integer"}}}}}}},
		{"annotations", {{"readOnlyHint", true}}}});
	arr.push_back({{"name", "openttd.view.news.show"},
		{"description", "GUI only, no approval, local to this client: show a news message to the player (from the bot, a script or another AI). With a tile, town or vehicle it gets a viewport there (style thin); style: normal (newspaper), thin (headline + viewport), small (information window), company (the newspaper with a company's manager face: company id, optional title launch|trouble|merger|bankrupt)"},
		{"inputSchema", {{"type", "object"}, {"required", json::array({"text"})}, {"properties", {{"text", {{"type", "string"}}},
			{"style", {{"type", "string"}, {"enum", json::array({"normal", "thin", "small", "company"})}}},
			{"company", {{"type", "integer"}}}, {"title", {{"type", "string"}, {"enum", json::array({"none", "launch", "trouble", "merger", "bankrupt"})}}},
			{"tile", {{"type", "integer"}}}, {"x", {{"type", "integer"}}}, {"y", {{"type", "integer"}}},
			{"town", {{"type", "integer"}}}, {"vehicle", {{"type", "integer"}}}}}}}});
	arr.push_back({{"name", "openttd.view.ask"},
		{"description", "GUI only, no approval: ask the player a yes/no question in the game window (a dialog laid out like the game's own query). Answers {id}; the answer arrives in openttd.view.ask.answers: yes, no (also when the player closes the dialog), the auto-confirm answer when its countdown runs out (openttd.view.ask.auto), or replaced when a newer question takes its place"},
		{"inputSchema", {{"type", "object"}, {"required", json::array({"question"})}, {"properties", {{"question", {{"type", "string"}}}, {"caption", {{"type", "string"}}}}}}}});
	arr.push_back({{"name", "openttd.view.ask.answers"},
		{"description", "GUI only, no approval: every question asked through openttd.view.ask, newest first, with its answer (yes, no, replaced, or null while it is open)"},
		{"inputSchema", obj},
		{"annotations", {{"readOnlyHint", true}}}});
	arr.push_back({{"name", "openttd.view.ask.auto"},
		{"description", "Owner switch: auto-answer the bot's questions. With an answer (yes, no, or any text) and no countdown, openttd.view.ask records it at once and shows no dialog. With countdown N (seconds, 1-600) the dialog is shown with a bar that empties over N seconds; if the player does nothing the answer is given for them and the game goes on, a click on No or Yes answers instead, and a click on the bar stops the countdown. countdown alone means answer yes. Pass neither, or answer null, to turn it off. Answers {auto, answer, countdown}. From the tilde console: 'mcp view.ask.auto answer=yes countdown=10'"},
		{"inputSchema", {{"type", "object"}, {"properties", {{"answer", {{"type", json::array({"string", "null"})}}},
			{"countdown", {{"type", "integer"}, {"minimum", 0}, {"maximum", ASK_COUNTDOWN_MAX}}}}}}}});
	arr.push_back({{"name", "openttd.view.console.print"},
		{"description", "GUI only, no approval: print a line in this client's ~ console, coloured by level: info (yellow), warning (light blue), error (red), debug (light brown), rhai (green), plan (gold), plain (silver). For streaming script output and answers to the player"},
		{"inputSchema", {{"type", "object"}, {"required", json::array({"text"})}, {"properties", {{"text", {{"type", "string"}}},
			{"level", {{"type", "string"}, {"enum", json::array({"info", "warning", "error", "debug", "rhai", "plan", "plain"})}}}}}}}});
	arr.push_back({{"name", "openttd.view.news.close"},
		{"description", "GUI only, no approval: close the news window the player sees"},
		{"inputSchema", obj}});
	arr.push_back({{"name", "openttd.view.state"},
		{"description", "GUI only: the main viewport's zoom, zoom limits and followed vehicle"},
		{"inputSchema", obj},
		{"annotations", {{"readOnlyHint", true}}}});

	/* A tool that does not declare its annotations changes this client's own view or dialogs, never
	 * the game: not read-only, not destructive, local. It is idempotent when it sets a state rather
	 * than adding something (a screenshot, a news item, a question, a console line). Clients and the
	 * coverage probe decide by readOnlyHint, so every view tool carries one. */
	static constexpr std::string_view SETS_A_STATE[] = {"openttd.view.scroll_to", "openttd.view.follow", "openttd.view.zoom",
		"openttd.view.mark", "openttd.view.ask.auto", "openttd.view.news.close"};
	for (json &t : arr) {
		if (t.contains("annotations")) continue;
		const std::string name = t["name"].get<std::string>();
		const bool sets = std::find(std::begin(SETS_A_STATE), std::end(SETS_A_STATE), name) != std::end(SETS_A_STATE);
		t["annotations"] = {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", sets}, {"openWorldHint", false}};
	}
	return arr;
}

std::optional<json> Call(const std::string &name, const json &args)
{
	if (name.rfind("openttd.view.", 0) != 0) return std::nullopt;

	/* These need no viewport, so they work on a dedicated server too: the owner's auto-answer
	 * switch, and an already-auto-answered question (no dialog is shown). */
	if (name == "openttd.view.ask.auto") {
		uint countdown = 0;
		if (args.contains("countdown") && !args["countdown"].is_null()) {
			if (!args["countdown"].is_number_integer() || args["countdown"].get<int64_t>() < 0 || args["countdown"].get<int64_t>() > ASK_COUNTDOWN_MAX) {
				return Error(fmt::format("countdown is a whole number of seconds from 0 to {}", ASK_COUNTDOWN_MAX));
			}
			countdown = args["countdown"].get<uint>();
		}
		if (args.contains("answer") && args["answer"].is_null()) {
			_ask_auto.reset();
		} else if (args.contains("answer")) {
			if (!args["answer"].is_string()) return Error("answer is yes, no, any text, or null");
			std::string a = args["answer"].get<std::string>();
			if (a == "true" || a == "1" || a == "on") a = "yes";
			else if (a == "false" || a == "0" || a == "off") a = "no";
			_ask_auto = a;
		} else if (countdown > 0) {
			_ask_auto = "yes"; // a countdown alone confirms
		} else {
			_ask_auto.reset();
		}
		_ask_countdown = _ask_auto.has_value() ? countdown : 0;
		return json{{"ok", true}, {"auto", _ask_auto.has_value()}, {"answer", _ask_auto.has_value() ? json(*_ask_auto) : json(nullptr)},
			{"countdown", _ask_countdown}};
	}
	/* Answered at once, with no dialog: the switch without a countdown, or a countdown in a process
	 * that has no window to count down in (a dedicated server, a test). */
	if (name == "openttd.view.ask" && _ask_auto.has_value() && (_ask_countdown == 0 || MainWindow() == nullptr)) {
		std::string question = args.value("question", "");
		if (question.empty()) return Error("question is required");
		uint32_t id = ++_ask_next;
		_asks.push_front({id, question, *_ask_auto});
		if (_asks.size() > 100) _asks.pop_back();
		return json{{"ok", true}, {"id", id}, {"auto_answered", true}, {"answer", *_ask_auto}};
	}

	Window *w = MainWindow();
	if (w == nullptr || w->viewport == nullptr) return Error("no main viewport in this process (a dedicated server has none)");

	if (name == "openttd.view.state") return State(w);

	if (name == "openttd.view.news.list") {
		static constexpr std::string_view TYPES[] = {"arrival_company", "arrival_other", "accident", "accident_other",
			"company_info", "industry_open", "industry_close_company", "industry_close_other", "industry_close_nobody",
			"economy", "industry_company", "industry_other", "industry_nobody", "advice", "new_vehicles",
			"acceptance", "subsidies", "general"};
		static constexpr std::string_view STYLES[] = {"thin", "small", "normal", "vehicle", "company"};
		size_t limit = std::min<size_t>(args.value("limit", 20), 200);
		json list = json::array();
		for (const NewsItem &ni : GetNews()) {
			if (list.size() >= limit) break;
			auto ymd = TimerGameCalendar::ConvertDateToYMD(ni.date);
			auto t = static_cast<size_t>(ni.type), st = static_cast<size_t>(ni.style);
			list.push_back({{"text", PlainText(ni.headline.GetDecodedString())},
				{"type", t < std::size(TYPES) ? json(TYPES[t]) : json(t)},
				{"style", st < std::size(STYLES) ? json(STYLES[st]) : json(st)},
				{"date", fmt::format("{:04}-{:02}-{:02}", ymd.year.base(), ymd.month + 1, ymd.day)}});
		}
		return json{{"ok", true}, {"news", std::move(list)}};
	}

	if (name == "openttd.view.news.show") {
		/* Local to this client: a news item is GUI state, not game state, and is never sent to the server. */
		std::string text = args.value("text", "");
		if (text.empty()) return Error("text is required");
		std::string style = args.value("style", "");
		std::string error;
		NewsReference ref{};
		bool has_target = args.contains("tile") || args.contains("x") || args.contains("town") || args.contains("vehicle");
		if (has_target) {
			auto tile = Target(args, error);
			if (!tile.has_value()) return Error(error);
			ref = *tile;
		}
		if (style == "company") {
			const Company *c = Company::GetIfValid(CompanyID(static_cast<uint8_t>(args.value("company", -1))));
			if (c == nullptr) return Error("style company needs the id of an existing company");
			std::string title = args.value("title", "none");
			StringID tid = title == "launch" ? STR_NEWS_COMPANY_LAUNCH_TITLE : title == "trouble" ? STR_NEWS_COMPANY_IN_TROUBLE_TITLE :
				title == "merger" ? STR_NEWS_COMPANY_MERGER_TITLE : title == "bankrupt" ? STR_NEWS_COMPANY_BANKRUPT_TITLE : STR_EMPTY;
			AddCompanyNewsItem(GetEncodedString(STR_JUST_BIG_RAW_STRING, text), std::make_unique<CompanyNewsInformation>(tid, c));
			return json{{"ok", true}, {"shown", text}, {"style", style}, {"company", c->index.base()}};
		}
		if (style.empty()) style = has_target ? "thin" : "normal";
		NewsStyle ns = style == "thin" ? NewsStyle::Thin : (style == "small" ? NewsStyle::Small : NewsStyle::Normal);
		NewsFlags flags = ns == NewsStyle::Normal ? NewsFlags{} : NewsFlags{NewsFlag::NoTransparency, NewsFlag::Shaded};
		if (ns != NewsStyle::Normal && !has_target) return Error("styles thin and small need a tile, x/y, town or vehicle");
		AddNewsItem(GetEncodedString(STR_JUST_RAW_STRING, text), NewsType::General, ns, flags, ref);
		return json{{"ok", true}, {"shown", text}, {"style", style}};
	}

	if (name == "openttd.view.ask") {
		std::string question = args.value("question", "");
		if (question.empty()) return Error("question is required");
		std::string caption = args.value("caption", "Your bot asks");
		uint32_t id = ++_ask_next;
		/* A question still open is replaced, not answered: mark it before its dialog closes. */
		if (_ask_open != 0) RecordAnswer(_ask_open, "replaced");
		CloseWindowById(WindowClass::PrismaticAsk, 0);
		_asks.push_front({id, question, std::nullopt});
		if (_asks.size() > 100) _asks.pop_back();
		_ask_open = id;
		const uint countdown = _ask_auto.has_value() ? _ask_countdown : 0;
		new BotAskWindow(_bot_ask_desc, id, caption, question, _ask_auto, countdown); // never focused: a stray Enter must not answer for the player
		return json{{"ok", true}, {"id", id}, {"countdown", countdown}};
	}

	if (name == "openttd.view.ask.answers") {
		json list = json::array();
		for (const Ask &a : _asks) {
			list.push_back({{"id", a.id}, {"question", a.question}, {"answer", a.answer.has_value() ? json(*a.answer) : json(nullptr)}});
		}
		return json{{"ok", true}, {"asks", std::move(list)}};
	}

	if (name == "openttd.view.console.print") {
		std::string text = args.value("text", "");
		if (text.empty()) return Error("text is required");
		std::string level = args.value("level", "plain");
		TextColour colour = level == "info" ? CC_INFO : level == "warning" ? CC_WARNING : level == "error" ? CC_ERROR :
			level == "debug" ? CC_DEBUG : level == "rhai" ? TextColour::Green : level == "plan" ? CC_COMMAND : CC_DEFAULT;
		/* One console line per text line; the console draws each on its own row. */
		size_t start = 0;
		int lines = 0;
		while (start <= text.size()) {
			size_t end = text.find('\n', start);
			std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
			if (!line.empty()) { IConsolePrint(colour, line); lines++; }
			if (end == std::string::npos) break;
			start = end + 1;
		}
		return json{{"ok", true}, {"lines", lines}, {"level", level}};
	}

	if (name == "openttd.view.news.close") {
		CloseWindowById(WindowClass::News, 0);
		return json{{"ok", true}};
	}

	if (name == "openttd.view.screenshot") {
		/* The screen buffer as the player sees it, captured on the drawing thread (never here, on
		 * the game thread, where the video buffer may be unlocked). The file appears within a frame. */
		std::string path = _personal_dir + "prismatic_mcp_view." + std::string(GetCurrentScreenshotExtension());
		QueueQuietScreenshot(path);
		json out = State(w);
		out["path"] = path;
		out["queued"] = true;
		return out;
	}

	if (name == "openttd.view.scroll_to") {
		std::string error;
		auto tile = Target(args, error);
		if (!tile.has_value()) return Error(error);
		w->viewport->follow_vehicle = VehicleID::Invalid();
		ScrollMainWindowToTile(*tile, args.value("instant", false));
		json out = State(w);
		out["tile"] = tile->base();
		out["x"] = TileX(*tile);
		out["y"] = TileY(*tile);
		return out;
	}

	if (name == "openttd.view.mark") {
		if (args.value("clear", false)) {
			SetRedErrorSquare(INVALID_TILE);
			return State(w);
		}
		std::string error;
		auto tile = Target(args, error);
		if (!tile.has_value()) return Error(error);
		SetRedErrorSquare(*tile);
		json out = State(w);
		out["marked"] = tile->base();
		out["x"] = TileX(*tile);
		out["y"] = TileY(*tile);
		return out;
	}

	if (name == "openttd.view.follow") {
		if (!args.contains("vehicle") || args["vehicle"].is_null()) {
			w->viewport->follow_vehicle = VehicleID::Invalid();
			return State(w);
		}
		if (!args["vehicle"].is_number_unsigned()) return Error("vehicle must be an id or null");
		const Vehicle *v = Vehicle::GetIfValid(args["vehicle"].get<uint32_t>());
		if (v == nullptr) return Error("no such vehicle");
		w->viewport->follow_vehicle = v->index;
		return State(w);
	}

	if (name == "openttd.view.zoom") {
		if (!args.contains("level") || !args["level"].is_number_integer()) return Error("level must be an integer");
		auto level = static_cast<ZoomLevel>(args["level"].get<int>());
		if (!IsInsideMM(level, _settings_client.gui.zoom_min, _settings_client.gui.zoom_max + 1)) {
			return Error(fmt::format("zoom level outside the client's limits {}..{}", _settings_client.gui.zoom_min, _settings_client.gui.zoom_max));
		}
		while (w->viewport->zoom > level && DoZoomInOutWindow(ZOOM_IN, w)) {}
		while (w->viewport->zoom < level && DoZoomInOutWindow(ZOOM_OUT, w)) {}
		return State(w);
	}

	return Error("unknown view tool");
}

} // namespace PrismaticMCP::View

#endif /* OTTD_PRISMATIC_MCP_SERVER */
