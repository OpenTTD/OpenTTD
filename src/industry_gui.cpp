/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file industry_gui.cpp GUIs related to industries. */

#include "stdafx.h"
#include <ranges>
#include "error.h"
#include "gui.h"
#include "settings_gui.h"
#include "sound_func.h"
#include "window_func.h"
#include "textbuf_gui.h"
#include "command_func.h"
#include "viewport_func.h"
#include "industry.h"
#include "town.h"
#include "cargo_type.h"
#include "cheat_type.h"
#include "newgrf_badge.h"
#include "newgrf_badge_gui.h"
#include "newgrf_industries.h"
#include "newgrf_text.h"
#include "newgrf_debug.h"
#include "network/network.h"
#include "strings_func.h"
#include "company_func.h"
#include "tilehighlight_func.h"
#include "string_func.h"
#include "sortlist_type.h"
#include "dropdown_func.h"
#include "company_base.h"
#include "core/geometry_func.hpp"
#include "core/random_func.hpp"
#include "core/backup_type.hpp"
#include "genworld.h"
#include "smallmap_gui.h"
#include "dropdown_type.h"
#include "clear_map.h"
#include "zoom_func.h"
#include "industry_cmd.h"
#include "graph_gui.h"
#include "querystring_gui.h"
#include "stringfilter_type.h"
#include "timer/timer.h"
#include "timer/timer_window.h"
#include "hotkeys.h"
#include "core/string_consumer.hpp"

#include "widgets/industry_widget.h"

#include "table/strings.h"

#include <bitset>

#include "safeguards.h"

bool _ignore_industry_restrictions;
std::bitset<NUM_INDUSTRYTYPES> _displayed_industries; ///< Communication from the industry chain window to the smallmap window about what industries to display.

/** Cargo suffix type (for which window is it requested) */
enum class CargoSuffixType : uint8_t {
	Fund = 0, ///< Fund-industry window
	View = 1, ///< View-industry window
	Directory = 2, ///< Industry-directory window
};

/** Ways of displaying the cargo. */
enum class CargoSuffixDisplay : uint8_t {
	None, ///< Display the cargo without sub-type (cb37 result 401).
	Amount, ///< Display the cargo and amount (if useful), but no sub-type (cb37 result 400 or fail).
	Text, ///< Display the cargo and supplied string (cb37 result 800-BFF).
	AmountAndText, ///< Display the cargo, amount, and string (cb37 result 000-3FF).
};

/** Transfer storage of cargo suffix information. */
struct CargoSuffix {
	CargoSuffixDisplay display; ///< How to display the cargo and text.
	std::string text;           ///< Cargo suffix text.
};

extern void GenerateIndustries();
static void ShowIndustryCargoesWindow(IndustryType id);

/**
 * Gets the string to display after the cargo name (using callback 37)
 * @param cargo the cargo for which the suffix is requested, meaning depends on presence of flag 18 in prop 1A
 * @param cst the cargo suffix type (for which window is it requested). @see CargoSuffixType
 * @param ind the industry (nullptr if in fund window)
 * @param ind_type the industry type
 * @param indspec the industry spec
 * @param suffix is filled with the string to display
 */
static void GetCargoSuffix(uint cargo, CargoSuffixType cst, const Industry *ind, IndustryType ind_type, const IndustrySpec *indspec, CargoSuffix &suffix)
{
	suffix.text.clear();
	suffix.display = CargoSuffixDisplay::Amount;

	if (indspec->callback_mask.Test(IndustryCallbackMask::CargoSuffix)) {
		TileIndex t = (cst != CargoSuffixType::Fund) ? ind->location.tile : INVALID_TILE;
		std::array<int32_t, 16> regs100;
		uint16_t callback = GetIndustryCallback(CBID_INDUSTRY_CARGO_SUFFIX, 0, (to_underlying(cst) << 8) | cargo, const_cast<Industry *>(ind), ind_type, t, regs100);
		if (callback == CALLBACK_FAILED) return;

		if (indspec->grf_prop.grffile->grf_version < 8) {
			if (GB(callback, 0, 8) == 0xFF) return;
			if (callback < 0x400) {
				suffix.text = GetGRFStringWithTextStack(indspec->grf_prop.grffile, GRFSTR_MISC_GRF_TEXT + callback, regs100);
				suffix.display = CargoSuffixDisplay::AmountAndText;
				return;
			}
			ErrorUnknownCallbackResult(indspec->grf_prop.grfid, CBID_INDUSTRY_CARGO_SUFFIX, callback);
			return;

		} else { // GRF version 8 or higher.
			switch (callback) {
				case 0x400:
					return;
				case 0x401:
					suffix.display = CargoSuffixDisplay::None;
					return;
				case 0x40E:
					suffix.display = CargoSuffixDisplay::Text;
					suffix.text = GetGRFStringWithTextStack(indspec->grf_prop.grffile, static_cast<GRFStringID>(regs100[0]), std::span{regs100}.subspan(1));
					return;
				case 0x40F:
					suffix.display = CargoSuffixDisplay::AmountAndText;
					suffix.text = GetGRFStringWithTextStack(indspec->grf_prop.grffile, static_cast<GRFStringID>(regs100[0]), std::span{regs100}.subspan(1));
					return;
				default:
					break;
			}
			if (callback < 0x400) {
				suffix.text = GetGRFStringWithTextStack(indspec->grf_prop.grffile, GRFSTR_MISC_GRF_TEXT + callback, regs100);
				suffix.display = CargoSuffixDisplay::AmountAndText;
				return;
			}
			if (callback >= 0x800 && callback < 0xC00) {
				suffix.text = GetGRFStringWithTextStack(indspec->grf_prop.grffile, GRFSTR_MISC_GRF_TEXT + callback - 0x800, regs100);
				suffix.display = CargoSuffixDisplay::Text;
				return;
			}
			ErrorUnknownCallbackResult(indspec->grf_prop.grfid, CBID_INDUSTRY_CARGO_SUFFIX, callback);
			return;
		}
	}
}

/** Cargo direction for cargo suffixes. */
enum class CargoSuffixDirection : uint8_t {
	Out = 0, ///< Get cargo suffix for output cargoes.
	In = 1, ///< Get cargo suffix for input cargoes.
};

/**
 * Gets all strings to display after the cargoes of industries (using callback 37)
 * @param use_input get suffixes for output cargoes or input cargoes?
 * @param cst the cargo suffix type (for which window is it requested). @see CargoSuffixType
 * @param ind the industry (nullptr if in fund window)
 * @param ind_type the industry type
 * @param indspec the industry spec
 * @param cargoes array with cargotypes. for INVALID_CARGO no suffix will be determined
 * @param suffixes is filled with the suffixes
 */
template <typename TC, typename TS>
static inline void GetAllCargoSuffixes(CargoSuffixDirection use_input, CargoSuffixType cst, const Industry *ind, IndustryType ind_type, const IndustrySpec *indspec, const TC &cargoes, TS &suffixes)
{
	static_assert(std::tuple_size_v<std::remove_reference_t<decltype(cargoes)>> <= lengthof(suffixes));

	if (indspec->behaviour.Test(IndustryBehaviour::CargoTypesUnlimited)) {
		/* Reworked behaviour with new many-in-many-out scheme */
		for (uint j = 0; j < lengthof(suffixes); j++) {
			if (IsValidCargoType(cargoes[j])) {
				uint8_t local_id = indspec->grf_prop.grffile->cargo_map[cargoes[j]]; // should we check the value for valid?
				uint cargotype = local_id << 16 | to_underlying(use_input);
				GetCargoSuffix(cargotype, cst, ind, ind_type, indspec, suffixes[j]);
			} else {
				suffixes[j].text.clear();
				suffixes[j].display = CargoSuffixDisplay::None;
			}
		}
	} else {
		/* Compatible behaviour with old 3-in-2-out scheme */
		for (uint j = 0; j < lengthof(suffixes); j++) {
			suffixes[j].text.clear();
			suffixes[j].display = CargoSuffixDisplay::None;
		}
		switch (use_input) {
			case CargoSuffixDirection::Out:
				/* Handle INDUSTRY_ORIGINAL_NUM_OUTPUTS cargoes */
				if (IsValidCargoType(cargoes[0])) GetCargoSuffix(3, cst, ind, ind_type, indspec, suffixes[0]);
				if (IsValidCargoType(cargoes[1])) GetCargoSuffix(4, cst, ind, ind_type, indspec, suffixes[1]);
				break;
			case CargoSuffixDirection::In:
				/* Handle INDUSTRY_ORIGINAL_NUM_INPUTS cargoes */
				if (IsValidCargoType(cargoes[0])) GetCargoSuffix(0, cst, ind, ind_type, indspec, suffixes[0]);
				if (IsValidCargoType(cargoes[1])) GetCargoSuffix(1, cst, ind, ind_type, indspec, suffixes[1]);
				if (IsValidCargoType(cargoes[2])) GetCargoSuffix(2, cst, ind, ind_type, indspec, suffixes[2]);
				break;
			default:
				NOT_REACHED();
		}
	}
}

/**
 * Gets the strings to display after the cargo of industries (using callback 37)
 * @param use_input get suffixes for output cargo or input cargo?
 * @param cst the cargo suffix type (for which window is it requested). @see CargoSuffixType
 * @param ind the industry (nullptr if in fund window)
 * @param ind_type the industry type
 * @param indspec the industry spec
 * @param cargo cargotype. for INVALID_CARGO no suffix will be determined
 * @param slot accepts/produced slot number, used for old-style 3-in/2-out industries.
 * @param suffix is filled with the suffix
 */
void GetCargoSuffix(CargoSuffixDirection use_input, CargoSuffixType cst, const Industry *ind, IndustryType ind_type, const IndustrySpec *indspec, CargoType cargo, uint8_t slot, CargoSuffix &suffix)
{
	suffix.text.clear();
	suffix.display = CargoSuffixDisplay::None;
	if (!IsValidCargoType(cargo)) return;
	if (indspec->behaviour.Test(IndustryBehaviour::CargoTypesUnlimited)) {
		uint8_t local_id = indspec->grf_prop.grffile->cargo_map[cargo]; // should we check the value for valid?
		uint cargotype = local_id << 16 | to_underlying(use_input);
		GetCargoSuffix(cargotype, cst, ind, ind_type, indspec, suffix);
	} else if (use_input == CargoSuffixDirection::In) {
		if (slot < INDUSTRY_ORIGINAL_NUM_INPUTS) GetCargoSuffix(slot, cst, ind, ind_type, indspec, suffix);
	} else if (use_input == CargoSuffixDirection::Out) {
		if (slot < INDUSTRY_ORIGINAL_NUM_OUTPUTS) GetCargoSuffix(slot + INDUSTRY_ORIGINAL_NUM_INPUTS, cst, ind, ind_type, indspec, suffix);
	}
}

std::array<IndustryType, NUM_INDUSTRYTYPES> _sorted_industry_types; ///< Industry types sorted by name.

/** Sort industry types by their name. @copydoc GUIList::Sorter */
static bool IndustryTypeNameSorter(const IndustryType &a, const IndustryType &b)
{
	int r = StrNaturalCompare(GetString(GetIndustrySpec(a)->name), GetString(GetIndustrySpec(b)->name)); // Sort by name (natural sorting).

	/* If the names are equal, sort by industry type. */
	return (r != 0) ? r < 0 : (a < b);
}

/**
 * Initialize the list of sorted industry types.
 */
void SortIndustryTypes()
{
	/* Add each industry type to the list. */
	for (IndustryType i = 0; i < NUM_INDUSTRYTYPES; i++) {
		_sorted_industry_types[i] = i;
	}

	/* Sort industry types by name. */
	std::sort(_sorted_industry_types.begin(), _sorted_industry_types.end(), IndustryTypeNameSorter);
}

static constexpr std::initializer_list<NWidgetPart> _nested_build_industry_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::DarkGreen),
		NWidget(WWT_CAPTION, Colours::DarkGreen), SetStringTip(STR_FUND_INDUSTRY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::DarkGreen),
		NWidget(WWT_DEFSIZEBOX, Colours::DarkGreen),
		NWidget(WWT_STICKYBOX, Colours::DarkGreen),
	EndContainer(),
	NWidget(NWID_SELECTION, Colours::DarkGreen, WID_DPI_SCENARIO_EDITOR_PANE),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_TEXTBTN, Colours::DarkGreen, WID_DPI_CREATE_RANDOM_INDUSTRIES_WIDGET), SetMinimalSize(0, 12), SetFill(1, 0), SetResize(1, 0),
					SetStringTip(STR_FUND_INDUSTRY_MANY_RANDOM_INDUSTRIES, STR_FUND_INDUSTRY_MANY_RANDOM_INDUSTRIES_TOOLTIP),
			NWidget(WWT_TEXTBTN, Colours::DarkGreen, WID_DPI_REMOVE_ALL_INDUSTRIES_WIDGET), SetMinimalSize(0, 12), SetFill(1, 0), SetResize(1, 0),
					SetStringTip(STR_FUND_INDUSTRY_REMOVE_ALL_INDUSTRIES, STR_FUND_INDUSTRY_REMOVE_ALL_INDUSTRIES_TOOLTIP),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, Colours::DarkGreen, WID_DPI_MATRIX_WIDGET), SetMatrixDataTip(1, 0, STR_FUND_INDUSTRY_SELECTION_TOOLTIP), SetFill(1, 0), SetResize(1, 1), SetScrollbar(WID_DPI_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, Colours::DarkGreen, WID_DPI_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen, WID_DPI_INFOPANEL), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, Colours::DarkGreen, WID_DPI_DISPLAY_WIDGET), SetFill(1, 0), SetResize(1, 0),
				SetStringTip(STR_INDUSTRY_DISPLAY_CHAIN, STR_INDUSTRY_DISPLAY_CHAIN_TOOLTIP),
		NWidget(WWT_TEXTBTN, Colours::DarkGreen, WID_DPI_FUND_WIDGET), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, Colours::DarkGreen),
	EndContainer(),
};

/** Window definition of the dynamic place industries gui */
static WindowDesc _build_industry_desc(
	WindowPosition::Automatic, "build_industry", 170, 212,
	WindowClass::BuildIndustry, WindowClass::None,
	WindowDefaultFlag::Construction,
	_nested_build_industry_widgets
);

/** Build (fund or prospect) a new industry, */
class BuildIndustryWindow : public Window {
	IndustryType selected_type = IT_INVALID; ///< industry corresponding to the above index
	std::vector<IndustryType> list{}; ///< List of industries.
	bool enabled = false; ///< Availability state of the selected industry.
	Scrollbar *vscroll = nullptr;
	Dimension legend{}; ///< Dimension of the legend 'blob'.
	GUIBadgeClasses badge_classes{};

	/** The largest allowed minimum-width of the window, given in line heights */
	static const int MAX_MINWIDTH_LINEHEIGHTS = 20;

	void UpdateAvailability()
	{
		this->enabled = this->selected_type != IT_INVALID && (_game_mode == GameMode::Editor || GetIndustryProbabilityCallback(this->selected_type, IndustryAvailabilityCallType::UserCreation, 1) > 0);
	}

	void SetupArrays()
	{
		this->list.clear();

		/* Fill the arrays with industries.
		 * The tests performed after the enabled allow to load the industries
		 * In the same way they are inserted by grf (if any)
		 */
		for (IndustryType ind : _sorted_industry_types) {
			const IndustrySpec *indsp = GetIndustrySpec(ind);
			if (indsp->enabled) {
				/* Rule is that editor mode loads all industries.
				 * In game mode, all non raw industries are loaded too
				 * and raw ones are loaded only when setting allows it */
				if (_game_mode != GameMode::Editor && indsp->IsRawIndustry() && _settings_game.construction.raw_industry_construction == 0) {
					/* Unselect if the industry is no longer in the list */
					if (this->selected_type == ind) this->selected_type = IT_INVALID;
					continue;
				}

				this->list.push_back(ind);
			}
		}

		/* First industry type is selected if the current selection is invalid. */
		if (this->selected_type == IT_INVALID && !this->list.empty()) this->selected_type = this->list[0];

		this->UpdateAvailability();

		this->vscroll->SetCount(this->list.size());
	}

	/** Update status of the fund and display-chain widgets. */
	void SetButtons()
	{
		this->SetWidgetDisabledState(WID_DPI_FUND_WIDGET, this->selected_type != IT_INVALID && !this->enabled);
		this->SetWidgetDisabledState(WID_DPI_DISPLAY_WIDGET, this->selected_type == IT_INVALID && this->enabled);
	}

	/**
	 * Build a string of cargo names with suffixes attached.
	 * This is distinct from the CARGO_LIST string formatting code in two ways:
	 *  - This cargo list uses the order defined by the industry, rather than alphabetic.
	 *  - NewGRF-supplied suffix strings can be attached to each cargo.
	 *
	 * @param cargolist    Array of CargoType to display
	 * @param cargo_suffix Array of suffixes to attach to each cargo
	 * @param prefixstr    String to use for the first item
	 * @return A formatted raw string
	 */
	std::string MakeCargoListString(const std::span<const CargoType> cargolist, const std::span<const CargoSuffix> cargo_suffix, StringID prefixstr) const
	{
		assert(cargolist.size() == cargo_suffix.size());

		std::string cargostring;
		std::string_view list_separator = GetListSeparator();

		for (size_t j = 0; j < cargolist.size(); j++) {
			if (!IsValidCargoType(cargolist[j])) continue;

			if (!cargostring.empty()) cargostring += list_separator;
			auto params = MakeParameters(CargoSpec::Get(cargolist[j])->name, cargo_suffix[j].text);
			AppendStringWithArgsInPlace(cargostring, STR_INDUSTRY_VIEW_CARGO_LIST_EXTENSION, params);
		}

		if (cargostring.empty()) AppendStringInPlace(cargostring, STR_JUST_NOTHING);
		return GetString(prefixstr, cargostring);
	}

public:
	BuildIndustryWindow() : Window(_build_industry_desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_DPI_SCROLLBAR);
		/* Show scenario editor tools in editor. */
		if (_game_mode != GameMode::Editor) {
			this->GetWidget<NWidgetStacked>(WID_DPI_SCENARIO_EDITOR_PANE)->SetDisplayedPlane(SZSP_HORIZONTAL);
		}
		this->FinishInitNested(0);

