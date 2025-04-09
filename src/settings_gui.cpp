/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file settings_gui.cpp GUI for settings. */

#include "stdafx.h"
#include "currency.h"
#include "error.h"
#include "settings_gui.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "network/network.h"
#include "network/network_content.h"
#include "town.h"
#include "settings_internal.h"
#include "strings_func.h"
#include "window_func.h"
#include "string_func.h"
#include "dropdown_type.h"
#include "dropdown_func.h"
#include "slider_func.h"
#include "highscore.h"
#include "base_media_base.h"
#include "base_media_graphics.h"
#include "base_media_music.h"
#include "base_media_sounds.h"
#include "company_base.h"
#include "company_func.h"
#include "viewport_func.h"
#include "core/geometry_func.hpp"
#include "ai/ai.hpp"
#include "blitter/factory.hpp"
#include "language.h"
#include "textfile_gui.h"
#include "stringfilter_type.h"
#include "querystring_gui.h"
#include "fontcache.h"
#include "zoom_func.h"
#include "rev.h"
#include "video/video_driver.hpp"
#include "music/music_driver.hpp"
#include "gui.h"
#include "mixer.h"
#include "newgrf_config.h"
#include "network/core/config.h"
#include "network/network_gui.h"
#include "network/network_survey.h"
#include "video/video_driver.hpp"
#include "social_integration.h"
#include "sound_func.h"
#include "settingentry_gui.h"

#include "table/strings.h"

#include "dropdown_common_type.h"

#include "safeguards.h"


#if defined(WITH_FREETYPE) || defined(_WIN32) || defined(WITH_COCOA)
#	define HAS_TRUETYPE_FONT
#endif

static const StringID _autosave_dropdown[] = {
	STR_GAME_OPTIONS_AUTOSAVE_DROPDOWN_OFF,
	STR_GAME_OPTIONS_AUTOSAVE_DROPDOWN_EVERY_10_MINUTES,
	STR_GAME_OPTIONS_AUTOSAVE_DROPDOWN_EVERY_30_MINUTES,
	STR_GAME_OPTIONS_AUTOSAVE_DROPDOWN_EVERY_60_MINUTES,
	STR_GAME_OPTIONS_AUTOSAVE_DROPDOWN_EVERY_120_MINUTES,
	INVALID_STRING_ID,
};

/** Available settings for autosave intervals. */
static const uint32_t _autosave_dropdown_to_minutes[] = {
	0, ///< never
	10,
	30,
	60,
	120,
};

Dimension _setting_circle_size; ///< Dimension of the circle +/- icon. This is here as not all users are within the class of the settings window.

/**
 * Get index of the current screen resolution.
 * @return Index of the current screen resolution if it is a known resolution, _resolutions.size() otherwise.
 */
static uint GetCurrentResolutionIndex()
{
	auto it = std::ranges::find(_resolutions, Dimension(_screen.width, _screen.height));
	return std::distance(_resolutions.begin(), it);
}

static void ShowCustCurrency();

/** Window for displaying the textfile of a BaseSet. */
struct BaseSetTextfileWindow : public TextfileWindow {
	const std::string name; ///< Name of the content.
	const StringID content_type; ///< STR_CONTENT_TYPE_xxx for title.

	BaseSetTextfileWindow(TextfileType file_type, const std::string &name, const std::string &textfile, StringID content_type) : TextfileWindow(file_type), name(name), content_type(content_type)
	{
		this->ConstructWindow();
		this->LoadTextfile(textfile, BASESET_DIR);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_TF_CAPTION) {
			return GetString(stringid, this->content_type, this->name);
		}

		return this->Window::GetWidgetString(widget, stringid);
	}
};

/**
 * Open the BaseSet version of the textfile window.
 * @param file_type The type of textfile to display.
 * @param baseset The BaseSet to use.
 * @param content_type STR_CONTENT_TYPE_xxx for title.
 */
template <class TBaseSet>
void ShowBaseSetTextfileWindow(TextfileType file_type, const TBaseSet *baseset, StringID content_type)
{
	CloseWindowById(WC_TEXTFILE, file_type);
	new BaseSetTextfileWindow(file_type, baseset->name, *baseset->GetTextfile(file_type), content_type);
}

/**
 * Get string to use when listing this set in the settings window.
 * If there are no invalid files, then this is just the set name,
 * otherwise a string is formatted including the number of invalid files.
 * @return the string to display.
 */
template <typename TBaseSet>
static std::string GetListLabel(const TBaseSet *baseset)
{
	if (baseset->GetNumInvalid() == 0) return GetString(STR_JUST_RAW_STRING, baseset->name);
	return GetString(STR_BASESET_STATUS, baseset->name, baseset->GetNumInvalid());
}

template <class T>
DropDownList BuildSetDropDownList(int *selected_index)
{
	int n = T::GetNumSets();
	*selected_index = T::GetIndexOfUsedSet();
	DropDownList list;
	for (int i = 0; i < n; i++) {
		list.push_back(MakeDropDownListStringItem(GetListLabel(T::GetSet(i)), i));
	}
	return list;
}

std::set<int> _refresh_rates = { 30, 60, 75, 90, 100, 120, 144, 240 };

/**
 * Add the refresh rate from the config and the refresh rates from all the monitors to
 * our list of refresh rates shown in the GUI.
 */
static void AddCustomRefreshRates()
{
	/* Add the refresh rate as selected in the config. */
	_refresh_rates.insert(_settings_client.gui.refresh_rate);

	/* Add all the refresh rates of all monitors connected to the machine.  */
	std::vector<int> monitor_rates = VideoDriver::GetInstance()->GetListOfMonitorRefreshRates();
	std::copy(monitor_rates.begin(), monitor_rates.end(), std::inserter(_refresh_rates, _refresh_rates.end()));
}

static const int SCALE_NMARKS = (MAX_INTERFACE_SCALE - MIN_INTERFACE_SCALE) / 25 + 1; // Show marks at 25% increments
static const int VOLUME_NMARKS = 9; // Show 5 values and 4 empty marks.

static std::optional<std::string> ScaleMarkFunc(int, int, int value)
{
	/* Label only every 100% mark. */
	if (value % 100 != 0) return std::string{};

	return GetString(STR_GAME_OPTIONS_GUI_SCALE_MARK, value / 100, 0);
}

static std::optional<std::string> VolumeMarkFunc(int, int mark, int value)
{
	/* Label only every other mark. */
	if (mark % 2 != 0) return std::string{};

	// 0-127 does not map nicely to 0-100. Dividing first gives us nice round numbers.
	return GetString(STR_GAME_OPTIONS_VOLUME_MARK, value / 31 * 25);
}

static constexpr NWidgetPart _nested_social_plugins_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_FRAME, COLOUR_GREY, WID_GO_SOCIAL_PLUGIN_TITLE),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR), SetMinimalSize(0, 12), SetFill(1, 0), SetStringTip(STR_GAME_OPTIONS_SOCIAL_PLUGIN_PLATFORM),
				NWidget(WWT_TEXT, INVALID_COLOUR, WID_GO_SOCIAL_PLUGIN_PLATFORM), SetMinimalSize(100, 12), SetAlignment(SA_RIGHT),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR), SetMinimalSize(0, 12), SetFill(1, 0), SetStringTip(STR_GAME_OPTIONS_SOCIAL_PLUGIN_STATE),
				NWidget(WWT_TEXT, INVALID_COLOUR, WID_GO_SOCIAL_PLUGIN_STATE), SetMinimalSize(100, 12), SetAlignment(SA_RIGHT),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static constexpr NWidgetPart _nested_social_plugins_none_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXT, INVALID_COLOUR), SetMinimalSize(0, 12), SetFill(1, 0), SetStringTip(STR_GAME_OPTIONS_SOCIAL_PLUGINS_NONE),
	EndContainer(),
};

class NWidgetSocialPlugins : public NWidgetVertical {
public:
	NWidgetSocialPlugins()
	{
		this->plugins = SocialIntegration::GetPlugins();

		if (this->plugins.empty()) {
			auto widget = MakeNWidgets(_nested_social_plugins_none_widgets, nullptr);
			this->Add(std::move(widget));
		} else {
			for (size_t i = 0; i < this->plugins.size(); i++) {
				auto widget = MakeNWidgets(_nested_social_plugins_widgets, nullptr);
				this->Add(std::move(widget));
			}
		}

		this->SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0);
	}

	void FillWidgetLookup(WidgetLookup &widget_lookup) override
	{
		widget_lookup[WID_GO_SOCIAL_PLUGINS] = this;
		NWidgetVertical::FillWidgetLookup(widget_lookup);
	}

	void SetupSmallestSize(Window *w) override
	{
		this->current_index = -1;
		NWidgetVertical::SetupSmallestSize(w);
	}

	/**
	 * Find of all the plugins the one where the member is the widest (in pixels).
	 *
	 * @param member The member to check with.
	 * @return The plugin that has the widest value (in pixels) for the given member.
	 */
	template <typename T>
	std::string &GetWidestPlugin(T SocialIntegrationPlugin::*member) const
	{
		std::string *longest = &(this->plugins[0]->*member);
		int longest_length = 0;

		for (auto *plugin : this->plugins) {
			int length = GetStringBoundingBox(plugin->*member).width;
			if (length > longest_length) {
				longest_length = length;
				longest = &(plugin->*member);
			}
		}

		return *longest;
	}

	std::string GetWidgetString(WidgetID widget, StringID) const
	{
		switch (widget) {
			case WID_GO_SOCIAL_PLUGIN_TITLE:
				/* For SetupSmallestSize, use the longest string we have. */
				if (this->current_index < 0) {
					return GetString(STR_GAME_OPTIONS_SOCIAL_PLUGIN_TITLE, GetWidestPlugin(&SocialIntegrationPlugin::name), GetWidestPlugin(&SocialIntegrationPlugin::version));
				}

				if (this->plugins[this->current_index]->name.empty()) {
					return this->plugins[this->current_index]->basepath;
				}

				return GetString(STR_GAME_OPTIONS_SOCIAL_PLUGIN_TITLE, this->plugins[this->current_index]->name, this->plugins[this->current_index]->version);

			case WID_GO_SOCIAL_PLUGIN_PLATFORM:
				/* For SetupSmallestSize, use the longest string we have. */
				if (this->current_index < 0) {
					return GetWidestPlugin(&SocialIntegrationPlugin::social_platform);
				}

				return this->plugins[this->current_index]->social_platform;

			case WID_GO_SOCIAL_PLUGIN_STATE: {
				static const std::pair<SocialIntegrationPlugin::State, StringID> state_to_string[] = {
					{ SocialIntegrationPlugin::RUNNING, STR_GAME_OPTIONS_SOCIAL_PLUGIN_STATE_RUNNING },
					{ SocialIntegrationPlugin::FAILED, STR_GAME_OPTIONS_SOCIAL_PLUGIN_STATE_FAILED },
					{ SocialIntegrationPlugin::PLATFORM_NOT_RUNNING, STR_GAME_OPTIONS_SOCIAL_PLUGIN_STATE_PLATFORM_NOT_RUNNING },
					{ SocialIntegrationPlugin::UNLOADED, STR_GAME_OPTIONS_SOCIAL_PLUGIN_STATE_UNLOADED },
					{ SocialIntegrationPlugin::DUPLICATE, STR_GAME_OPTIONS_SOCIAL_PLUGIN_STATE_DUPLICATE },
					{ SocialIntegrationPlugin::UNSUPPORTED_API, STR_GAME_OPTIONS_SOCIAL_PLUGIN_STATE_UNSUPPORTED_API },
					{ SocialIntegrationPlugin::INVALID_SIGNATURE, STR_GAME_OPTIONS_SOCIAL_PLUGIN_STATE_INVALID_SIGNATURE },
				};

				/* For SetupSmallestSize, use the longest string we have. */
				if (this->current_index < 0) {
					auto longest_plugin = GetWidestPlugin(&SocialIntegrationPlugin::social_platform);

					/* Set the longest plugin when looking for the longest status. */
					StringID longest = STR_NULL;
					int longest_length = 0;
					for (const auto &[state, string] : state_to_string) {
						int length = GetStringBoundingBox(GetString(string, longest_plugin)).width;
						if (length > longest_length) {
							longest_length = length;
							longest = string;
						}
					}

					return GetString(longest, longest_plugin);
				}

				const auto plugin = this->plugins[this->current_index];

				/* Find the string for the state. */
				for (const auto &[state, string] : state_to_string) {
					if (plugin->state == state) {
						return GetString(string, plugin->social_platform);
					}
				}

				/* Default string, in case no state matches. */
				return GetString(STR_GAME_OPTIONS_SOCIAL_PLUGIN_STATE_FAILED, plugin->social_platform);
			}

			default: NOT_REACHED();
		}
	}

	void Draw(const Window *w) override
	{
		this->current_index = 0;

		for (auto &wid : this->children) {
			wid->Draw(w);
			this->current_index++;
		}
	}

