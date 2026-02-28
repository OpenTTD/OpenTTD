/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file intro_gui.cpp The main menu GUI. */

#include "stdafx.h"
#include "error.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "textbuf_gui.h"
#include "help_gui.h"
#include "network/network.h"
#include "genworld.h"
#include "network/network_gui.h"
#include "network/network_content.h"
#include "network/network_survey.h"
#include "landscape_type.h"
#include "landscape.h"
#include "strings_func.h"
#include "fios.h"
#include "ai/ai_gui.hpp"
#include "game/game_gui.hpp"
#include "gfx_func.h"
#include "core/string_consumer.hpp"
#include "language.h"
#include "rev.h"
#include "highscore.h"
#include "signs_base.h"
#include "viewport_func.h"
#include "vehicle_base.h"
#include <regex>

#include "widgets/intro_widget.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "safeguards.h"


/**
 * A viewport command for the main menu background (intro game).
 */
struct IntroGameViewportCommand {
	/** Horizontal alignment value. */
	enum AlignmentH : uint8_t {
		LEFT,
		CENTRE,
		RIGHT,
	};
	/** Vertical alignment value. */
	enum AlignmentV : uint8_t {
		TOP,
		MIDDLE,
		BOTTOM,
	};

	int command_index = 0;               ///< Sequence number of the command (order they are performed in).
	Point position{ 0, 0 };              ///< Calculated world coordinate to position viewport top-left at.
	VehicleID vehicle = VehicleID::Invalid(); ///< Vehicle to follow, or VehicleID::Invalid() if not following a vehicle.
	uint delay = 0;                      ///< Delay until next command.
	int zoom_adjust = 0;                 ///< Adjustment to zoom level from base zoom level.
	bool pan_to_next = false;            ///< If true, do a smooth pan from this position to the next.
	AlignmentH align_h = CENTRE;         ///< Horizontal alignment.
	AlignmentV align_v = MIDDLE;         ///< Vertical alignment.

	/**
	 * Calculate effective position.
	 * This will update the position field if a vehicle is followed.
	 * @param vp Viewport to calculate position for.
	 * @return Calculated position in the viewport.
	 */
	Point PositionForViewport(const Viewport &vp)
	{
		if (this->vehicle != VehicleID::Invalid()) {
			const Vehicle *v = Vehicle::Get(this->vehicle);
			this->position = RemapCoords(v->x_pos, v->y_pos, v->z_pos);
		}

		Point p;
		switch (this->align_h) {
			case LEFT: p.x = this->position.x; break;
			case CENTRE: p.x = this->position.x - vp.virtual_width / 2; break;
			case RIGHT: p.x = this->position.x - vp.virtual_width; break;
		}
		switch (this->align_v) {
			case TOP: p.y = this->position.y; break;
			case MIDDLE: p.y = this->position.y - vp.virtual_height / 2; break;
			case BOTTOM: p.y = this->position.y - vp.virtual_height; break;
		}
		return p;
	}
};


struct SelectGameWindow : public Window {
	/** Vector of viewport commands parsed. */
	std::vector<IntroGameViewportCommand> intro_viewport_commands{};
	/** Index of currently active viewport command. */
	size_t cur_viewport_command_index = SIZE_MAX;
	/** Time spent (milliseconds) on current viewport command. */
	uint cur_viewport_command_time = 0;
	uint mouse_idle_time = 0;
	Point mouse_idle_pos{};