		this->SetButtons();
	}

	void OnInit() override
	{
		this->badge_classes = GUIBadgeClasses{GrfSpecFeature::Industries};

		/* Width of the legend blob -- slightly larger than the smallmap legend blob. */
		this->legend.height = GetCharacterHeight(FontSize::Small);
		this->legend.width = this->legend.height * 9 / 6;

		this->SetupArrays();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_DPI_MATRIX_WIDGET: {
				Dimension count = GetStringBoundingBox(GetString(STR_JUST_COMMA, GetParamMaxDigits(4)), FontSize::Small);
				Dimension d{};
				for (const auto &indtype : this->list) {
					d = maxdim(d, GetStringBoundingBox(GetIndustrySpec(indtype)->name));
				}
				fill.height = resize.height = std::max<uint>({this->legend.height, d.height, count.height}) + padding.height;
				d.width += this->badge_classes.GetTotalColumnsWidth() + this->legend.width + WidgetDimensions::scaled.hsep_wide + WidgetDimensions::scaled.hsep_normal + count.width + padding.width;
				d.height = 5 * resize.height;
				size = maxdim(size, d);
				break;
			}

			case WID_DPI_INFOPANEL: {
				/* Extra line for cost outside of editor. */
				int height = 2 + (_game_mode == GameMode::Editor ? 0 : 1);
				uint extra_lines_req = 0;
				uint extra_lines_prd = 0;
				uint extra_lines_newgrf = 0;
				uint max_minwidth = GetCharacterHeight(FontSize::Normal) * MAX_MINWIDTH_LINEHEIGHTS;
				Dimension d = {0, 0};
				for (const auto &indtype : this->list) {
					const IndustrySpec *indsp = GetIndustrySpec(indtype);
					CargoSuffix cargo_suffix[std::tuple_size_v<decltype(indsp->accepts_cargo)>];

					/* Measure the accepted cargoes, if any. */
					GetAllCargoSuffixes(CargoSuffixDirection::In, CargoSuffixType::Fund, nullptr, indtype, indsp, indsp->accepts_cargo, cargo_suffix);
					std::string cargostring = this->MakeCargoListString(indsp->accepts_cargo, cargo_suffix, STR_INDUSTRY_VIEW_REQUIRES_N_CARGO);
					Dimension strdim = GetStringBoundingBox(cargostring);
					if (strdim.width > max_minwidth) {
						extra_lines_req = std::max(extra_lines_req, strdim.width / max_minwidth + 1);
						strdim.width = max_minwidth;
					}
					d = maxdim(d, strdim);

					/* Measure the produced cargoes, if any. */
					GetAllCargoSuffixes(CargoSuffixDirection::Out, CargoSuffixType::Fund, nullptr, indtype, indsp, indsp->produced_cargo, cargo_suffix);
					cargostring = this->MakeCargoListString(indsp->produced_cargo, cargo_suffix, STR_INDUSTRY_VIEW_PRODUCES_N_CARGO);
					strdim = GetStringBoundingBox(cargostring);
					if (strdim.width > max_minwidth) {
						extra_lines_prd = std::max(extra_lines_prd, strdim.width / max_minwidth + 1);
						strdim.width = max_minwidth;
					}
					d = maxdim(d, strdim);

					if (indsp->grf_prop.HasGrfFile()) {
						/* Reserve a few extra lines for text from an industry NewGRF. */
						extra_lines_newgrf = 4;
					}
				}

				/* Set it to something more sane :) */
				height += extra_lines_prd + extra_lines_req + extra_lines_newgrf;
				size.height = height * GetCharacterHeight(FontSize::Normal) + padding.height;
				size.width = d.width + padding.width;
				break;
			}

			case WID_DPI_FUND_WIDGET: {
				Dimension d = GetStringBoundingBox(STR_FUND_INDUSTRY_BUILD_NEW_INDUSTRY);
				d = maxdim(d, GetStringBoundingBox(STR_FUND_INDUSTRY_PROSPECT_NEW_INDUSTRY));
				d = maxdim(d, GetStringBoundingBox(STR_FUND_INDUSTRY_FUND_NEW_INDUSTRY));
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_DPI_FUND_WIDGET:
				/* Raw industries might be prospected. Show this fact by changing the string
				 * In Editor, you just build, while ingame, or you fund or you prospect */
				if (_game_mode == GameMode::Editor) {
					/* We've chosen many random industries but no industries have been specified */
					return GetString(STR_FUND_INDUSTRY_BUILD_NEW_INDUSTRY);
				}
				if (this->selected_type != IT_INVALID) {
					const IndustrySpec *indsp = GetIndustrySpec(this->selected_type);
					return GetString((_settings_game.construction.raw_industry_construction == 2 && indsp->IsRawIndustry()) ? STR_FUND_INDUSTRY_PROSPECT_NEW_INDUSTRY : STR_FUND_INDUSTRY_FUND_NEW_INDUSTRY);
				}
				return GetString(STR_FUND_INDUSTRY_FUND_NEW_INDUSTRY);

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_DPI_MATRIX_WIDGET: {
				bool rtl = _current_text_dir == TD_RTL;
				Rect text = r.WithHeight(this->resize.step_height).Shrink(WidgetDimensions::scaled.matrix);
				Rect icon = text.WithWidth(this->legend.width, rtl);
				text = text.Indent(this->legend.width + WidgetDimensions::scaled.hsep_wide, rtl);

				/* Vertical offset for legend icon. */
				icon.top    = r.top + (this->resize.step_height - this->legend.height + 1) / 2;
				icon.bottom = icon.top + this->legend.height - 1;

				auto badge_column_widths = badge_classes.GetColumnWidths();

				auto [first, last] = this->vscroll->GetVisibleRangeIterators(this->list);
				for (auto it = first; it != last; ++it) {
					IndustryType type = *it;
					bool selected = this->selected_type == type;
					const IndustrySpec *indsp = GetIndustrySpec(type);

					Rect tr = text;
					if (badge_column_widths.size() >= 1 && badge_column_widths[0] > 0) {
						DrawBadgeColumn(tr.WithWidth(badge_column_widths[0], rtl), 0, this->badge_classes, indsp->badges, GrfSpecFeature::Industries, std::nullopt, PAL_NONE);
						tr = tr.Indent(badge_column_widths[0], rtl);
					}
					if (badge_column_widths.size() >= 2 && badge_column_widths[1] > 0) {
						DrawBadgeColumn(tr.WithWidth(badge_column_widths[1], !rtl), 0, this->badge_classes, indsp->badges, GrfSpecFeature::Industries, std::nullopt, PAL_NONE);
						tr = tr.Indent(badge_column_widths[1], !rtl);
					}

					/* Draw the name of the industry in white is selected, otherwise, in orange */
					DrawString(tr, indsp->name, selected ? TextColour::White : TextColour::Orange);
					GfxFillRect(icon, selected ? PC_WHITE : PC_BLACK);
					GfxFillRect(icon.Shrink(WidgetDimensions::scaled.bevel), indsp->map_colour);
					DrawString(tr, GetString(STR_JUST_COMMA, Industry::GetIndustryTypeCount(type)), TextColour::Black, AlignmentH::End, false, FontSize::Small);

					text = text.Translate(0, this->resize.step_height);
					icon = icon.Translate(0, this->resize.step_height);
				}
				break;
			}

			case WID_DPI_INFOPANEL: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);

				if (this->selected_type == IT_INVALID) {
					DrawStringMultiLine(ir, STR_FUND_INDUSTRY_MANY_RANDOM_INDUSTRIES_TOOLTIP);
					break;
				}

				const IndustrySpec *indsp = GetIndustrySpec(this->selected_type);

				if (_game_mode != GameMode::Editor) {
					DrawString(ir, GetString(STR_FUND_INDUSTRY_INDUSTRY_BUILD_COST, indsp->GetConstructionCost()));
					ir.top += GetCharacterHeight(FontSize::Normal);
				}

				CargoSuffix cargo_suffix[std::tuple_size_v<decltype(indsp->accepts_cargo)>];

				/* Draw the accepted cargoes, if any. Otherwise, will print "Nothing". */
				GetAllCargoSuffixes(CargoSuffixDirection::In, CargoSuffixType::Fund, nullptr, this->selected_type, indsp, indsp->accepts_cargo, cargo_suffix);
				std::string cargostring = this->MakeCargoListString(indsp->accepts_cargo, cargo_suffix, STR_INDUSTRY_VIEW_REQUIRES_N_CARGO);
				ir.top = DrawStringMultiLine(ir, cargostring);

				/* Draw the produced cargoes, if any. Otherwise, will print "Nothing". */
				GetAllCargoSuffixes(CargoSuffixDirection::Out, CargoSuffixType::Fund, nullptr, this->selected_type, indsp, indsp->produced_cargo, cargo_suffix);
				cargostring = this->MakeCargoListString(indsp->produced_cargo, cargo_suffix, STR_INDUSTRY_VIEW_PRODUCES_N_CARGO);
				ir.top = DrawStringMultiLine(ir, cargostring);

				ir.top = DrawBadgeNameList(ir, indsp->badges, GrfSpecFeature::Industries);

				/* Get the additional purchase info text, if it has not already been queried. */
				if (indsp->callback_mask.Test(IndustryCallbackMask::FundMoreText)) {
					std::array<int32_t, 16> regs100;
					uint16_t callback_res = GetIndustryCallback(CBID_INDUSTRY_FUND_MORE_TEXT, 0, 0, nullptr, this->selected_type, INVALID_TILE, regs100);
					if (callback_res != CALLBACK_FAILED && callback_res != 0x400) {
						std::string str;
						if (callback_res == 0x40F) {
							str = GetGRFStringWithTextStack(indsp->grf_prop.grffile, static_cast<GRFStringID>(regs100[0]), std::span{regs100}.subspan(1));
						} else if (callback_res > 0x400) {
							ErrorUnknownCallbackResult(indsp->grf_prop.grfid, CBID_INDUSTRY_FUND_MORE_TEXT, callback_res);
						} else {
							str = GetGRFStringWithTextStack(indsp->grf_prop.grffile, GRFSTR_MISC_GRF_TEXT + callback_res, regs100);
						}
						if (!str.empty()) {
							DrawStringMultiLine(ir, str, TextColour::Yellow);
						}
					}
				}
				break;
			}
		}
	}

	static void AskManyRandomIndustriesCallback(Window *, bool confirmed)
	{
		if (!confirmed) return;

		if (Town::GetNumItems() == 0) {
			ShowErrorMessage(GetEncodedString(STR_ERROR_CAN_T_GENERATE_INDUSTRIES), GetEncodedString(STR_ERROR_MUST_FOUND_TOWN_FIRST), WarningLevel::Info);
		} else {
			Map::CountLandTiles();
			AutoRestoreBackup old_generating_world(_generating_world, true);
			BasePersistentStorageArray::SwitchMode(PSM_ENTER_GAMELOOP);
			GenerateIndustries();
			BasePersistentStorageArray::SwitchMode(PSM_LEAVE_GAMELOOP);
		}
	}

	static void AskRemoveAllIndustriesCallback(Window *, bool confirmed)
	{
		if (!confirmed) return;

		for (Industry *industry : Industry::Iterate()) delete industry;

		/* Clear farmland. */
		for (const auto tile : Map::Iterate()) {
			if (IsTileType(tile, TileType::Clear) && GetClearGround(tile) == ClearGround::Fields) {
				MakeClear(tile, ClearGround::Grass, 3);
			}
		}

		MarkWholeScreenDirty();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_DPI_CREATE_RANDOM_INDUSTRIES_WIDGET: {
				assert(_game_mode == GameMode::Editor);
				this->HandleButtonClick(WID_DPI_CREATE_RANDOM_INDUSTRIES_WIDGET);
				ShowQuery(
					GetEncodedString(STR_FUND_INDUSTRY_MANY_RANDOM_INDUSTRIES_CAPTION),
					GetEncodedString(STR_FUND_INDUSTRY_MANY_RANDOM_INDUSTRIES_QUERY),
					nullptr, AskManyRandomIndustriesCallback);
				break;
			}

			case WID_DPI_REMOVE_ALL_INDUSTRIES_WIDGET: {
				assert(_game_mode == GameMode::Editor);
				this->HandleButtonClick(WID_DPI_REMOVE_ALL_INDUSTRIES_WIDGET);
				ShowQuery(
					GetEncodedString(STR_FUND_INDUSTRY_REMOVE_ALL_INDUSTRIES_CAPTION),
					GetEncodedString(STR_FUND_INDUSTRY_REMOVE_ALL_INDUSTRIES_QUERY),
					nullptr, AskRemoveAllIndustriesCallback);
				break;
			}

			case WID_DPI_MATRIX_WIDGET: {
				auto it = this->vscroll->GetScrolledItemFromWidget(this->list, pt.y, this, WID_DPI_MATRIX_WIDGET);
				if (it != this->list.end()) { // Is it within the boundaries of available data?
					this->selected_type = *it;
					this->UpdateAvailability();

					const IndustrySpec *indsp = GetIndustrySpec(this->selected_type);

					this->SetDirty();

					if (_thd.GetCallbackWnd() == this &&
							((_game_mode != GameMode::Editor && _settings_game.construction.raw_industry_construction == 2 && indsp != nullptr && indsp->IsRawIndustry()) || !this->enabled)) {
						/* Reset the button state if going to prospecting or "build many industries" */
						this->RaiseButtons();
						ResetObjectToPlace();
					}

					this->SetButtons();
					if (this->enabled && click_count > 1) this->OnClick(pt, WID_DPI_FUND_WIDGET, 1);
				}
				break;
			}

			case WID_DPI_DISPLAY_WIDGET:
				if (this->selected_type != IT_INVALID) ShowIndustryCargoesWindow(this->selected_type);
				break;

			case WID_DPI_FUND_WIDGET: {
				if (this->selected_type != IT_INVALID) {
					if (_game_mode != GameMode::Editor && _settings_game.construction.raw_industry_construction == 2 && GetIndustrySpec(this->selected_type)->IsRawIndustry()) {
						Command<Commands::BuildIndustry>::Post(STR_ERROR_CAN_T_CONSTRUCT_THIS_INDUSTRY, TileIndex{}, this->selected_type, 0, false, InteractiveRandom());
						this->HandleButtonClick(WID_DPI_FUND_WIDGET);
					} else {
						HandlePlacePushButton(this, WID_DPI_FUND_WIDGET, SPR_CURSOR_INDUSTRY, HT_RECT);
					}
				}
				break;
			}
		}
	}

	void OnResize() override
	{
		/* Adjust the number of items in the matrix depending of the resize */
		this->vscroll->SetCapacityFromWidget(this, WID_DPI_MATRIX_WIDGET);
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		bool success = true;
		/* We do not need to protect ourselves against "Random Many Industries" in this mode */
		const IndustrySpec *indsp = GetIndustrySpec(this->selected_type);
		uint32_t seed = InteractiveRandom();
		uint32_t layout_index = InteractiveRandomRange((uint32_t)indsp->layouts.size());

		if (_game_mode == GameMode::Editor) {
			/* Show error if no town exists at all */
			if (Town::GetNumItems() == 0) {
				ShowErrorMessage(GetEncodedString(STR_ERROR_CAN_T_BUILD_HERE, indsp->name),
					GetEncodedString(STR_ERROR_MUST_FOUND_TOWN_FIRST), WarningLevel::Info, pt.x, pt.y);
				return;
			}

			AutoRestoreBackup backup_cur_company(_current_company, OWNER_NONE);
			AutoRestoreBackup backup_generating_world(_generating_world, true);
			AutoRestoreBackup backup_ignore_industry_restritions(_ignore_industry_restrictions, true);

			Command<Commands::BuildIndustry>::Post(STR_ERROR_CAN_T_CONSTRUCT_THIS_INDUSTRY, tile, this->selected_type, layout_index, false, seed);
		} else {
			success = Command<Commands::BuildIndustry>::Post(STR_ERROR_CAN_T_CONSTRUCT_THIS_INDUSTRY, tile, this->selected_type, layout_index, false, seed);
		}

		/* If an industry has been built, just reset the cursor and the system */
		if (success && !_settings_client.gui.persistent_buildingtools) ResetObjectToPlace();
	}

	const IntervalTimer<TimerWindow> update_interval = {std::chrono::seconds(3), [this](auto) {
		if (_game_mode == GameMode::Editor) return;
		if (this->selected_type == IT_INVALID) return;

		bool enabled = this->enabled;
		this->UpdateAvailability();
		if (enabled != this->enabled) {
			this->SetButtons();
			this->SetDirty();
		}
	}};

	void OnTimeout() override
	{
		this->RaiseButtons();
	}

	void OnPlaceObjectAbort() override
	{
		this->RaiseButtons();
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		this->SetupArrays();
		this->SetButtons();
		this->SetDirty();
	}
};

void ShowBuildIndustryWindow()
{
	if (_game_mode != GameMode::Editor && !Company::IsValidID(_local_company)) return;
	if (BringWindowToFrontById(WindowClass::BuildIndustry, 0)) return;
	new BuildIndustryWindow();
}