private:
	int current_index = -1;
	std::vector<SocialIntegrationPlugin *> plugins{};
};

/** Construct nested container widget for managing the list of social plugins. */
std::unique_ptr<NWidgetBase> MakeNWidgetSocialPlugins()
{
	return std::make_unique<NWidgetSocialPlugins>();
}

struct GameOptionsWindow : Window {
	GameSettings *opt = nullptr;
	bool reload = false;
	int gui_scale = 0;
	static inline WidgetID active_tab = WID_GO_TAB_GENERAL;

	GameOptionsWindow(WindowDesc &desc) : Window(desc), gui_scale(_gui_scale)
	{
		this->opt = &GetGameSettings();

		AddCustomRefreshRates();

		this->InitNested(WN_GAME_OPTIONS_GAME_OPTIONS);
		this->OnInvalidateData(0);

		this->SetTab(GameOptionsWindow::active_tab);

		if constexpr (!NetworkSurveyHandler::IsSurveyPossible()) this->GetWidget<NWidgetStacked>(WID_GO_SURVEY_SEL)->SetDisplayedPlane(SZSP_NONE);
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		CloseWindowById(WC_CUSTOM_CURRENCY, 0);
		CloseWindowByClass(WC_TEXTFILE);
		if (this->reload) _switch_mode = SM_MENU;
		this->Window::Close();
	}

