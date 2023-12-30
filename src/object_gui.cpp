/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file object_gui.cpp The GUI for objects. */

#include "stdafx.h"
#include "command_func.h"
#include "hotkeys.h"
#include "newgrf.h"
#include "newgrf_object.h"
#include "newgrf_text.h"
#include "object.h"
#include "querystring_gui.h"
#include "sortlist_type.h"
#include "stringfilter_type.h"
#include "string_func.h"
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

static ObjectClassID _selected_object_class; ///< Currently selected available object class.
static int _selected_object_index;           ///< Index of the currently selected object if existing, else \c -1.
static uint8_t _selected_object_view;          ///< the view of the selected object

/** Enum referring to the Hotkeys in the build object window */
enum BuildObjectHotkeys {
	BOHK_FOCUS_FILTER_BOX, ///< Focus the edit box for editing the filter string
};

/** The window used for building objects. */
class BuildObjectWindow : public Window {
	typedef GUIList<ObjectClassID, std::nullptr_t, StringFilter &> GUIObjectClassList; ///< Type definition for the list to hold available object classes.

	static const uint EDITBOX_MAX_SIZE = 16; ///< The maximum number of characters for the filter edit box.

	int object_margin;                     ///< The margin (in pixels) around an object.
	int line_height;                       ///< The height of a single line.
	int info_height;                       ///< The height of the info box.
	Scrollbar *vscroll;                    ///< The scrollbar.

	static Listing   last_sorting;         ///< Default sorting of #GUIObjectClassList.
	static Filtering last_filtering;       ///< Default filtering of #GUIObjectClassList.
	static GUIObjectClassList::SortFunction   * const sorter_funcs[]; ///< Sort functions of the #GUIObjectClassList.
	static GUIObjectClassList::FilterFunction * const filter_funcs[]; ///< Filter functions of the #GUIObjectClassList.
	GUIObjectClassList object_classes;     ///< Available object classes.
	StringFilter string_filter;            ///< Filter for available objects.
	QueryString filter_editbox;            ///< Filter editbox.

	/** Scroll #WID_BO_CLASS_LIST so that the selected object class is visible. */
	void EnsureSelectedObjectClassIsVisible()
	{
		uint pos = 0;
		for (auto object_class_id : this->object_classes) {
			if (object_class_id == _selected_object_class) break;
			pos++;
		}
		this->vscroll->ScrollTowards(pos);
	}

	/**
	 * Tests whether the previously selected object can be selected.
	 * @return \c true if the selected object is available, \c false otherwise.
	 */
	bool CanRestoreSelectedObject()
	{
		if (_selected_object_index == -1) return false;

		ObjectClass *objclass = ObjectClass::Get(_selected_object_class);
		if ((int)objclass->GetSpecCount() <= _selected_object_index) return false;

		return objclass->GetSpec(_selected_object_index)->IsAvailable();
	}

public:
	BuildObjectWindow(WindowDesc *desc, WindowNumber number) : Window(desc), info_height(1), filter_editbox(EDITBOX_MAX_SIZE * MAX_CHAR_LENGTH, EDITBOX_MAX_SIZE)
	{
		this->CreateNestedTree();

		this->vscroll = this->GetScrollbar(WID_BO_SCROLLBAR);

		this->querystrings[WID_BO_FILTER] = &this->filter_editbox;

		this->object_classes.SetListing(this->last_sorting);
		this->object_classes.SetFiltering(this->last_filtering);
		this->object_classes.SetSortFuncs(this->sorter_funcs);
		this->object_classes.SetFilterFuncs(this->filter_funcs);
		this->object_classes.ForceRebuild();

		BuildObjectClassesAvailable();
		SelectClassAndObject();

		this->FinishInitNested(number);

		NWidgetMatrix *matrix = this->GetWidget<NWidgetMatrix>(WID_BO_SELECT_MATRIX);
		matrix->SetScrollbar(this->GetScrollbar(WID_BO_SELECT_SCROLL));
		matrix->SetCount(ObjectClass::Get(_selected_object_class)->GetUISpecCount());

		this->GetWidget<NWidgetMatrix>(WID_BO_OBJECT_MATRIX)->SetCount(4);

		ResetObjectToPlace();

		this->vscroll->SetCount(this->object_classes.size());

		EnsureSelectedObjectClassIsVisible();

		this->InvalidateData();
	}