static void UpdateIndustryProduction(Industry *i);

static inline bool IsProductionAlterable(const Industry *i)
{
	const IndustrySpec *is = GetIndustrySpec(i->type);
	bool has_prod = std::any_of(std::begin(is->production_rate), std::end(is->production_rate), [](auto rate) { return rate != 0; });
	return ((_game_mode == GameMode::Editor || _cheats.setup_prod.value) &&
			(has_prod || is->IsRawIndustry()) &&
			!_networking);
}

class IndustryViewWindow : public Window
{
	/** Modes for changing production */
	enum class Editability : uint8_t {
		None, ///< Not alterable
		Multiplier, ///< Allow changing the production multiplier
		Rate, ///< Allow changing the production rates
	};

	/** Clicked cargo line in the info panel. */
	using ClickedCargoLine = int8_t;

	/* Special clicked cargo line values. */
	static constexpr ClickedCargoLine CCL_NONE = -1; ///< No cargo line has been clicked.
	static constexpr ClickedCargoLine CCL_MULTIPLIER = -2; ///< Product multiplier line is clicked.

	Dimension cargo_icon_size{}; ///< Largest cargo icon dimension.
	Editability editable{}; ///< Mode for changing production
	ClickedCargoLine editbox_line = CCL_NONE; ///< The line clicked to open the edit box
	ClickedCargoLine clicked_line = CCL_NONE; ///< The line of the button that has been clicked
	uint8_t clicked_button = 0; ///< The button that has been clicked (to raise)
	int production_offset_y = 0; ///< The offset of the production texts/buttons
	int info_height = 0; ///< Height needed for the #WID_IV_INFO panel
	int cheat_line_height = 0; ///< Height of each line for the #WID_IV_INFO panel

public:
	IndustryViewWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->flags.Set(WindowFlag::DisableVpScroll);
		this->info_height = WidgetDimensions::scaled.framerect.Vertical() + 2 * GetCharacterHeight(FontSize::Normal); // Info panel has at least two lines text.

		this->InitNested(window_number);
		NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WID_IV_VIEWPORT);
		nvp->InitializeViewport(this, Industry::Get(window_number)->location.GetCenterTile(), ScaleZoomGUI(ZoomLevel::Industry));

		const Industry *i = Industry::Get(window_number);
		if (!i->IsCargoProduced() && !i->IsCargoAccepted()) this->DisableWidget(WID_IV_GRAPH);

		this->InvalidateData();
	}

	/** Close the industry production window. */
	~IndustryViewWindow() override
	{
		CloseWindowById(WindowClass::IndustryProductionGraph, this->window_number, false);
	}

	void OnInit() override
	{
		/* This only used when the cheat to alter industry production is enabled */
		this->cheat_line_height = std::max(SETTING_BUTTON_HEIGHT + WidgetDimensions::scaled.vsep_normal, GetCharacterHeight(FontSize::Normal));
		this->cargo_icon_size = GetLargestCargoIconSize();
	}

	void OnPaint() override
	{
		this->DrawWidgets();

		if (this->IsShaded()) return; // Don't draw anything when the window is shaded.

		const Rect r = this->GetWidget<NWidgetBase>(WID_IV_INFO)->GetCurrentRect();
		int expected = this->DrawInfo(r);
		if (expected != r.bottom) {
			this->info_height = expected - r.top + 1;
			this->ReInit();
			return;
		}
	}

	void DrawCargoIcon(const Rect &r, CargoType cargo_type) const
	{
		bool rtl = _current_text_dir == TD_RTL;
		SpriteID icon = CargoSpec::Get(cargo_type)->GetCargoIcon();
		Dimension d = GetSpriteSize(icon);
		Rect ir = r.WithWidth(this->cargo_icon_size.width, rtl).WithHeight(GetCharacterHeight(FontSize::Normal));
		DrawSprite(icon, PAL_NONE, CentreBounds(ir.left, ir.right, d.width), CentreBounds(ir.top, ir.bottom, this->cargo_icon_size.height));
	}

	std::string GetAcceptedCargoString(const Industry::AcceptedCargo &ac, const CargoSuffix &suffix) const
	{
		auto params = MakeParameters(CargoSpec::Get(ac.cargo)->name, ac.cargo, ac.waiting, suffix.text);
		switch (suffix.display) {
			case CargoSuffixDisplay::AmountAndText: return GetStringWithArgs(STR_INDUSTRY_VIEW_ACCEPT_CARGO_AMOUNT_SUFFIX, params);
			case CargoSuffixDisplay::Text: return GetStringWithArgs(STR_INDUSTRY_VIEW_ACCEPT_CARGO_SUFFIX, params);
			case CargoSuffixDisplay::Amount: return GetStringWithArgs(STR_INDUSTRY_VIEW_ACCEPT_CARGO_AMOUNT_NOSUFFIX, params);
			case CargoSuffixDisplay::None: return GetStringWithArgs(STR_INDUSTRY_VIEW_ACCEPT_CARGO_NOSUFFIX, params);
			default: NOT_REACHED();
		}
	}

	/**
	 * Draw the text in the #WID_IV_INFO panel.
	 * @param r Rectangle of the panel.
	 * @return Expected position of the bottom edge of the panel.
	 */
	int DrawInfo(const Rect &r)
	{
		bool rtl = _current_text_dir == TD_RTL;
		Industry *i = Industry::Get(this->window_number);
		const IndustrySpec *ind = GetIndustrySpec(i->type);
		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		bool first = true;
		bool has_accept = false;

		/* Use all the available space past the rect, so that we can enlarge the window if needed. */
		ir.bottom = INT_MAX;

		if (i->prod_level == PRODLEVEL_CLOSURE) {
			DrawString(ir, STR_INDUSTRY_VIEW_INDUSTRY_ANNOUNCED_CLOSURE);
			ir.top += GetCharacterHeight(FontSize::Normal) + WidgetDimensions::scaled.vsep_wide;
		}

		const int label_indent = WidgetDimensions::scaled.hsep_normal + this->cargo_icon_size.width;
		bool stockpiling = ind->callback_mask.Any({IndustryCallbackMask::ProductionCargoArrival, IndustryCallbackMask::Production256Ticks});

		for (const auto &a : i->accepted) {
			if (!IsValidCargoType(a.cargo)) continue;
			has_accept = true;
			if (first) {
				DrawString(ir, STR_INDUSTRY_VIEW_REQUIRES);
				ir.top += GetCharacterHeight(FontSize::Normal);
				first = false;
			}

			DrawCargoIcon(ir, a.cargo);

			CargoSuffix suffix;
			GetCargoSuffix(CargoSuffixDirection::In, CargoSuffixType::View, i, i->type, ind, a.cargo, &a - i->accepted.data(), suffix);
			/* if the industry is not stockpiling then don't show amount in the acceptance display. */
			if (!stockpiling && suffix.display == CargoSuffixDisplay::AmountAndText) suffix.display = CargoSuffixDisplay::Text;
			if (!stockpiling && suffix.display == CargoSuffixDisplay::Amount) suffix.display = CargoSuffixDisplay::None;

			DrawString(ir.Indent(label_indent, rtl), this->GetAcceptedCargoString(a, suffix));
			ir.top += GetCharacterHeight(FontSize::Normal);
		}

		int line_height = this->editable == Editability::Rate ? this->cheat_line_height : GetCharacterHeight(FontSize::Normal);
		int text_y_offset = (line_height - GetCharacterHeight(FontSize::Normal)) / 2;
		int button_y_offset = (line_height - SETTING_BUTTON_HEIGHT) / 2;
		first = true;
		for (const auto &p : i->produced) {
			if (!IsValidCargoType(p.cargo)) continue;
			if (first) {
				if (has_accept) ir.top += WidgetDimensions::scaled.vsep_wide;
				DrawString(ir, TimerGameEconomy::UsingWallclockUnits() ? STR_INDUSTRY_VIEW_PRODUCTION_LAST_MINUTE_TITLE : STR_INDUSTRY_VIEW_PRODUCTION_LAST_MONTH_TITLE);
				ir.top += GetCharacterHeight(FontSize::Normal);
				if (this->editable == Editability::Rate) this->production_offset_y = ir.top;
				first = false;
			}

			DrawCargoIcon(ir, p.cargo);

			CargoSuffix suffix;
			GetCargoSuffix(CargoSuffixDirection::Out, CargoSuffixType::View, i, i->type, ind, p.cargo, &p - i->produced.data(), suffix);

			DrawString(ir.Indent(label_indent + (this->editable == Editability::Rate ? SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_normal : 0), rtl).Translate(0, text_y_offset),
				GetString(STR_INDUSTRY_VIEW_TRANSPORTED, p.cargo, p.history[LAST_MONTH].production, suffix.text, ToPercent8(p.history[LAST_MONTH].PctTransported())));
			/* Let's put out those buttons.. */
			if (this->editable == Editability::Rate) {
				DrawArrowButtons(ir.Indent(label_indent, rtl).WithWidth(SETTING_BUTTON_WIDTH, rtl).left, ir.top + button_y_offset, Colours::Yellow, (this->clicked_line == &p - i->produced.data()) ? this->clicked_button : 0,
						p.rate > 0, p.rate < 255);
			}
			ir.top += line_height;
		}

		/* Display production multiplier if editable */
		if (this->editable == Editability::Multiplier) {
			line_height = this->cheat_line_height;
			text_y_offset = (line_height - GetCharacterHeight(FontSize::Normal)) / 2;
			button_y_offset = (line_height - SETTING_BUTTON_HEIGHT) / 2;
			ir.top += WidgetDimensions::scaled.vsep_wide;
			this->production_offset_y = ir.top;
			DrawString(ir.Indent(label_indent + SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_normal, rtl).Translate(0, text_y_offset),
					GetString(STR_INDUSTRY_VIEW_PRODUCTION_LEVEL, RoundDivSU(i->prod_level * 100, PRODLEVEL_DEFAULT)));
			DrawArrowButtons(ir.Indent(label_indent, rtl).WithWidth(SETTING_BUTTON_WIDTH, rtl).left, ir.top + button_y_offset, Colours::Yellow, (this->clicked_line == CCL_MULTIPLIER) ? this->clicked_button : 0,
					i->prod_level > PRODLEVEL_MINIMUM, i->prod_level < PRODLEVEL_MAXIMUM);
			ir.top += line_height;
		}

		/* Get the extra message for the GUI */
		if (ind->callback_mask.Test(IndustryCallbackMask::WindowMoreText)) {
			std::array<int32_t, 16> regs100;
			uint16_t callback_res = GetIndustryCallback(CBID_INDUSTRY_WINDOW_MORE_TEXT, 0, 0, i, i->type, i->location.tile, regs100);
			if (callback_res != CALLBACK_FAILED && callback_res != 0x400) {
				std::string str;
				if (callback_res == 0x40F) {
					str = GetGRFStringWithTextStack(ind->grf_prop.grffile, static_cast<GRFStringID>(regs100[0]), std::span{regs100}.subspan(1));
				} else if (callback_res > 0x400) {
					ErrorUnknownCallbackResult(ind->grf_prop.grfid, CBID_INDUSTRY_WINDOW_MORE_TEXT, callback_res);
				} else {
					str = GetGRFStringWithTextStack(ind->grf_prop.grffile, GRFSTR_MISC_GRF_TEXT + callback_res, regs100);
				}
				if (!str.empty()) {
					ir.top += WidgetDimensions::scaled.vsep_wide;
					ir.top = DrawStringMultiLine(ir, str, TextColour::Yellow);
				}
			}
		}

		if (!i->text.empty()) {
			ir.top += WidgetDimensions::scaled.vsep_wide;
			ir.top = DrawStringMultiLine(ir, i->text.GetDecodedString(), TextColour::Black);
		}

		/* Return required bottom position, the last pixel row plus some padding. */
		return ir.top - 1 + WidgetDimensions::scaled.framerect.bottom;
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_IV_CAPTION) return GetString(STR_INDUSTRY_VIEW_CAPTION, this->window_number);

		return this->Window::GetWidgetString(widget, stringid);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget == WID_IV_INFO) size.height = this->info_height;
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_IV_INFO: {
				Industry *i = Industry::Get(this->window_number);
				ClickedCargoLine line = CCL_NONE;

				switch (this->editable) {
					case Editability::None: break;

					case Editability::Multiplier:
						if (IsInsideBS(pt.y, this->production_offset_y, this->cheat_line_height)) line = CCL_MULTIPLIER;
						break;

					case Editability::Rate:
						if (pt.y >= this->production_offset_y) {
							int row = (pt.y - this->production_offset_y) / this->cheat_line_height;
							for (auto itp = std::begin(i->produced); itp != std::end(i->produced); ++itp) {
								if (!IsValidCargoType(itp->cargo)) continue;
								row--;
								if (row < 0) {
									line = itp - std::begin(i->produced);
									break;
								}
							}
						}
						break;
				}
				if (line == CCL_NONE) return;

				bool rtl = _current_text_dir == TD_RTL;
				Rect r = this->GetWidget<NWidgetBase>(widget)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect).Indent(this->cargo_icon_size.width + WidgetDimensions::scaled.hsep_normal, rtl);

				if (r.WithWidth(SETTING_BUTTON_WIDTH, rtl).Contains(pt)) {
					/* Clicked buttons, decrease or increase production */
					bool decrease = r.WithWidth(SETTING_BUTTON_WIDTH / 2, rtl).Contains(pt);
					switch (this->editable) {
						case Editability::Multiplier:
							if (decrease) {
								if (i->prod_level <= PRODLEVEL_MINIMUM) return;
								i->prod_level = static_cast<uint8_t>(std::max<uint>(i->prod_level / 2, PRODLEVEL_MINIMUM));
							} else {
								if (i->prod_level >= PRODLEVEL_MAXIMUM) return;
								i->prod_level = static_cast<uint8_t>(std::min<uint>(i->prod_level * 2, PRODLEVEL_MAXIMUM));
							}
							break;

						case Editability::Rate:
							if (decrease) {
								if (i->produced[line].rate <= 0) return;
								i->produced[line].rate = std::max(i->produced[line].rate / 2, 0);
							} else {
								if (i->produced[line].rate >= 255) return;
								/* a zero production industry is unlikely to give anything but zero, so push it a little bit */
								int new_prod = i->produced[line].rate == 0 ? 1 : i->produced[line].rate * 2;
								i->produced[line].rate = ClampTo<uint8_t>(new_prod);
							}
							break;

						default: NOT_REACHED();
					}

					UpdateIndustryProduction(i);
					this->SetDirty();
					this->SetTimeout();
					this->clicked_line = line;
					this->clicked_button = (decrease ^ rtl) ? 1 : 2;
				} else if (r.Indent(SETTING_BUTTON_WIDTH + WidgetDimensions::scaled.hsep_normal, rtl).Contains(pt)) {
					/* clicked the text */
					this->editbox_line = line;
					switch (this->editable) {
						case Editability::Multiplier:
							ShowQueryString(GetString(STR_JUST_INT, RoundDivSU(i->prod_level * 100, PRODLEVEL_DEFAULT)), STR_CONFIG_GAME_PRODUCTION_LEVEL, 10, this, CS_ALPHANUMERAL, {});
							break;

						case Editability::Rate:
							ShowQueryString(GetString(STR_JUST_INT, i->produced[line].rate * 8), STR_CONFIG_GAME_PRODUCTION, 10, this, CS_ALPHANUMERAL, {});
							break;

						default: NOT_REACHED();
					}
				}
				break;
			}

			case WID_IV_GOTO: {
				Industry *i = Industry::Get(this->window_number);
				if (_ctrl_pressed) {
					ShowExtraViewportWindow(i->location.GetCenterTile());
				} else {
					ScrollMainWindowToTile(i->location.GetCenterTile());
				}
				break;
			}

			case WID_IV_DISPLAY: {
				Industry *i = Industry::Get(this->window_number);
				ShowIndustryCargoesWindow(i->type);
				break;
			}

			case WID_IV_GRAPH:
				ShowIndustryProductionGraph(this->window_number);
				break;
		}
	}

	void OnTimeout() override
	{
		this->clicked_line = CCL_NONE;
		this->clicked_button = 0;
		this->SetDirty();
	}

	void OnResize() override
	{
		if (this->viewport != nullptr) {
			NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(WID_IV_VIEWPORT);
			nvp->UpdateViewportCoordinates(this);

			ScrollWindowToTile(Industry::Get(this->window_number)->location.GetCenterTile(), this, true); // Re-center viewport.
		}
	}

	void OnMouseWheel(int wheel, WidgetID widget) override
	{
		if (widget != WID_IV_VIEWPORT) return;
		if (_settings_client.gui.scrollwheel_scrolling != ScrollWheelScrolling::Off) {
			DoZoomInOutWindow(wheel < 0 ? ZOOM_IN : ZOOM_OUT, this);
		}
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value() || str->empty()) return;

		Industry *i = Industry::Get(this->window_number);
		auto value = ParseInteger(*str, 10, true);
		if (!value.has_value()) return;
		switch (this->editbox_line) {
			case CCL_NONE: NOT_REACHED();

			case CCL_MULTIPLIER:
				i->prod_level = ClampU(RoundDivSU(*value * PRODLEVEL_DEFAULT, 100), PRODLEVEL_MINIMUM, PRODLEVEL_MAXIMUM);
				break;

			default:
				i->produced[this->editbox_line].rate = ClampU(RoundDivSU(*value, 8), 0, 255);
				break;
		}
		UpdateIndustryProduction(i);
		this->SetDirty();
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		const Industry *i = Industry::Get(this->window_number);
		if (IsProductionAlterable(i)) {
			const IndustrySpec *ind = GetIndustrySpec(i->type);
			this->editable = ind->UsesOriginalEconomy() ? Editability::Multiplier : Editability::Rate;
		} else {
			this->editable = Editability::None;
		}
	}

	bool IsNewGRFInspectable() const override
	{
		return ::IsNewGRFInspectable(GrfSpecFeature::Industries, this->window_number);
	}

	void ShowNewGRFInspectWindow() const override
	{
		::ShowNewGRFInspectWindow(GrfSpecFeature::Industries, this->window_number);
	}
};

