/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file object_gui.cpp The GUI for objects. */

#include "stdafx.h"
#include "command_func.h"
#include "company_func.h"
#include "hotkeys.h"
#include "newgrf.h"
#include "newgrf_badge_gui.h"
#include "newgrf_object.h"
#include "newgrf_text.h"
#include "object.h"
#include "object_base.h"
#include "picker_gui.h"
#include "sound_func.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "tilehighlight_func.h"
#include "window_gui.h"
#include "window_func.h"
#include "zoom_func.h"
#include "terraform_cmd.h"
#include "object_cmd.h"
#include "road_cmd.h"

#include "widgets/object_widget.h"

#include "table/strings.h"

#include "safeguards.h"

struct ObjectPickerSelection {
	ObjectClassID sel_class; ///< Selected object class.
	uint16_t sel_type; ///< Selected object type within the class.
	uint8_t sel_view; ///< Selected view of the object.
};
static ObjectPickerSelection _object_gui; ///< Settings of the object picker.

class ObjectPickerCallbacks : public PickerCallbacksNewGRFClass<ObjectClass> {
public:
	ObjectPickerCallbacks() : PickerCallbacksNewGRFClass<ObjectClass>("fav_objects") {}

	GrfSpecFeature GetFeature() const override { return GSF_OBJECTS; }

	StringID GetClassTooltip() const override { return STR_PICKER_OBJECT_CLASS_TOOLTIP; }
	StringID GetTypeTooltip() const override { return STR_PICKER_OBJECT_TYPE_TOOLTIP; }

	bool IsActive() const override
	{
		for (const auto &cls : ObjectClass::Classes()) {
			for (const auto *spec : cls.Specs()) {
				if (spec != nullptr && spec->IsEverAvailable()) return true;
			}
		}
		return false;
	}

	int GetSelectedClass() const override { return _object_gui.sel_class; }
	void SetSelectedClass(int id) const override { _object_gui.sel_class = this->GetClassIndex(id); }

	StringID GetClassName(int id) const override
	{
		const auto *objclass = this->GetClass(id);
		if (objclass->GetUISpecCount() == 0) return INVALID_STRING_ID;
		return objclass->name;
	}

	int GetSelectedType() const override { return _object_gui.sel_type; }
	void SetSelectedType(int id) const override { _object_gui.sel_type = id; }

	StringID GetTypeName(int cls_id, int id) const override
	{
		const auto *spec = this->GetSpec(cls_id, id);
		return (spec == nullptr || !spec->IsEverAvailable()) ? INVALID_STRING_ID : spec->name;
	}

	std::span<const BadgeID> GetTypeBadges(int cls_id, int id) const override
	{
		const auto *spec = this->GetSpec(cls_id, id);
		if (spec == nullptr || !spec->IsEverAvailable()) return {};
		return spec->badges;
	}

	bool IsTypeAvailable(int cls_id, int id) const override
	{
		const auto *spec = this->GetSpec(cls_id, id);
		return spec->IsAvailable();
	}

	void DrawType(int x, int y, int cls_id, int id) const override
	{
		const auto *spec = this->GetSpec(cls_id, id);
		if (!spec->grf_prop.HasGrfFile()) {
			extern const DrawTileSpriteSpan _objects[];
			const DrawTileSprites *dts = &_objects[spec->grf_prop.local_id];
			DrawOrigTileSeqInGUI(x, y, dts, PAL_NONE);
		} else {
			DrawNewObjectTileInGUI(x, y, spec, std::min<int>(_object_gui.sel_view, spec->views - 1));
		}
	}

	void FillUsedItems(std::set<PickerItem> &items) override
	{
		for (const Object *o : Object::Iterate()) {
			if (GetTileOwner(o->location.tile) != _current_company) continue;
			const ObjectSpec *spec = ObjectSpec::Get(o->type);
			if (spec == nullptr || spec->class_index == INVALID_OBJECT_CLASS || !spec->IsEverAvailable()) continue;
			items.insert(GetPickerItem(spec));
		}
	}

	static ObjectPickerCallbacks instance;
};
/* static */ ObjectPickerCallbacks ObjectPickerCallbacks::instance;

