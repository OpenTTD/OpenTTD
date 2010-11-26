/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file object_gui.cpp The GUI for objects. */

#include "stdafx.h"
#include "command_func.h"
#include "core/geometry_func.hpp"
#include "newgrf.h"
#include "newgrf_object.h"
#include "newgrf_text.h"
#include "sprite.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "widgets/dropdown_type.h"
#include "window_gui.h"

#include "table/strings.h"

static ObjectClassID _selected_object_class; ///< the currently visible object class
static int _selected_object_index;           ///< the index of the selected object in the current class or -1

/** Object widgets in the object picker window. */
enum BuildObjectWidgets {
	BOW_CLASS_DROPDOWN, ///< The dropdown with classes.
	BOW_OBJECT_LIST,    ///< The list with objects of a given class.
	BOW_SCROLLBAR,      ///< The scrollbar associated with the list.
	BOW_OBJECT_SPRITE,  ///< A preview sprite of the object.
	BOW_OBJECT_SIZE,    ///< The size of an object.
	BOW_INFO,           ///< Other information about the object (from the NewGRF).
};

/** The window used for building objects. */
class BuildObjectWindow : public PickerWindowBase {
	static const int OBJECT_MARGIN = 4; ///< The margin (in pixels) around an object.
	int line_height;                    ///< The height of a single line.
	int object_height;                  ///< The height of the object box.
	int info_height;                    ///< The height of the info box.
	Scrollbar *vscroll;                 ///< The scollbar.

	/** Build a dropdown list of available object classes */
	static DropDownList *BuildObjectClassDropDown()
	{
		DropDownList *list = new DropDownList();

		for (uint i = 0; i < ObjectClass::GetCount(); i++) {
			list->push_back(new DropDownListStringItem(ObjectClass::GetName((ObjectClassID)i), i, false));
		}

		return list;
	}

public:
	BuildObjectWindow(const WindowDesc *desc, Window *w) : PickerWindowBase(w), info_height(1)
	{
		this->CreateNestedTree(desc);

		this->vscroll = this->GetScrollbar(BOW_SCROLLBAR);
		this->vscroll->SetCapacity(5);
		this->vscroll->SetPosition(0);

		this->FinishInitNested(desc, 0);

		this->vscroll->SetCount(ObjectClass::GetCount(_selected_object_class));
		this->SelectFirstAvailableObject(true);
	}

	virtual ~BuildObjectWindow()
	{
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case BOW_CLASS_DROPDOWN:
				SetDParam(0, ObjectClass::GetName(_selected_object_class));
				break;

			case BOW_OBJECT_SIZE: {
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class, _selected_object_index);
				int size = spec == NULL ? 0 : spec->size;
				SetDParam(0, GB(size, 0, 4));
				SetDParam(1, GB(size, 4, 4));
				break;
			}

			default: break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case BOW_CLASS_DROPDOWN: {
				Dimension d = {0, 0};
				for (uint i = 0; i < ObjectClass::GetCount(); i++) {
					SetDParam(0, ObjectClass::GetName((ObjectClassID)i));
					d = maxdim(d, GetStringBoundingBox(STR_BLACK_STRING));
				}
				d.width += padding.width;
				d.height += padding.height;
				*size = maxdim(*size, d);
				break;
			}

			case BOW_OBJECT_LIST: {
				for (int i = 0; i < NUM_OBJECTS; i++) {
					const ObjectSpec *spec = ObjectSpec::Get(i);
					if (!spec->enabled) continue;

					size->width = max(size->width, GetStringBoundingBox(spec->name).width);
				}

				this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;
				size->height = this->vscroll->GetCapacity() * this->line_height;
				break;
			}

			case BOW_OBJECT_SPRITE: {
				byte height = 0;
				for (int i = 0; i < NUM_OBJECTS; i++) {
					height = max(ObjectSpec::Get(i)->height, height);
				}
				this->object_height = height * TILE_HEIGHT;
				size->height = TILE_PIXELS + this->object_height + 2 * OBJECT_MARGIN;
				break;
			}

			case BOW_INFO:
				size->height = this->info_height;
				break;