	/** Sort object classes by ObjectClassID. */
	static bool ObjectClassIDSorter(ObjectClassID const &a, ObjectClassID const &b)
	{
		return a < b;
	}

	/** Filter object classes by class name. */
	static bool CDECL TagNameFilter(ObjectClassID const *oc, StringFilter &filter)
	{
		ObjectClass *objclass = ObjectClass::Get(*oc);

		filter.ResetState();
		filter.AddLine(GetString(objclass->name));
		return filter.GetState();
	}

	/** Builds the filter list of available object classes. */
	void BuildObjectClassesAvailable()
	{
		if (!this->object_classes.NeedRebuild()) return;

		this->object_classes.clear();

		for (uint i = 0; i < ObjectClass::GetClassCount(); i++) {
			ObjectClass *objclass = ObjectClass::Get((ObjectClassID)i);
			if (objclass->GetUISpecCount() == 0) continue; // Is this needed here?
			object_classes.push_back((ObjectClassID)i);
		}

		this->object_classes.Filter(this->string_filter);
		this->object_classes.shrink_to_fit();
		this->object_classes.RebuildDone();
		this->object_classes.Sort();

		this->vscroll->SetCount(this->object_classes.size());
	}

	/**
	 * Checks if the previously selected current object class and object
	 * can be shown as selected to the user when the dialog is opened.
	 */
	void SelectClassAndObject()
	{
		assert(!this->object_classes.empty()); // object GUI should be disabled elsewise
		if (_selected_object_class == ObjectClassID::OBJECT_CLASS_BEGIN) {
			/* This happens during the first time the window is open during the game life cycle. */
			this->SelectOtherClass(this->object_classes[0]);
		} else {
			/* Check if the previously selected object class is not available anymore as a
			 * result of starting a new game without the corresponding NewGRF. */
			bool available = _selected_object_class < ObjectClass::GetClassCount();
			this->SelectOtherClass(available ? _selected_object_class : this->object_classes[0]);
		}

		if (this->CanRestoreSelectedObject()) {
			this->SelectOtherObject(_selected_object_index);
		} else {
			this->SelectFirstAvailableObject(true);
		}
		assert(ObjectClass::Get(_selected_object_class)->GetUISpecCount() > 0); // object GUI should be disabled elsewise
	}

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case WID_BO_OBJECT_NAME: {
				ObjectClass *objclass = ObjectClass::Get(_selected_object_class);
				const ObjectSpec *spec = objclass->GetSpec(_selected_object_index);
				SetDParam(0, spec != nullptr ? spec->name : STR_EMPTY);
				break;
			}

			case WID_BO_OBJECT_SIZE: {
				ObjectClass *objclass = ObjectClass::Get(_selected_object_class);
				const ObjectSpec *spec = objclass->GetSpec(_selected_object_index);
				int size = spec == nullptr ? 0 : spec->size;
				SetDParam(0, GB(size, HasBit(_selected_object_view, 0) ? 4 : 0, 4));
				SetDParam(1, GB(size, HasBit(_selected_object_view, 0) ? 0 : 4, 4));
				break;
			}