static void UpdateIndustryProduction(Industry *i)
{
	const IndustrySpec *indspec = GetIndustrySpec(i->type);
	if (indspec->UsesOriginalEconomy()) i->RecomputeProductionMultipliers();

	for (auto &p : i->produced) {
		if (IsValidCargoType(p.cargo)) {
			p.history[LAST_MONTH].production = ScaleByCargoScale(8 * p.rate, false);
		}
	}
}

/** Widget definition of the view industry gui */
static constexpr std::initializer_list<NWidgetPart> _nested_industry_view_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Cream),
		NWidget(WWT_CAPTION, Colours::Cream, WID_IV_CAPTION),
		NWidget(WWT_PUSHIMGBTN, Colours::Cream, WID_IV_GOTO), SetAspect(WidgetDimensions::ASPECT_LOCATION), SetSpriteTip(SPR_GOTO_LOCATION, STR_INDUSTRY_VIEW_LOCATION_TOOLTIP),
		NWidget(WWT_DEBUGBOX, Colours::Cream),
		NWidget(WWT_SHADEBOX, Colours::Cream),
		NWidget(WWT_DEFSIZEBOX, Colours::Cream),
		NWidget(WWT_STICKYBOX, Colours::Cream),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Cream),
		NWidget(WWT_INSET, Colours::Cream), SetPadding(2, 2, 2, 2),
			NWidget(NWID_VIEWPORT, Colours::Invalid, WID_IV_VIEWPORT), SetMinimalSize(254, 86), SetFill(1, 0), SetResize(1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Cream, WID_IV_INFO), SetMinimalSize(260, 0), SetMinimalTextLines(2, WidgetDimensions::unscaled.framerect.Vertical()), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::Cream, WID_IV_DISPLAY), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_INDUSTRY_DISPLAY_CHAIN, STR_INDUSTRY_DISPLAY_CHAIN_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Cream, WID_IV_GRAPH), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_INDUSTRY_VIEW_CARGO_GRAPH, STR_INDUSTRY_VIEW_CARGO_GRAPH_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Cream),
	EndContainer(),
};

/** Window definition of the view industry gui */
static WindowDesc _industry_view_desc(
	WindowPosition::Automatic, "view_industry", 260, 120,
	WindowClass::IndustryView, WindowClass::None,
	{},
	_nested_industry_view_widgets
);

void ShowIndustryViewWindow(IndustryID industry)
{
	AllocateWindowDescFront<IndustryViewWindow>(_industry_view_desc, industry);
}

/** Widget definition of the industry directory gui */
static constexpr std::initializer_list<NWidgetPart> _nested_industry_directory_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_ID_CAPTION),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_DEFSIZEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, Colours::Brown, WID_ID_DROPDOWN_ORDER), SetStringTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
				NWidget(WWT_DROPDOWN, Colours::Brown, WID_ID_DROPDOWN_CRITERIA), SetToolTip(STR_TOOLTIP_SORT_CRITERIA),
				NWidget(WWT_EDITBOX, Colours::Brown, WID_ID_FILTER), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_DROPDOWN, Colours::Brown, WID_ID_FILTER_BY_ACC_CARGO), SetMinimalSize(225, 12), SetFill(0, 1), SetToolTip(STR_TOOLTIP_FILTER_CRITERIA),
				NWidget(WWT_DROPDOWN, Colours::Brown, WID_ID_FILTER_BY_PROD_CARGO), SetMinimalSize(225, 12), SetFill(0, 1), SetToolTip(STR_TOOLTIP_FILTER_CRITERIA),
				NWidget(WWT_PANEL, Colours::Brown), SetResize(1, 0), EndContainer(),
			EndContainer(),
			NWidget(WWT_PANEL, Colours::Brown, WID_ID_INDUSTRY_LIST), SetToolTip(STR_INDUSTRY_DIRECTORY_LIST_TOOLTIP), SetResize(1, 1), SetScrollbar(WID_ID_VSCROLLBAR),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::Brown, WID_ID_VSCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HSCROLLBAR, Colours::Brown, WID_ID_HSCROLLBAR),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

typedef GUIList<const Industry *, const CargoType &, const std::pair<CargoType, CargoType> &> GUIIndustryList;

/** Cargo filter functions */
/**
 * Check whether an industry accepts and produces a certain cargo pair.
 * @param industry The industry whose cargoes will being checked.
 * @param cargoes The accepted and produced cargo pair to look for.
 * @return bool Whether the given cargoes accepted and produced by the industry.
 */
static bool CargoFilter(const Industry * const *industry, const std::pair<CargoType, CargoType> &cargoes)
{
	auto accepted_cargo = cargoes.first;
	auto produced_cargo = cargoes.second;

	bool accepted_cargo_matches;

	switch (accepted_cargo) {
		case CargoFilterCriteria::CF_ANY:
			accepted_cargo_matches = true;
			break;

		case CargoFilterCriteria::CF_NONE:
			accepted_cargo_matches = !(*industry)->IsCargoAccepted();
			break;

		default:
			accepted_cargo_matches = (*industry)->IsCargoAccepted(accepted_cargo);
			break;
	}

	bool produced_cargo_matches;

	switch (produced_cargo) {
		case CargoFilterCriteria::CF_ANY:
			produced_cargo_matches = true;
			break;

		case CargoFilterCriteria::CF_NONE:
			produced_cargo_matches = !(*industry)->IsCargoProduced();
			break;

		default:
			produced_cargo_matches = (*industry)->IsCargoProduced(produced_cargo);
			break;
	}

	return accepted_cargo_matches && produced_cargo_matches;
}

static GUIIndustryList::FilterFunction * const _industry_filter_funcs[] = { &CargoFilter };

/**
 * The list of industries.
 */
class IndustryDirectoryWindow : public Window {
protected:
	/* Runtime saved values */
	static Listing last_sorting;

	/** Strings describing how industries are sorted. */
	static inline const StringID sorter_names[] = {
		STR_SORT_BY_NAME,
		STR_SORT_BY_TYPE,
		STR_SORT_BY_PRODUCTION,
		STR_SORT_BY_TRANSPORTED,
	};
	static const std::initializer_list<GUIIndustryList::SortFunction * const> sorter_funcs; ///< Functions to sort industries.

	GUIIndustryList industries{IndustryDirectoryWindow::produced_cargo_filter};
	Scrollbar *vscroll{};
	Scrollbar *hscroll{};

	CargoType produced_cargo_filter_criteria{}; ///< Selected produced cargo filter index
	CargoType accepted_cargo_filter_criteria{}; ///< Selected accepted cargo filter index
	static CargoType produced_cargo_filter;

	const int MAX_FILTER_LENGTH = 16; ///< The max length of the filter, in chars
	StringFilter string_filter{}; ///< Filter for industries
	QueryString industry_editbox; ///< Filter editbox

	/** Ways to sort industries. */
	enum class SorterType : uint8_t {
		ByName, ///< Sorter type to sort by name
		ByType, ///< Sorter type to sort by type
		ByProduction, ///< Sorter type to sort by production amount
		ByTransported, ///< Sorter type to sort by transported percentage
	};

	/**
	 * Set produced cargo filter for the industry list.
	 * @param cargo_type The cargo to be set
	 */
	void SetProducedCargoFilter(CargoType cargo_type)
	{
		if (this->produced_cargo_filter_criteria != cargo_type) {
			this->produced_cargo_filter_criteria = cargo_type;
			/* deactivate filter if criteria is 'Show All', activate it otherwise */
			bool is_filtering_necessary = this->produced_cargo_filter_criteria != CargoFilterCriteria::CF_ANY || this->accepted_cargo_filter_criteria != CargoFilterCriteria::CF_ANY;

			this->industries.SetFilterState(is_filtering_necessary);
			this->industries.SetFilterType(0);
			this->industries.ForceRebuild();
		}
	}

	/**
	 * Set accepted cargo filter for the industry list.
	 * @param cargo_type The cargo to be set
	 */
	void SetAcceptedCargoFilter(CargoType cargo_type)
	{
		if (this->accepted_cargo_filter_criteria != cargo_type) {
			this->accepted_cargo_filter_criteria = cargo_type;
			/* deactivate filter if criteria is 'Show All', activate it otherwise */
			bool is_filtering_necessary = this->produced_cargo_filter_criteria != CargoFilterCriteria::CF_ANY || this->accepted_cargo_filter_criteria != CargoFilterCriteria::CF_ANY;

			this->industries.SetFilterState(is_filtering_necessary);
			this->industries.SetFilterType(0);
			this->industries.ForceRebuild();
		}
	}

	StringID GetCargoFilterLabel(CargoType cargo_type) const
	{
		switch (cargo_type) {
			case CargoFilterCriteria::CF_ANY: return STR_INDUSTRY_DIRECTORY_FILTER_ALL_TYPES;
			case CargoFilterCriteria::CF_NONE: return STR_INDUSTRY_DIRECTORY_FILTER_NONE;
			default: return CargoSpec::Get(cargo_type)->name;
		}
	}

	/**
	 * Populate the filter list and set the cargo filter criteria.
	 */
	void SetCargoFilterArray()
	{
		this->produced_cargo_filter_criteria = CargoFilterCriteria::CF_ANY;
		this->accepted_cargo_filter_criteria = CargoFilterCriteria::CF_ANY;

		this->industries.SetFilterFuncs(_industry_filter_funcs);

		bool is_filtering_necessary = this->produced_cargo_filter_criteria != CargoFilterCriteria::CF_ANY || this->accepted_cargo_filter_criteria != CargoFilterCriteria::CF_ANY;

		this->industries.SetFilterState(is_filtering_necessary);
	}

	/**
	 * Get the width needed to draw the longest industry line.
	 * @return Returns width of the longest industry line, including padding.
	 */
	uint GetIndustryListWidth() const
	{
		uint width = this->hscroll->GetCount();
		auto [first, last] = this->vscroll->GetVisibleRangeIterators(this->industries);
		for (auto it = first; it != last; ++it) {
			width = std::max(width, GetStringBoundingBox(this->GetIndustryString(*it)).width);
		}
		return width;
	}

	/** (Re)Build industries list */
	void BuildSortIndustriesList()
	{
		if (this->industries.NeedRebuild()) {
			this->industries.clear();
			this->industries.reserve(Industry::GetNumItems());

			for (const Industry *i : Industry::Iterate()) {
				if (this->string_filter.IsEmpty()) {
					this->industries.push_back(i);
					continue;
				}
				this->string_filter.ResetState();
				this->string_filter.AddLine(i->GetCachedName());
				if (this->string_filter.GetState()) this->industries.push_back(i);
			}

			this->industries.RebuildDone();

			auto filter = std::make_pair(this->accepted_cargo_filter_criteria, this->produced_cargo_filter_criteria);

			this->industries.Filter(filter);

			this->vscroll->SetCount(this->industries.size()); // Update scrollbar as well.
		}

		IndustryDirectoryWindow::produced_cargo_filter = this->produced_cargo_filter_criteria;
		this->industries.Sort();

		this->SetDirty();
	}

	/**
	 * Returns percents of cargo transported if industry produces this cargo, else -1
	 *
	 * @param p industry produced cargo
	 * @return percents of cargo transported, or -1 if industry doesn't use this cargo slot
	 */
	static inline int GetCargoTransportedPercentsIfValid(const Industry::ProducedCargo &p)
	{
		if (!IsValidCargoType(p.cargo)) return -1;
		return ToPercent8(p.history[LAST_MONTH].PctTransported());
	}

	/**
	 * Returns value representing industry's transported cargo
	 *  percentage for industry sorting
	 *
	 * @param i industry to check
	 * @return value used for sorting
	 */
	static int GetCargoTransportedSortValue(const Industry *i)
	{
		CargoType filter = IndustryDirectoryWindow::produced_cargo_filter;
		if (filter == CargoFilterCriteria::CF_NONE) return 0;

		int percentage = 0, produced_cargo_count = 0;
		for (const auto &p : i->produced) {
			if (filter == CargoFilterCriteria::CF_ANY) {
				int transported = GetCargoTransportedPercentsIfValid(p);
				if (transported != -1) {
					produced_cargo_count++;
					percentage += transported;
				}
				if (produced_cargo_count == 0 && &p == &i->produced.back() && percentage == 0) {
					return transported;
				}
			} else if (filter == p.cargo) {
				return GetCargoTransportedPercentsIfValid(p);
			}
		}

		if (produced_cargo_count == 0) return percentage;
		return percentage / produced_cargo_count;
	}

	/** Sort industries by name. @copydoc GUIList::SorterWithFilter */
	static bool IndustryNameSorter(const Industry * const &a, const Industry * const &b, [[maybe_unused]] const CargoType &filter)
	{
		int r = StrNaturalCompare(a->GetCachedName(), b->GetCachedName()); // Sort by name (natural sorting).
		if (r == 0) return a->index < b->index;
		return r < 0;
	}

	/** Sort industries by type and name. @copydoc GUIList::SorterWithFilter */
	static bool IndustryTypeSorter(const Industry * const &a, const Industry * const &b, const CargoType &filter)
	{
		int it_a = 0;
		while (it_a != NUM_INDUSTRYTYPES && a->type != _sorted_industry_types[it_a]) it_a++;
		int it_b = 0;
		while (it_b != NUM_INDUSTRYTYPES && b->type != _sorted_industry_types[it_b]) it_b++;
		int r = it_a - it_b;
		return (r == 0) ? IndustryNameSorter(a, b, filter) : r < 0;
	}

	/** Sort industries by production and name. @copydoc GUIList::SorterWithFilter */
	static bool IndustryProductionSorter(const Industry * const &a, const Industry * const &b, const CargoType &filter)
	{
		if (filter == CargoFilterCriteria::CF_NONE) return IndustryTypeSorter(a, b, filter);

		uint prod_a = 0, prod_b = 0;
		if (filter == CargoFilterCriteria::CF_ANY) {
			for (const auto &pa : a->produced) {
				if (IsValidCargoType(pa.cargo)) prod_a += pa.history[LAST_MONTH].production;
			}
			for (const auto &pb : b->produced) {
				if (IsValidCargoType(pb.cargo)) prod_b += pb.history[LAST_MONTH].production;
			}
		} else {
			if (auto ita = a->GetCargoProduced(filter); ita != std::end(a->produced)) prod_a = ita->history[LAST_MONTH].production;
			if (auto itb = b->GetCargoProduced(filter); itb != std::end(b->produced)) prod_b = itb->history[LAST_MONTH].production;
		}
		int r = prod_a - prod_b;

		return (r == 0) ? IndustryTypeSorter(a, b, filter) : r < 0;
	}

	/** Sort industries by transported cargo and name. @copydoc GUIList::SorterWithFilter */
	static bool IndustryTransportedCargoSorter(const Industry * const &a, const Industry * const &b, const CargoType &filter)
	{
		int r = GetCargoTransportedSortValue(a) - GetCargoTransportedSortValue(b);
		return (r == 0) ? IndustryNameSorter(a, b, filter) : r < 0;
	}

	StringID GetStringForNumCargo(size_t count) const
	{
		switch (count) {
			case 0: return STR_INDUSTRY_DIRECTORY_ITEM_NOPROD;
			case 1: return STR_INDUSTRY_DIRECTORY_ITEM_PROD1;
			case 2: return STR_INDUSTRY_DIRECTORY_ITEM_PROD2;
			case 3: return STR_INDUSTRY_DIRECTORY_ITEM_PROD3;
			default: return STR_INDUSTRY_DIRECTORY_ITEM_PRODMORE;
		}
	}