	/**
	 * Build the dropdown list for a specific widget.
	 * @param widget         Widget to build list for
	 * @param selected_index Currently selected item
	 * @return the built dropdown list, or nullptr if the widget has no dropdown menu.
	 */
	DropDownList BuildDropDownList(WidgetID widget, int *selected_index) const
	{
		DropDownList list;
		switch (widget) {
			case WID_GO_CURRENCY_DROPDOWN: { // Setup currencies dropdown
				*selected_index = this->opt->locale.currency;
				uint64_t disabled = _game_mode == GM_MENU ? 0LL : ~GetMaskOfAllowedCurrencies();

				/* Add non-custom currencies; sorted naturally */
				for (const CurrencySpec &currency : _currency_specs) {
					int i = &currency - _currency_specs.data();
					if (i == CURRENCY_CUSTOM) continue;
					if (currency.code.empty()) {
						list.push_back(MakeDropDownListStringItem(currency.name, i, HasBit(disabled, i)));
					} else {
						list.push_back(MakeDropDownListStringItem(GetString(STR_GAME_OPTIONS_CURRENCY_CODE, currency.name, currency.code), i, HasBit(disabled, i)));
					}
				}
				std::sort(list.begin(), list.end(), DropDownListStringItem::NatSortFunc);

				/* Append custom currency at the end */
				list.push_back(MakeDropDownListDividerItem()); // separator line
				list.push_back(MakeDropDownListStringItem(STR_GAME_OPTIONS_CURRENCY_CUSTOM, CURRENCY_CUSTOM, HasBit(disabled, CURRENCY_CUSTOM)));
				break;
			}

			case WID_GO_AUTOSAVE_DROPDOWN: { // Setup autosave dropdown
				int index = 0;
				for (auto &minutes : _autosave_dropdown_to_minutes) {
					index++;
					if (_settings_client.gui.autosave_interval <= minutes) break;
				}
				*selected_index = index - 1;

				const StringID *items = _autosave_dropdown;
				for (uint i = 0; *items != INVALID_STRING_ID; items++, i++) {
					list.push_back(MakeDropDownListStringItem(*items, i));
				}
				break;
			}

			case WID_GO_LANG_DROPDOWN: { // Setup interface language dropdown
				for (uint i = 0; i < _languages.size(); i++) {
					bool hide_language = IsReleasedVersion() && !_languages[i].IsReasonablyFinished();
					if (hide_language) continue;
					bool hide_percentage = IsReleasedVersion() || _languages[i].missing < _settings_client.gui.missing_strings_threshold;
					char *name;
					if (&_languages[i] == _current_language) {
						*selected_index = i;
						name = _languages[i].own_name;
					} else {
						/* Especially with sprite-fonts, not all localized
						 * names can be rendered. So instead, we use the
						 * international names for anything but the current
						 * selected language. This avoids showing a few ????
						 * entries in the dropdown list. */
						name = _languages[i].name;
					}
					if (hide_percentage) {
						list.push_back(MakeDropDownListStringItem(name, i));
					} else {
						int percentage = (LANGUAGE_TOTAL_STRINGS - _languages[i].missing) * 100 / LANGUAGE_TOTAL_STRINGS;
						list.push_back(MakeDropDownListStringItem(GetString(STR_GAME_OPTIONS_LANGUAGE_PERCENTAGE, name, percentage), i));
					}
				}
				std::sort(list.begin(), list.end(), DropDownListStringItem::NatSortFunc);
				break;
			}

			case WID_GO_RESOLUTION_DROPDOWN: // Setup resolution dropdown
				if (_resolutions.empty()) break;

				*selected_index = GetCurrentResolutionIndex();
				for (uint i = 0; i < _resolutions.size(); i++) {
					list.push_back(MakeDropDownListStringItem(GetString(STR_GAME_OPTIONS_RESOLUTION_ITEM, _resolutions[i].width, _resolutions[i].height), i));
				}
				break;

			case WID_GO_REFRESH_RATE_DROPDOWN: // Setup refresh rate dropdown
				for (auto it = _refresh_rates.begin(); it != _refresh_rates.end(); it++) {
					auto i = std::distance(_refresh_rates.begin(), it);
					if (*it == _settings_client.gui.refresh_rate) *selected_index = i;
					list.push_back(MakeDropDownListStringItem(GetString(STR_GAME_OPTIONS_REFRESH_RATE_ITEM, *it), i));
				}
				break;

			case WID_GO_BASE_GRF_DROPDOWN:
				list = BuildSetDropDownList<BaseGraphics>(selected_index);
				break;

			case WID_GO_BASE_SFX_DROPDOWN:
				list = BuildSetDropDownList<BaseSounds>(selected_index);
				break;

			case WID_GO_BASE_MUSIC_DROPDOWN:
				list = BuildSetDropDownList<BaseMusic>(selected_index);
				break;
		}

		return list;
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_GO_CURRENCY_DROPDOWN: {
				const CurrencySpec &currency = _currency_specs[this->opt->locale.currency];
				if (currency.code.empty()) return GetString(currency.name);
				return GetString(STR_GAME_OPTIONS_CURRENCY_CODE, currency.name, currency.code);
			}

			case WID_GO_AUTOSAVE_DROPDOWN: {
				int index = 0;
				for (auto &minutes : _autosave_dropdown_to_minutes) {
					index++;
					if (_settings_client.gui.autosave_interval <= minutes) break;
				}
				return GetString(_autosave_dropdown[index - 1]);
			}

			case WID_GO_LANG_DROPDOWN:         return _current_language->own_name;
			case WID_GO_BASE_GRF_DROPDOWN:     return GetListLabel(BaseGraphics::GetUsedSet());
			case WID_GO_BASE_SFX_DROPDOWN:     return GetListLabel(BaseSounds::GetUsedSet());
			case WID_GO_BASE_MUSIC_DROPDOWN:   return GetListLabel(BaseMusic::GetUsedSet());
			case WID_GO_REFRESH_RATE_DROPDOWN: return GetString(STR_GAME_OPTIONS_REFRESH_RATE_ITEM, _settings_client.gui.refresh_rate);
			case WID_GO_RESOLUTION_DROPDOWN: {
				auto current_resolution = GetCurrentResolutionIndex();

				if (current_resolution == _resolutions.size()) {
					return GetString(STR_GAME_OPTIONS_RESOLUTION_OTHER);
				}
				return GetString(STR_GAME_OPTIONS_RESOLUTION_ITEM, _resolutions[current_resolution].width, _resolutions[current_resolution].height);
			}

			case WID_GO_SOCIAL_PLUGIN_TITLE:
			case WID_GO_SOCIAL_PLUGIN_PLATFORM:
			case WID_GO_SOCIAL_PLUGIN_STATE: {
				const NWidgetSocialPlugins *plugin = this->GetWidget<NWidgetSocialPlugins>(WID_GO_SOCIAL_PLUGINS);
				assert(plugin != nullptr);

				return plugin->GetWidgetString(widget, stringid);
			}

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_GO_BASE_GRF_DESCRIPTION:
				DrawStringMultiLine(r, GetString(STR_JUST_RAW_STRING, BaseGraphics::GetUsedSet()->GetDescription(GetCurrentLanguageIsoCode())), TC_BLACK);
				break;

			case WID_GO_BASE_SFX_DESCRIPTION:
				DrawStringMultiLine(r, GetString(STR_JUST_RAW_STRING, BaseSounds::GetUsedSet()->GetDescription(GetCurrentLanguageIsoCode())), TC_BLACK);
				break;

			case WID_GO_BASE_MUSIC_DESCRIPTION:
				DrawStringMultiLine(r, GetString(STR_JUST_RAW_STRING, BaseMusic::GetUsedSet()->GetDescription(GetCurrentLanguageIsoCode())), TC_BLACK);
				break;

			case WID_GO_GUI_SCALE:
				DrawSliderWidget(r, MIN_INTERFACE_SCALE, MAX_INTERFACE_SCALE, SCALE_NMARKS, this->gui_scale, ScaleMarkFunc);
				break;

			case WID_GO_VIDEO_DRIVER_INFO:
				DrawStringMultiLine(r, GetString(STR_GAME_OPTIONS_VIDEO_DRIVER_INFO, std::string{VideoDriver::GetInstance()->GetInfoString()}));
				break;

			case WID_GO_BASE_SFX_VOLUME:
				DrawSliderWidget(r, 0, INT8_MAX, VOLUME_NMARKS, _settings_client.music.effect_vol, VolumeMarkFunc);
				break;

			case WID_GO_BASE_MUSIC_VOLUME:
				DrawSliderWidget(r, 0, INT8_MAX, VOLUME_NMARKS, _settings_client.music.music_vol, VolumeMarkFunc);
				break;
		}
	}

	void SetTab(WidgetID widget)
	{
		this->SetWidgetsLoweredState(false, WID_GO_TAB_GENERAL, WID_GO_TAB_GRAPHICS, WID_GO_TAB_SOUND, WID_GO_TAB_SOCIAL);
		this->LowerWidget(widget);
		GameOptionsWindow::active_tab = widget;

		int pane;
		switch (widget) {
			case WID_GO_TAB_GENERAL: pane = 0; break;
			case WID_GO_TAB_GRAPHICS: pane = 1; break;
			case WID_GO_TAB_SOUND: pane = 2; break;
			case WID_GO_TAB_SOCIAL: pane = 3; break;
			default: NOT_REACHED();
		}

		this->GetWidget<NWidgetStacked>(WID_GO_TAB_SELECTION)->SetDisplayedPlane(pane);
		this->SetDirty();
	}

	void OnResize() override
	{
		bool changed = false;

		NWidgetResizeBase *wid = this->GetWidget<NWidgetResizeBase>(WID_GO_BASE_GRF_DESCRIPTION);
		int y = 0;
		for (int i = 0; i < BaseGraphics::GetNumSets(); i++) {
			std::string str = GetString(STR_JUST_RAW_STRING, BaseGraphics::GetSet(i)->GetDescription(GetCurrentLanguageIsoCode()));
			y = std::max(y, GetStringHeight(str, wid->current_x));
		}
		changed |= wid->UpdateVerticalSize(y);

		wid = this->GetWidget<NWidgetResizeBase>(WID_GO_BASE_SFX_DESCRIPTION);
		y = 0;
		for (int i = 0; i < BaseSounds::GetNumSets(); i++) {
			std::string str = GetString(STR_JUST_RAW_STRING, BaseSounds::GetSet(i)->GetDescription(GetCurrentLanguageIsoCode()));
			y = std::max(y, GetStringHeight(str, wid->current_x));
		}
		changed |= wid->UpdateVerticalSize(y);

		wid = this->GetWidget<NWidgetResizeBase>(WID_GO_BASE_MUSIC_DESCRIPTION);
		y = 0;
		for (int i = 0; i < BaseMusic::GetNumSets(); i++) {
			std::string str = GetString(STR_JUST_RAW_STRING, BaseMusic::GetSet(i)->GetDescription(GetCurrentLanguageIsoCode()));
			y = std::max(y, GetStringHeight(str, wid->current_x));
		}
		changed |= wid->UpdateVerticalSize(y);

		wid = this->GetWidget<NWidgetResizeBase>(WID_GO_VIDEO_DRIVER_INFO);
		std::string str = GetString(STR_GAME_OPTIONS_VIDEO_DRIVER_INFO, std::string{VideoDriver::GetInstance()->GetInfoString()});
		y = GetStringHeight(str, wid->current_x);
		changed |= wid->UpdateVerticalSize(y);

		if (changed) this->ReInit(0, 0, this->flags.Test(WindowFlag::Centred));
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_GO_TEXT_SFX_VOLUME:
			case WID_GO_TEXT_MUSIC_VOLUME: {
				Dimension d = maxdim(GetStringBoundingBox(STR_GAME_OPTIONS_SFX_VOLUME), GetStringBoundingBox(STR_GAME_OPTIONS_MUSIC_VOLUME));
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_GO_CURRENCY_DROPDOWN:
			case WID_GO_AUTOSAVE_DROPDOWN:
			case WID_GO_LANG_DROPDOWN:
			case WID_GO_RESOLUTION_DROPDOWN:
			case WID_GO_REFRESH_RATE_DROPDOWN:
			case WID_GO_BASE_GRF_DROPDOWN:
			case WID_GO_BASE_SFX_DROPDOWN:
			case WID_GO_BASE_MUSIC_DROPDOWN: {
				int selected;
				size.width = std::max(size.width, GetDropDownListDimension(this->BuildDropDownList(widget, &selected)).width + padding.width);
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (widget >= WID_GO_BASE_GRF_TEXTFILE && widget < WID_GO_BASE_GRF_TEXTFILE + TFT_CONTENT_END) {
			if (BaseGraphics::GetUsedSet() == nullptr) return;

			ShowBaseSetTextfileWindow((TextfileType)(widget - WID_GO_BASE_GRF_TEXTFILE), BaseGraphics::GetUsedSet(), STR_CONTENT_TYPE_BASE_GRAPHICS);
			return;
		}
		if (widget >= WID_GO_BASE_SFX_TEXTFILE && widget < WID_GO_BASE_SFX_TEXTFILE + TFT_CONTENT_END) {
			if (BaseSounds::GetUsedSet() == nullptr) return;

			ShowBaseSetTextfileWindow((TextfileType)(widget - WID_GO_BASE_SFX_TEXTFILE), BaseSounds::GetUsedSet(), STR_CONTENT_TYPE_BASE_SOUNDS);
			return;
		}
		if (widget >= WID_GO_BASE_MUSIC_TEXTFILE && widget < WID_GO_BASE_MUSIC_TEXTFILE + TFT_CONTENT_END) {
			if (BaseMusic::GetUsedSet() == nullptr) return;

			ShowBaseSetTextfileWindow((TextfileType)(widget - WID_GO_BASE_MUSIC_TEXTFILE), BaseMusic::GetUsedSet(), STR_CONTENT_TYPE_BASE_MUSIC);
			return;
		}
		switch (widget) {
			case WID_GO_TAB_GENERAL:
			case WID_GO_TAB_GRAPHICS:
			case WID_GO_TAB_SOUND:
			case WID_GO_TAB_SOCIAL:
				this->SetTab(widget);
				break;

			case WID_GO_SURVEY_PARTICIPATE_BUTTON:
				switch (_settings_client.network.participate_survey) {
					case PS_ASK:
					case PS_NO:
						_settings_client.network.participate_survey = PS_YES;
						break;

					case PS_YES:
						_settings_client.network.participate_survey = PS_NO;
						break;
				}

				this->SetWidgetLoweredState(WID_GO_SURVEY_PARTICIPATE_BUTTON, _settings_client.network.participate_survey == PS_YES);
				this->SetWidgetDirty(WID_GO_SURVEY_PARTICIPATE_BUTTON);
				break;

			case WID_GO_SURVEY_LINK_BUTTON:
				OpenBrowser(NETWORK_SURVEY_DETAILS_LINK);
				break;

			case WID_GO_SURVEY_PREVIEW_BUTTON:
				ShowSurveyResultTextfileWindow();
				break;

			case WID_GO_FULLSCREEN_BUTTON: // Click fullscreen on/off
				/* try to toggle full-screen on/off */
				if (!ToggleFullScreen(!_fullscreen)) {
					ShowErrorMessage(GetEncodedString(STR_ERROR_FULLSCREEN_FAILED), {}, WL_ERROR);
				}
				this->SetWidgetLoweredState(WID_GO_FULLSCREEN_BUTTON, _fullscreen);
				this->SetWidgetDirty(WID_GO_FULLSCREEN_BUTTON);
				break;

			case WID_GO_VIDEO_ACCEL_BUTTON:
				_video_hw_accel = !_video_hw_accel;
				ShowErrorMessage(GetEncodedString(STR_GAME_OPTIONS_VIDEO_ACCELERATION_RESTART), {}, WL_INFO);
				this->SetWidgetLoweredState(WID_GO_VIDEO_ACCEL_BUTTON, _video_hw_accel);
				this->SetWidgetDirty(WID_GO_VIDEO_ACCEL_BUTTON);
#ifndef __APPLE__
				this->SetWidgetLoweredState(WID_GO_VIDEO_VSYNC_BUTTON, _video_hw_accel && _video_vsync);
				this->SetWidgetDisabledState(WID_GO_VIDEO_VSYNC_BUTTON, !_video_hw_accel);
				this->SetWidgetDirty(WID_GO_VIDEO_VSYNC_BUTTON);
#endif
				break;

			case WID_GO_VIDEO_VSYNC_BUTTON:
				if (!_video_hw_accel) break;

				_video_vsync = !_video_vsync;
				VideoDriver::GetInstance()->ToggleVsync(_video_vsync);

				this->SetWidgetLoweredState(WID_GO_VIDEO_VSYNC_BUTTON, _video_vsync);
				this->SetWidgetDirty(WID_GO_VIDEO_VSYNC_BUTTON);
				this->SetWidgetDisabledState(WID_GO_REFRESH_RATE_DROPDOWN, _video_vsync);
				this->SetWidgetDirty(WID_GO_REFRESH_RATE_DROPDOWN);
				break;

			case WID_GO_GUI_SCALE_BEVEL_BUTTON: {
				_settings_client.gui.scale_bevels = !_settings_client.gui.scale_bevels;

				this->SetWidgetLoweredState(WID_GO_GUI_SCALE_BEVEL_BUTTON, _settings_client.gui.scale_bevels);
				this->SetDirty();

				SetupWidgetDimensions();
				ReInitAllWindows(true);
				break;
			}

#ifdef HAS_TRUETYPE_FONT
			case WID_GO_GUI_FONT_SPRITE:
				_fcsettings.prefer_sprite = !_fcsettings.prefer_sprite;

				this->SetWidgetLoweredState(WID_GO_GUI_FONT_SPRITE, _fcsettings.prefer_sprite);
				this->SetWidgetDisabledState(WID_GO_GUI_FONT_AA, _fcsettings.prefer_sprite);
				this->SetDirty();

				InitFontCache(false);
				InitFontCache(true);
				ClearFontCache();
				CheckForMissingGlyphs();
				SetupWidgetDimensions();
				UpdateAllVirtCoords();
				ReInitAllWindows(true);
				break;

			case WID_GO_GUI_FONT_AA:
				_fcsettings.global_aa = !_fcsettings.global_aa;

				this->SetWidgetLoweredState(WID_GO_GUI_FONT_AA, _fcsettings.global_aa);
				MarkWholeScreenDirty();

				ClearFontCache();
				break;
#endif /* HAS_TRUETYPE_FONT */

			case WID_GO_GUI_SCALE:
				if (ClickSliderWidget(this->GetWidget<NWidgetBase>(widget)->GetCurrentRect(), pt, MIN_INTERFACE_SCALE, MAX_INTERFACE_SCALE, _ctrl_pressed ? 0 : SCALE_NMARKS, this->gui_scale)) {
					this->SetWidgetDirty(widget);
				}

				if (click_count > 0) this->mouse_capture_widget = widget;
				break;

			case WID_GO_GUI_SCALE_AUTO:
			{
				if (_gui_scale_cfg == -1) {
					_gui_scale_cfg = _gui_scale;
					this->SetWidgetLoweredState(WID_GO_GUI_SCALE_AUTO, false);
				} else {
					_gui_scale_cfg = -1;
					this->SetWidgetLoweredState(WID_GO_GUI_SCALE_AUTO, true);
					if (AdjustGUIZoom(false)) ReInitAllWindows(true);
					this->gui_scale = _gui_scale;
				}
				this->SetWidgetDirty(widget);
				break;
			}

			case WID_GO_BASE_GRF_PARAMETERS: {
				auto *used_set = BaseGraphics::GetUsedSet();
				if (used_set == nullptr || !used_set->IsConfigurable()) break;
				GRFConfig &extra_cfg = used_set->GetOrCreateExtraConfig();
				if (extra_cfg.param.empty()) extra_cfg.SetParameterDefaults();
				OpenGRFParameterWindow(true, extra_cfg, _game_mode == GM_MENU);
				if (_game_mode == GM_MENU) this->reload = true;
				break;
			}

			case WID_GO_BASE_SFX_VOLUME:
			case WID_GO_BASE_MUSIC_VOLUME: {
				uint8_t &vol = (widget == WID_GO_BASE_MUSIC_VOLUME) ? _settings_client.music.music_vol : _settings_client.music.effect_vol;
				if (ClickSliderWidget(this->GetWidget<NWidgetBase>(widget)->GetCurrentRect(), pt, 0, INT8_MAX, 0, vol)) {
					if (widget == WID_GO_BASE_MUSIC_VOLUME) {
						MusicDriver::GetInstance()->SetVolume(vol);
					} else {
						SetEffectVolume(vol);
					}
					this->SetWidgetDirty(widget);
					SetWindowClassesDirty(WC_MUSIC_WINDOW);
				}

				if (click_count > 0) this->mouse_capture_widget = widget;
				break;
			}

			case WID_GO_BASE_MUSIC_JUKEBOX: {
				ShowMusicWindow();
				break;
			}

			case WID_GO_BASE_GRF_OPEN_URL:
				if (BaseGraphics::GetUsedSet() == nullptr || BaseGraphics::GetUsedSet()->url.empty()) return;
				OpenBrowser(BaseGraphics::GetUsedSet()->url);
				break;

			case WID_GO_BASE_SFX_OPEN_URL:
				if (BaseSounds::GetUsedSet() == nullptr || BaseSounds::GetUsedSet()->url.empty()) return;
				OpenBrowser(BaseSounds::GetUsedSet()->url);
				break;

			case WID_GO_BASE_MUSIC_OPEN_URL:
				if (BaseMusic::GetUsedSet() == nullptr || BaseMusic::GetUsedSet()->url.empty()) return;
				OpenBrowser(BaseMusic::GetUsedSet()->url);
				break;

			case WID_GO_BASE_GRF_CONTENT_DOWNLOAD:
				ShowNetworkContentListWindow(nullptr, CONTENT_TYPE_BASE_GRAPHICS);
				break;

			case WID_GO_BASE_SFX_CONTENT_DOWNLOAD:
				ShowNetworkContentListWindow(nullptr, CONTENT_TYPE_BASE_SOUNDS);
				break;

			case WID_GO_BASE_MUSIC_CONTENT_DOWNLOAD:
				ShowNetworkContentListWindow(nullptr, CONTENT_TYPE_BASE_MUSIC);
				break;

			case WID_GO_CURRENCY_DROPDOWN:
			case WID_GO_AUTOSAVE_DROPDOWN:
			case WID_GO_LANG_DROPDOWN:
			case WID_GO_RESOLUTION_DROPDOWN:
			case WID_GO_REFRESH_RATE_DROPDOWN:
			case WID_GO_BASE_GRF_DROPDOWN:
			case WID_GO_BASE_SFX_DROPDOWN:
			case WID_GO_BASE_MUSIC_DROPDOWN: {
				int selected;
				DropDownList list = this->BuildDropDownList(widget, &selected);
				if (!list.empty()) {
					ShowDropDownList(this, std::move(list), selected, widget);
				} else {
					if (widget == WID_GO_RESOLUTION_DROPDOWN) ShowErrorMessage(GetEncodedString(STR_ERROR_RESOLUTION_LIST_FAILED), {}, WL_ERROR);
				}
				break;
			}
		}
	}

	void OnMouseLoop() override
	{
		if (_left_button_down || this->gui_scale == _gui_scale) return;

		_gui_scale_cfg = this->gui_scale;

		if (AdjustGUIZoom(false)) {
			ReInitAllWindows(true);
			this->SetWidgetLoweredState(WID_GO_GUI_SCALE_AUTO, false);
			this->SetDirty();
		}
	}

	void OnDropdownSelect(WidgetID widget, int index) override
	{
		switch (widget) {
			case WID_GO_CURRENCY_DROPDOWN: // Currency
				if (index == CURRENCY_CUSTOM) ShowCustCurrency();
				this->opt->locale.currency = index;
				ReInitAllWindows(false);
				break;

			case WID_GO_AUTOSAVE_DROPDOWN: // Autosave options
				_settings_client.gui.autosave_interval = _autosave_dropdown_to_minutes[index];
				ChangeAutosaveFrequency(false);
				this->SetDirty();
				break;

			case WID_GO_LANG_DROPDOWN: // Change interface language
				ReadLanguagePack(&_languages[index]);
				CloseWindowByClass(WC_QUERY_STRING);
				CheckForMissingGlyphs();
				ClearAllCachedNames();
				UpdateAllVirtCoords();
				CheckBlitter();
				ReInitAllWindows(false);
				break;

			case WID_GO_RESOLUTION_DROPDOWN: // Change resolution
				if ((uint)index < _resolutions.size() && ChangeResInGame(_resolutions[index].width, _resolutions[index].height)) {
					this->SetDirty();
				}
				break;

			case WID_GO_REFRESH_RATE_DROPDOWN: {
				_settings_client.gui.refresh_rate = *std::next(_refresh_rates.begin(), index);
				if (_settings_client.gui.refresh_rate > 60) {
					/* Show warning to the user that this refresh rate might not be suitable on
					 * larger maps with many NewGRFs and vehicles. */
					ShowErrorMessage(GetEncodedString(STR_GAME_OPTIONS_REFRESH_RATE_WARNING), {}, WL_INFO);
				}
				break;
			}

			case WID_GO_BASE_GRF_DROPDOWN:
				if (_game_mode == GM_MENU) {
					CloseWindowByClass(WC_GRF_PARAMETERS);
					auto set = BaseGraphics::GetSet(index);
					BaseGraphics::SetSet(set);
					this->reload = true;
					this->InvalidateData();
				}
				break;

			case WID_GO_BASE_SFX_DROPDOWN:
				ChangeSoundSet(index);
				break;

			case WID_GO_BASE_MUSIC_DROPDOWN:
				ChangeMusicSet(index);
				break;
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data. @see GameOptionsInvalidationData
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->SetWidgetLoweredState(WID_GO_SURVEY_PARTICIPATE_BUTTON, _settings_client.network.participate_survey == PS_YES);
		this->SetWidgetLoweredState(WID_GO_FULLSCREEN_BUTTON, _fullscreen);
		this->SetWidgetLoweredState(WID_GO_VIDEO_ACCEL_BUTTON, _video_hw_accel);
		this->SetWidgetDisabledState(WID_GO_REFRESH_RATE_DROPDOWN, _video_vsync);

#ifndef __APPLE__
		this->SetWidgetLoweredState(WID_GO_VIDEO_VSYNC_BUTTON, _video_hw_accel && _video_vsync);
		this->SetWidgetDisabledState(WID_GO_VIDEO_VSYNC_BUTTON, !_video_hw_accel);
#endif

		this->SetWidgetLoweredState(WID_GO_GUI_SCALE_AUTO, _gui_scale_cfg == -1);
		this->SetWidgetLoweredState(WID_GO_GUI_SCALE_BEVEL_BUTTON, _settings_client.gui.scale_bevels);
#ifdef HAS_TRUETYPE_FONT
		this->SetWidgetLoweredState(WID_GO_GUI_FONT_SPRITE, _fcsettings.prefer_sprite);
		this->SetWidgetLoweredState(WID_GO_GUI_FONT_AA, _fcsettings.global_aa);
		this->SetWidgetDisabledState(WID_GO_GUI_FONT_AA, _fcsettings.prefer_sprite);
#endif /* HAS_TRUETYPE_FONT */

		this->SetWidgetDisabledState(WID_GO_BASE_GRF_DROPDOWN, _game_mode != GM_MENU);

		this->SetWidgetDisabledState(WID_GO_BASE_GRF_PARAMETERS, BaseGraphics::GetUsedSet() == nullptr || !BaseGraphics::GetUsedSet()->IsConfigurable());

		this->SetWidgetDisabledState(WID_GO_BASE_GRF_OPEN_URL, BaseGraphics::GetUsedSet() == nullptr || BaseGraphics::GetUsedSet()->url.empty());
		this->SetWidgetDisabledState(WID_GO_BASE_SFX_OPEN_URL, BaseSounds::GetUsedSet() == nullptr || BaseSounds::GetUsedSet()->url.empty());
		this->SetWidgetDisabledState(WID_GO_BASE_MUSIC_OPEN_URL, BaseMusic::GetUsedSet() == nullptr || BaseMusic::GetUsedSet()->url.empty());

		for (TextfileType tft = TFT_CONTENT_BEGIN; tft < TFT_CONTENT_END; tft++) {
			this->SetWidgetDisabledState(WID_GO_BASE_GRF_TEXTFILE + tft, BaseGraphics::GetUsedSet() == nullptr || !BaseGraphics::GetUsedSet()->GetTextfile(tft).has_value());
			this->SetWidgetDisabledState(WID_GO_BASE_SFX_TEXTFILE + tft, BaseSounds::GetUsedSet() == nullptr || !BaseSounds::GetUsedSet()->GetTextfile(tft).has_value());
			this->SetWidgetDisabledState(WID_GO_BASE_MUSIC_TEXTFILE + tft, BaseMusic::GetUsedSet() == nullptr || !BaseMusic::GetUsedSet()->GetTextfile(tft).has_value());
		}

		this->SetWidgetsDisabledState(!_network_available, WID_GO_BASE_GRF_CONTENT_DOWNLOAD, WID_GO_BASE_SFX_CONTENT_DOWNLOAD, WID_GO_BASE_MUSIC_CONTENT_DOWNLOAD);
	}
};

static constexpr NWidgetPart _nested_game_options_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetStringTip(STR_GAME_OPTIONS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPadding(WidgetDimensions::unscaled.sparse),
			NWidget(WWT_TEXTBTN, COLOUR_YELLOW, WID_GO_TAB_GENERAL),  SetMinimalTextLines(2, 0), SetStringTip(STR_GAME_OPTIONS_TAB_GENERAL, STR_GAME_OPTIONS_TAB_GENERAL_TOOLTIP), SetFill(1, 0),
			NWidget(WWT_TEXTBTN, COLOUR_YELLOW, WID_GO_TAB_GRAPHICS), SetMinimalTextLines(2, 0), SetStringTip(STR_GAME_OPTIONS_TAB_GRAPHICS, STR_GAME_OPTIONS_TAB_GRAPHICS_TOOLTIP), SetFill(1, 0),
			NWidget(WWT_TEXTBTN, COLOUR_YELLOW, WID_GO_TAB_SOUND),    SetMinimalTextLines(2, 0), SetStringTip(STR_GAME_OPTIONS_TAB_SOUND, STR_GAME_OPTIONS_TAB_SOUND_TOOLTIP), SetFill(1, 0),
			NWidget(WWT_TEXTBTN, COLOUR_YELLOW, WID_GO_TAB_SOCIAL),   SetMinimalTextLines(2, 0), SetStringTip(STR_GAME_OPTIONS_TAB_SOCIAL, STR_GAME_OPTIONS_TAB_SOCIAL_TOOLTIP), SetFill(1, 0),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_GO_TAB_SELECTION),
			/* General tab */
			NWidget(NWID_VERTICAL), SetPadding(WidgetDimensions::unscaled.sparse), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0),
				NWidget(WWT_FRAME, COLOUR_GREY), SetStringTip(STR_GAME_OPTIONS_LANGUAGE),
					NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_LANG_DROPDOWN), SetMinimalSize(100, 12), SetToolTip(STR_GAME_OPTIONS_LANGUAGE_TOOLTIP), SetFill(1, 0),
				EndContainer(),

				NWidget(WWT_FRAME, COLOUR_GREY), SetStringTip(STR_GAME_OPTIONS_AUTOSAVE_FRAME),
					NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_AUTOSAVE_DROPDOWN), SetMinimalSize(100, 12), SetToolTip(STR_GAME_OPTIONS_AUTOSAVE_DROPDOWN_TOOLTIP), SetFill(1, 0),
				EndContainer(),

				NWidget(WWT_FRAME, COLOUR_GREY), SetStringTip(STR_GAME_OPTIONS_CURRENCY_UNITS_FRAME),
					NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_CURRENCY_DROPDOWN), SetMinimalSize(100, 12), SetToolTip(STR_GAME_OPTIONS_CURRENCY_UNITS_DROPDOWN_TOOLTIP), SetFill(1, 0),
				EndContainer(),

				NWidget(NWID_SELECTION, INVALID_COLOUR, WID_GO_SURVEY_SEL),
					NWidget(WWT_FRAME, COLOUR_GREY), SetStringTip(STR_GAME_OPTIONS_PARTICIPATE_SURVEY_FRAME), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetMinimalSize(0, 12), SetFill(1, 0), SetStringTip(STR_GAME_OPTIONS_PARTICIPATE_SURVEY),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_GO_SURVEY_PARTICIPATE_BUTTON), SetAspect(WidgetDimensions::ASPECT_SETTINGS_BUTTON), SetStringTip(STR_EMPTY, STR_GAME_OPTIONS_PARTICIPATE_SURVEY_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_GO_SURVEY_PREVIEW_BUTTON), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_GAME_OPTIONS_PARTICIPATE_SURVEY_PREVIEW, STR_GAME_OPTIONS_PARTICIPATE_SURVEY_PREVIEW_TOOLTIP),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_GO_SURVEY_LINK_BUTTON), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_GAME_OPTIONS_PARTICIPATE_SURVEY_LINK, STR_GAME_OPTIONS_PARTICIPATE_SURVEY_LINK_TOOLTIP),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),

			/* Graphics tab */
			NWidget(NWID_VERTICAL), SetPadding(WidgetDimensions::unscaled.sparse), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0),
				NWidget(WWT_FRAME, COLOUR_GREY), SetStringTip(STR_GAME_OPTIONS_GUI_SCALE_FRAME),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
						NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GO_GUI_SCALE), SetMinimalSize(67, 0), SetMinimalTextLines(1, 12 + WidgetDimensions::unscaled.vsep_normal, FS_SMALL), SetFill(0, 0), SetToolTip(STR_GAME_OPTIONS_GUI_SCALE_TOOLTIP),
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetMinimalSize(0, 12), SetFill(1, 0), SetStringTip(STR_GAME_OPTIONS_GUI_SCALE_AUTO),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_GO_GUI_SCALE_AUTO), SetAspect(WidgetDimensions::ASPECT_SETTINGS_BUTTON), SetStringTip(STR_EMPTY, STR_GAME_OPTIONS_GUI_SCALE_AUTO_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetMinimalSize(0, 12), SetFill(1, 0), SetStringTip(STR_GAME_OPTIONS_GUI_SCALE_BEVELS),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_GO_GUI_SCALE_BEVEL_BUTTON), SetAspect(WidgetDimensions::ASPECT_SETTINGS_BUTTON), SetStringTip(STR_EMPTY, STR_GAME_OPTIONS_GUI_SCALE_BEVELS_TOOLTIP),
						EndContainer(),