			default: break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case BOW_OBJECT_LIST: {
				int y = r.top;
				for (uint i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && i < ObjectClass::GetCount(_selected_object_class); i++) {
					const ObjectSpec *spec = ObjectClass::Get(_selected_object_class, i);
					if (!spec->IsAvailable()) {
						GfxFillRect(r.left + 1, y + 1, r.right - 1, y + this->line_height - 2, 0, FILLRECT_CHECKER);
					}
					DrawString(r.left + WD_MATRIX_LEFT, r.right + WD_MATRIX_RIGHT, y + WD_MATRIX_TOP, spec->name, ((int)i == _selected_object_index) ? TC_WHITE : TC_BLACK);
					y += this->line_height;
				}
				break;
			}

			case BOW_OBJECT_SPRITE: {
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class, _selected_object_index);
				if (spec == NULL) break;

				DrawPixelInfo tmp_dpi;
				/* Set up a clipping area for the preview. */
				if (FillDrawPixelInfo(&tmp_dpi, r.left, r.top, r.right - r.left + 1, r.bottom - r.top + 1)) {
					DrawPixelInfo *old_dpi = _cur_dpi;
					_cur_dpi = &tmp_dpi;
					if (spec->grf_prop.grffile == NULL) {
						extern const DrawTileSprites _objects[];
						const DrawTileSprites *dts = &_objects[spec->grf_prop.local_id];
						DrawOrigTileSeqInGUI((r.right - r.left) / 2 - 1, this->object_height + OBJECT_MARGIN, dts, PAL_NONE);
					} else {
						DrawNewObjectTileInGUI((r.right - r.left) / 2 - 1, this->object_height + OBJECT_MARGIN, spec);
					}
					_cur_dpi = old_dpi;
				}
				break;
			}

			case BOW_INFO: {
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class, _selected_object_index);
				if (spec == NULL) break;

				/* Get the extra message for the GUI */
				if (HasBit(spec->callback_mask, CBM_OBJ_FUND_MORE_TEXT)) {
					uint16 callback_res = GetObjectCallback(CBID_OBJECT_FUND_MORE_TEXT, 0, 0, spec, NULL, INVALID_TILE);
					if (callback_res != CALLBACK_FAILED) {
						StringID message = GetGRFStringID(spec->grf_prop.grffile->grfid, 0xD000 + callback_res);
						if (message != STR_NULL && message != STR_UNDEFINED) {
							PrepareTextRefStackUsage(6);
							/* Use all the available space left from where we stand up to the
							 * end of the window. We ALSO enlarge the window if needed, so we
							 * can 'go' wild with the bottom of the window. */
							int y = DrawStringMultiLine(r.left, r.right, r.top, UINT16_MAX, message) - r.top;
							StopTextRefStackUsage();
							if (y > this->info_height) {
								BuildObjectWindow *bow = const_cast<BuildObjectWindow *>(this);
								bow->info_height = y + 2;
								bow->ReInit();
							}
						}
					}
				}
			}
		}
	}

	void SelectOtherObject(int object_index)
	{
		_selected_object_index = object_index;

		this->UpdateSelectSize();
		this->SetDirty();
	}

	void UpdateSelectSize()
	{
		if (_selected_object_index == -1) {
			SetTileSelectSize(1, 1);
		} else {
			const ObjectSpec *spec = ObjectClass::Get(_selected_object_class, _selected_object_index);
			int w = GB(spec->size, 0, 4);
			int h = GB(spec->size, 4, 4);
			SetTileSelectSize(w, h);
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case BOW_CLASS_DROPDOWN:
				ShowDropDownList(this, BuildObjectClassDropDown(), _selected_object_class, BOW_CLASS_DROPDOWN);
				break;

			case BOW_OBJECT_LIST: {
				int num_clicked = this->vscroll->GetPosition() + (pt.y - this->nested_array[widget]->pos_y) / this->line_height;
				if (num_clicked >= this->vscroll->GetCount()) break;
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class, num_clicked);
				if (spec->IsAvailable()) this->SelectOtherObject(num_clicked);
				break;
			}
		}
	}

	/**
	 * Select the first available object.
	 * @param change_class If true, change the class if no object in the current
	 *   class is available.
	 */
	void SelectFirstAvailableObject(bool change_class)
	{
		/* First try to select an object in the selected class. */
		for (uint i = 0; i < ObjectClass::GetCount(_selected_object_class); i++) {
			const ObjectSpec *spec = ObjectClass::Get(_selected_object_class, i);
			if (spec->IsAvailable()) {
				this->SelectOtherObject(0);
				return;
			}
		}
		if (change_class) {
			/* If that fails, select the first available object
			 * from a random class. */
			for (ObjectClassID j = OBJECT_CLASS_BEGIN; j < OBJECT_CLASS_MAX; j++) {
				for (uint i = 0; i < ObjectClass::GetCount(j); i++) {
					const ObjectSpec *spec = ObjectClass::Get(j, i);
					if (spec->IsAvailable()) {
						_selected_object_class = j;
						this->SelectOtherObject(i);
						return;
					}
				}
			}
		}
		/* If all objects are unavailable, select nothing. */
		this->SelectOtherObject(-1);
	}

	virtual void OnDropdownSelect(int widget, int index)
	{
		assert(widget == BOW_CLASS_DROPDOWN);
		_selected_object_class = (ObjectClassID)index;
		this->vscroll->SetCount(ObjectClass::GetCount(_selected_object_class));
		this->SelectFirstAvailableObject(false);
	}
};