	/**
	 * Get the StringID to draw and set the appropriate DParams.
	 * @param i the industry to get the StringID of.
	 * @return the StringID.
	 */
	std::string GetIndustryString(const Industry *i) const
	{
		const IndustrySpec *indsp = GetIndustrySpec(i->type);

		/* Get industry productions (CargoType, production, suffix, transported) */
		struct CargoInfo {
			CargoType cargo_type; ///< Cargo type.
			uint16_t production; ///< Production last month.
			uint transported; ///< Percent transported last month.
			std::string suffix; ///< Cargo suffix.

			CargoInfo(CargoType cargo_type, uint16_t production, uint transported, std::string &&suffix) : cargo_type(cargo_type), production(production), transported(transported), suffix(std::move(suffix)) {}
		};
		std::vector<CargoInfo> cargos;

		for (auto itp = std::begin(i->produced); itp != std::end(i->produced); ++itp) {
			if (!IsValidCargoType(itp->cargo)) continue;
			CargoSuffix cargo_suffix;
			GetCargoSuffix(CargoSuffixDirection::Out, CargoSuffixType::Directory, i, i->type, indsp, itp->cargo, itp - std::begin(i->produced), cargo_suffix);
			cargos.emplace_back(itp->cargo, itp->history[LAST_MONTH].production, ToPercent8(itp->history[LAST_MONTH].PctTransported()), std::move(cargo_suffix.text));
		}

		switch (static_cast<IndustryDirectoryWindow::SorterType>(this->industries.SortType())) {
			case IndustryDirectoryWindow::SorterType::ByName:
			case IndustryDirectoryWindow::SorterType::ByType:
			case IndustryDirectoryWindow::SorterType::ByProduction:
				/* Sort by descending production, then descending transported */
				std::sort(cargos.begin(), cargos.end(), [](const CargoInfo &a, const CargoInfo &b) {
					if (a.production != b.production) return a.production > b.production;
					return a.transported > b.transported;
				});
				break;

			case IndustryDirectoryWindow::SorterType::ByTransported:
				/* Sort by descending transported, then descending production */
				std::sort(cargos.begin(), cargos.end(), [](const CargoInfo &a, const CargoInfo &b) {
					if (a.transported != b.transported) return a.transported > b.transported;
					return a.production > b.production;
				});
				break;
		}

		/* If the produced cargo filter is active then move the filtered cargo to the beginning of the list,
		 * because this is the one the player interested in, and that way it is not hidden in the 'n' more cargos */
		const CargoType cargo_type = this->produced_cargo_filter_criteria;
		if (cargo_type != CargoFilterCriteria::CF_ANY && cargo_type != CargoFilterCriteria::CF_NONE) {
			auto filtered_ci = std::ranges::find(cargos, cargo_type, &CargoInfo::cargo_type);
			if (filtered_ci != cargos.end()) {
				std::rotate(cargos.begin(), filtered_ci, filtered_ci + 1);
			}
		}

		static constexpr size_t MAX_DISPLAYED_CARGOES = 3;
		std::array<StringParameter, 2 + 5 * MAX_DISPLAYED_CARGOES> params{};
		auto it = params.begin();

		/* Industry name */
		*it++ = i->index;

		/* Display first MAX_DISPLAYED_CARGOES cargoes */
		for (CargoInfo &ci : cargos | std::views::take(MAX_DISPLAYED_CARGOES)) {
			*it++ = STR_INDUSTRY_DIRECTORY_ITEM_INFO;
			*it++ = ci.cargo_type;
			*it++ = ci.production;
			*it++ = std::move(ci.suffix);
			*it++ = ci.transported;
		}

		/* Undisplayed cargos if any */
		if (std::size(cargos) > MAX_DISPLAYED_CARGOES) *it++ = std::size(cargos) - MAX_DISPLAYED_CARGOES;

		return GetStringWithArgs(GetStringForNumCargo(std::size(cargos)), {params.begin(), it});
	}

public:
	IndustryDirectoryWindow(WindowDesc &desc, WindowNumber) : Window(desc), industry_editbox(MAX_FILTER_LENGTH * MAX_CHAR_LENGTH, MAX_FILTER_LENGTH)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_ID_VSCROLLBAR);
		this->hscroll = this->GetScrollbar(WID_ID_HSCROLLBAR);

		this->industries.SetListing(this->last_sorting);
		this->industries.SetSortFuncs(IndustryDirectoryWindow::sorter_funcs);
		this->industries.ForceRebuild();

		this->FinishInitNested(0);

		this->BuildSortIndustriesList();

		this->querystrings[WID_ID_FILTER] = &this->industry_editbox;
		this->industry_editbox.cancel_button = QueryString::ACTION_CLEAR;
	}

	/** Save the last sorting state. */
	~IndustryDirectoryWindow() override
	{
		this->last_sorting = this->industries.GetListing();
	}

	void OnInit() override
	{
		this->SetCargoFilterArray();
		this->hscroll->SetCount(0);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_ID_CAPTION:
				return GetString(STR_INDUSTRY_DIRECTORY_CAPTION, this->vscroll->GetCount(), Industry::GetNumItems());

			case WID_ID_DROPDOWN_CRITERIA:
				return GetString(IndustryDirectoryWindow::sorter_names[this->industries.SortType()]);

			case WID_ID_FILTER_BY_ACC_CARGO:
				return GetString(STR_INDUSTRY_DIRECTORY_ACCEPTED_CARGO_FILTER, this->GetCargoFilterLabel(this->accepted_cargo_filter_criteria));

			case WID_ID_FILTER_BY_PROD_CARGO:
				return GetString(STR_INDUSTRY_DIRECTORY_PRODUCED_CARGO_FILTER, this->GetCargoFilterLabel(this->produced_cargo_filter_criteria));

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_ID_DROPDOWN_ORDER:
				this->DrawSortButton(widget, this->industries.IsDescSortOrder());
				break;

			case WID_ID_INDUSTRY_LIST: {
				Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);

				/* Setup a clipping rectangle... */
				DrawPixelInfo tmp_dpi;
				if (!FillDrawPixelInfo(&tmp_dpi, ir)) return;
				/* ...but keep coordinates relative to the window. */
				tmp_dpi.left += ir.left;
				tmp_dpi.top += ir.top;

				AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);

				ir = ScrollRect(ir, *this->hscroll, 1);

				if (this->industries.empty()) {
					DrawString(ir, STR_INDUSTRY_DIRECTORY_NONE);
					break;
				}
				const CargoType acf_cargo_type = this->accepted_cargo_filter_criteria;
				auto [first, last] = this->vscroll->GetVisibleRangeIterators(this->industries);
				for (auto it = first; it != last; ++it) {
					ExtendedTextColour tc{TextColour::FromString};
					if (acf_cargo_type != CargoFilterCriteria::CF_ANY && acf_cargo_type != CargoFilterCriteria::CF_NONE) {
						Industry *ind = const_cast<Industry *>(*it);
						if (IndustryTemporarilyRefusesCargo(ind, acf_cargo_type)) {
							tc = ExtendedTextColour{TextColour::Grey, ExtendedTextColourFlag::Forced};
						}
					}
					DrawString(ir, this->GetIndustryString(*it), tc);

					ir.top += this->resize.step_height;
				}
				break;
			}
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_ID_DROPDOWN_ORDER: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->GetString());
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_ID_DROPDOWN_CRITERIA: {
				Dimension d = GetStringListBoundingBox(IndustryDirectoryWindow::sorter_names);
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_ID_INDUSTRY_LIST: {
				Dimension d = GetStringBoundingBox(STR_INDUSTRY_DIRECTORY_NONE);
				fill.height = resize.height = d.height;
				d.height *= 5;
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}
		}
	}

	DropDownList BuildCargoDropDownList() const
	{
		DropDownList list;

		/* Add item for disabling filtering. */
		list.push_back(MakeDropDownListStringItem(this->GetCargoFilterLabel(CargoFilterCriteria::CF_ANY), CargoFilterCriteria::CF_ANY));
		/* Add item for industries not producing anything, e.g. power plants */
		list.push_back(MakeDropDownListStringItem(this->GetCargoFilterLabel(CargoFilterCriteria::CF_NONE), CargoFilterCriteria::CF_NONE));

		/* Add cargos */
		Dimension d = GetLargestCargoIconSize();
		for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
			list.push_back(MakeDropDownListIconItem(d, cs->GetCargoIcon(), PAL_NONE, cs->name, cs->Index()));
		}

		return list;
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_ID_DROPDOWN_ORDER:
				this->industries.ToggleSortOrder();
				this->SetDirty();
				break;

			case WID_ID_DROPDOWN_CRITERIA:
				ShowDropDownMenu(this, IndustryDirectoryWindow::sorter_names, this->industries.SortType(), WID_ID_DROPDOWN_CRITERIA, 0, 0);
				break;

			case WID_ID_FILTER_BY_ACC_CARGO: { // Cargo filter dropdown
				static std::string acc_cargo_filter;
				ShowDropDownList(this, this->BuildCargoDropDownList(), this->accepted_cargo_filter_criteria, widget, 0, DropDownOption::Filterable, &acc_cargo_filter);
				break;
			}

			case WID_ID_FILTER_BY_PROD_CARGO: { // Cargo filter dropdown
				static std::string prod_cargo_filter;
				ShowDropDownList(this, this->BuildCargoDropDownList(), this->produced_cargo_filter_criteria, widget, 0, DropDownOption::Filterable, &prod_cargo_filter);
				break;
			}

			case WID_ID_INDUSTRY_LIST: {
				auto it = this->vscroll->GetScrolledItemFromWidget(this->industries, pt.y, this, WID_ID_INDUSTRY_LIST, WidgetDimensions::scaled.framerect.top);
				if (it != this->industries.end()) {
					if (_ctrl_pressed) {
						ShowExtraViewportWindow((*it)->location.tile);
					} else {
						ScrollMainWindowToTile((*it)->location.tile);
					}
				}
				break;
			}
		}
	}

	void OnDropdownSelect(WidgetID widget, int index, int) override
	{
		switch (widget) {
			case WID_ID_DROPDOWN_CRITERIA: {
				if (this->industries.SortType() != index) {
					this->industries.SetSortType(index);
					this->BuildSortIndustriesList();
				}
				break;
			}

			case WID_ID_FILTER_BY_ACC_CARGO: {
				this->SetAcceptedCargoFilter(static_cast<CargoType>(index));
				this->BuildSortIndustriesList();
				break;
			}

			case WID_ID_FILTER_BY_PROD_CARGO: {
				this->SetProducedCargoFilter(static_cast<CargoType>(index));
				this->BuildSortIndustriesList();
				break;
			}
		}
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_ID_INDUSTRY_LIST, WidgetDimensions::scaled.framerect.Vertical());
		this->hscroll->SetCapacityFromWidget(this, WID_ID_INDUSTRY_LIST, WidgetDimensions::scaled.framerect.Horizontal());
	}

	void OnEditboxChanged(WidgetID wid) override
	{
		if (wid == WID_ID_FILTER) {
			this->string_filter.SetFilterTerm(this->industry_editbox.text.GetText());
			this->InvalidateData(IDIWD_FORCE_REBUILD);
		}
	}

	void OnPaint() override
	{
		if (this->industries.NeedRebuild()) this->BuildSortIndustriesList();
		this->hscroll->SetCount(this->GetIndustryListWidth());
		this->DrawWidgets();
	}

	/** Rebuild the industry list on a regular interval. */
	const IntervalTimer<TimerWindow> rebuild_interval = {std::chrono::seconds(3), [this](auto) {
		this->industries.ForceResort();
		this->BuildSortIndustriesList();
	}};

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		switch (data) {
			case IDIWD_FORCE_REBUILD:
				/* This needs to be done in command-scope to enforce rebuilding before resorting invalid data */
				this->industries.ForceRebuild();
				break;

			case IDIWD_PRODUCTION_CHANGE:
				if (this->industries.SortType() == 2) this->industries.ForceResort();
				break;

			default:
				this->industries.ForceResort();
				break;
		}
	}

	static inline HotkeyList hotkeys {"industrydirectory", {
		Hotkey('F', "focus_filter_box", WID_ID_FILTER),
	}};
};

Listing IndustryDirectoryWindow::last_sorting = {false, 0};

/* Available station sorting functions. */
const std::initializer_list<GUIIndustryList::SortFunction * const> IndustryDirectoryWindow::sorter_funcs = {
	&IndustryNameSorter,
	&IndustryTypeSorter,
	&IndustryProductionSorter,
	&IndustryTransportedCargoSorter
};

CargoType IndustryDirectoryWindow::produced_cargo_filter = CargoFilterCriteria::CF_ANY;


/** Window definition of the industry directory gui */
static WindowDesc _industry_directory_desc(
	WindowPosition::Automatic, "list_industries", 428, 190,
	WindowClass::IndustryDirectory, WindowClass::None,
	{},
	_nested_industry_directory_widgets,
	&IndustryDirectoryWindow::hotkeys
);

void ShowIndustryDirectory()
{
	AllocateWindowDescFront<IndustryDirectoryWindow>(_industry_directory_desc, 0);
}

/** Widgets of the industry cargoes window. */
static constexpr std::initializer_list<NWidgetPart> _nested_industry_cargoes_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_IC_CAPTION),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_DEFSIZEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::Brown, WID_IC_PANEL), SetResize(1, 10), SetScrollbar(WID_IC_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::Brown, WID_IC_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, Colours::Brown, WID_IC_NOTIFY),
			SetStringTip(STR_INDUSTRY_CARGOES_NOTIFY_SMALLMAP, STR_INDUSTRY_CARGOES_NOTIFY_SMALLMAP_TOOLTIP),
		NWidget(WWT_PANEL, Colours::Brown), SetFill(1, 0), SetResize(0, 0), EndContainer(),
		NWidget(WWT_DROPDOWN, Colours::Brown, WID_IC_IND_DROPDOWN), SetFill(0, 0), SetResize(0, 0),
				SetStringTip(STR_INDUSTRY_CARGOES_SELECT_INDUSTRY, STR_INDUSTRY_CARGOES_SELECT_INDUSTRY_TOOLTIP),
		NWidget(WWT_DROPDOWN, Colours::Brown, WID_IC_CARGO_DROPDOWN), SetFill(0, 0), SetResize(0, 0),
				SetStringTip(STR_INDUSTRY_CARGOES_SELECT_CARGO, STR_INDUSTRY_CARGOES_SELECT_CARGO_TOOLTIP),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

/** Window description for the industry cargoes window. */
static WindowDesc _industry_cargoes_desc(
	WindowPosition::Automatic, "industry_cargoes", 300, 210,
	WindowClass::IndustryCargoes, WindowClass::None,
	{},
	_nested_industry_cargoes_widgets
);

/** Base cargo chain field. */
class ChainField {
public:
	static constexpr uint MAX_CARGOES = 16; ///< Maximum number of cargoes carried in a field.

	using CargoSlotMask = uint16_t; ///< Type present a mask of cargo slots.
	static_assert(std::numeric_limits<CargoSlotMask>::digits >= MAX_CARGOES);

	static_assert(MAX_CARGOES >= std::tuple_size_v<decltype(IndustrySpec::produced_cargo)>);
	static_assert(MAX_CARGOES >= std::tuple_size_v<decltype(IndustrySpec::accepts_cargo)>);

	static constexpr PixelColour CARGO_LINE_COLOUR = PC_BLACK; ///< Line colour around the cargo.

	static inline FontSize fontsize = FontSize::Normal; ///< Font size of industry chain strings.

	static inline CargoTypes town_accepts; ///< Mask of cargo types accepted by towns.
	static inline CargoTypes town_produces; ///< Mask of cargo types produced by towns.

	static inline int vert_inter_industry_space; ///< Amount of space between two industries in a column.
	static inline int blob_distance; ///< Distance of the industry legend colour from the edge of the industry box.

	static inline Dimension legend; ///< Dimension of the legend blob.
	static inline Dimension cargo_border; ///< Dimensions of border between cargo lines and industry boxes.
	static inline Dimension cargo_line; ///< Dimensions of cargo lines.
	static inline Dimension cargo_space; ///< Dimensions of space between cargo lines.
	static inline Dimension cargo_stub; ///< Dimensions of cargo stub (unconnected cargo line.)

	static inline int small_height; ///< Height of the header row.
	static inline int normal_height; ///< Height of the non-header rows.
	static inline int connection_field_width; ///< Width of a cargo connection field.
	static inline int industry_width; ///< Width of an industry field.
	static inline uint max_cargoes; ///< Largest number of cargoes actually on any industry.

	/**
	 * Get the height of all cargo connections in a row.
	 * @return The height of all cargo connections.
	 */
	static uint GetConnectionHeight()
	{
		return ChainField::max_cargoes * (ChainField::cargo_line.height + ChainField::cargo_space.height) - ChainField::cargo_space.height;
	}

	virtual ~ChainField() = default;

	/**
	 * Get this field as a specific type implemention.
	 * @tparam T The type of chain field.
	 * @return The field if is of the requested type.
	 */
	template <typename T> T *Get() { return dynamic_cast<T *>(this); }

	/**
	 * Get this field as a specific type implemention.
	 * @tparam T The type of chain field.
	 * @return The field if is of the requested type.
	 */
	template <typename T> const T *Get() const { return dynamic_cast<const T *>(this); }

	/**
	 * Get the width of this field.
	 * @return THe width.
	 */
	virtual int Width() const { return ChainField::industry_width; }

	/**
	 * Get the height of this field.
	 * @return The height.
	 */
	virtual int Height() const { return ChainField::normal_height; }

	/**
	 * Draw the field.
	 * @param r Rect to draw within.
	 */
	virtual void Draw([[maybe_unused]] Rect r) = 0;

	/** Result type of testing clicked position. */
	using ClickedAtResult = std::variant<std::monostate, HouseID, IndustryType, CargoType>;

	/**
	 * Decide which industry or cargo was clicked at.
	 * @param r Rect of this cargo field.
	 * @param pt Click position in the cargo field.
	 * @return Industry or cargo clicked at.
	 */
	virtual ClickedAtResult ClickedAt([[maybe_unused]] Rect r, [[maybe_unused]] Point pt) const { return {}; }
};

/** Field representing a header label. */
class HeaderChainField : public ChainField {
public:
	StringID header; ///< Header string.

	/**
	 * Construct a new Header chain field.
	 * @param header The header string.
	 */
	HeaderChainField(StringID header) : header(header) {}

	int Height() const override;
	void Draw(Rect r) override;
};

int HeaderChainField::Height() const
{
	return ChainField::small_height;
}

void HeaderChainField::Draw(Rect r)
{
	DrawStringMultiLine(r, this->header, TextColour::White, {AlignmentH::Centre, AlignmentV::Middle}, false, ChainField::fontsize);
}

/** Field representing cargo connections. */
class ConnectionChainField : public ChainField {
public:
	std::array<CargoType, ChainField::MAX_CARGOES> vertical_cargoes; ///< Cargoes running from top to bottom (cargo type or #INVALID_CARGO).
	CargoSlotMask supp_cargoes = 0; ///< Bitmask of cargoes in \c vertical_cargoes entering from the left.
	CargoSlotMask cust_cargoes = 0; ///< Bitmask of cargoes in \c vertical_cargoes leaving to the right.
	CargoSlotMask skip_cargoes = 0; ///< Stop at the top of the vertical cargoes.
	CargoSlotMask top_end = 0; ///< Stop at the top of the vertical cargoes.
	CargoSlotMask bottom_end = 0; ///< Stop at the bottom of the vertical cargoes.
	uint8_t num_cargoes; ///< Number of cargoes.