#ifdef HAS_TRUETYPE_FONT
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetMinimalSize(0, 12), SetFill(1, 0), SetStringTip(STR_GAME_OPTIONS_GUI_FONT_SPRITE),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_GO_GUI_FONT_SPRITE), SetAspect(WidgetDimensions::ASPECT_SETTINGS_BUTTON), SetStringTip(STR_EMPTY, STR_GAME_OPTIONS_GUI_FONT_SPRITE_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetMinimalSize(0, 12), SetFill(1, 0), SetStringTip(STR_GAME_OPTIONS_GUI_FONT_AA),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_GO_GUI_FONT_AA), SetAspect(WidgetDimensions::ASPECT_SETTINGS_BUTTON), SetStringTip(STR_EMPTY, STR_GAME_OPTIONS_GUI_FONT_AA_TOOLTIP),
						EndContainer(),
#endif /* HAS_TRUETYPE_FONT */
					EndContainer(),
				EndContainer(),

				NWidget(WWT_FRAME, COLOUR_GREY), SetStringTip(STR_GAME_OPTIONS_GRAPHICS),
					NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetMinimalSize(0, 12), SetFill(1, 0), SetStringTip(STR_GAME_OPTIONS_RESOLUTION),
							NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_RESOLUTION_DROPDOWN), SetMinimalSize(100, 12), SetToolTip(STR_GAME_OPTIONS_RESOLUTION_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetMinimalSize(0, 12), SetFill(1, 0), SetStringTip(STR_GAME_OPTIONS_REFRESH_RATE),
							NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_REFRESH_RATE_DROPDOWN), SetMinimalSize(100, 12), SetToolTip(STR_GAME_OPTIONS_REFRESH_RATE_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetMinimalSize(0, 12), SetFill(1, 0), SetStringTip(STR_GAME_OPTIONS_FULLSCREEN),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_GO_FULLSCREEN_BUTTON), SetAspect(WidgetDimensions::ASPECT_SETTINGS_BUTTON), SetStringTip(STR_EMPTY, STR_GAME_OPTIONS_FULLSCREEN_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetMinimalSize(0, 12), SetFill(1, 0), SetStringTip(STR_GAME_OPTIONS_VIDEO_ACCELERATION),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_GO_VIDEO_ACCEL_BUTTON), SetAspect(WidgetDimensions::ASPECT_SETTINGS_BUTTON), SetStringTip(STR_EMPTY, STR_GAME_OPTIONS_VIDEO_ACCELERATION_TOOLTIP),
						EndContainer(),