/** The window used for building objects. */
class BuildObjectWindow : public PickerWindow {
	int info_height = 1; ///< The height of the info box.

public:
	BuildObjectWindow(WindowDesc &desc, WindowNumber) : PickerWindow(desc, nullptr, 0, ObjectPickerCallbacks::instance)
	{
		ResetObjectToPlace();
		this->ConstructWindow();
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_BO_OBJECT_SIZE: {
				ObjectClass *objclass = ObjectClass::Get(_object_gui.sel_class);
				const ObjectSpec *spec = objclass->GetSpec(_object_gui.sel_type);
				int size = spec == nullptr ? 0 : spec->size;
				return GetString(STR_OBJECT_BUILD_SIZE, GB(size, HasBit(_object_gui.sel_view, 0) ? 4 : 0, 4), GB(size, HasBit(_object_gui.sel_view, 0) ? 0 : 4, 4));
			}

			default:
				return this->PickerWindow::GetWidgetString(widget, stringid);
		}
	}

	void OnInit() override
	{
		this->GetWidget<NWidgetMatrix>(WID_BO_OBJECT_MATRIX)->SetCount(4);
		this->PickerWindow::OnInit();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_BO_OBJECT_SIZE:
				/* We do not want the window to resize when selecting objects; better clip texts */
				size.width = 0;
				break;

			case WID_BO_OBJECT_MATRIX: {
				/* Get the right amount of buttons based on the current spec. */
				const ObjectClass *objclass = ObjectClass::Get(_object_gui.sel_class);
				const ObjectSpec *spec = objclass->GetSpec(_object_gui.sel_type);
				if (spec != nullptr) {
					if (spec->views >= 2) size.width  += resize.width;
					if (spec->views >= 4) size.height += resize.height;
				}
				resize.width = 0;
				resize.height = 0;
				break;
			}

			case WID_BO_OBJECT_SPRITE: {
				/* Get the right amount of buttons based on the current spec. */
				const ObjectClass *objclass = ObjectClass::Get(_object_gui.sel_class);
				const ObjectSpec *spec = objclass->GetSpec(_object_gui.sel_type);
				size.width  = ScaleGUITrad(PREVIEW_WIDTH) + WidgetDimensions::scaled.fullbevel.Horizontal();
				size.height = ScaleGUITrad(PREVIEW_HEIGHT) + WidgetDimensions::scaled.fullbevel.Vertical();
				if (spec != nullptr) {
					if (spec->views <= 1) size.width = size.width * 2 + WidgetDimensions::scaled.hsep_normal;
					if (spec->views <= 2) size.height = size.height * 2 + WidgetDimensions::scaled.vsep_normal;
				}
				break;
			}

			case WID_BO_INFO:
				size.height = this->info_height;
				fill.height = this->has_class_picker ? 0 : 1;
				resize.height = this->has_class_picker ? 0 : 1;
				break;

			default:
				this->PickerWindow::UpdateWidgetSize(widget, size, padding, fill, resize);
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_BO_OBJECT_SPRITE: {
				const ObjectClass *objclass = ObjectClass::Get(_object_gui.sel_class);
				const ObjectSpec *spec = objclass->GetSpec(_object_gui.sel_type);
				if (spec == nullptr) break;

				const NWidgetMatrix *matrix = this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>();

				DrawPixelInfo tmp_dpi;
				/* Set up a clipping area for the preview. */
				Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
				if (FillDrawPixelInfo(&tmp_dpi, ir)) {
					AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
					int x = (ir.Width()  - ScaleSpriteTrad(PREVIEW_WIDTH)) / 2 + ScaleSpriteTrad(PREVIEW_LEFT);
					int y = (ir.Height() + ScaleSpriteTrad(PREVIEW_HEIGHT)) / 2 - ScaleSpriteTrad(PREVIEW_BOTTOM);

					if (!spec->grf_prop.HasGrfFile()) {
						extern const DrawTileSpriteSpan _objects[];
						const DrawTileSprites *dts = &_objects[spec->grf_prop.local_id];
						DrawOrigTileSeqInGUI(x, y, dts, PAL_NONE);
					} else {
						DrawNewObjectTileInGUI(x, y, spec, matrix->GetCurrentElement());
					}
				}
				break;
			}

			case WID_BO_INFO: {
				const ObjectClass *objclass = ObjectClass::Get(_object_gui.sel_class);
				const ObjectSpec *spec = objclass->GetSpec(_object_gui.sel_type);
				if (spec == nullptr) break;

				Rect tr = r;
				const int bottom = tr.bottom;
				/* Use all the available space past the rect, so that we can enlarge the window if needed. */
				tr.bottom = INT16_MAX;
				tr.top = DrawBadgeNameList(tr, spec->badges, GSF_OBJECTS);

				/* Get the extra message for the GUI */
				if (spec->callback_mask.Test(ObjectCallbackMask::FundMoreText)) {
					std::array<int32_t, 16> regs100;
					uint16_t callback_res = GetObjectCallback(CBID_OBJECT_FUND_MORE_TEXT, 0, 0, spec, nullptr, INVALID_TILE, regs100, _object_gui.sel_view);
					if (callback_res != CALLBACK_FAILED && callback_res != 0x400) {
						std::string str;
						if (callback_res == 0x40F) {
							str = GetGRFStringWithTextStack(spec->grf_prop.grffile, static_cast<GRFStringID>(regs100[0]), std::span{regs100}.subspan(1));
						} else if (callback_res > 0x400) {
							ErrorUnknownCallbackResult(spec->grf_prop.grfid, CBID_OBJECT_FUND_MORE_TEXT, callback_res);
						} else {
							str = GetGRFStringWithTextStack(spec->grf_prop.grffile, GRFSTR_MISC_GRF_TEXT + callback_res, regs100);
						}
						if (!str.empty()) {
							tr.top = DrawStringMultiLine(tr, str, TC_ORANGE);
						}
					}
				}

				if (tr.top > bottom) {
					BuildObjectWindow *bow = const_cast<BuildObjectWindow *>(this);
					bow->info_height += tr.top - bottom;
					bow->ReInit();
				}

				break;
			}

			default:
				this->PickerWindow::DrawWidget(r, widget);
				break;
		}
	}

	void UpdateSelectSize(const ObjectSpec *spec)
	{
		if (spec == nullptr) {
			SetTileSelectSize(1, 1);
			ResetObjectToPlace();
		} else {
			_object_gui.sel_view = std::min<int>(_object_gui.sel_view, spec->views - 1);
			SetObjectToPlaceWnd(SPR_CURSOR_TRANSMITTER, PAL_NONE, HT_RECT | HT_DIAGONAL, this);
			int w = GB(spec->size, HasBit(_object_gui.sel_view, 0) ? 4 : 0, 4);
			int h = GB(spec->size, HasBit(_object_gui.sel_view, 0) ? 0 : 4, 4);
			SetTileSelectSize(w, h);
			this->ReInit();
		}
	}

	/**
	 * Update buttons to show the selection to the user.
	 * @param spec The object spec of the selected object.
	 */
	void UpdateButtons(const ObjectSpec *spec)
	{
		this->GetWidget<NWidgetMatrix>(WID_BO_OBJECT_MATRIX)->SetClicked(_object_gui.sel_view);
		this->UpdateSelectSize(spec);
		this->SetDirty();
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		this->PickerWindow::OnInvalidateData(data, gui_scope);

		if (!gui_scope) return;

		PickerInvalidations pi(data);
		if (pi.Test(PickerInvalidation::Position)) {
			const auto objclass = ObjectClass::Get(_object_gui.sel_class);
			const auto spec = objclass->GetSpec(_object_gui.sel_type);
			_object_gui.sel_view = std::min<int>(_object_gui.sel_view, spec->views - 1);
			this->UpdateButtons(spec);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BO_OBJECT_SPRITE:
				if (_object_gui.sel_type != std::numeric_limits<uint16_t>::max()) {
					_object_gui.sel_view = this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->GetCurrentElement();
					this->InvalidateData(PickerInvalidation::Position);
					SndClickBeep();
				}
				break;

			default:
				this->PickerWindow::OnClick(pt, widget, click_count);
				break;
		}
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		const ObjectSpec *spec = ObjectClass::Get(_object_gui.sel_class)->GetSpec(_object_gui.sel_type);

		if (spec->size == OBJECT_SIZE_1X1) {
			VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_BUILD_OBJECT);
		} else {
			Command<CMD_BUILD_OBJECT>::Post(STR_ERROR_CAN_T_BUILD_OBJECT, CcPlaySound_CONSTRUCTION_OTHER, tile, spec->Index(), _object_gui.sel_view);
		}
	}

	void OnPlaceDrag(ViewportPlaceMethod select_method, [[maybe_unused]] ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt) override
	{
		VpSelectTilesWithMethod(pt.x, pt.y, select_method);
	}

	void OnPlaceMouseUp([[maybe_unused]] ViewportPlaceMethod select_method, [[maybe_unused]] ViewportDragDropSelectionProcess select_proc, [[maybe_unused]] Point pt, TileIndex start_tile, TileIndex end_tile) override
	{
		if (pt.x == -1) return;

		assert(select_proc == DDSP_BUILD_OBJECT);

		if (!_settings_game.construction.freeform_edges) {
			/* When end_tile is MP_VOID, the error tile will not be visible to the
			 * user. This happens when terraforming at the southern border. */
			if (TileX(end_tile) == Map::MaxX()) end_tile += TileDiffXY(-1, 0);
			if (TileY(end_tile) == Map::MaxY()) end_tile += TileDiffXY(0, -1);
		}
		const ObjectSpec *spec = ObjectClass::Get(_object_gui.sel_class)->GetSpec(_object_gui.sel_type);
		Command<CMD_BUILD_OBJECT_AREA>::Post(STR_ERROR_CAN_T_BUILD_OBJECT, CcPlaySound_CONSTRUCTION_OTHER,
			end_tile, start_tile, spec->Index(), _object_gui.sel_view, (_ctrl_pressed ? true : false));
	}

	void OnPlaceObjectAbort() override
	{
		this->UpdateButtons(nullptr);
	}

	/**
	 * Handler for global hotkeys of the BuildObjectWindow.
	 * @param hotkey Hotkey
	 * @return ES_HANDLED if hotkey was accepted.
	 */
	static EventState BuildObjectGlobalHotkeys(int hotkey)
	{
		if (_game_mode == GM_MENU) return ES_NOT_HANDLED;
		Window *w = ShowBuildObjectPicker();
		if (w == nullptr) return ES_NOT_HANDLED;
		return w->OnHotkey(hotkey);
	}

	static inline HotkeyList hotkeys{"buildobject", {
		Hotkey('F', "focus_filter_box", PCWHK_FOCUS_FILTER_BOX),
	}, BuildObjectGlobalHotkeys};
};