	ConnectionChainField(CargoTypes cargo_types);
	int Width() const override;
	void Draw(Rect r) override;
	int ConnectCargo(CargoType cargo, bool producer);
	CargoSlotMask GetConnections(bool accepting, bool supplying) const;
	ClickedAtResult ClickedAt(Rect r, Point pt) const override;
};

/** Field representing something that accepts and produces cargo. */
class AcceptsProducesChainField : public ChainField {
public:
	std::array<CargoType, ChainField::MAX_CARGOES> other_produced; ///< Cargoes produced but not used in this figure.
	std::array<CargoType, ChainField::MAX_CARGOES> other_accepted; ///< Cargoes accepted but not used in this figure.

	AcceptsProducesChainField();
	void Draw(Rect r) override;
	ClickedAtResult ClickedAt(Rect r, Point pt) const override;

	/**
	 * Connected the cargo types produced by this field.
	 * @return The produced cargo types.
	 */
	virtual CargoTypes GetProduced() = 0;

	/**
	 * Connected the cargo types accepted by this field.
	 * @return The accepted cargo types..
	 */
	virtual CargoTypes GetAccepted() = 0;
};

/**
 * Construct a new Accepts/Produces chain field.
 */
AcceptsProducesChainField::AcceptsProducesChainField()
{
	this->other_produced.fill(INVALID_CARGO);
	this->other_accepted.fill(INVALID_CARGO);
}

void AcceptsProducesChainField::Draw(Rect r)
{
	bool rtl = _current_text_dir == TD_RTL;

	/* Draw the other_produced/other_accepted cargoes. */
	std::span<const CargoType> other_right, other_left;
	if (rtl) {
		other_right = this->other_accepted;
		other_left = this->other_produced;
	} else {
		other_right = this->other_produced;
		other_left = this->other_accepted;
	}

	/* Draw the unconnected cargo stubs. */
	r = r.CentreToHeight(ChainField::GetConnectionHeight()).WithHeight(ChainField::cargo_line.height);
	for (uint i = 0; i < ChainField::max_cargoes; ++i) {
		if (IsValidCargoType(other_right[i])) {
			Rect r_stub = r.WithX(r.right + 1, r.right + ChainField::cargo_stub.width);
			GfxFillRect(r_stub, CARGO_LINE_COLOUR);
			GfxFillRect(r_stub.Shrink({0, WidgetDimensions::scaled.bevel.top, WidgetDimensions::scaled.bevel.right, WidgetDimensions::scaled.bevel.bottom}), CargoSpec::Get(other_right[i])->legend_colour);
		}

		if (IsValidCargoType(other_left[i])) {
			Rect r_stub = r.WithX(r.left - ChainField::cargo_stub.width, r.left - 1);
			GfxFillRect(r_stub, CARGO_LINE_COLOUR);
			GfxFillRect(r_stub.Shrink({WidgetDimensions::scaled.bevel.left, WidgetDimensions::scaled.bevel.top, 0, WidgetDimensions::scaled.bevel.bottom}), CargoSpec::Get(other_left[i])->legend_colour);
		}

		r = r.Translate(0, ChainField::cargo_line.height + ChainField::cargo_space.height);
	}
}

ChainField::ClickedAtResult AcceptsProducesChainField::ClickedAt(Rect r, Point pt) const
{
	/* Click is outside the rect, check the cargo stubs. */
	bool rtl = _current_text_dir == TD_RTL;

	std::span<const CargoType> other_right, other_left;
	if (rtl) {
		other_right = this->other_accepted;
		other_left = this->other_produced;
	} else {
		other_right = this->other_produced;
		other_left = this->other_accepted;
	}

	r = r.CentreToHeight(ChainField::GetConnectionHeight()).WithHeight(ChainField::cargo_line.height);
	for (uint i = 0; i < ChainField::max_cargoes; ++i) {
		if (IsValidCargoType(other_right[i])) {
			Rect r_stub = r.WithX(r.right + 1, r.right + ChainField::cargo_stub.width);
			if (r_stub.Contains(pt)) return other_right[i];
		}

		if (IsValidCargoType(other_left[i])) {
			Rect r_stub = r.WithX(r.left - ChainField::cargo_stub.width, r.left - 1);
			if (r_stub.Contains(pt)) return other_left[i];
		}

		r = r.Translate(0, ChainField::cargo_line.height + ChainField::cargo_space.height);
	}

	return {};
}

/** Field representing an industry. */
class IndustryChainField : public AcceptsProducesChainField {
public:
	IndustryType industry_type; ///< Industry type (#NUM_INDUSTRYTYPES means 'houses').
	Colours colour; ///< Colour for this industry.

	IndustryChainField(IndustryType industry_type);
	void Draw(Rect r) override;
	ClickedAtResult ClickedAt(Rect r, Point pt) const override;
	CargoTypes GetProduced() override;
	CargoTypes GetAccepted() override;
};

/**
 * Construct a new Industry chain field.
 * @param industry_type The industry type.
 */
IndustryChainField::IndustryChainField(IndustryType industry_type) : industry_type(industry_type)
{
	const IndustrySpec *indsp = GetIndustrySpec(this->industry_type);
	if (indsp->life_type.Test(IndustryLifeType::Extractive)) {
		this->colour = Colours::LightBlue;
	} else if (indsp->life_type.Test(IndustryLifeType::Processing)) {
		this->colour = Colours::Brown;
	} else if (indsp->life_type.Test(IndustryLifeType::Organic)) {
		this->colour = Colours::PaleGreen;
	} else {
		/* Black hole industry */
		this->colour = Colours::DarkGreen;
	}
}

void IndustryChainField::Draw(Rect r)
{
	const IndustrySpec *indsp = GetIndustrySpec(this->industry_type);
	DrawFrameRect(r, this->colour, {});
	DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.frametext), indsp->name, TextColour::White, {AlignmentH::Centre, AlignmentV::Middle}, false, ChainField::fontsize);

	Rect blob = r.Shrink(blob_distance).CentreToWidth(ChainField::legend.width).WithHeight(ChainField::legend.height, true);
	GfxFillRect(blob, PC_BLACK);
	GfxFillRect(blob.Shrink(WidgetDimensions::scaled.bevel), indsp->map_colour);

	this->AcceptsProducesChainField::Draw(r);
}

ChainField::ClickedAtResult IndustryChainField::ClickedAt(Rect r, Point pt) const
{
	if (r.Contains(pt)) return this->industry_type;
	return this->AcceptsProducesChainField::ClickedAt(r, pt);
}

CargoTypes IndustryChainField::GetProduced()
{
	const IndustrySpec *indsp = GetIndustrySpec(this->industry_type);
	return CargoTypes(indsp->produced_cargo);
}

CargoTypes IndustryChainField::GetAccepted()
{
	const IndustrySpec *indsp = GetIndustrySpec(this->industry_type);
	return CargoTypes(indsp->accepts_cargo);
}

/** Field representing a house. */
class HouseChainField : public AcceptsProducesChainField {
public:
	void Draw(Rect r) override;
	ClickedAtResult ClickedAt(Rect r, Point pt) const override;
	CargoTypes GetProduced() override;
	CargoTypes GetAccepted() override;
};

void HouseChainField::Draw(Rect r)
{
	DrawFrameRect(r, Colours::Grey, {});
	DrawStringMultiLine(r.Shrink(WidgetDimensions::scaled.frametext), STR_INDUSTRY_CARGOES_HOUSES, TextColour::White, {AlignmentH::Centre, AlignmentV::Middle}, false, ChainField::fontsize);

	this->AcceptsProducesChainField::Draw(r);
}

ChainField::ClickedAtResult HouseChainField::ClickedAt(Rect r, Point pt) const
{
	if (r.Contains(pt)) return HouseID{};
	return this->AcceptsProducesChainField::ClickedAt(r, pt);
}

CargoTypes HouseChainField::GetProduced()
{
	return ChainField::town_produces;
}

CargoTypes HouseChainField::GetAccepted()
{
	return ChainField::town_accepts;
}

/** Field presenting cargo names. */
class CargoChainField : public ChainField {
public:
	std::array<CargoType, ChainField::MAX_CARGOES> cargo_types; ///< Cargoes to display (or #INVALID_CARGO).
	Alignment align; ///< The text alignment.

	CargoChainField(std::span<const CargoType> cargo_types, Alignment align);
	void Draw(Rect r) override;
	ClickedAtResult ClickedAt(Rect r, Point pt) const override;
};

/**
 * Construct a new cargo label chain field.
 * @param cargo_types The cargo types of this label field.
 * @param align The text alignment.
 */
CargoChainField::CargoChainField(std::span<const CargoType> cargo_types, Alignment align) : align(align)
{
	assert(std::size(cargo_types) <= std::size(this->cargo_types));

	auto r = std::ranges::copy(cargo_types, std::begin(this->cargo_types));
	std::fill(r.out, std::end(this->cargo_types), INVALID_CARGO);
}

void CargoChainField::Draw(Rect r)
{
	r = r.CentreToHeight(ChainField::GetConnectionHeight() + GetCharacterHeight(ChainField::fontsize) - ChainField::cargo_line.height);

	for (uint i = 0; i < ChainField::MAX_CARGOES; i++) {
		if (IsValidCargoType(this->cargo_types[i])) {
			DrawString(r.Shrink(WidgetDimensions::scaled.framerect, RectPadding::zero), CargoSpec::Get(this->cargo_types[i])->name, TextColour::White, this->align, false, ChainField::fontsize);
		}
		r = r.Translate(0, ChainField::cargo_line.height + ChainField::cargo_space.height);
	}
}

ChainField::ClickedAtResult CargoChainField::ClickedAt(Rect r, Point pt) const
{
	r = r.CentreToHeight(ChainField::GetConnectionHeight() + GetCharacterHeight(ChainField::fontsize) - ChainField::cargo_line.height);
	r = r.WithHeight(GetCharacterHeight(ChainField::fontsize), false);

	for (CargoType cargo_type : this->cargo_types) {
		if (!IsValidCargoType(cargo_type)) break;
		if (r.Contains(pt)) return cargo_type;
		r = r.Translate(0, ChainField::cargo_line.height + ChainField::cargo_space.height);
	}

	return {};
}

/**
 * Construct a new cargo connection chain field.
 * @param cargo_types The cargo types.
 */
ConnectionChainField::ConnectionChainField(CargoTypes cargo_types)
{
	assert(cargo_types.Count() <= std::size(this->vertical_cargoes));

	auto it = this->vertical_cargoes.begin();
	for (CargoType cargo_type : cargo_types) *it++ = cargo_type;

	this->num_cargoes = static_cast<uint8_t>(std::distance(std::begin(this->vertical_cargoes), it));

	std::sort(std::begin(this->vertical_cargoes), it, CargoTypeComparator{});
	std::fill(it, std::end(this->vertical_cargoes), INVALID_CARGO);
}

int ConnectionChainField::Width() const
{
	return ChainField::connection_field_width;
}

void ConnectionChainField::Draw(Rect r)
{
	if (this->skip_cargoes == UINT16_MAX) return;

	int col_step = ChainField::cargo_line.width + ChainField::cargo_space.width;
	int row_step = ChainField::cargo_line.height + ChainField::cargo_space.height;
	uint width = this->num_cargoes * col_step - ChainField::cargo_space.width;
	Rect col_base = r.CentreToWidth(width).WithWidth(ChainField::cargo_line.width, false);
	Rect row_base = r.CentreToHeight(ChainField::GetConnectionHeight()).WithHeight(ChainField::cargo_line.height, false);

	uint16_t hor_left, hor_right;
	if (_current_text_dir == TD_RTL) {
		hor_left = this->cust_cargoes;
		hor_right = this->supp_cargoes;
	} else {
		hor_left = this->supp_cargoes;
		hor_right = this->cust_cargoes;
	}

	/* Draw columns */
	for (int i = 0; i < this->num_cargoes; ++i) {
		if (!HasBit(this->skip_cargoes, i)) {
			Rect col = col_base;
			RectPadding col_padding = WidgetDimensions::scaled.bevel;
			if (HasBit(this->top_end, i)) {
				col.top = row_base.top;
			} else {
				col_padding.top = 0;
				col.top -= ChainField::vert_inter_industry_space / 2;
			}
			if (HasBit(this->bottom_end, i)) {
				col.bottom = row_base.bottom;
			} else {
				col_padding.bottom = 0;
				col.bottom += ChainField::vert_inter_industry_space / 2;
			}

			GfxFillRect(col, CARGO_LINE_COLOUR);

			if (HasBit(hor_left, i)) GfxFillRect(row_base.WithX(row_base.left, col_base.left), CARGO_LINE_COLOUR);
			if (HasBit(hor_right, i)) GfxFillRect(row_base.WithX(col_base.right, row_base.right), CARGO_LINE_COLOUR);

			PixelColour pc = CargoSpec::Get(this->vertical_cargoes[i])->legend_colour;
			GfxFillRect(col.Shrink(col_padding), pc);

			if (HasBit(hor_left, i)) GfxFillRect(row_base.WithX(row_base.left, col_base.left + WidgetDimensions::scaled.bevel.left - 1).Shrink(RectPadding::zero, WidgetDimensions::scaled.bevel), pc);
			if (HasBit(hor_right, i)) GfxFillRect(row_base.WithX(col_base.right - WidgetDimensions::scaled.bevel.left + 1, row_base.right).Shrink(RectPadding::zero, WidgetDimensions::scaled.bevel), pc);
		}

		col_base = col_base.Translate(col_step, 0);
		row_base = row_base.Translate(0, row_step);
	}
}

/**
 * Connect a cargo to the cargo column.
 * @param cargo Cargo to connect.
 * @param producer Cargo is produced (if \c false, cargo is assumed to be accepted).
 * @return Horizontal connection index, or \c -1 if not connected at all.
 */
int ConnectionChainField::ConnectCargo(CargoType cargo, bool producer)
{
	assert(IsValidCargoType(cargo));

	/* Find the vertical cargo column carrying the cargo. */
	auto it = std::ranges::find(this->vertical_cargoes, cargo);
	if (it == this->vertical_cargoes.end()) return -1;

	int column = static_cast<int>(std::distance(this->vertical_cargoes.begin(), it));

	if (producer) {
		assert(!HasBit(this->supp_cargoes, column));
		SetBit(this->supp_cargoes, column);
	} else {
		assert(!HasBit(this->cust_cargoes, column));
		SetBit(this->cust_cargoes, column);
	}

	return column;
}

/**
 * Get the cargo connections.
 * @param accepting Include accepting connections.
 * @param supplying Include supplying connections.
 * @return The requested cargo connections.
 */
ChainField::CargoSlotMask ConnectionChainField::GetConnections(bool accepting, bool supplying) const
{
	CargoSlotMask mask = 0;
	if (accepting) mask |= this->supp_cargoes;
	if (supplying) mask |= this->cust_cargoes;
	return mask;
}

ChainField::ClickedAtResult ConnectionChainField::ClickedAt(Rect r, Point pt) const
{
	if (this->skip_cargoes == UINT16_MAX) return {};

	int col_step = ChainField::cargo_line.width + ChainField::cargo_space.width;
	int row_step = ChainField::cargo_line.height + ChainField::cargo_space.height;
	uint width = this->num_cargoes * col_step - ChainField::cargo_space.width;
	Rect col_base = r.CentreToWidth(width).WithWidth(ChainField::cargo_line.width, false);
	Rect row_base = r.CentreToHeight(ChainField::GetConnectionHeight()).WithHeight(ChainField::cargo_line.height, false);

	uint16_t hor_left, hor_right;
	if (_current_text_dir == TD_RTL) {
		hor_left = this->cust_cargoes;
		hor_right = this->supp_cargoes;
	} else {
		hor_left = this->supp_cargoes;
		hor_right = this->cust_cargoes;
	}

	col_base = col_base.Translate(this->num_cargoes * col_step, 0);
	row_base = row_base.Translate(0, this->num_cargoes * row_step);

	/* Work backwards as higher slots are drawn last. */
	for (int i = static_cast<int>(this->num_cargoes) - 1; i >= 0; --i) {
		col_base = col_base.Translate(-col_step, 0);
		row_base = row_base.Translate(0, -row_step);

		if (HasBit(this->skip_cargoes, i)) continue;

		Rect col = col_base;
		if (HasBit(this->top_end, i)) {
			col.top = row_base.top;
		} else {
			col.top -= ChainField::vert_inter_industry_space / 2;
		}
		if (HasBit(this->bottom_end, i)) {
			col.bottom = row_base.bottom;
		} else {
			col.bottom += ChainField::vert_inter_industry_space / 2;
		}

		CargoType cargo_type = this->vertical_cargoes[i];

		if (col.Contains(pt)) return cargo_type;
		if (HasBit(hor_left, i) && row_base.WithX(row_base.left, col_base.left).Contains(pt)) return cargo_type;
		if (HasBit(hor_right, i) && row_base.WithX(col_base.right, row_base.right).Contains(pt)) return cargo_type;
	}

	return {};
}

/** A single row of #ChainField. */
class ChainRow {
public:
	static const int MAX_COLUMNS = 5; ///< Maximum number of columns in a row.
	std::array<std::unique_ptr<ChainField>, MAX_COLUMNS> columns{}; ///< One row of fields.

	/**
	 * Test if a given column index is valid.
	 * @param column The column index.
	 * @return \c true iff the column index is valid.
	 */
	static bool IsValidColumn(int column)
	{
		return column >= 0 && column < static_cast<int>(std::tuple_size_v<decltype(ChainRow::columns)>);
	}