#ifndef __APPLE__
						NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
							NWidget(WWT_TEXT, INVALID_COLOUR), SetMinimalSize(0, 12), SetFill(1, 0), SetStringTip(STR_GAME_OPTIONS_VIDEO_VSYNC),
							NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_GO_VIDEO_VSYNC_BUTTON), SetAspect(WidgetDimensions::ASPECT_SETTINGS_BUTTON), SetStringTip(STR_EMPTY, STR_GAME_OPTIONS_VIDEO_VSYNC_TOOLTIP),
						EndContainer(),
#endif
						NWidget(NWID_HORIZONTAL),
							NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GO_VIDEO_DRIVER_INFO), SetMinimalTextLines(1, 0), SetFill(1, 0),
						EndContainer(),
					EndContainer(),
				EndContainer(),

				NWidget(WWT_FRAME, COLOUR_GREY), SetStringTip(STR_GAME_OPTIONS_BASE_GRF), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0), SetFill(1, 0),
					NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
						NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_BASE_GRF_DROPDOWN), SetMinimalSize(100, 12), SetToolTip(STR_GAME_OPTIONS_BASE_GRF_TOOLTIP), SetFill(1, 0),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_GRF_PARAMETERS), SetStringTip(STR_NEWGRF_SETTINGS_SET_PARAMETERS),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_GRF_CONTENT_DOWNLOAD), SetStringTip(STR_GAME_OPTIONS_ONLINE_CONTENT, STR_GAME_OPTIONS_ONLINE_CONTENT_TOOLTIP),
					EndContainer(),
					NWidget(WWT_TEXT, INVALID_COLOUR, WID_GO_BASE_GRF_DESCRIPTION), SetMinimalSize(200, 0), SetStringTip(STR_EMPTY, STR_GAME_OPTIONS_BASE_GRF_DESCRIPTION_TOOLTIP), SetFill(1, 0),
					NWidget(NWID_VERTICAL),
						NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_GRF_OPEN_URL), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_CONTENT_OPEN_URL, STR_CONTENT_OPEN_URL_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_GRF_TEXTFILE + TFT_README), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_TEXTFILE_VIEW_README, STR_TEXTFILE_VIEW_README_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_GRF_TEXTFILE + TFT_CHANGELOG), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_TEXTFILE_VIEW_CHANGELOG, STR_TEXTFILE_VIEW_CHANGELOG_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_GRF_TEXTFILE + TFT_LICENSE), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_TEXTFILE_VIEW_LICENCE, STR_TEXTFILE_VIEW_LICENCE_TOOLTIP),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),

			/* Sound/Music tab */
			NWidget(NWID_VERTICAL), SetPadding(WidgetDimensions::unscaled.sparse), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0),
				NWidget(WWT_FRAME, COLOUR_GREY), SetStringTip(STR_GAME_OPTIONS_VOLUME), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0),
					NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
						NWidget(WWT_TEXT, INVALID_COLOUR, WID_GO_TEXT_SFX_VOLUME), SetMinimalSize(0, 12), SetStringTip(STR_GAME_OPTIONS_SFX_VOLUME),
						NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GO_BASE_SFX_VOLUME), SetMinimalSize(67, 0), SetMinimalTextLines(1, 12 + WidgetDimensions::unscaled.vsep_normal, FS_SMALL), SetFill(1, 0), SetToolTip(STR_MUSIC_TOOLTIP_DRAG_SLIDERS_TO_SET_MUSIC),
					EndContainer(),
					NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
						NWidget(WWT_TEXT, INVALID_COLOUR, WID_GO_TEXT_MUSIC_VOLUME), SetMinimalSize(0, 12), SetStringTip(STR_GAME_OPTIONS_MUSIC_VOLUME),
						NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GO_BASE_MUSIC_VOLUME), SetMinimalSize(67, 0), SetMinimalTextLines(1, 12 + WidgetDimensions::unscaled.vsep_normal, FS_SMALL), SetFill(1, 0), SetToolTip(STR_MUSIC_TOOLTIP_DRAG_SLIDERS_TO_SET_MUSIC),
					EndContainer(),
				EndContainer(),

				NWidget(WWT_FRAME, COLOUR_GREY), SetStringTip(STR_GAME_OPTIONS_BASE_SFX), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
					NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
						NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_BASE_SFX_DROPDOWN), SetMinimalSize(100, 12), SetToolTip(STR_GAME_OPTIONS_BASE_SFX_TOOLTIP), SetFill(1, 0),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_SFX_CONTENT_DOWNLOAD), SetStringTip(STR_GAME_OPTIONS_ONLINE_CONTENT, STR_GAME_OPTIONS_ONLINE_CONTENT_TOOLTIP),
					EndContainer(),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GO_BASE_SFX_DESCRIPTION), SetMinimalSize(200, 0), SetMinimalTextLines(1, 0), SetToolTip(STR_GAME_OPTIONS_BASE_SFX_DESCRIPTION_TOOLTIP), SetFill(1, 0),
					NWidget(NWID_VERTICAL),
						NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_SFX_OPEN_URL), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_CONTENT_OPEN_URL, STR_CONTENT_OPEN_URL_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_SFX_TEXTFILE + TFT_README), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_TEXTFILE_VIEW_README, STR_TEXTFILE_VIEW_README_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_SFX_TEXTFILE + TFT_CHANGELOG), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_TEXTFILE_VIEW_CHANGELOG, STR_TEXTFILE_VIEW_CHANGELOG_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_SFX_TEXTFILE + TFT_LICENSE), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_TEXTFILE_VIEW_LICENCE, STR_TEXTFILE_VIEW_LICENCE_TOOLTIP),
						EndContainer(),
					EndContainer(),
				EndContainer(),

				NWidget(WWT_FRAME, COLOUR_GREY), SetStringTip(STR_GAME_OPTIONS_BASE_MUSIC), SetPIP(0, WidgetDimensions::unscaled.vsep_sparse, 0),
					NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
						NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_GO_BASE_MUSIC_DROPDOWN), SetMinimalSize(100, 12), SetToolTip(STR_GAME_OPTIONS_BASE_MUSIC_TOOLTIP), SetFill(1, 0),
						NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_MUSIC_CONTENT_DOWNLOAD), SetStringTip(STR_GAME_OPTIONS_ONLINE_CONTENT, STR_GAME_OPTIONS_ONLINE_CONTENT_TOOLTIP),
					EndContainer(),
					NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
						NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GO_BASE_MUSIC_DESCRIPTION), SetMinimalSize(200, 0), SetMinimalTextLines(1, 0), SetToolTip(STR_GAME_OPTIONS_BASE_MUSIC_DESCRIPTION_TOOLTIP), SetFill(1, 0),
						NWidget(NWID_VERTICAL), SetPIPRatio(0, 0, 1),
							NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_GO_BASE_MUSIC_JUKEBOX), SetMinimalSize(22, 22), SetSpriteTip(SPR_IMG_MUSIC, STR_TOOLBAR_TOOLTIP_SHOW_SOUND_MUSIC_WINDOW),
						EndContainer(),
					EndContainer(),
					NWidget(NWID_VERTICAL),
						NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_MUSIC_OPEN_URL), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_CONTENT_OPEN_URL, STR_CONTENT_OPEN_URL_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_MUSIC_TEXTFILE + TFT_README), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_TEXTFILE_VIEW_README, STR_TEXTFILE_VIEW_README_TOOLTIP),
						EndContainer(),
						NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_MUSIC_TEXTFILE + TFT_CHANGELOG), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_TEXTFILE_VIEW_CHANGELOG, STR_TEXTFILE_VIEW_CHANGELOG_TOOLTIP),
							NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_GO_BASE_MUSIC_TEXTFILE + TFT_LICENSE), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_TEXTFILE_VIEW_LICENCE, STR_TEXTFILE_VIEW_LICENCE_TOOLTIP),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),

			/* Social tab */
			NWidget(NWID_VERTICAL), SetPadding(WidgetDimensions::unscaled.sparse), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0),
				NWidgetFunction(MakeNWidgetSocialPlugins),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _game_options_desc(
	WDP_CENTER, nullptr, 0, 0,
	WC_GAME_OPTIONS, WC_NONE,
	{},
	_nested_game_options_widgets
);

/** Open the game options window. */
void ShowGameOptions()
{
	CloseWindowByClass(WC_GAME_OPTIONS);
	new GameOptionsWindow(_game_options_desc);
}

int SETTING_HEIGHT = 11;    ///< Height of a single setting in the tree view in pixels

static const StringID _game_settings_restrict_dropdown[] = {
	STR_CONFIG_SETTING_RESTRICT_BASIC,                            // RM_BASIC
	STR_CONFIG_SETTING_RESTRICT_ADVANCED,                         // RM_ADVANCED
	STR_CONFIG_SETTING_RESTRICT_ALL,                              // RM_ALL
	STR_CONFIG_SETTING_RESTRICT_CHANGED_AGAINST_DEFAULT,          // RM_CHANGED_AGAINST_DEFAULT
	STR_CONFIG_SETTING_RESTRICT_CHANGED_AGAINST_NEW,              // RM_CHANGED_AGAINST_NEW
};
static_assert(lengthof(_game_settings_restrict_dropdown) == RM_END);

/** Warnings about hidden search results. */
enum WarnHiddenResult : uint8_t {
	WHR_NONE,          ///< Nothing was filtering matches away.
	WHR_CATEGORY,      ///< Category setting filtered matches away.
	WHR_TYPE,          ///< Type setting filtered matches away.
	WHR_CATEGORY_TYPE, ///< Both category and type settings filtered matches away.
};

/**
 * Callback function for the reset all settings button
 * @param w Window which is calling this callback
 * @param confirmed boolean value, true when yes was clicked, false otherwise
 */
static void ResetAllSettingsConfirmationCallback(Window *w, bool confirmed)
{
	if (confirmed) {
		GetSettingsTree().ResetAll();
		GetSettingsTree().FoldAll();
		w->InvalidateData();
	}
}

/** Window to edit settings of the game. */
struct GameSettingsWindow : Window {
	static GameSettings *settings_ptr; ///< Pointer to the game settings being displayed and modified.

	SettingEntry *valuewindow_entry = nullptr; ///< If non-nullptr, pointer to setting for which a value-entering window has been opened.
	SettingEntry *clicked_entry = nullptr; ///< If non-nullptr, pointer to a clicked numeric setting (with a depressed left or right button).
	SettingEntry *last_clicked = nullptr; ///< If non-nullptr, pointer to the last clicked setting.
	SettingEntry *valuedropdown_entry = nullptr; ///< If non-nullptr, pointer to the value for which a dropdown window is currently opened.
	bool closing_dropdown = false; ///< True, if the dropdown list is currently closing.

	SettingFilter filter{}; ///< Filter for the list.
	QueryString filter_editbox; ///< Filter editbox;
	bool manually_changed_folding = false; ///< Whether the user expanded/collapsed something manually.
	WarnHiddenResult warn_missing = WHR_NONE; ///< Whether and how to warn about missing search results.
	int warn_lines = 0; ///< Number of lines used for warning about missing search results.

	Scrollbar *vscroll;

	GameSettingsWindow(WindowDesc &desc) : Window(desc), filter_editbox(50)
	{
		this->filter.mode = (RestrictionMode)_settings_client.gui.settings_restriction_mode;
		this->filter.min_cat = RM_ALL;
		this->filter.type = ST_ALL;
		this->filter.type_hides = false;
		this->settings_ptr = &GetGameSettings();

		GetSettingsTree().FoldAll(); // Close all sub-pages

		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_GS_SCROLLBAR);
		this->FinishInitNested(WN_GAME_OPTIONS_GAME_SETTINGS);

		this->querystrings[WID_GS_FILTER] = &this->filter_editbox;
		this->filter_editbox.cancel_button = QueryString::ACTION_CLEAR;
		this->SetFocusedWidget(WID_GS_FILTER);