	/**
	 * Find and parse all viewport command signs.
	 * Fills the intro_viewport_commands vector and deletes parsed signs from the world.
	 */
	void ReadIntroGameViewportCommand()
	{
		intro_viewport_commands.clear();

		/* Regular expression matching the commands: T, spaces, integer, spaces, flags, spaces, integer */
		static const std::string sign_language = "^T\\s*([0-9]+)\\s*([-+A-Z0-9]+)\\s*([0-9]+)";
		std::regex re(sign_language, std::regex_constants::icase);

		/* List of signs successfully parsed to delete afterwards. */
		std::vector<SignID> signs_to_delete;

		for (const Sign *sign : Sign::Iterate()) {
			std::smatch match;
			if (!std::regex_search(sign->name, match, re)) continue;

			IntroGameViewportCommand vc;
			/* Sequence index from the first matching group. */
			if (auto value = ParseInteger<int>(match[1].str()); value.has_value()) {
				vc.command_index = *value;
			} else {
				continue;
			}
			/* Sign coordinates for positioning. */
			vc.position = RemapCoords(sign->x, sign->y, sign->z);
			/* Delay from the third matching group. */
			if (auto value = ParseInteger<uint>(match[3].str()); value.has_value()) {
				vc.delay = *value * 1000; // milliseconds
			} else {
				continue;
			}

			/* Parse flags from second matching group. */
			auto flags = match[2].str();
			StringConsumer consumer{flags};
			while (consumer.AnyBytesLeft()) {
				auto c = consumer.ReadUtf8();
				switch (toupper(c)) {
					case '-': vc.zoom_adjust = +1; break;
					case '+': vc.zoom_adjust = -1; break;
					case 'T': vc.align_v = IntroGameViewportCommand::TOP; break;
					case 'M': vc.align_v = IntroGameViewportCommand::MIDDLE; break;
					case 'B': vc.align_v = IntroGameViewportCommand::BOTTOM; break;
					case 'L': vc.align_h = IntroGameViewportCommand::LEFT; break;
					case 'C': vc.align_h = IntroGameViewportCommand::CENTRE; break;
					case 'R': vc.align_h = IntroGameViewportCommand::RIGHT; break;
					case 'P': vc.pan_to_next = true; break;
					case 'V': vc.vehicle = static_cast<VehicleID>(consumer.ReadIntegerBase<uint32_t>(10, VehicleID::Invalid().base())); break;
				}
			}

			/* Successfully parsed, store. */
			intro_viewport_commands.push_back(vc);
			signs_to_delete.push_back(sign->index);
		}

		/* Sort the commands by sequence index. */
		std::sort(intro_viewport_commands.begin(), intro_viewport_commands.end(), [](const IntroGameViewportCommand &a, const IntroGameViewportCommand &b) { return a.command_index < b.command_index; });

		/* Delete all the consumed signs, from last ID to first ID. */
		std::sort(signs_to_delete.begin(), signs_to_delete.end(), [](SignID a, SignID b) { return a > b; });
		for (SignID sign_id : signs_to_delete) {
			delete Sign::Get(sign_id);
		}
	}

	SelectGameWindow(WindowDesc &desc) : Window(desc), mouse_idle_pos(_cursor.pos)
	{
		this->CreateNestedTree();
		this->FinishInitNested(0);
		this->OnInvalidateData();

		this->ReadIntroGameViewportCommand();
	}