	/**
	 * Get the height of this row of fields.
	 * @return The height of this row.
	 */
	int Height() const
	{
		int height = 0;
		for (const std::unique_ptr<ChainField> &fld : this->columns) {
			if (fld == nullptr) continue;
			height = std::max(height, fld->Height());
		}
		return height;
	}

	/**
	 * Draw this row of fields.
	 * @param r Rect to draw within.
	 */
	void Draw(Rect r) const
	{
		bool rtl = _current_text_dir == TD_RTL;
		for (int col = 0; col < MAX_COLUMNS; ++col) {
			int width = (col & 1) ? ChainField::connection_field_width : ChainField::industry_width;
			if (this->columns[col] != nullptr) this->columns[col]->Draw(r.WithWidth(width, rtl));
			r = r.Indent(width, rtl);
		}
	}

	/**
	 * Get the ClickedAt result for a field in this row.
	 * @param r The rect of the row.
	 * @param pt The click position.
	 * @return \c ChainField::ClickedAtResult
	 */
	ChainField::ClickedAtResult ClickedAt(Rect r, Point pt) const
	{
		bool rtl = _current_text_dir == TD_RTL;

		for (int col = 0; col < MAX_COLUMNS; ++col) {
			int width = (col & 1) ? ChainField::connection_field_width : ChainField::industry_width;
			Rect r_col = r.WithWidth(width, rtl);

			if (IsValidColumn(col) && this->columns[col] != nullptr) {
				auto result = this->columns[col]->ClickedAt(r_col, pt);
				if (!std::holds_alternative<std::monostate>(result)) return result;
			}

			r = r.Indent(width, rtl);
		}

		return {};
	}

	/**
	 * Connect produced cargoes to the connection column after it.
	 * @param column Column of the industry or house.
	 */
	void ConnectProducedCargo(int column)
	{
		assert(IsValidColumn(column));
		AcceptsProducesChainField *ind_fld = this->columns[column]->Get<AcceptsProducesChainField>();
		ConnectionChainField *conn_fld = IsValidColumn(column + 1) ? this->columns[column + 1]->Get<ConnectionChainField>() : nullptr;
		assert(ind_fld != nullptr);

		ind_fld->other_produced.fill(INVALID_CARGO);
		CargoTypes others = ind_fld->GetProduced();

		ChainField::CargoSlotMask used_slots{};
		if (conn_fld != nullptr) {
			for (CargoType cargo_type : others) {
				if (conn_fld->ConnectCargo(cargo_type, true) >= 0) others.Reset(cargo_type);
			}
			used_slots = conn_fld->supp_cargoes;
		}

		/* Allocate other cargoes in the empty holes of the horizontal cargo connections. */
		auto it = others.begin();
		auto last = others.end();
		for (uint i = 0; i != ChainField::max_cargoes && it != last; ++i) {
			if (HasBit(used_slots, i)) continue;
			ind_fld->other_produced[i] = *it;
			++it;
		}
	}

	/**
	 * Construct a Cargo chain field.
	 * @param column Column for the new field.
	 * @param accepting Display accepted cargo (if \c false, display produced cargo).
	 */
	void MakeCargoLabel(int column, bool accepting)
	{
		assert(IsValidColumn(column) && IsValidColumn(accepting ? column - 1 : column + 1));
		assert(this->columns[column] == nullptr);

		std::array<CargoType, ChainField::ChainField::MAX_CARGOES> cargo_types;
		cargo_types.fill(INVALID_CARGO);

		ConnectionChainField *conn_fld = this->columns[accepting ? column - 1 : column + 1]->Get<ConnectionChainField>();
		assert(conn_fld != nullptr);

		for (uint i = 0; i < conn_fld->num_cargoes; i++) {
			int col = conn_fld->ConnectCargo(conn_fld->vertical_cargoes[i], !accepting);
			if (col >= 0) cargo_types[col] = conn_fld->vertical_cargoes[i];
		}

		this->columns[column] = std::make_unique<CargoChainField>(cargo_types, accepting ? AlignmentH::Start : AlignmentH::End);
	}

	/**
	 * Connect accepted cargoes to the connection column before it.
	 * @param column Column of the industry or house.
	 */
	void ConnectAcceptedCargo(int column)
	{
		assert(IsValidColumn(column));
		AcceptsProducesChainField *ind_fld = this->columns[column]->Get<AcceptsProducesChainField>();
		ConnectionChainField *conn_fld = IsValidColumn(column - 1) ? this->columns[column - 1]->Get<ConnectionChainField>() : nullptr;
		assert(ind_fld != nullptr);

		ind_fld->other_accepted.fill(INVALID_CARGO);
		CargoTypes others = ind_fld->GetAccepted();

		ChainField::CargoSlotMask used_slots{};
		if (conn_fld != nullptr) {
			for (CargoType cargo_type : others) {
				if (conn_fld->ConnectCargo(cargo_type, false) >= 0) others.Reset(cargo_type);
			}
			used_slots = conn_fld->cust_cargoes;
		}

		/* Allocate other cargoes in the empty holes of the horizontal cargo connections. */
		auto it = others.begin();
		auto last = others.end();
		for (uint i = 0; i != ChainField::max_cargoes && it != last; ++i) {
			if (HasBit(used_slots, i)) continue;
			ind_fld->other_accepted[i] = *it;
			++it;
		}
	}
};

/**
 * Get the maximal size for cargo names.
 * @param fs The font size.
 * @return The maximal size.
 */
static Dimension GetMaximalSizeCargoString(FontSize fs = FontSize::Normal)
{
	auto op = [fs](const Dimension &d, const CargoSpec *cs) { return maxdim(d, GetStringBoundingBox(cs->name, fs)); };
	return std::accumulate(_sorted_cargo_specs.begin(), _sorted_cargo_specs.end(), Dimension{}, op);
}

/**
 * Get the maximal size for industry type names.
 * @param fs The font size.
 * @return The maximal size.
 */
static Dimension GetMaximalSizeIndustryString(FontSize fs = FontSize::Normal)
{
	auto op = [fs](const Dimension &d, IndustryType it) { return maxdim(d, GetStringBoundingBox(GetIndustrySpec(it)->name, fs)); };
	return std::accumulate(_sorted_industry_types.begin(), _sorted_industry_types.end(), Dimension{}, op);
}

/**
 * Window displaying the cargo connections around an industry (or cargo).
 *
 * The main display is constructed from 'fields', rectangles that contain an industry, piece of the cargo connection, cargo labels, or headers.
 * For a nice display, the following should be kept in mind:
 * - A \c HeaderChainField is always at the top of an column of \c AcceptsProducesChainField fields.
 * - A \c CargoChainField field is also always put in a column of \c AcceptsProducesChainField fields.
 * - The top row contains \c HeaderChainField and empty fields.
 * - Cargo connections have a column of their own, made up of \c ConnectionChainField fields.
 * - Cargo accepted or produced by an industry/house, but not carried in a cargo connection, is drawn in the space of a cargo column attached to the industry/house.
 *   The information however is part of the industry/house.
 *
 * This results in the following invariants:
 * - Width of a \c AcceptsProducesChainField column is large enough to hold all industry type labels, all cargo labels, and all header texts.
 * - Height of a \c AcceptsProducesChainField is large enough to hold a header line, or a industry type line, \c ChainField::MAX_CARGOES cargo labels
 *   (where \c ChainField::MAX_CARGOES is the maximum number of cargoes connected between industries), \c ChainField::MAX_CARGOES connections of cargo types, and space
 *   between two industry types (1/2 above it, and 1/2 underneath it).
 * - Width of a \c ConnectionChainField is large enough to hold \c ChainField::MAX_CARGOES vertical columns (one for each type of cargo).
 *   Also, space is needed between an industry and the leftmost/rightmost column to draw the non-carried cargoes.
 * - Height of a \c ConnectionChainField field is equally high as the height of the \c AcceptsProducesChainField.
 * - A \c HeaderChainField or empty field at the top match the width of the fields below them, the height should be sufficient to display the header text.
 *
 * When displaying the cargoes around an industry type, five columns are needed (supplying industries, accepted cargoes, the industry,
 * produced cargoes, customer industries). Displaying the industries around a cargo needs three columns (supplying industries, the cargo,
 * customer industries). The remaining two columns are empty and unused.
 */
struct IndustryCargoesWindow : public Window {
	std::vector<ChainRow> rows{}; ///< Fields to display in the #WID_IC_PANEL.
	std::variant<IndustryType, CargoType, HouseID> ind_cargo; ///< The displayed industry or cargo type.
	Dimension cargo_textsize{}; ///< Size to hold any cargo text, as well as STR_INDUSTRY_CARGOES_SELECT_CARGO.
	Dimension ind_textsize{}; ///< Size to hold any industry type text, as well as STR_INDUSTRY_CARGOES_SELECT_INDUSTRY.
	Scrollbar *vscroll = nullptr;