			default: break;
		}
	}

	void OnInit() override
	{
		this->object_margin = ScaleGUITrad(4);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension *size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension *fill, [[maybe_unused]] Dimension *resize) override
	{
		switch (widget) {
			case WID_BO_CLASS_LIST: {
				for (auto object_class_id : this->object_classes) {
					ObjectClass *objclass = ObjectClass::Get(object_class_id);
					if (objclass->GetUISpecCount() == 0) continue;
					size->width = std::max(size->width, GetStringBoundingBox(objclass->name).width + padding.width);
				}
				this->line_height = GetCharacterHeight(FS_NORMAL) + padding.height;
				resize->height = this->line_height;
				size->height = 5 * this->line_height;
				break;
			}

			case WID_BO_OBJECT_NAME:
			case WID_BO_OBJECT_SIZE:
				/* We do not want the window to resize when selecting objects; better clip texts */
				size->width = 0;
				break;

			case WID_BO_OBJECT_MATRIX: {
				/* Get the right amount of buttons based on the current spec. */
				ObjectClass *objclass = ObjectClass::Get(_selected_object_class);
				const ObjectSpec *spec = objclass->GetSpec(_selected_object_index);
				if (spec != nullptr) {
					if (spec->views >= 2) size->width  += resize->width;
					if (spec->views >= 4) size->height += resize->height;
				}
				resize->width = 0;
				resize->height = 0;
				break;
			}

			case WID_BO_OBJECT_SPRITE: {
				bool two_wide = false;  // Whether there will be two widgets next to each other in the matrix or not.
				uint height[2] = {0, 0}; // The height for the different views; in this case views 1/2 and 4.

				/* Get the height and view information. */
				for (const auto &spec : ObjectSpec::Specs()) {
					if (!spec.IsEverAvailable()) continue;
					two_wide |= spec.views >= 2;
					height[spec.views / 4] = std::max<int>(spec.height, height[spec.views / 4]);
				}

				/* Determine the pixel heights. */
				for (size_t i = 0; i < lengthof(height); i++) {
					height[i] *= ScaleGUITrad(TILE_HEIGHT);
					height[i] += ScaleGUITrad(TILE_PIXELS) + 2 * this->object_margin;
				}

				/* Now determine the size of the minimum widgets. When there are two columns, then
				 * we want these columns to be slightly less wide. When there are two rows, then
				 * determine the size of the widgets based on the maximum size for a single row
				 * of widgets, or just the twice the widget height of the two row ones. */
				size->height = std::max(height[0], height[1] * 2);
				if (two_wide) {
					size->width  = (3 * ScaleGUITrad(TILE_PIXELS) + 2 * this->object_margin) * 2;
				} else {
					size->width  = 4 * ScaleGUITrad(TILE_PIXELS) + 2 * this->object_margin;
				}

				/* Get the right size for the single widget based on the current spec. */
				ObjectClass *objclass = ObjectClass::Get(_selected_object_class);
				const ObjectSpec *spec = objclass->GetSpec(_selected_object_index);
				if (spec != nullptr) {
					if (spec->views <= 1) size->width  += WidgetDimensions::scaled.hsep_normal;
					if (spec->views <= 2) size->height += WidgetDimensions::scaled.vsep_normal;
					if (spec->views >= 2) size->width  /= 2;
					if (spec->views >= 4) size->height /= 2;
				}
				break;
			}

			case WID_BO_INFO:
				size->height = this->info_height;
				break;

			case WID_BO_SELECT_MATRIX:
				fill->height = 1;
				resize->height = 1;
				break;

			case WID_BO_SELECT_IMAGE:
				size->width  = ScaleGUITrad(64) + WidgetDimensions::scaled.fullbevel.Horizontal();
				size->height = ScaleGUITrad(58) + WidgetDimensions::scaled.fullbevel.Vertical();
				break;

			default: break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_BO_CLASS_LIST: {
				Rect mr = r.Shrink(WidgetDimensions::scaled.matrix);
				uint pos = 0;
				for (auto object_class_id : this->object_classes) {
					ObjectClass *objclass = ObjectClass::Get(object_class_id);
					if (objclass->GetUISpecCount() == 0) continue;
					if (!this->vscroll->IsVisible(pos++)) continue;
					DrawString(mr, objclass->name,
							(object_class_id == _selected_object_class) ? TC_WHITE : TC_BLACK);
					mr.top += this->line_height;
				}
				break;
			}

			case WID_BO_OBJECT_SPRITE: {
				if (_selected_object_index == -1) break;

				ObjectClass *objclass = ObjectClass::Get(_selected_object_class);
				const ObjectSpec *spec = objclass->GetSpec(_selected_object_index);
				if (spec == nullptr) break;

				/* Height of the selection matrix.
				 * Depending on the number of views, the matrix has a 1x1, 1x2, 2x1 or 2x2 layout. To make the previews
				 * look nice in all layouts, we use the 4x4 layout (smallest previews) as starting point. For the bigger
				 * previews in the layouts with less views we add space homogeneously on all sides, so the 4x4 preview-rectangle
				 * is centered in the 2x1, 1x2 resp. 1x1 buttons. */
				const NWidgetMatrix *matrix = this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>();
				uint matrix_height = matrix->current_y;

				DrawPixelInfo tmp_dpi;
				/* Set up a clipping area for the preview. */
				Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
				if (FillDrawPixelInfo(&tmp_dpi, ir)) {
					AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
					if (spec->grf_prop.grffile == nullptr) {
						extern const DrawTileSprites _objects[];
						const DrawTileSprites *dts = &_objects[spec->grf_prop.local_id];
						DrawOrigTileSeqInGUI(ir.Width() / 2 - 1, (ir.Height() + matrix_height / 2) / 2 - this->object_margin - ScaleSpriteTrad(TILE_PIXELS), dts, PAL_NONE);
					} else {
						DrawNewObjectTileInGUI(ir.Width() / 2 - 1, (ir.Height() + matrix_height / 2) / 2 - this->object_margin - ScaleSpriteTrad(TILE_PIXELS), spec, matrix->GetCurrentElement());
					}
				}
				break;
			}

			case WID_BO_SELECT_IMAGE: {
				ObjectClass *objclass = ObjectClass::Get(_selected_object_class);
				int obj_index = objclass->GetIndexFromUI(this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->GetCurrentElement());
				if (obj_index < 0) break;
				const ObjectSpec *spec = objclass->GetSpec(obj_index);
				if (spec == nullptr) break;

				DrawPixelInfo tmp_dpi;
				/* Set up a clipping area for the preview. */
				Rect ir = r.Shrink(WidgetDimensions::scaled.bevel);
				if (FillDrawPixelInfo(&tmp_dpi, ir)) {
					AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);
					if (spec->grf_prop.grffile == nullptr) {
						extern const DrawTileSprites _objects[];
						const DrawTileSprites *dts = &_objects[spec->grf_prop.local_id];
						DrawOrigTileSeqInGUI(ir.Width() / 2 - 1, ir.Height() - this->object_margin - ScaleSpriteTrad(TILE_PIXELS), dts, PAL_NONE);
					} else {
						DrawNewObjectTileInGUI(ir.Width() / 2 - 1, ir.Height() - this->object_margin - ScaleSpriteTrad(TILE_PIXELS), spec,
								std::min<int>(_selected_object_view, spec->views - 1));
					}
				}
				if (!spec->IsAvailable()) {
					GfxFillRect(ir, PC_BLACK, FILLRECT_CHECKER);
				}
				break;
			}

			case WID_BO_INFO: {
				ObjectClass *objclass = ObjectClass::Get(_selected_object_class);
				const ObjectSpec *spec = objclass->GetSpec(_selected_object_index);
				if (spec == nullptr) break;

				/* Get the extra message for the GUI */
				if (HasBit(spec->callback_mask, CBM_OBJ_FUND_MORE_TEXT)) {
					uint16_t callback_res = GetObjectCallback(CBID_OBJECT_FUND_MORE_TEXT, 0, 0, spec, nullptr, INVALID_TILE, _selected_object_view);
					if (callback_res != CALLBACK_FAILED && callback_res != 0x400) {
						if (callback_res > 0x400) {
							ErrorUnknownCallbackResult(spec->grf_prop.grffile->grfid, CBID_OBJECT_FUND_MORE_TEXT, callback_res);
						} else {
							StringID message = GetGRFStringID(spec->grf_prop.grffile->grfid, 0xD000 + callback_res);
							if (message != STR_NULL && message != STR_UNDEFINED) {
								StartTextRefStackUsage(spec->grf_prop.grffile, 6);
								/* Use all the available space left from where we stand up to the
								 * end of the window. We ALSO enlarge the window if needed, so we
								 * can 'go' wild with the bottom of the window. */
								int y = DrawStringMultiLine(r.left, r.right, r.top, UINT16_MAX, message, TC_ORANGE) - r.top - 1;
								StopTextRefStackUsage();
								if (y > this->info_height) {
									BuildObjectWindow *bow = const_cast<BuildObjectWindow *>(this);
									bow->info_height = y;
									bow->ReInit();
								}
							}
						}
					}
				}
			}
		}
	}

	/**
	 * Select the specified object class.
	 * @param object_class Object class select.
	 */
	void SelectOtherClass(ObjectClassID object_class)
	{
		_selected_object_class = object_class;
		ObjectClass *objclass = ObjectClass::Get(object_class);
		this->GetWidget<NWidgetMatrix>(WID_BO_SELECT_MATRIX)->SetCount(objclass->GetUISpecCount());
	}

	/**
	 * Select the specified object in #_selected_object_class class.
	 * @param object_index Object index to select, \c -1 means select nothing.
	 */
	void SelectOtherObject(int object_index)
	{
		_selected_object_index = object_index;
		if (_selected_object_index != -1) {
			ObjectClass *objclass = ObjectClass::Get(_selected_object_class);
			const ObjectSpec *spec = objclass->GetSpec(_selected_object_index);
			_selected_object_view = std::min<int>(_selected_object_view, spec->views - 1);
			this->ReInit();
		} else {
			_selected_object_view = 0;
		}

		if (_selected_object_index != -1) {
			SetObjectToPlaceWnd(SPR_CURSOR_TRANSMITTER, PAL_NONE, HT_RECT | HT_DIAGONAL, this);
		}

		this->UpdateButtons(_selected_object_class, _selected_object_index, _selected_object_view);
	}

	void UpdateSelectSize()
	{
		if (_selected_object_index == -1) {
			SetTileSelectSize(1, 1);
		} else {
			ObjectClass *objclass = ObjectClass::Get(_selected_object_class);
			const ObjectSpec *spec = objclass->GetSpec(_selected_object_index);
			int w = GB(spec->size, HasBit(_selected_object_view, 0) ? 4 : 0, 4);
			int h = GB(spec->size, HasBit(_selected_object_view, 0) ? 0 : 4, 4);
			SetTileSelectSize(w, h);
		}
	}

	/**
	 * Update buttons to show the selection to the user.
	 * @param object_class The class of the selected object.
	 * @param sel_index Index of the object to select, or \c -1 .
	 * @param sel_view View of the object to select.
	 */
	void UpdateButtons(ObjectClassID object_class, int sel_index, uint sel_view)
	{
		int view_number, object_number;
		if (sel_index == -1) {
			view_number = -1; // If no object selected, also hide the selected view.
			object_number = -1;
		} else {
			view_number = sel_view;
			ObjectClass *objclass = ObjectClass::Get(object_class);
			object_number = objclass->GetUIFromIndex(sel_index);
		}

		this->GetWidget<NWidgetMatrix>(WID_BO_OBJECT_MATRIX)->SetClicked(view_number);
		this->GetWidget<NWidgetMatrix>(WID_BO_SELECT_MATRIX)->SetClicked(object_number);
		this->UpdateSelectSize();
		this->SetDirty();
	}

	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (!gui_scope) return;

		this->BuildObjectClassesAvailable();
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_BO_CLASS_LIST);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BO_CLASS_LIST: {
				auto it = this->vscroll->GetScrolledItemFromWidget(this->object_classes, pt.y, this, widget);
				if (it == this->object_classes.end()) break;

				this->SelectOtherClass(*it);
				this->SelectFirstAvailableObject(false);
				break;
			}

			case WID_BO_SELECT_IMAGE: {
				ObjectClass *objclass = ObjectClass::Get(_selected_object_class);
				int num_clicked = objclass->GetIndexFromUI(this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->GetCurrentElement());
				if (num_clicked >= 0 && objclass->GetSpec(num_clicked)->IsAvailable()) this->SelectOtherObject(num_clicked);
				break;
			}

			case WID_BO_OBJECT_SPRITE:
				if (_selected_object_index != -1) {
					_selected_object_view = this->GetWidget<NWidgetBase>(widget)->GetParentWidget<NWidgetMatrix>()->GetCurrentElement();
					this->SelectOtherObject(_selected_object_index); // Re-select the object for a different view.
				}
				break;
		}
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		const ObjectSpec *spec = ObjectClass::Get(_selected_object_class)->GetSpec(_selected_object_index);

		if (spec->size == OBJECT_SIZE_1X1) {
			VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_BUILD_OBJECT);
		} else {
			Command<CMD_BUILD_OBJECT>::Post(STR_ERROR_CAN_T_BUILD_OBJECT, CcPlaySound_CONSTRUCTION_OTHER, tile, spec->Index(), _selected_object_view);
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
		const ObjectSpec *spec = ObjectClass::Get(_selected_object_class)->GetSpec(_selected_object_index);
		Command<CMD_BUILD_OBJECT_AREA>::Post(STR_ERROR_CAN_T_BUILD_OBJECT, CcPlaySound_CONSTRUCTION_OTHER,
			end_tile, start_tile, spec->Index(), _selected_object_view, (_ctrl_pressed ? true : false));
	}

	void OnPlaceObjectAbort() override
	{
		this->UpdateButtons(_selected_object_class, -1, _selected_object_view);
	}

	EventState OnHotkey(int hotkey) override
	{
		switch (hotkey) {
			case BOHK_FOCUS_FILTER_BOX:
				this->SetFocusedWidget(WID_BO_FILTER);
				SetFocusedWindow(this); // The user has asked to give focus to the text box, so make sure this window is focused.
				break;

			default:
				return ES_NOT_HANDLED;
		}

		return ES_HANDLED;
	}

	void OnEditboxChanged(WidgetID widget) override
	{
		if (widget == WID_BO_FILTER) {
			string_filter.SetFilterTerm(this->filter_editbox.text.buf);
			this->object_classes.SetFilterState(!string_filter.IsEmpty());
			this->object_classes.ForceRebuild();
			this->InvalidateData();
		}
	}

	/**
	 * Select the first available object.
	 * @param change_class If true, change the class if no object in the current
	 *   class is available.
	 */
	void SelectFirstAvailableObject(bool change_class)
	{
		ObjectClass *objclass = ObjectClass::Get(_selected_object_class);

		/* First try to select an object in the selected class. */
		for (uint i = 0; i < objclass->GetSpecCount(); i++) {
			const ObjectSpec *spec = objclass->GetSpec(i);
			if (spec->IsAvailable()) {
				this->SelectOtherObject(i);
				return;
			}
		}
		if (change_class) {
			/* If that fails, select the first available object
			 * from a random class. */
			for (auto object_class_id : this->object_classes) {
				ObjectClass *objclass = ObjectClass::Get(object_class_id);
				for (uint i = 0; i < objclass->GetSpecCount(); i++) {
					const ObjectSpec *spec = objclass->GetSpec(i);
					if (spec->IsAvailable()) {
						this->SelectOtherClass(object_class_id);
						this->SelectOtherObject(i);
						return;
					}
				}
			}
		}
		/* If all objects are unavailable, select nothing... */
		if (objclass->GetUISpecCount() == 0) {
			/* ... but make sure that the class is not empty. */
			for (auto object_class_id : this->object_classes) {
				ObjectClass *objclass = ObjectClass::Get(object_class_id);
				if (objclass->GetUISpecCount() > 0) {
					this->SelectOtherClass(object_class_id);
					break;
				}
			}
		}
		this->SelectOtherObject(-1);
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
		Hotkey('F', "focus_filter_box", BOHK_FOCUS_FILTER_BOX),
	}, BuildObjectGlobalHotkeys};
};