	void OnRealtimeTick(uint delta_ms) override
	{
		/* Move the main game viewport according to intro viewport commands. */

		if (intro_viewport_commands.empty()) return;

		bool suppress_panning = true;
		if (this->mouse_idle_pos.x != _cursor.pos.x || this->mouse_idle_pos.y != _cursor.pos.y) {
			this->mouse_idle_pos = _cursor.pos;
			this->mouse_idle_time = 2000;
		} else if (this->mouse_idle_time > delta_ms) {
			this->mouse_idle_time -= delta_ms;
		} else {
			this->mouse_idle_time = 0;
			suppress_panning = false;
		}

		/* Determine whether to move to the next command or stay at current. */
		bool changed_command = false;
		if (this->cur_viewport_command_index >= intro_viewport_commands.size()) {
			/* Reached last, rotate back to start of the list. */
			this->cur_viewport_command_index = 0;
			changed_command = true;
		} else {
			/* Check if current command has elapsed and switch to next. */
			this->cur_viewport_command_time += delta_ms;
			if (this->cur_viewport_command_time >= intro_viewport_commands[this->cur_viewport_command_index].delay) {
				this->cur_viewport_command_index = (this->cur_viewport_command_index + 1) % intro_viewport_commands.size();
				this->cur_viewport_command_time = 0;
				changed_command = true;
			}
		}

		IntroGameViewportCommand &vc = intro_viewport_commands[this->cur_viewport_command_index];
		Window *mw = GetMainWindow();

		/* Early exit if the current command hasn't elapsed and isn't animated. */
		if (!changed_command && !vc.pan_to_next && vc.vehicle == VehicleID::Invalid()) return;

		/* Suppress panning commands, while user interacts with GUIs. */
		if (!changed_command && suppress_panning) return;

		/* Reset the zoom level. */
		if (changed_command) FixTitleGameZoom(vc.zoom_adjust);

		/* Calculate current command position (updates followed vehicle coordinates). */
		Point pos = vc.PositionForViewport(*mw->viewport);

		/* Calculate panning (linear interpolation between current and next command position). */
		if (vc.pan_to_next) {
			size_t next_command_index = (this->cur_viewport_command_index + 1) % intro_viewport_commands.size();
			IntroGameViewportCommand &nvc = intro_viewport_commands[next_command_index];
			Point pos2 = nvc.PositionForViewport(*mw->viewport);
			const double t = this->cur_viewport_command_time / (double)vc.delay;
			pos.x = pos.x + (int)(t * (pos2.x - pos.x));
			pos.y = pos.y + (int)(t * (pos2.y - pos.y));
		}

		/* Update the viewport position. */
		mw->viewport->dest_scrollpos_x = mw->viewport->scrollpos_x = pos.x;
		mw->viewport->dest_scrollpos_y = mw->viewport->scrollpos_y = pos.y;
		UpdateViewportPosition(mw, delta_ms);
		mw->SetDirty(); // Required during panning, otherwise logo graphics disappears

		/* If there is only one command, we just executed it and don't need to do any more */
		if (intro_viewport_commands.size() == 1 && vc.vehicle == VehicleID::Invalid()) intro_viewport_commands.clear();
	}