	IndustryCargoesWindow(int id) : Window(_industry_cargoes_desc)
	{
		this->OnInit();
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_IC_SCROLLBAR);
		this->FinishInitNested(0);
		this->OnInvalidateData(id);
	}

	/**
	 * Count the maximal number of cargo types handled by all industry types.
	 * @return Number of cargo types handled by industry types.
	 */
	uint CountIndustryCargoTypes()
	{
		uint max_cargoes = 0;
		for (IndustryType it = 0; it < NUM_INDUSTRYTYPES; it++) {
			const IndustrySpec *indsp = GetIndustrySpec(it);
			if (!indsp->enabled) continue;
			max_cargoes = std::max<uint>(max_cargoes, std::ranges::count_if(indsp->accepts_cargo, IsValidCargoType));
			max_cargoes = std::max<uint>(max_cargoes, std::ranges::count_if(indsp->produced_cargo, IsValidCargoType));
		}
		return max_cargoes;
	}

	/**
	 * Count the maximal number of cargo types handled by all houses.
	 * Updates the accept and produce cargo type masks handled by houses.
	 * @return Number of cargo types handled by houses.
	 */
	uint CountHouseCargoTypes() const
	{
		HouseZones climate_mask = GetClimateMaskForLandscape();

		ChainField::town_accepts.Reset();
		ChainField::town_produces.Reset();

		/* Count cargoes accepted by houses. Houses are single field, so we need the total across all house types. */
		for (const HouseSpec &hs : HouseSpec::Specs()) {
			if (!hs.enabled || !hs.building_availability.Any(climate_mask)) continue;
			ChainField::town_accepts.Set({hs.accepts_cargo});
		}

		/* Count cargoes produced by town effects. */
		for (const CargoSpec *cs : _sorted_cargo_specs) {
			if (cs->town_production_effect != TownProductionEffect::None) ChainField::town_produces.Set(cs->Index());
		}

		return std::max(ChainField::town_accepts.Count(), ChainField::town_produces.Count());
	}

	void OnInit() override
	{
		/* Initialize static CargoesField size variables. */
		Dimension d = GetStringBoundingBox(STR_INDUSTRY_CARGOES_SOURCES, ChainField::fontsize);
		d = maxdim(d, GetStringBoundingBox(STR_INDUSTRY_CARGOES_DESTINATIONS, ChainField::fontsize));
		ChainField::small_height = d.height + WidgetDimensions::scaled.frametext.Vertical();

		/* Size of the legend blob -- same size as the smallmap legend blob. */
		ChainField::legend.height = GetCharacterHeight(FontSize::Small) - ScaleGUITrad(1);
		ChainField::legend.width = GetCharacterHeight(FontSize::Small) * 9 / 6;

		/* Size of cargo lines. */
		ChainField::cargo_line.width = ScaleGUITrad(6);
		ChainField::cargo_line.height = ChainField::cargo_line.width;

		/* Size of border between cargo lines and industry boxes. */
		ChainField::cargo_border.width = ChainField::cargo_line.width * 3 / 2;
		ChainField::cargo_border.height = ChainField::cargo_line.width / 2;

		/* Size of space between cargo lines. */
		ChainField::cargo_space.width = ChainField::cargo_line.width / 2;
		ChainField::cargo_space.height = std::max<uint>(GetCharacterHeight(ChainField::fontsize) + WidgetDimensions::scaled.vsep_normal - ChainField::cargo_line.height, ChainField::cargo_line.height / 2);

		/* Size of cargo stub (unconnected cargo line.) */
		ChainField::cargo_stub.width = ChainField::cargo_line.width * 2 / 3;
		ChainField::cargo_stub.height = ChainField::cargo_line.height; /* Unused */

		ChainField::vert_inter_industry_space = WidgetDimensions::scaled.vsep_wide;
		ChainField::blob_distance = WidgetDimensions::scaled.hsep_normal;

		/* Get the number of cargo types that need to be displayed. */
		ChainField::max_cargoes = std::max<uint>(this->CountIndustryCargoTypes(), this->CountHouseCargoTypes());

		/* Compute size of the cargo and industry labels. */
		d = maxdim(d, GetMaximalSizeCargoString(ChainField::fontsize));
		d = maxdim(d, GetMaximalSizeIndustryString(ChainField::fontsize));

		d.width += WidgetDimensions::scaled.frametext.Horizontal();
		/* Ensure the height is enough for all connections. */
		uint min_ind_height = ChainField::cargo_border.height * 2 + ChainField::GetConnectionHeight();
		d.height = std::max(d.height + WidgetDimensions::scaled.frametext.Vertical(), min_ind_height);

		ChainField::industry_width = d.width;
		ChainField::normal_height = d.height;

		/* Width of a cargo connection field. */
		ChainField::connection_field_width = ChainField::cargo_border.width * 2 + ChainField::max_cargoes * (ChainField::cargo_line.width + ChainField::cargo_space.width) - ChainField::cargo_space.width;

		/* Compute size for cargo selection dropdown. */
		this->cargo_textsize = GetMaximalSizeCargoString();
		this->cargo_textsize.width += GetLargestCargoIconSize().width + WidgetDimensions::scaled.hsep_normal;
		this->cargo_textsize = maxdim(this->cargo_textsize, GetStringBoundingBox(STR_INDUSTRY_CARGOES_SELECT_CARGO));

		/* Compute size for industry selection dropdown. */
		this->ind_textsize = maxdim(GetMaximalSizeIndustryString(), GetStringBoundingBox(STR_INDUSTRY_CARGOES_SELECT_INDUSTRY));
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_IC_PANEL:
				fill.height = resize.height = ChainField::normal_height + ChainField::vert_inter_industry_space;
				size.width = ChainField::cargo_stub.width * 2 + ChainField::industry_width * 3 + ChainField::connection_field_width * 2 + WidgetDimensions::scaled.frametext.Horizontal();
				size.height = ChainField::small_height + 2 * resize.height + WidgetDimensions::scaled.frametext.Vertical();
				break;

			case WID_IC_IND_DROPDOWN:
				size.width = std::max(size.width, this->ind_textsize.width + padding.width);
				break;

			case WID_IC_CARGO_DROPDOWN:
				size.width = std::max(size.width, this->cargo_textsize.width + padding.width);
				break;
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget != WID_IC_CAPTION) return this->Window::GetWidgetString(widget, stringid);

		struct visitor {
			std::string operator()(IndustryType industry_type) { return GetString(STR_INDUSTRY_CARGOES_INDUSTRY_CAPTION, GetIndustrySpec(industry_type)->name); }
			std::string operator()(CargoType cargo_type) { return GetString(STR_INDUSTRY_CARGOES_CARGO_CAPTION, CargoSpec::Get(cargo_type)->name); }
			std::string operator()(HouseID) { return GetString(STR_INDUSTRY_CARGOES_INDUSTRY_CAPTION, STR_INDUSTRY_CARGOES_HOUSES); }
		};
		return std::visit(visitor{}, this->ind_cargo);
	}

	/**
	 * Can houses be used to supply one of the cargoes?
	 * @param cargoes Span of cargo list.
	 * @return Houses can supply at least one of the cargoes.
	 */
	static bool HousesCanSupply(CargoTypes cargoes)
	{
		return ChainField::town_produces.Any(cargoes);
	}

	/**
	 * Can houses be used as customers of the produced cargoes?
	 * @param cargoes Span of cargo list.
	 * @return Houses can accept at least one of the cargoes.
	 */
	static bool HousesCanAccept(CargoTypes cargoes)
	{
		return ChainField::town_accepts.Any(cargoes);
	}

	/**
	 * Count how many industries have accepted cargoes in common with one of the supplied set.
	 * @param cargoes Cargoes to search.
	 * @return Number of industries that have an accepted cargo in common with the supplied set.
	 */
	static int CountMatchingAcceptingIndustries(CargoTypes cargoes)
	{
		int count = 0;
		for (IndustryType it = 0; it < NUM_INDUSTRYTYPES; it++) {
			const IndustrySpec *indsp = GetIndustrySpec(it);
			if (!indsp->enabled) continue;

			if (cargoes.Any({indsp->accepts_cargo})) count++;
		}
		return count;
	}

	/**
	 * Count how many industries have produced cargoes in common with one of the supplied set.
	 * @param cargoes Cargoes to search.
	 * @return Number of industries that have a produced cargo in common with the supplied set.
	 */
	static int CountMatchingProducingIndustries(CargoTypes cargoes)
	{
		int count = 0;
		for (IndustryType it = 0; it < NUM_INDUSTRYTYPES; it++) {
			const IndustrySpec *indsp = GetIndustrySpec(it);
			if (!indsp->enabled) continue;

			if (cargoes.Any({indsp->produced_cargo})) count++;
		}
		return count;
	}

	/**
	 * Shorten the cargo column to just the part between industries.
	 * @param column Column number of the cargo column.
	 * @param top Current top row.
	 * @param middle Current middle row.
	 * @param bottom Current bottom row.
	 * @param accepting Handle accepting cargoes.
	 * @param supplying Handle supplying cargoes.
	 */
	void ShortenConnectionsColumn(int column, int top, int middle, int bottom, bool accepting, bool supplying)
	{
		CargoChainField::CargoSlotMask last_skip = UINT16_MAX;

		for (int i = top; i <= middle; ++i) {
			ConnectionChainField *fld = this->rows[i].columns[column]->Get<ConnectionChainField>();
			if (fld == nullptr) continue;

			fld->top_end = last_skip;

			if (accepting && this->rows[i].columns[column + 1]->Get<CargoChainField>() != nullptr) break;
			if (i == middle) break;

			/* Skip cargos until they are first connected. */
			fld->skip_cargoes = last_skip & ~fld->GetConnections(accepting, supplying);
			last_skip = fld->skip_cargoes;
		}

		last_skip = UINT16_MAX;
		for (int i = bottom; i >= middle; --i) {
			ConnectionChainField *fld = this->rows[i].columns[column]->Get<ConnectionChainField>();
			if (fld == nullptr) continue;

			fld->bottom_end = last_skip;

			if (supplying && this->rows[i].columns[column - 1]->Get<CargoChainField>() != nullptr) break;
			if (i == middle) break;

			/* Skip cargos until they are first connected. */
			fld->skip_cargoes = last_skip & ~fld->GetConnections(accepting, supplying);
			last_skip = fld->skip_cargoes;
		}
	}

	/**
	 * Place a industry or house in the fields.
	 * @param field The industry or house field to place.
	 * @param row Row of the new field.
	 * @param col Column of the new field.
	 */
	void PlaceAndConnect(std::unique_ptr<AcceptsProducesChainField> &&field, int row, int col)
	{
		assert(this->rows[row].columns[col] == nullptr);
		this->rows[row].columns[col] = std::move(field);
		this->rows[row].ConnectProducedCargo(col);
		this->rows[row].ConnectAcceptedCargo(col);
	}

	/**
	 * Notify smallmap that new displayed industries have been selected (in #_displayed_industries).
	 */
	void NotifySmallmap()
	{
		if (!this->IsWidgetLowered(WID_IC_NOTIFY)) return;

		/* Only notify the smallmap window if it exists. In particular, do not
		 * bring it to the front to prevent messing up any nice layout of the user. */
		InvalidateWindowClassesData(WindowClass::SmallMap, 0);
	}

	/**
	 * Compute what and where to display for an Accepts/Produces chain field.
	 * @param field The Accepts/Produces chain field.
	 * @param accepts List of cargo types the field accepts.
	 * @param produces List of cargo types the field produces.
	 */
	void ComputeAcceptsProducesIndustryDisplay(std::unique_ptr<AcceptsProducesChainField> &&field, CargoTypes accepts, CargoTypes produces)
	{
		this->rows.clear();
		ChainRow &first_row = this->rows.emplace_back();
		first_row.columns[0] = std::make_unique<HeaderChainField>(STR_INDUSTRY_CARGOES_SOURCES);
		first_row.columns[4] = std::make_unique<HeaderChainField>(STR_INDUSTRY_CARGOES_DESTINATIONS);

		bool houses_supply = HousesCanSupply(accepts);
		bool houses_accept = HousesCanAccept(produces);

		int num_supp = CountMatchingProducingIndustries(accepts) + houses_supply;
		int num_cust = CountMatchingAcceptingIndustries(produces) + houses_accept;
		int num_indrows = std::max(3, std::max(num_supp, num_cust)); // One is needed for the 'it' industry, and 2 for the cargo labels.

		/* Make a field consisting of two cargo columns. */
		for (int i = 0; i < num_indrows; i++) {
			ChainRow &row = this->rows.emplace_back();
			row.columns[1] = std::make_unique<ConnectionChainField>(accepts);
			row.columns[3] = std::make_unique<ConnectionChainField>(produces);
		}

		/* Add central accepts/produces field. */
		int central_row = 1 + num_indrows / 2;
		this->rows[central_row].columns[2] = std::move(field);
		this->rows[central_row].ConnectProducedCargo(2);
		this->rows[central_row].ConnectAcceptedCargo(2);

		/* Add cargo labels. */
		this->rows[central_row - 1].MakeCargoLabel(2, true);
		this->rows[central_row + 1].MakeCargoLabel(2, false);

		/* Determine start positions, with different rounding to look better with the label position for each side. */
		int supp_pos = 1 + (num_indrows + 1 - num_supp) / 2;
		int cust_pos = 1 + (num_indrows - num_cust) / 2;

		/* Add suppliers and customers. */
		for (IndustryType it : _sorted_industry_types) {
			const IndustrySpec *indsp = GetIndustrySpec(it);
			if (!indsp->enabled) continue;

			if (accepts.Any({indsp->produced_cargo})) {
				this->PlaceAndConnect(std::make_unique<IndustryChainField>(it), supp_pos++, 0);
				_displayed_industries.set(it);
			}
			if (produces.Any({indsp->accepts_cargo})) {
				this->PlaceAndConnect(std::make_unique<IndustryChainField>(it), cust_pos++, 4);
				_displayed_industries.set(it);
			}
		}

		if (houses_supply) this->PlaceAndConnect(std::make_unique<HouseChainField>(), supp_pos++, 0);
		if (houses_accept) this->PlaceAndConnect(std::make_unique<HouseChainField>(), cust_pos++, 4);

		this->ShortenConnectionsColumn(1, 1, central_row, num_indrows, true, false);
		this->ShortenConnectionsColumn(3, 1, central_row, num_indrows, false, true);
		this->vscroll->SetCount(num_indrows);
		this->SetDirty();
		this->NotifySmallmap();
	}

	/**
	 * Compute the display for an industry type.
	 * @param industry_type Industry type to display.
	 * @return \c true iff the industry type is valid.
	 */
	bool ComputeIndustryDisplay(IndustryType industry_type)
	{
		if (industry_type >= NUM_INDUSTRYTYPES) return false;
		this->ind_cargo = industry_type;
		_displayed_industries.reset();
		_displayed_industries.set(industry_type);

		const IndustrySpec *indsp = GetIndustrySpec(industry_type);
		ComputeAcceptsProducesIndustryDisplay(std::make_unique<IndustryChainField>(industry_type), {indsp->accepts_cargo}, {indsp->produced_cargo});
		return true;
	}

	/**
	 * Compute the display for a house
	 * @return \c true
	 */
	bool ComputeHouseDisplay()
	{
		this->ind_cargo = HouseID{};
		_displayed_industries.reset();

		ComputeAcceptsProducesIndustryDisplay(std::make_unique<HouseChainField>(), ChainField::town_accepts, ChainField::town_produces);
		return true;
	}

	/**
	 * Compute what and where to display for cargo type \a cargo_type.
	 * @param cargo_type Cargo type to display.
	 * @return \c true iff the cargo type is valid.
	 */
	bool ComputeCargoDisplay(CargoType cargo_type)
	{
		if (!IsValidCargoType(cargo_type)) return false;
		this->ind_cargo = cargo_type;
		_displayed_industries.reset();

		this->rows.clear();
		ChainRow &first_row = this->rows.emplace_back();
		first_row.columns[0] = std::make_unique<HeaderChainField>(STR_INDUSTRY_CARGOES_SOURCES);
		first_row.columns[2] = std::make_unique<HeaderChainField>(STR_INDUSTRY_CARGOES_DESTINATIONS);

		CargoTypes cargoes = cargo_type;
		bool houses_supply = HousesCanSupply(cargoes);
		bool houses_accept = HousesCanAccept(cargoes);
		int num_supp = CountMatchingProducingIndustries(cargoes) + houses_supply + 1; // Ensure room for the cargo label.
		int num_cust = CountMatchingAcceptingIndustries(cargoes) + houses_accept;
		int num_indrows = std::max(num_supp, num_cust);
		for (int i = 0; i < num_indrows; i++) {
			ChainRow &row = this->rows.emplace_back();
			row.columns[1] = std::make_unique<ConnectionChainField>(cargoes);
		}

		/* Add suppliers and customers of the cargo. */
		int supp_pos = 1 + (num_indrows - num_supp) / 2;
		int cust_pos = 1 + (num_indrows - num_cust) / 2;
		for (IndustryType it : _sorted_industry_types) {
			const IndustrySpec *indsp = GetIndustrySpec(it);
			if (!indsp->enabled) continue;

			if (cargoes.Any({indsp->produced_cargo})) {
				this->PlaceAndConnect(std::make_unique<IndustryChainField>(it), supp_pos++, 0);
				_displayed_industries.set(it);
			}
			if (cargoes.Any({indsp->accepts_cargo})) {
				this->PlaceAndConnect(std::make_unique<IndustryChainField>(it), cust_pos++, 2);
				_displayed_industries.set(it);
			}
		}
		if (houses_supply) this->PlaceAndConnect(std::make_unique<HouseChainField>(), supp_pos++, 0);
		if (houses_accept) this->PlaceAndConnect(std::make_unique<HouseChainField>(), cust_pos++, 2);

		this->rows[supp_pos].MakeCargoLabel(0, false); // Add cargo labels at the left bottom.

		this->ShortenConnectionsColumn(1, 1, num_indrows, num_indrows, true, true);
		this->vscroll->SetCount(num_indrows);
		this->SetDirty();
		this->NotifySmallmap();
		return true;
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * - data = 0 .. NUM_INDUSTRYTYPES - 1: Display the chain around the given industry.
	 * - data = NUM_INDUSTRYTYPES: Stop sending updates to the smallmap window.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;
		if (data == NUM_INDUSTRYTYPES) {
			this->RaiseWidgetWhenLowered(WID_IC_NOTIFY);
			return;
		}

		assert(data >= 0 && data < NUM_INDUSTRYTYPES);
		this->ComputeIndustryDisplay(data);
	}

	/**
	 * Get the area covered by the cargo chain display.
	 * @param r Rect of the panel widget.
	 * @return Rect wtihin the panel widget.
	 */
	Rect GetRowRect(const Rect &r) const
	{
		const NWidgetBase *nw = this->GetWidget<NWidgetBase>(WID_IC_PANEL);
		bool showing_cargo = std::holds_alternative<CargoType>(this->ind_cargo);

		return r
			.Shrink(WidgetDimensions::scaled.frametext)
			.Translate(0, -this->vscroll->GetPosition() * nw->resize_y)
			.CentreToWidth(showing_cargo
				? (2 * ChainField::industry_width + 1 * ChainField::connection_field_width)
				: (3 * ChainField::industry_width + 2 * ChainField::connection_field_width));
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_IC_PANEL) return;

		Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
		DrawPixelInfo tmp_dpi;
		if (!FillDrawPixelInfo(&tmp_dpi, ir)) return;
		/* Keep coordinates relative to the window. */
		tmp_dpi.left += ir.left;
		tmp_dpi.top += ir.top;
		AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);

		Rect r_row = this->GetRowRect(r);
		for (const ChainRow &row : this->rows) {
			int row_height = row.Height();
			row.Draw(r_row.WithHeight(row_height));
			r_row = r_row.Translate(0, row_height + ChainField::vert_inter_industry_space);
		}
	}

	/**
	 * Calculate in which field was clicked.
	 * @param pt Clicked position in the #WID_IC_PANEL widget.
	 * @return Whether a field was clicked, the field's column and row, and the Rect of the field.
	 */
	ChainField::ClickedAtResult ClickedAt(Point pt) const
	{
		const NWidgetBase *nw = this->GetWidget<NWidgetBase>(WID_IC_PANEL);
		Rect r = this->GetRowRect(nw->GetCurrentRect());

		for (auto it = this->rows.begin(); it != this->rows.end(); ++it) {
			uint row_height = it->Height();
			r = r.WithHeight(row_height);
			if (pt.y >= r.top && pt.y <= r.bottom) return it->ClickedAt(r, pt);
			r = r.Translate(0, row_height + ChainField::vert_inter_industry_space);
		}

		return {};
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_IC_PANEL: {
				struct visitor {
					IndustryCargoesWindow *w;
					bool operator()(std::monostate) { return false; }
					bool operator()(HouseID) { return this->w->ComputeHouseDisplay(); }
					bool operator()(IndustryType industry_type) { return this->w->ComputeIndustryDisplay(industry_type); }
					bool operator()(CargoType cargo_type) { return this->w->ComputeCargoDisplay(cargo_type); }
				};
				if (std::visit(visitor{this}, this->ClickedAt(pt))) SndClickBeep();
				break;
			}

			case WID_IC_NOTIFY:
				this->ToggleWidgetLoweredState(WID_IC_NOTIFY);
				this->SetWidgetDirty(WID_IC_NOTIFY);
				SndClickBeep();

				if (this->IsWidgetLowered(WID_IC_NOTIFY)) {
					if (FindWindowByClass(WindowClass::SmallMap) == nullptr) ShowSmallMap();
					this->NotifySmallmap();
				}
				break;

			case WID_IC_CARGO_DROPDOWN: {
				DropDownList lst;
				Dimension d = GetLargestCargoIconSize();
				for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
					lst.push_back(MakeDropDownListIconItem(d, cs->GetCargoIcon(), PAL_NONE, cs->name, cs->Index()));
				}
				if (!lst.empty()) {
					static std::string cargo_filter;
					int selected = -1;
					if (CargoType *ptr = std::get_if<CargoType>(&this->ind_cargo); ptr != nullptr) selected = to_underlying(*ptr);
					ShowDropDownList(this, std::move(lst), selected, WID_IC_CARGO_DROPDOWN, 0, DropDownOption::Filterable, &cargo_filter);
				}
				break;
			}

			case WID_IC_IND_DROPDOWN: {
				DropDownList lst;
				for (IndustryType ind : _sorted_industry_types) {
					const IndustrySpec *indsp = GetIndustrySpec(ind);
					if (!indsp->enabled) continue;
					lst.push_back(MakeDropDownListStringItem(indsp->name, ind));
				}
				if (!lst.empty()) lst.push_back(MakeDropDownListDividerItem());
				lst.push_back(MakeDropDownListStringItem(STR_INDUSTRY_CARGOES_HOUSES, INT_MAX));
				if (!lst.empty()) {
					static std::string cargo_filter;
					int selected = -1;
					if (IndustryType *ptr = std::get_if<IndustryType>(&this->ind_cargo); ptr != nullptr) {
						selected = *ptr;
					} else if (std::holds_alternative<HouseID>(this->ind_cargo)) {
						selected = INT_MAX;
					}
					ShowDropDownList(this, std::move(lst), selected, WID_IC_IND_DROPDOWN, 0, DropDownOption::Filterable, &cargo_filter);
				}
				break;
			}
		}
	}

	void OnDropdownSelect(WidgetID widget, int index, int) override
	{
		switch (widget) {
			case WID_IC_CARGO_DROPDOWN:
				if (index >= 0) this->ComputeCargoDisplay(static_cast<CargoType>(index));
				break;

			case WID_IC_IND_DROPDOWN:
				if (index == INT_MAX) {
					this->ComputeHouseDisplay();
				} else if (index >= 0) {
					this->ComputeIndustryDisplay(index);
				}
				break;
		}
	}

	bool OnTooltip([[maybe_unused]] Point pt, WidgetID widget, TooltipCloseCondition close_cond) override
	{
		if (widget != WID_IC_PANEL) return false;

		struct visitor {
			IndustryCargoesWindow *w; ///< The industry cargoes window.
			TooltipCloseCondition close_cond; ///< The tooltip condition.
			bool operator()(std::monostate)
			{
				return false;
			}
			bool operator()(HouseID)
			{
				GuiShowTooltips(this->w, GetEncodedString(STR_INDUSTRY_CARGOES_ACCEPTS_PRODUCES_TOOLTIP, STR_INDUSTRY_CARGOES_HOUSES, ChainField::town_accepts, ChainField::town_produces), this->close_cond);
				return true;
			}
			bool operator()(IndustryType industry_type)
			{
				const IndustrySpec *indsp = GetIndustrySpec(industry_type);
				GuiShowTooltips(this->w, GetEncodedString(STR_INDUSTRY_CARGOES_ACCEPTS_PRODUCES_TOOLTIP, indsp->name, CargoTypes{indsp->accepts_cargo}, CargoTypes{indsp->produced_cargo}), this->close_cond);
				return true;
			}
			bool operator()(CargoType cargo_type)
			{
				if (!IsValidCargoType(cargo_type)) return false;
				GuiShowTooltips(this->w, GetEncodedString(CargoSpec::Get(cargo_type)->name), close_cond);
				return true;
			}
		};

		return std::visit(visitor{this, close_cond}, this->ClickedAt(pt));
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_IC_PANEL, WidgetDimensions::scaled.framerect.Vertical() + ChainField::small_height);
	}
};

/**
 * Open the industry and cargoes window.
 * @param id Industry type to display, \c NUM_INDUSTRYTYPES selects a default industry type.
 */
static void ShowIndustryCargoesWindow(IndustryType id)
{
	if (id >= NUM_INDUSTRYTYPES) {
		for (IndustryType ind : _sorted_industry_types) {
			const IndustrySpec *indsp = GetIndustrySpec(ind);
			if (indsp->enabled) {
				id = ind;
				break;
			}
		}
		if (id >= NUM_INDUSTRYTYPES) return;
	}

	Window *w = BringWindowToFrontById(WindowClass::IndustryCargoes, 0);
	if (w != nullptr) {
		w->InvalidateData(id);
		return;
	}
	new IndustryCargoesWindow(id);
}

/** Open the industry and cargoes window with an industry. */
void ShowIndustryCargoesWindow()
{
	ShowIndustryCargoesWindow(NUM_INDUSTRYTYPES);
}