Listing BuildObjectWindow::last_sorting = { false, 0 };
Filtering BuildObjectWindow::last_filtering = { false, 0 };

BuildObjectWindow::GUIObjectClassList::SortFunction * const BuildObjectWindow::sorter_funcs[] = {
	&ObjectClassIDSorter,
};

BuildObjectWindow::GUIObjectClassList::FilterFunction * const BuildObjectWindow::filter_funcs[] = {
	&TagNameFilter,
};

static const NWidgetPart _nested_build_object_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_OBJECT_BUILD_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
			NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0), SetPadding(WidgetDimensions::unscaled.picker),
				NWidget(NWID_VERTICAL), SetPIP(0, WidgetDimensions::unscaled.vsep_picker, 0),
					NWidget(NWID_HORIZONTAL), SetPIP(0, WidgetDimensions::unscaled.hsep_normal, 0),
						NWidget(WWT_TEXT, COLOUR_DARK_GREEN), SetFill(0, 1), SetDataTip(STR_LIST_FILTER_TITLE, STR_NULL),
						NWidget(WWT_EDITBOX, COLOUR_GREY, WID_BO_FILTER), SetFill(1, 0), SetResize(1, 0),
								SetDataTip(STR_LIST_FILTER_OSKTITLE, STR_LIST_FILTER_TOOLTIP),
					EndContainer(),
					NWidget(NWID_HORIZONTAL),
						NWidget(WWT_MATRIX, COLOUR_GREY, WID_BO_CLASS_LIST), SetFill(1, 0), SetMatrixDataTip(1, 0, STR_OBJECT_BUILD_CLASS_TOOLTIP), SetScrollbar(WID_BO_SCROLLBAR),
						NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_BO_SCROLLBAR),
					EndContainer(),
					NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetDataTip(STR_STATION_BUILD_ORIENTATION, STR_NULL), SetFill(1, 0),
					NWidget(NWID_HORIZONTAL), SetPIPRatio(1, 0, 1),
						NWidget(NWID_MATRIX, COLOUR_DARK_GREEN, WID_BO_OBJECT_MATRIX), SetPIP(0, 2, 0),
							NWidget(WWT_PANEL, COLOUR_GREY, WID_BO_OBJECT_SPRITE), SetDataTip(0x0, STR_OBJECT_BUILD_PREVIEW_TOOLTIP), EndContainer(),
						EndContainer(),
					EndContainer(),
					NWidget(WWT_TEXT, COLOUR_DARK_GREEN, WID_BO_OBJECT_NAME), SetDataTip(STR_JUST_STRING, STR_NULL), SetTextStyle(TC_ORANGE), SetAlignment(SA_CENTER),
					NWidget(WWT_TEXT, COLOUR_DARK_GREEN, WID_BO_OBJECT_SIZE), SetDataTip(STR_OBJECT_BUILD_SIZE, STR_NULL), SetAlignment(SA_CENTER),
				EndContainer(),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetScrollbar(WID_BO_SELECT_SCROLL),
					NWidget(NWID_MATRIX, COLOUR_DARK_GREEN, WID_BO_SELECT_MATRIX), SetPIP(0, 2, 0),
						NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BO_SELECT_IMAGE), SetMinimalSize(66, 60), SetDataTip(0x0, STR_OBJECT_BUILD_TOOLTIP),
								SetFill(0, 0), SetResize(0, 0), SetScrollbar(WID_BO_SELECT_SCROLL),
						EndContainer(),
					EndContainer(),
				EndContainer(),
			EndContainer(),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BO_INFO), SetPadding(WidgetDimensions::unscaled.framerect), SetFill(1, 0), SetResize(1, 0),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_BO_SELECT_SCROLL),
			NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _build_object_desc(__FILE__, __LINE__,
	WDP_AUTO, "build_object", 0, 0,
	WC_BUILD_OBJECT, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	std::begin(_nested_build_object_widgets), std::end(_nested_build_object_widgets),
	&BuildObjectWindow::hotkeys
);

/** Show our object picker.  */
Window *ShowBuildObjectPicker()
{
	/* Don't show the place object button when there are no objects to place. */
	if (ObjectClass::GetUIClassCount() > 0) {
		return AllocateWindowDescFront<BuildObjectWindow>(&_build_object_desc, 0);
	}
	return nullptr;
}

/** Reset all data of the object GUI. */
void InitializeObjectGui()
{
	_selected_object_class = ObjectClassID::OBJECT_CLASS_BEGIN;
}