	void OnInit() override
	{
		bool missing_sprites = _missing_extra_graphics > 0 && !IsReleasedVersion();
		this->GetWidget<NWidgetStacked>(WID_SGI_BASESET_SELECTION)->SetDisplayedPlane(missing_sprites ? 0 : SZSP_NONE);

		bool missing_lang = _current_language->missing >= _settings_client.gui.missing_strings_threshold && !IsReleasedVersion();
		this->GetWidget<NWidgetStacked>(WID_SGI_TRANSLATION_SELECTION)->SetDisplayedPlane(missing_lang ? 0 : SZSP_NONE);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_SGI_BASESET:
				DrawStringMultiLine(r, GetString(STR_INTRO_BASESET, _missing_extra_graphics), TC_FROMSTRING, SA_CENTER);
				break;

			case WID_SGI_TRANSLATION:
				DrawStringMultiLine(r, GetString(STR_INTRO_TRANSLATION, _current_language->missing), TC_FROMSTRING, SA_CENTER);
				break;
		}
	}

	void OnResize() override
	{
		bool changed = false;

		if (NWidgetResizeBase *wid = this->GetWidget<NWidgetResizeBase>(WID_SGI_BASESET); wid != nullptr && wid->current_x > 0) {
			changed |= wid->UpdateMultilineWidgetSize(GetString(STR_INTRO_BASESET, _missing_extra_graphics), 3);
		}

		if (NWidgetResizeBase *wid = this->GetWidget<NWidgetResizeBase>(WID_SGI_TRANSLATION); wid != nullptr && wid->current_x > 0) {
			changed |= wid->UpdateMultilineWidgetSize(GetString(STR_INTRO_TRANSLATION, _current_language->missing), 3);
		}

		if (changed) this->ReInit(0, 0, this->flags.Test(WindowFlag::Centred));
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_SGI_GENERATE_GAME:
				_is_network_server = false;
				if (_ctrl_pressed) {
					StartNewGameWithoutGUI(GENERATE_NEW_SEED);
				} else {
					ShowGenerateLandscape();
				}
				break;
			case WID_SGI_LOAD_GAME:
				_is_network_server = false;
				ShowSaveLoadDialog(FT_SAVEGAME, SLO_LOAD);
				break;
			case WID_SGI_PLAY_SCENARIO:
				_is_network_server = false;
				ShowSaveLoadDialog(FT_SCENARIO, SLO_LOAD);
				break;
			case WID_SGI_PLAY_HEIGHTMAP:
				_is_network_server = false;
				ShowSaveLoadDialog(FT_HEIGHTMAP,SLO_LOAD);
				break;
			case WID_SGI_EDIT_SCENARIO:
				_is_network_server = false;
				StartScenarioEditor();
				break;

			case WID_SGI_PLAY_NETWORK:
				if (!_network_available) {
					ShowErrorMessage(GetEncodedString(STR_NETWORK_ERROR_NOTAVAILABLE), {}, WL_ERROR);
				} else {
					ShowNetworkGameWindow();
				}
				break;

			case WID_SGI_OPTIONS:         ShowGameOptions(); break;
			case WID_SGI_HIGHSCORE:       ShowHighscoreTable(); break;
			case WID_SGI_HELP:            ShowHelpWindow(); break;
			case WID_SGI_CONTENT_DOWNLOAD:
				if (!_network_available) {
					ShowErrorMessage(GetEncodedString(STR_NETWORK_ERROR_NOTAVAILABLE), {}, WL_ERROR);
				} else {
					ShowNetworkContentListWindow();
				}
				break;
			case WID_SGI_EXIT:            HandleExitGameRequest(); break;
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_select_game_widgets = {
	NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_INTRO_CAPTION),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse),

			/* Single player */
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(WWT_PUSHIMGTEXTBTN, COLOUR_ORANGE, WID_SGI_GENERATE_GAME), SetToolbarMinimalSize(1), SetSpriteStringTip(SPR_IMG_LANDSCAPING, STR_INTRO_NEW_GAME, STR_INTRO_TOOLTIP_NEW_GAME), SetAlignment(SA_LEFT | SA_VERT_CENTER), SetFill(1, 0),
				NWidget(WWT_PUSHIMGTEXTBTN, COLOUR_ORANGE, WID_SGI_PLAY_HEIGHTMAP), SetToolbarMinimalSize(1), SetSpriteStringTip(SPR_IMG_SHOW_COUNTOURS, STR_INTRO_PLAY_HEIGHTMAP, STR_INTRO_TOOLTIP_PLAY_HEIGHTMAP), SetAlignment(SA_LEFT | SA_VERT_CENTER), SetFill(1, 0),
				NWidget(WWT_PUSHIMGTEXTBTN, COLOUR_ORANGE, WID_SGI_PLAY_SCENARIO), SetToolbarMinimalSize(1), SetSpriteStringTip(SPR_IMG_SUBSIDIES, STR_INTRO_PLAY_SCENARIO, STR_INTRO_TOOLTIP_PLAY_SCENARIO), SetAlignment(SA_LEFT | SA_VERT_CENTER), SetFill(1, 0),
				NWidget(WWT_PUSHIMGTEXTBTN, COLOUR_ORANGE, WID_SGI_LOAD_GAME), SetToolbarMinimalSize(1), SetSpriteStringTip(SPR_IMG_SAVE, STR_INTRO_LOAD_GAME, STR_INTRO_TOOLTIP_LOAD_GAME), SetAlignment(SA_LEFT | SA_VERT_CENTER), SetFill(1, 0),
				NWidget(WWT_PUSHIMGTEXTBTN, COLOUR_ORANGE, WID_SGI_HIGHSCORE), SetToolbarMinimalSize(1), SetSpriteStringTip(SPR_IMG_COMPANY_LEAGUE, STR_INTRO_HIGHSCORE, STR_INTRO_TOOLTIP_HIGHSCORE), SetAlignment(SA_LEFT | SA_VERT_CENTER), SetFill(1, 0),
			EndContainer(),

			/* Multi player */
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(WWT_PUSHIMGTEXTBTN, COLOUR_ORANGE, WID_SGI_PLAY_NETWORK), SetToolbarMinimalSize(1), SetSpriteStringTip(SPR_IMG_COMPANY_GENERAL, STR_INTRO_MULTIPLAYER, STR_INTRO_TOOLTIP_MULTIPLAYER), SetAlignment(SA_LEFT | SA_VERT_CENTER), SetFill(1, 0),
			EndContainer(),

			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_SGI_BASESET_SELECTION),
				NWidget(NWID_VERTICAL),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_SGI_BASESET), SetFill(1, 0),
				EndContainer(),
			EndContainer(),

			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_SGI_TRANSLATION_SELECTION),
				NWidget(NWID_VERTICAL),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_SGI_TRANSLATION), SetFill(1, 0),
				EndContainer(),
			EndContainer(),

			/* Other */
			NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(WWT_PUSHIMGTEXTBTN, COLOUR_ORANGE, WID_SGI_OPTIONS), SetToolbarMinimalSize(1), SetSpriteStringTip(SPR_IMG_SETTINGS, STR_INTRO_GAME_OPTIONS, STR_INTRO_TOOLTIP_GAME_OPTIONS), SetAlignment(SA_LEFT | SA_VERT_CENTER), SetFill(1, 0),
				NWidget(WWT_PUSHIMGTEXTBTN, COLOUR_ORANGE, WID_SGI_CONTENT_DOWNLOAD), SetToolbarMinimalSize(1), SetSpriteStringTip(SPR_IMG_SHOW_VEHICLES, STR_INTRO_ONLINE_CONTENT, STR_INTRO_TOOLTIP_ONLINE_CONTENT), SetAlignment(SA_LEFT | SA_VERT_CENTER), SetFill(1, 0),
				NWidget(WWT_PUSHIMGTEXTBTN, COLOUR_ORANGE, WID_SGI_EDIT_SCENARIO), SetToolbarMinimalSize(1), SetSpriteStringTip(SPR_IMG_SMALLMAP, STR_INTRO_SCENARIO_EDITOR, STR_INTRO_TOOLTIP_SCENARIO_EDITOR), SetAlignment(SA_LEFT | SA_VERT_CENTER), SetFill(1, 0),
				NWidget(WWT_PUSHIMGTEXTBTN, COLOUR_ORANGE, WID_SGI_HELP), SetToolbarMinimalSize(1), SetSpriteStringTip(SPR_IMG_QUERY, STR_INTRO_HELP, STR_INTRO_TOOLTIP_HELP), SetAlignment(SA_LEFT | SA_VERT_CENTER), SetFill(1, 0),
			EndContainer(),

			NWidget(NWID_VERTICAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WID_SGI_EXIT), SetToolbarMinimalSize(1), SetStringTip(STR_INTRO_QUIT, STR_INTRO_TOOLTIP_QUIT),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _select_game_desc(
	WDP_CENTER, {}, 0, 0,
	WC_SELECT_GAME, WC_NONE,
	WindowDefaultFlag::NoClose,
	_nested_select_game_widgets
);

void ShowSelectGameWindow()
{
	new SelectGameWindow(_select_game_desc);
}

static void AskExitGameCallback(Window *, bool confirmed)
{
	if (confirmed) {
		_survey.Transmit(NetworkSurveyHandler::Reason::Exit, true);
		_exit_game = true;
	}
}

void AskExitGame()
{
	ShowQuery(
		GetEncodedString(STR_QUIT_CAPTION),
		GetEncodedString(STR_QUIT_ARE_YOU_SURE_YOU_WANT_TO_EXIT_OPENTTD),
		nullptr,
		AskExitGameCallback,
		true
	);
}


static void AskExitToGameMenuCallback(Window *, bool confirmed)
{
	if (confirmed) {
		_switch_mode = SM_MENU;
		ClearErrorMessages();
	}
}

void AskExitToGameMenu()
{
	ShowQuery(
		GetEncodedString(STR_ABANDON_GAME_CAPTION),
		GetEncodedString((_game_mode != GM_EDITOR) ? STR_ABANDON_GAME_QUERY : STR_ABANDON_SCENARIO_QUERY),
		nullptr,
		AskExitToGameMenuCallback,
		true
	);
}