static const NWidgetPart _nested_build_object_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_OBJECT_BUILD_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetFill(1, 0),
		NWidget(WWT_LABEL, COLOUR_DARK_GREEN), SetPadding(2, 5, 2, 5), SetDataTip(STR_OBJECT_BUILD_CLASS_LABEL, STR_NULL), SetFill(1, 0),
		NWidget(WWT_DROPDOWN, COLOUR_GREY, BOW_CLASS_DROPDOWN), SetPadding(0, 5, 2, 5), SetFill(1, 0), SetDataTip(STR_BLACK_STRING, STR_OBJECT_BUILD_TOOLTIP),
		NWidget(NWID_HORIZONTAL), SetPadding(0, 5, 2, 5),
			NWidget(WWT_MATRIX, COLOUR_GREY, BOW_OBJECT_LIST), SetFill(1, 0), SetDataTip(0x501, STR_OBJECT_BUILD_TOOLTIP), SetScrollbar(BOW_SCROLLBAR),
			NWidget(NWID_VSCROLLBAR, COLOUR_GREY, BOW_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_HORIZONTAL), SetPadding(0, 5, 0, 5),
			NWidget(WWT_PANEL, COLOUR_GREY, BOW_OBJECT_SPRITE), SetMinimalSize(130, 0), SetFill(1, 0), SetDataTip(0x0, STR_STATION_BUILD_RAILROAD_ORIENTATION_TOOLTIP), EndContainer(),
		EndContainer(),
		NWidget(WWT_TEXT, COLOUR_DARK_GREEN, BOW_OBJECT_SIZE), SetDataTip(STR_OBJECT_BUILD_SIZE, STR_NULL), SetPadding(2, 5, 2, 5),
		NWidget(WWT_EMPTY, COLOUR_DARK_GREEN, BOW_INFO), SetPadding(2, 5, 0, 5),
	EndContainer(),
};

static const WindowDesc _build_object_desc(
	WDP_AUTO, 0, 0,
	WC_BUILD_OBJECT, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_build_object_widgets, lengthof(_nested_build_object_widgets)
);

/**
 * Show our object picker.
 * @param w The toolbar window we're associated with.
 */
void ShowBuildObjectPicker(Window *w)
{
	new BuildObjectWindow(&_build_object_desc, w);
}

/** Reset all data of the object GUI. */
void InitializeObjectGui()
{
	_selected_object_class = (ObjectClassID)0;
}

/**
 * PlaceProc function, called when someone pressed the button if the
 *  object-tool is selected
 * @param tile on which to place the object
 */
void PlaceProc_Object(TileIndex tile)
{
	DoCommandP(tile, ObjectClass::Get(_selected_object_class, _selected_object_index)->Index(), 0, CMD_BUILD_OBJECT | CMD_MSG(STR_ERROR_CAN_T_BUILD_OBJECT), CcTerraform);
}