		this->InvalidateData();
	}

	void OnInit() override
	{
		_setting_circle_size = maxdim(GetSpriteSize(SPR_CIRCLE_FOLDED), GetSpriteSize(SPR_CIRCLE_UNFOLDED));
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_GS_OPTIONSPANEL:
				resize.height = SETTING_HEIGHT = std::max({(int)_setting_circle_size.height, SETTING_BUTTON_HEIGHT, GetCharacterHeight(FS_NORMAL)}) + WidgetDimensions::scaled.vsep_normal;
				resize.width = 1;

				size.height = 5 * resize.height + WidgetDimensions::scaled.framerect.Vertical();
				break;

			case WID_GS_HELP_TEXT: {
				static const StringID setting_types[] = {
					STR_CONFIG_SETTING_TYPE_CLIENT,
					STR_CONFIG_SETTING_TYPE_COMPANY_MENU, STR_CONFIG_SETTING_TYPE_COMPANY_INGAME,
					STR_CONFIG_SETTING_TYPE_GAME_MENU, STR_CONFIG_SETTING_TYPE_GAME_INGAME,
				};
				for (const auto &setting_type : setting_types) {
					size.width = std::max(size.width, GetStringBoundingBox(GetString(STR_CONFIG_SETTING_TYPE, setting_type)).width + padding.width);
				}
				size.height = 2 * GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal +
						std::max(size.height, GetSettingsTree().GetMaxHelpHeight(size.width));
				break;
			}

			case WID_GS_RESTRICT_CATEGORY:
			case WID_GS_RESTRICT_TYPE:
				size.width = std::max(GetStringBoundingBox(STR_CONFIG_SETTING_RESTRICT_CATEGORY).width, GetStringBoundingBox(STR_CONFIG_SETTING_RESTRICT_TYPE).width);
				break;

			default:
				break;
		}
	}

	void OnPaint() override
	{
		if (this->closing_dropdown) {
			this->closing_dropdown = false;
			assert(this->valuedropdown_entry != nullptr);
			this->valuedropdown_entry->SetButtons({});
			this->valuedropdown_entry = nullptr;
		}

		/* Reserve the correct number of lines for the 'some search results are hidden' notice in the central settings display panel. */
		const Rect panel = this->GetWidget<NWidgetBase>(WID_GS_OPTIONSPANEL)->GetCurrentRect().Shrink(WidgetDimensions::scaled.frametext);
		StringID warn_str = STR_CONFIG_SETTING_CATEGORY_HIDES - 1 + this->warn_missing;
		int new_warn_lines;
		if (this->warn_missing == WHR_NONE) {
			new_warn_lines = 0;
		} else {
			new_warn_lines = GetStringLineCount(GetString(warn_str, _game_settings_restrict_dropdown[this->filter.min_cat]), panel.Width());
		}
		if (this->warn_lines != new_warn_lines) {
			this->vscroll->SetCount(this->vscroll->GetCount() - this->warn_lines + new_warn_lines);
			this->warn_lines = new_warn_lines;
		}

		this->DrawWidgets();

		/* Draw the 'some search results are hidden' notice. */
		if (this->warn_missing != WHR_NONE) {
			DrawStringMultiLine(panel.WithHeight(this->warn_lines * GetCharacterHeight(FS_NORMAL)),
				GetString(warn_str, _game_settings_restrict_dropdown[this->filter.min_cat]),
				TC_FROMSTRING, SA_CENTER);
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_GS_RESTRICT_DROPDOWN:
				return GetString(_game_settings_restrict_dropdown[this->filter.mode]);

			case WID_GS_TYPE_DROPDOWN:
				switch (this->filter.type) {
					case ST_GAME:    return GetString(_game_mode == GM_MENU ? STR_CONFIG_SETTING_TYPE_DROPDOWN_GAME_MENU : STR_CONFIG_SETTING_TYPE_DROPDOWN_GAME_INGAME);
					case ST_COMPANY: return GetString(_game_mode == GM_MENU ? STR_CONFIG_SETTING_TYPE_DROPDOWN_COMPANY_MENU : STR_CONFIG_SETTING_TYPE_DROPDOWN_COMPANY_INGAME);
					case ST_CLIENT:  return GetString(STR_CONFIG_SETTING_TYPE_DROPDOWN_CLIENT);
					default:         return GetString(STR_CONFIG_SETTING_TYPE_DROPDOWN_ALL);
				}

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	DropDownList BuildDropDownList(WidgetID widget) const
	{
		DropDownList list;
		switch (widget) {
			case WID_GS_RESTRICT_DROPDOWN:
				for (int mode = 0; mode != RM_END; mode++) {
					/* If we are in adv. settings screen for the new game's settings,
					 * we don't want to allow comparing with new game's settings. */
					bool disabled = mode == RM_CHANGED_AGAINST_NEW && settings_ptr == &_settings_newgame;

					list.push_back(MakeDropDownListStringItem(_game_settings_restrict_dropdown[mode], mode, disabled));
				}
				break;

			case WID_GS_TYPE_DROPDOWN:
				list.push_back(MakeDropDownListStringItem(STR_CONFIG_SETTING_TYPE_DROPDOWN_ALL, ST_ALL));
				list.push_back(MakeDropDownListStringItem(_game_mode == GM_MENU ? STR_CONFIG_SETTING_TYPE_DROPDOWN_GAME_MENU : STR_CONFIG_SETTING_TYPE_DROPDOWN_GAME_INGAME, ST_GAME));
				list.push_back(MakeDropDownListStringItem(_game_mode == GM_MENU ? STR_CONFIG_SETTING_TYPE_DROPDOWN_COMPANY_MENU : STR_CONFIG_SETTING_TYPE_DROPDOWN_COMPANY_INGAME, ST_COMPANY));
				list.push_back(MakeDropDownListStringItem(STR_CONFIG_SETTING_TYPE_DROPDOWN_CLIENT, ST_CLIENT));
				break;
		}
		return list;
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_GS_OPTIONSPANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.frametext, WidgetDimensions::scaled.framerect);
				tr.top += this->warn_lines * SETTING_HEIGHT;
				uint last_row = this->vscroll->GetPosition() + this->vscroll->GetCapacity() - this->warn_lines;
				int next_row = GetSettingsTree().Draw(settings_ptr, tr.left, tr.right, tr.top,
						this->vscroll->GetPosition(), last_row, this->last_clicked);
				if (next_row == 0) DrawString(tr, STR_CONFIG_SETTINGS_NONE);
				break;
			}

			case WID_GS_HELP_TEXT:
				if (this->last_clicked != nullptr) {
					const IntSettingDesc *sd = this->last_clicked->setting;

					Rect tr = r;
					std::string str;
					switch (sd->GetType()) {
						case ST_COMPANY: str = GetString(STR_CONFIG_SETTING_TYPE, _game_mode == GM_MENU ? STR_CONFIG_SETTING_TYPE_COMPANY_MENU : STR_CONFIG_SETTING_TYPE_COMPANY_INGAME); break;
						case ST_CLIENT:  str = GetString(STR_CONFIG_SETTING_TYPE, STR_CONFIG_SETTING_TYPE_CLIENT); break;
						case ST_GAME:    str = GetString(STR_CONFIG_SETTING_TYPE, _game_mode == GM_MENU ? STR_CONFIG_SETTING_TYPE_GAME_MENU : STR_CONFIG_SETTING_TYPE_GAME_INGAME); break;
						default: NOT_REACHED();
					}
					DrawString(tr, str);
					tr.top += GetCharacterHeight(FS_NORMAL);

					auto [param1, param2] = sd->GetValueParams(sd->GetDefaultValue());
					DrawString(tr, GetString(STR_CONFIG_SETTING_DEFAULT_VALUE, param1, param2));
					tr.top += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;

					DrawStringMultiLine(tr, sd->GetHelp(), TC_WHITE);
				}
				break;

			default:
				break;
		}
	}

	/**
	 * Set the entry that should have its help text displayed, and mark the window dirty so it gets repainted.
	 * @param pe Setting to display help text of, use \c nullptr to stop displaying help of the currently displayed setting.
	 */
	void SetDisplayedHelpText(SettingEntry *pe)
	{
		if (this->last_clicked != pe) this->SetDirty();
		this->last_clicked = pe;
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_GS_EXPAND_ALL:
				this->manually_changed_folding = true;
				GetSettingsTree().UnFoldAll();
				this->InvalidateData();
				break;

			case WID_GS_COLLAPSE_ALL:
				this->manually_changed_folding = true;
				GetSettingsTree().FoldAll();
				this->InvalidateData();
				break;

			case WID_GS_RESET_ALL:
				ShowQuery(
					GetEncodedString(STR_CONFIG_SETTING_RESET_ALL_CONFIRMATION_DIALOG_CAPTION),
					GetEncodedString(STR_CONFIG_SETTING_RESET_ALL_CONFIRMATION_DIALOG_TEXT),
					this,
					ResetAllSettingsConfirmationCallback
				);
				break;

			case WID_GS_RESTRICT_DROPDOWN: {
				DropDownList list = this->BuildDropDownList(widget);
				if (!list.empty()) {
					ShowDropDownList(this, std::move(list), this->filter.mode, widget);
				}
				break;
			}

			case WID_GS_TYPE_DROPDOWN: {
				DropDownList list = this->BuildDropDownList(widget);
				if (!list.empty()) {
					ShowDropDownList(this, std::move(list), this->filter.type, widget);
				}
				break;
			}
		}

		if (widget != WID_GS_OPTIONSPANEL) return;

		int32_t btn = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_GS_OPTIONSPANEL, WidgetDimensions::scaled.framerect.top);
		if (btn == INT32_MAX || btn < this->warn_lines) return;
		btn -= this->warn_lines;

		uint cur_row = 0;
		BaseSettingEntry *clicked_entry = GetSettingsTree().FindEntry(btn, &cur_row);

		if (clicked_entry == nullptr) return;  // Clicked below the last setting of the page

		int x = (_current_text_dir == TD_RTL ? this->width - 1 - pt.x : pt.x) - WidgetDimensions::scaled.frametext.left - (clicked_entry->level + 1) * WidgetDimensions::scaled.hsep_indent;  // Shift x coordinate
		if (x < 0) return;  // Clicked left of the entry

		SettingsPage *clicked_page = dynamic_cast<SettingsPage*>(clicked_entry);
		if (clicked_page != nullptr) {
			this->SetDisplayedHelpText(nullptr);
			clicked_page->folded = !clicked_page->folded; // Flip 'folded'-ness of the sub-page

			this->manually_changed_folding = true;

			this->InvalidateData();
			return;
		}

		SettingEntry *pe = dynamic_cast<SettingEntry*>(clicked_entry);
		assert(pe != nullptr);
		const IntSettingDesc *sd = pe->setting;

		/* return if action is only active in network, or only settable by server */
		if (!sd->IsEditable()) {
			this->SetDisplayedHelpText(pe);
			return;
		}

		auto [min_val, max_val] = sd->GetRange();
		int32_t value = sd->Read(ResolveObject(settings_ptr, sd));

		/* clicked on the icon on the left side. Either scroller, bool on/off or dropdown */
		if (x < SETTING_BUTTON_WIDTH && sd->flags.Test(SettingFlag::GuiDropdown)) {
			this->SetDisplayedHelpText(pe);

			if (this->valuedropdown_entry == pe) {
				/* unclick the dropdown */
				this->CloseChildWindows(WC_DROPDOWN_MENU);
				this->closing_dropdown = false;
				this->valuedropdown_entry->SetButtons({});
				this->valuedropdown_entry = nullptr;
			} else {
				if (this->valuedropdown_entry != nullptr) this->valuedropdown_entry->SetButtons({});
				this->closing_dropdown = false;

				const NWidgetBase *wid = this->GetWidget<NWidgetBase>(WID_GS_OPTIONSPANEL);
				int rel_y = (pt.y - wid->pos_y - WidgetDimensions::scaled.framerect.top) % wid->resize_y;

				Rect wi_rect;
				wi_rect.left = pt.x - (_current_text_dir == TD_RTL ? SETTING_BUTTON_WIDTH - 1 - x : x);
				wi_rect.right = wi_rect.left + SETTING_BUTTON_WIDTH - 1;
				wi_rect.top = pt.y - rel_y + (SETTING_HEIGHT - SETTING_BUTTON_HEIGHT) / 2;
				wi_rect.bottom = wi_rect.top + SETTING_BUTTON_HEIGHT - 1;

				/* For dropdowns we also have to check the y position thoroughly, the mouse may not above the just opening dropdown */
				if (pt.y >= wi_rect.top && pt.y <= wi_rect.bottom) {
					this->valuedropdown_entry = pe;
					this->valuedropdown_entry->SetButtons(SettingEntryFlag::LeftDepressed);

					DropDownList list;
					for (int32_t i = min_val; i <= static_cast<int32_t>(max_val); i++) {
						auto [param1, param2] = sd->GetValueParams(i);
						list.push_back(MakeDropDownListStringItem(GetString(STR_JUST_STRING1, param1, param2), i));
					}

					ShowDropDownListAt(this, std::move(list), value, WID_GS_SETTING_DROPDOWN, wi_rect, COLOUR_ORANGE);
				}
			}
			this->SetDirty();
		} else if (x < SETTING_BUTTON_WIDTH) {
			this->SetDisplayedHelpText(pe);
			int32_t oldvalue = value;

			if (sd->IsBoolSetting()) {
				value ^= 1;
			} else {
				/* Add a dynamic step-size to the scroller. In a maximum of
				 * 50-steps you should be able to get from min to max,
				 * unless specified otherwise in the 'interval' variable
				 * of the current setting. */
				uint32_t step = (sd->interval == 0) ? ((max_val - min_val) / 50) : sd->interval;
				if (step == 0) step = 1;

				/* don't allow too fast scrolling */
				if (this->flags.Test(WindowFlag::Timeout) && this->timeout_timer > 1) {
					_left_button_clicked = false;
					return;
				}

				/* Increase or decrease the value and clamp it to extremes */
				if (x >= SETTING_BUTTON_WIDTH / 2) {
					value += step;
					if (min_val < 0) {
						assert(static_cast<int32_t>(max_val) >= 0);
						if (value > static_cast<int32_t>(max_val)) value = static_cast<int32_t>(max_val);
					} else {
						if (static_cast<uint32_t>(value) > max_val) value = static_cast<int32_t>(max_val);
					}
					if (value < min_val) value = min_val; // skip between "disabled" and minimum
				} else {
					value -= step;
					if (value < min_val) value = sd->flags.Test(SettingFlag::GuiZeroIsSpecial) ? 0 : min_val;
				}

				/* Set up scroller timeout for numeric values */
				if (value != oldvalue) {
					if (this->clicked_entry != nullptr) { // Release previous buttons if any
						this->clicked_entry->SetButtons({});
					}
					this->clicked_entry = pe;
					this->clicked_entry->SetButtons((x >= SETTING_BUTTON_WIDTH / 2) != (_current_text_dir == TD_RTL) ? SettingEntryFlag::RightDepressed : SettingEntryFlag::LeftDepressed);
					this->SetTimeout();
					_left_button_clicked = false;
				}
			}

			if (value != oldvalue) {
				SetSettingValue(sd, value);
				this->SetDirty();
			}
		} else {
			/* Only open editbox if clicked for the second time, and only for types where it is sensible for. */
			if (this->last_clicked == pe && !sd->IsBoolSetting() && !sd->flags.Test(SettingFlag::GuiDropdown)) {
				int64_t value64 = value;
				/* Show the correct currency-translated value */
				if (sd->flags.Test(SettingFlag::GuiCurrency)) value64 *= GetCurrency().rate;

				CharSetFilter charset_filter = CS_NUMERAL; //default, only numeric input allowed
				if (min_val < 0) charset_filter = CS_NUMERAL_SIGNED; // special case, also allow '-' sign for negative input

				this->valuewindow_entry = pe;
				/* Limit string length to 14 so that MAX_INT32 * max currency rate doesn't exceed MAX_INT64. */
				ShowQueryString(GetString(STR_JUST_INT, value64), STR_CONFIG_SETTING_QUERY_CAPTION, 15, this, charset_filter, QueryStringFlag::EnableDefault);
			}
			this->SetDisplayedHelpText(pe);
		}
	}

	void OnTimeout() override
	{
		if (this->clicked_entry != nullptr) { // On timeout, release any depressed buttons
			this->clicked_entry->SetButtons({});
			this->clicked_entry = nullptr;
			this->SetDirty();
		}
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		/* The user pressed cancel */
		if (!str.has_value()) return;

		assert(this->valuewindow_entry != nullptr);
		const IntSettingDesc *sd = this->valuewindow_entry->setting;

		int32_t value;
		if (!str->empty()) {
			long long llvalue = atoll(str->c_str());

			/* Save the correct currency-translated value */
			if (sd->flags.Test(SettingFlag::GuiCurrency)) llvalue /= GetCurrency().rate;

			value = ClampTo<int32_t>(llvalue);
		} else {
			value = sd->GetDefaultValue();
		}

		SetSettingValue(this->valuewindow_entry->setting, value);
		this->SetDirty();
	}

	void OnDropdownSelect(WidgetID widget, int index) override
	{
		switch (widget) {
			case WID_GS_RESTRICT_DROPDOWN:
				this->filter.mode = (RestrictionMode)index;
				if (this->filter.mode == RM_CHANGED_AGAINST_DEFAULT ||
						this->filter.mode == RM_CHANGED_AGAINST_NEW) {

					if (!this->manually_changed_folding) {
						/* Expand all when selecting 'changes'. Update the filter state first, in case it becomes less restrictive in some cases. */
						GetSettingsTree().UpdateFilterState(this->filter, false);
						GetSettingsTree().UnFoldAll();
					}
				} else {
					/* Non-'changes' filter. Save as default. */
					_settings_client.gui.settings_restriction_mode = this->filter.mode;
				}
				this->InvalidateData();
				break;

			case WID_GS_TYPE_DROPDOWN:
				this->filter.type = (SettingType)index;
				this->InvalidateData();
				break;

			case WID_GS_SETTING_DROPDOWN:
				/* Deal with drop down boxes on the panel. */
				assert(this->valuedropdown_entry != nullptr);
				const IntSettingDesc *sd = this->valuedropdown_entry->setting;
				assert(sd->flags.Test(SettingFlag::GuiDropdown));

				SetSettingValue(sd, index);
				this->SetDirty();
				break;
		}
	}

	void OnDropdownClose(Point pt, WidgetID widget, int index, bool instant_close) override
	{
		if (widget != WID_GS_SETTING_DROPDOWN) {
			/* Normally the default implementation of OnDropdownClose() takes care of
			 * a few things. We want that behaviour here too, but only for
			 * "normal" dropdown boxes. The special dropdown boxes added for every
			 * setting that needs one can't have this call. */
			Window::OnDropdownClose(pt, widget, index, instant_close);
		} else {
			/* We cannot raise the dropdown button just yet. OnClick needs some hint, whether
			 * the same dropdown button was clicked again, and then not open the dropdown again.
			 * So, we only remember that it was closed, and process it on the next OnPaint, which is
			 * after OnClick. */
			assert(this->valuedropdown_entry != nullptr);
			this->closing_dropdown = true;
			this->SetDirty();
		}
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;

		/* Update which settings are to be visible. */
		RestrictionMode min_level = (this->filter.mode <= RM_ALL) ? this->filter.mode : RM_BASIC;
		this->filter.min_cat = min_level;
		this->filter.type_hides = false;
		GetSettingsTree().UpdateFilterState(this->filter, false);

		if (this->filter.string.IsEmpty()) {
			this->warn_missing = WHR_NONE;
		} else if (min_level < this->filter.min_cat) {
			this->warn_missing = this->filter.type_hides ? WHR_CATEGORY_TYPE : WHR_CATEGORY;
		} else {
			this->warn_missing = this->filter.type_hides ? WHR_TYPE : WHR_NONE;
		}
		this->vscroll->SetCount(GetSettingsTree().Length() + this->warn_lines);

		if (this->last_clicked != nullptr && !GetSettingsTree().IsVisible(this->last_clicked)) {
			this->SetDisplayedHelpText(nullptr);
		}

		bool all_folded = true;
		bool all_unfolded = true;
		GetSettingsTree().GetFoldingState(all_folded, all_unfolded);
		this->SetWidgetDisabledState(WID_GS_EXPAND_ALL, all_unfolded);
		this->SetWidgetDisabledState(WID_GS_COLLAPSE_ALL, all_folded);
	}

	void OnEditboxChanged(WidgetID wid) override
	{
		if (wid == WID_GS_FILTER) {
			this->filter.string.SetFilterTerm(this->filter_editbox.text.GetText());
			if (!this->filter.string.IsEmpty() && !this->manually_changed_folding) {
				/* User never expanded/collapsed single pages and entered a filter term.
				 * Expand everything, to save weird expand clicks, */
				GetSettingsTree().UnFoldAll();
			}
			this->InvalidateData();
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_GS_OPTIONSPANEL, WidgetDimensions::scaled.framerect.Vertical());
	}
};