static constexpr std::initializer_list<NWidgetPart> _nested_build_object_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetStringTip(STR_OBJECT_BUILD_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidgetFunction(MakePickerClassWidgets),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_picker, 0), SetPadding(WidgetDimensions::unscaled.picker),
					NWidget(WWT_LABEL, INVALID_COLOUR), SetStringTip(STR_STATION_BUILD_ORIENTATION), SetFill(1, 0),
					NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
						NWidget(NWID_MATRIX, COLOUR_DARK_GREEN, WID_BO_OBJECT_MATRIX), SetPIP(0, 2, 0),
							NWidget(WWT_PANEL, COLOUR_GREY, WID_BO_OBJECT_SPRITE), SetToolTip(STR_OBJECT_BUILD_PREVIEW_TOOLTIP), EndContainer(),
							EndContainer(),
						EndContainer(),
						NWidget(WWT_TEXT, INVALID_COLOUR, WID_BO_OBJECT_SIZE), SetAlignment(SA_CENTER),
						NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BO_INFO), SetFill(1, 0), SetResize(1, 0),
					EndContainer(),
				EndContainer(),
			EndContainer(),
		NWidgetFunction(MakePickerTypeWidgets),
	EndContainer(),
};

static WindowDesc _build_object_desc(
	WDP_AUTO, "build_object", 0, 0,
	WC_BUILD_OBJECT, WC_BUILD_TOOLBAR,
	WindowDefaultFlag::Construction,
	_nested_build_object_widgets,
	&BuildObjectWindow::hotkeys
);

/** Show our object picker.  */
Window *ShowBuildObjectPicker()
{
	/* Don't show the place object button when there are no objects to place. */
	if (ObjectPickerCallbacks::instance.IsActive()) {
		return AllocateWindowDescFront<BuildObjectWindow>(_build_object_desc, 0);
	}
	return nullptr;
}

/** Reset all data of the object GUI. */
void InitializeObjectGui()
{
	_object_gui.sel_class = ObjectClassID::OBJECT_CLASS_BEGIN;
}