GameSettings *GameSettingsWindow::settings_ptr = nullptr;

static constexpr NWidgetPart _nested_settings_selection_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_MAUVE),
		NWidget(WWT_CAPTION, COLOUR_MAUVE), SetStringTip(STR_CONFIG_SETTING_TREE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_MAUVE),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE),
		NWidget(NWID_VERTICAL), SetPIP(WidgetDimensions::unscaled.frametext.top, WidgetDimensions::unscaled.vsep_normal, WidgetDimensions::unscaled.frametext.bottom),
			NWidget(NWID_HORIZONTAL), SetPIP(WidgetDimensions::unscaled.frametext.left, WidgetDimensions::unscaled.hsep_wide, WidgetDimensions::unscaled.frametext.right),
				NWidget(WWT_TEXT, INVALID_COLOUR, WID_GS_RESTRICT_CATEGORY), SetStringTip(STR_CONFIG_SETTING_RESTRICT_CATEGORY),
				NWidget(WWT_DROPDOWN, COLOUR_MAUVE, WID_GS_RESTRICT_DROPDOWN), SetMinimalSize(100, 12), SetToolTip(STR_CONFIG_SETTING_RESTRICT_DROPDOWN_HELPTEXT), SetFill(1, 0), SetResize(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(WidgetDimensions::unscaled.frametext.left, WidgetDimensions::unscaled.hsep_wide, WidgetDimensions::unscaled.frametext.right),
				NWidget(WWT_TEXT, INVALID_COLOUR, WID_GS_RESTRICT_TYPE), SetStringTip(STR_CONFIG_SETTING_RESTRICT_TYPE),
				NWidget(WWT_DROPDOWN, COLOUR_MAUVE, WID_GS_TYPE_DROPDOWN), SetMinimalSize(100, 12), SetToolTip(STR_CONFIG_SETTING_TYPE_DROPDOWN_HELPTEXT), SetFill(1, 0), SetResize(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(WidgetDimensions::unscaled.frametext.left, WidgetDimensions::unscaled.hsep_wide, WidgetDimensions::unscaled.frametext.right),
				NWidget(WWT_TEXT, INVALID_COLOUR), SetFill(0, 1), SetStringTip(STR_CONFIG_SETTING_FILTER_TITLE),
				NWidget(WWT_EDITBOX, COLOUR_MAUVE, WID_GS_FILTER), SetStringTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_MAUVE, WID_GS_OPTIONSPANEL), SetMinimalSize(400, 174), SetScrollbar(WID_GS_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_MAUVE, WID_GS_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_MAUVE),
		NWidget(WWT_EMPTY, INVALID_COLOUR, WID_GS_HELP_TEXT), SetMinimalSize(300, 25), SetFill(1, 1), SetResize(1, 0),
				SetPadding(WidgetDimensions::unscaled.frametext),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_GS_EXPAND_ALL), SetStringTip(STR_CONFIG_SETTING_EXPAND_ALL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_GS_COLLAPSE_ALL), SetStringTip(STR_CONFIG_SETTING_COLLAPSE_ALL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_MAUVE, WID_GS_RESET_ALL), SetStringTip(STR_CONFIG_SETTING_RESET_ALL),
		NWidget(WWT_PANEL, COLOUR_MAUVE), SetFill(1, 0), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_MAUVE),
	EndContainer(),
};

static WindowDesc _settings_selection_desc(
	WDP_CENTER, "settings", 510, 450,
	WC_GAME_OPTIONS, WC_NONE,
	{},
	_nested_settings_selection_widgets
);

/** Open advanced settings window. */
void ShowGameSettings()
{
	CloseWindowByClass(WC_GAME_OPTIONS);
	new GameSettingsWindow(_settings_selection_desc);
}


/**
 * Draw [<][>] boxes.
 * @param x the x position to draw
 * @param y the y position to draw
 * @param button_colour the colour of the button
 * @param state 0 = none clicked, 1 = first clicked, 2 = second clicked
 * @param clickable_left is the left button clickable?
 * @param clickable_right is the right button clickable?
 */
void DrawArrowButtons(int x, int y, Colours button_colour, uint8_t state, bool clickable_left, bool clickable_right)
{
	int colour = GetColourGradient(button_colour, SHADE_DARKER);
	Dimension dim = NWidgetScrollbar::GetHorizontalDimension();

	Rect lr = {x,                  y, x + (int)dim.width     - 1, y + (int)dim.height - 1};
	Rect rr = {x + (int)dim.width, y, x + (int)dim.width * 2 - 1, y + (int)dim.height - 1};

	DrawFrameRect(lr, button_colour, (state == 1) ? FrameFlag::Lowered : FrameFlags{});
	DrawFrameRect(rr, button_colour, (state == 2) ? FrameFlag::Lowered : FrameFlags{});
	DrawSpriteIgnorePadding(SPR_ARROW_LEFT,  PAL_NONE, lr, SA_CENTER);
	DrawSpriteIgnorePadding(SPR_ARROW_RIGHT, PAL_NONE, rr, SA_CENTER);

	/* Grey out the buttons that aren't clickable */
	bool rtl = _current_text_dir == TD_RTL;
	if (rtl ? !clickable_right : !clickable_left) {
		GfxFillRect(lr.Shrink(WidgetDimensions::scaled.bevel), colour, FILLRECT_CHECKER);
	}
	if (rtl ? !clickable_left : !clickable_right) {
		GfxFillRect(rr.Shrink(WidgetDimensions::scaled.bevel), colour, FILLRECT_CHECKER);
	}
}

/**
 * Draw a dropdown button.
 * @param x the x position to draw
 * @param y the y position to draw
 * @param button_colour the colour of the button
 * @param state true = lowered
 * @param clickable is the button clickable?
 */
void DrawDropDownButton(int x, int y, Colours button_colour, bool state, bool clickable)
{
	int colour = GetColourGradient(button_colour, SHADE_DARKER);

	Rect r = {x, y, x + SETTING_BUTTON_WIDTH - 1, y + SETTING_BUTTON_HEIGHT - 1};

	DrawFrameRect(r, button_colour, state ? FrameFlag::Lowered : FrameFlags{});
	DrawSpriteIgnorePadding(SPR_ARROW_DOWN, PAL_NONE, r, SA_CENTER);

	if (!clickable) {
		GfxFillRect(r.Shrink(WidgetDimensions::scaled.bevel), colour, FILLRECT_CHECKER);
	}
}

/**
 * Draw a toggle button.
 * @param x the x position to draw
 * @param y the y position to draw
 * @param state true = lowered
 * @param clickable is the button clickable?
 */
void DrawBoolButton(int x, int y, bool state, bool clickable)
{
	static const Colours _bool_ctabs[2][2] = {{COLOUR_CREAM, COLOUR_RED}, {COLOUR_DARK_GREEN, COLOUR_GREEN}};

	Rect r = {x, y, x + SETTING_BUTTON_WIDTH - 1, y + SETTING_BUTTON_HEIGHT - 1};
	DrawFrameRect(r, _bool_ctabs[state][clickable], state ? FrameFlag::Lowered : FrameFlags{});
}

struct CustomCurrencyWindow : Window {
	WidgetID query_widget{};

	CustomCurrencyWindow(WindowDesc &desc) : Window(desc)
	{
		this->InitNested();

		SetButtonState();
	}

	void SetButtonState()
	{
		this->SetWidgetDisabledState(WID_CC_RATE_DOWN, GetCustomCurrency().rate == 1);
		this->SetWidgetDisabledState(WID_CC_RATE_UP, GetCustomCurrency().rate == UINT16_MAX);
		this->SetWidgetDisabledState(WID_CC_YEAR_DOWN, GetCustomCurrency().to_euro == CF_NOEURO);
		this->SetWidgetDisabledState(WID_CC_YEAR_UP, GetCustomCurrency().to_euro == CalendarTime::MAX_YEAR);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_CC_RATE:      return GetString(STR_CURRENCY_EXCHANGE_RATE, 1, 1);
			case WID_CC_SEPARATOR: return GetString(STR_CURRENCY_SEPARATOR, GetCustomCurrency().separator);
			case WID_CC_PREFIX:    return GetString(STR_CURRENCY_PREFIX, GetCustomCurrency().prefix);
			case WID_CC_SUFFIX:    return GetString(STR_CURRENCY_SUFFIX, GetCustomCurrency().suffix);
			case WID_CC_YEAR:
				return GetString((GetCustomCurrency().to_euro != CF_NOEURO) ? STR_CURRENCY_SWITCH_TO_EURO : STR_CURRENCY_SWITCH_TO_EURO_NEVER, GetCustomCurrency().to_euro);

			case WID_CC_PREVIEW:
				return GetString(STR_CURRENCY_PREVIEW, 10000);

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			/* Set the appropriate width for the up/down buttons. */
			case WID_CC_RATE_DOWN:
			case WID_CC_RATE_UP:
			case WID_CC_YEAR_DOWN:
			case WID_CC_YEAR_UP:
				size = maxdim(size, {(uint)SETTING_BUTTON_WIDTH / 2, (uint)SETTING_BUTTON_HEIGHT});
				break;

			/* Set the appropriate width for the edit buttons. */
			case WID_CC_SEPARATOR_EDIT:
			case WID_CC_PREFIX_EDIT:
			case WID_CC_SUFFIX_EDIT:
				size = maxdim(size, {(uint)SETTING_BUTTON_WIDTH, (uint)SETTING_BUTTON_HEIGHT});
				break;

			/* Make sure the window is wide enough for the widest exchange rate */
			case WID_CC_RATE:
				size = GetStringBoundingBox(GetString(STR_CURRENCY_EXCHANGE_RATE, 1, INT32_MAX));
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		int line = 0;
		int len = 0;
		std::string str;
		CharSetFilter afilter = CS_ALPHANUMERAL;

		switch (widget) {
			case WID_CC_RATE_DOWN:
				if (GetCustomCurrency().rate > 1) GetCustomCurrency().rate--;
				if (GetCustomCurrency().rate == 1) this->DisableWidget(WID_CC_RATE_DOWN);
				this->EnableWidget(WID_CC_RATE_UP);
				break;

			case WID_CC_RATE_UP:
				if (GetCustomCurrency().rate < UINT16_MAX) GetCustomCurrency().rate++;
				if (GetCustomCurrency().rate == UINT16_MAX) this->DisableWidget(WID_CC_RATE_UP);
				this->EnableWidget(WID_CC_RATE_DOWN);
				break;

			case WID_CC_RATE:
				str = GetString(STR_JUST_INT, GetCustomCurrency().rate);
				len = 5;
				line = WID_CC_RATE;
				afilter = CS_NUMERAL;
				break;

			case WID_CC_SEPARATOR_EDIT:
			case WID_CC_SEPARATOR:
				str = GetCustomCurrency().separator;
				len = 7;
				line = WID_CC_SEPARATOR;
				break;

			case WID_CC_PREFIX_EDIT:
			case WID_CC_PREFIX:
				str = GetCustomCurrency().prefix;
				len = 15;
				line = WID_CC_PREFIX;
				break;

			case WID_CC_SUFFIX_EDIT:
			case WID_CC_SUFFIX:
				str = GetCustomCurrency().suffix;
				len = 15;
				line = WID_CC_SUFFIX;
				break;

			case WID_CC_YEAR_DOWN:
				GetCustomCurrency().to_euro = (GetCustomCurrency().to_euro <= MIN_EURO_YEAR) ? CF_NOEURO : GetCustomCurrency().to_euro - 1;
				if (GetCustomCurrency().to_euro == CF_NOEURO) this->DisableWidget(WID_CC_YEAR_DOWN);
				this->EnableWidget(WID_CC_YEAR_UP);
				break;

			case WID_CC_YEAR_UP:
				GetCustomCurrency().to_euro = Clamp(GetCustomCurrency().to_euro + 1, MIN_EURO_YEAR, CalendarTime::MAX_YEAR);
				if (GetCustomCurrency().to_euro == CalendarTime::MAX_YEAR) this->DisableWidget(WID_CC_YEAR_UP);
				this->EnableWidget(WID_CC_YEAR_DOWN);
				break;

			case WID_CC_YEAR:
				str = GetString(STR_JUST_INT, GetCustomCurrency().to_euro);
				len = 7;
				line = WID_CC_YEAR;
				afilter = CS_NUMERAL;
				break;
		}

		if (len != 0) {
			this->query_widget = line;
			ShowQueryString(str, STR_CURRENCY_CHANGE_PARAMETER, len + 1, this, afilter, {});
		}

		this->SetTimeout();
		this->SetDirty();
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value()) return;

		switch (this->query_widget) {
			case WID_CC_RATE:
				GetCustomCurrency().rate = Clamp(atoi(str->c_str()), 1, UINT16_MAX);
				break;

			case WID_CC_SEPARATOR: // Thousands separator
				GetCustomCurrency().separator = std::move(*str);
				break;

			case WID_CC_PREFIX:
				GetCustomCurrency().prefix = std::move(*str);
				break;

			case WID_CC_SUFFIX:
				GetCustomCurrency().suffix = std::move(*str);
				break;

			case WID_CC_YEAR: { // Year to switch to euro
				TimerGameCalendar::Year val{atoi(str->c_str())};

				GetCustomCurrency().to_euro = (val < MIN_EURO_YEAR ? CF_NOEURO : std::min(val, CalendarTime::MAX_YEAR));
				break;
			}
		}
		MarkWholeScreenDirty();
		SetButtonState();
	}

	void OnTimeout() override
	{
		this->SetDirty();
	}
};

static constexpr NWidgetPart _nested_cust_currency_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetStringTip(STR_CURRENCY_WINDOW, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_wide, 0), SetPadding(WidgetDimensions::unscaled.sparse),
			NWidget(NWID_VERTICAL, NWidContainerFlag::EqualSize), SetPIP(0, WidgetDimensions::unscaled.vsep_normal, 0),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
						NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_CC_RATE_DOWN), SetArrowWidgetTypeTip(AWV_DECREASE, STR_CURRENCY_DECREASE_EXCHANGE_RATE_TOOLTIP),
						NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_CC_RATE_UP), SetArrowWidgetTypeTip(AWV_INCREASE, STR_CURRENCY_INCREASE_EXCHANGE_RATE_TOOLTIP),
					EndContainer(),
					NWidget(WWT_TEXT, INVALID_COLOUR, WID_CC_RATE), SetToolTip(STR_CURRENCY_SET_EXCHANGE_RATE_TOOLTIP), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(WWT_PUSHBTN, COLOUR_DARK_BLUE, WID_CC_SEPARATOR_EDIT), SetToolTip(STR_CURRENCY_SET_CUSTOM_CURRENCY_SEPARATOR_TOOLTIP), SetFill(0, 1),
					NWidget(WWT_TEXT, INVALID_COLOUR, WID_CC_SEPARATOR), SetToolTip(STR_CURRENCY_SET_CUSTOM_CURRENCY_SEPARATOR_TOOLTIP), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(WWT_PUSHBTN, COLOUR_DARK_BLUE, WID_CC_PREFIX_EDIT), SetToolTip(STR_CURRENCY_SET_CUSTOM_CURRENCY_PREFIX_TOOLTIP), SetFill(0, 1),
					NWidget(WWT_TEXT, INVALID_COLOUR, WID_CC_PREFIX), SetToolTip(STR_CURRENCY_SET_CUSTOM_CURRENCY_PREFIX_TOOLTIP), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(WWT_PUSHBTN, COLOUR_DARK_BLUE, WID_CC_SUFFIX_EDIT), SetToolTip(STR_CURRENCY_SET_CUSTOM_CURRENCY_SUFFIX_TOOLTIP), SetFill(0, 1),
					NWidget(WWT_TEXT, INVALID_COLOUR, WID_CC_SUFFIX), SetToolTip(STR_CURRENCY_SET_CUSTOM_CURRENCY_SUFFIX_TOOLTIP), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_wide, 0),
					NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
						NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_CC_YEAR_DOWN), SetArrowWidgetTypeTip(AWV_DECREASE, STR_CURRENCY_DECREASE_CUSTOM_CURRENCY_TO_EURO_TOOLTIP),
						NWidget(WWT_PUSHARROWBTN, COLOUR_YELLOW, WID_CC_YEAR_UP), SetArrowWidgetTypeTip(AWV_INCREASE, STR_CURRENCY_INCREASE_CUSTOM_CURRENCY_TO_EURO_TOOLTIP),
					EndContainer(),
					NWidget(WWT_TEXT, INVALID_COLOUR, WID_CC_YEAR), SetToolTip(STR_CURRENCY_SET_CUSTOM_CURRENCY_TO_EURO_TOOLTIP), SetFill(1, 0),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_LABEL, INVALID_COLOUR, WID_CC_PREVIEW),
					SetToolTip(STR_CURRENCY_CUSTOM_CURRENCY_PREVIEW_TOOLTIP),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _cust_currency_desc(
	WDP_CENTER, nullptr, 0, 0,
	WC_CUSTOM_CURRENCY, WC_NONE,
	{},
	_nested_cust_currency_widgets
);

/** Open custom currency window. */
static void ShowCustCurrency()
{
	CloseWindowById(WC_CUSTOM_CURRENCY, 0);
	new CustomCurrencyWindow(_cust_currency_desc);
}
