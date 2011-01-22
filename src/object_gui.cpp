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
#include "newgrf.h"
#include "newgrf_object.h"
#include "newgrf_text.h"
#include "sprite.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "window_gui.h"

#include "table/strings.h"

static ObjectClassID _selected_object_class; ///< the currently visible object class
static int _selected_object_index;           ///< the index of the selected object in the current class or -1
static uint8 _selected_object_view;          ///< the view of the selected object

/** Object widgets in the object picker window. */
enum BuildObjectWidgets {
	BOW_CLASS_LIST,     ///< The list with classes.
	BOW_SCROLLBAR,      ///< The scrollbar associated with the list.
	BOW_OBJECT_MATRIX,  ///< The matrix with preview sprites.
	BOW_OBJECT_SPRITE,  ///< A preview sprite of the object.
	BOW_OBJECT_SIZE,    ///< The size of an object.
	BOW_INFO,           ///< Other information about the object (from the NewGRF).

	BOW_SELECT_MATRIX,  ///< Selection preview matrix of objects of a given class.
	BOW_SELECT_IMAGE,   ///< Preview image in the #BOW_SELECT_MATRIX.
	BOW_SELECT_SCROLL,  ///< Scrollbar next to the #BOW_SELECT_MATRIX.
};

/** The window used for building objects. */
class BuildObjectWindow : public PickerWindowBase {
	static const int OBJECT_MARGIN = 4; ///< The margin (in pixels) around an object.
	int line_height;                    ///< The height of a single line.
	int object_height;                  ///< The height of the object box.
	int info_height;                    ///< The height of the info box.
	Scrollbar *vscroll;                 ///< The scollbar.

public:
	BuildObjectWindow(const WindowDesc *desc, Window *w) : PickerWindowBase(w), info_height(1)
	{
		this->CreateNestedTree(desc);

		this->vscroll = this->GetScrollbar(BOW_SCROLLBAR);
		this->vscroll->SetCapacity(5);
		this->vscroll->SetPosition(0);
		this->vscroll->SetCount(ObjectClass::GetCount());

		this->FinishInitNested(desc, 0);

		this->SelectFirstAvailableObject(true);
		this->GetWidget<NWidgetMatrix>(BOW_OBJECT_MATRIX)->SetCount(4);

		NWidgetMatrix *matrix = this->GetWidget<NWidgetMatrix>(BOW_SELECT_MATRIX);
		matrix->SetScrollbar(this->GetScrollbar(BOW_SELECT_SCROLL));
		matrix->SetCount(ObjectClass::GetCount(_selected_object_class));
	}

	virtual ~BuildObjectWindow()
	{
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case BOW_OBJECT_SIZE: {
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class, _selected_object_index);
				int size = spec == NULL ? 0 : spec->size;
				SetDParam(0, GB(size, HasBit(_selected_object_view, 0) ? 4 : 0, 4));
				SetDParam(1, GB(size, HasBit(_selected_object_view, 0) ? 0 : 4, 4));
				break;
			}

			default: break;
		}
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case BOW_CLASS_LIST: {
				for (uint i = 0; i < ObjectClass::GetCount(); i++) {
					size->width = max(size->width, GetStringBoundingBox(ObjectClass::GetName((ObjectClassID)i)).width);
				}
				size->width += padding.width;
				this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;
				resize->height = this->line_height;
				size->height = this->vscroll->GetCapacity() * this->line_height;
				break;
			}

			case BOW_OBJECT_MATRIX: {
				/* Get the right amount of buttons based on the current spec. */
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class, _selected_object_index);
				if (spec != NULL) {
					if (spec->views >= 2) size->width  += resize->width;
					if (spec->views >= 4) size->height += resize->height;
				}
				resize->width = 0;
				resize->height = 0;
				break;
			}

			case BOW_OBJECT_SPRITE: {
				bool two_wide = false;  // Whether there will be two widgets next to eachother in the matrix or not.
				int height[2] = {0, 0}; // The height for the different views; in this case views 1/2 and 4.

				/* Get the height and view information. */
				for (int i = 0; i < NUM_OBJECTS; i++) {
					const ObjectSpec *spec = ObjectSpec::Get(i);
					if (!spec->enabled) continue;
					two_wide |= spec->views >= 2;
					height[spec->views / 4] = max<int>(ObjectSpec::Get(i)->height, height[spec->views / 4]);
				}

				/* Determine the pixel heights. */
				for (size_t i = 0; i < lengthof(height); i++) {
					height[i] *= TILE_HEIGHT;
					height[i] += TILE_PIXELS + 2 * OBJECT_MARGIN;
				}

				/* Now determine the size of the minimum widgets. When there are two columns, then
				 * we want these columns to be slightly less wide. When there are two rows, then
				 * determine the size of the widgets based on the maximum size for a single row
				 * of widgets, or just the twice the widget height of the two row ones. */
				size->height = max(height[0], height[1] * 2 + 2);
				if (two_wide) {
					size->width  = (3 * TILE_PIXELS + 2 * OBJECT_MARGIN) * 2 + 2;
				} else {
					size->width  = 4 * TILE_PIXELS + 2 * OBJECT_MARGIN;
				}

				/* Get the right size for the single widget based on the current spec. */
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class, _selected_object_index);
				if (spec != NULL) {
					if (spec->views >= 2) size->width  = size->width  / 2 - 1;
					if (spec->views >= 4) size->height = size->height / 2 - 1;
				}
				break;
			}

			case BOW_INFO:
				size->height = this->info_height;
				break;

			case BOW_SELECT_MATRIX:
				fill->height = 1;
				resize->height = 1;
				break;

			default: break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (GB(widget, 0, 16)) {
			case BOW_CLASS_LIST: {
				int y = r.top;
				for (uint i = this->vscroll->GetPosition(); this->vscroll->IsVisible(i) && i < ObjectClass::GetCount(); i++) {
					SetDParam(0, ObjectClass::GetName((ObjectClassID)i));
					DrawString(r.left + WD_MATRIX_LEFT, r.right + WD_MATRIX_RIGHT, y + WD_MATRIX_TOP, STR_JUST_STRING,
							((int)i == _selected_object_class) ? TC_WHITE : TC_BLACK);
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
						DrawOrigTileSeqInGUI((r.right - r.left) / 2 - 1, r.bottom - r.top - OBJECT_MARGIN - TILE_PIXELS, dts, PAL_NONE);
					} else {
						DrawNewObjectTileInGUI((r.right - r.left) / 2 - 1, r.bottom - r.top - OBJECT_MARGIN - TILE_PIXELS, spec, GB(widget, 16, 16));
					}
					_cur_dpi = old_dpi;
				}
				break;
			}

			case BOW_SELECT_IMAGE: {
				if (_selected_object_index < 0) break;

				int obj_index = GB(widget, 16, 16);
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class, obj_index);
				if (spec == NULL) break;

				if (!spec->IsAvailable()) {
					GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, 0, FILLRECT_CHECKER);
				}
				DrawPixelInfo tmp_dpi;
				/* Set up a clipping area for the preview. */
				if (FillDrawPixelInfo(&tmp_dpi, r.left + 1, r.top, (r.right - 1) - (r.left + 1) + 1, r.bottom - r.top + 1)) {
					DrawPixelInfo *old_dpi = _cur_dpi;
					_cur_dpi = &tmp_dpi;
					if (spec->grf_prop.grffile == NULL) {
						extern const DrawTileSprites _objects[];
						const DrawTileSprites *dts = &_objects[spec->grf_prop.local_id];
						DrawOrigTileSeqInGUI((r.right - r.left) / 2 - 1, r.bottom - r.top - OBJECT_MARGIN - TILE_PIXELS, dts, PAL_NONE);
					} else {
						DrawNewObjectTileInGUI((r.right - r.left) / 2 - 1, r.bottom - r.top - OBJECT_MARGIN - TILE_PIXELS, spec,
								min(_selected_object_view, spec->views - 1));
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

	/**
	 * Select the specified object in #_selected_object_class class.
	 * @param object_index Object index to select, \c -1 means select nothing.
	 */
	void SelectOtherObject(int object_index)
	{
		_selected_object_index = object_index;
		if (_selected_object_index != -1) {
			const ObjectSpec *spec = ObjectClass::Get(_selected_object_class, _selected_object_index);
			_selected_object_view = min(_selected_object_view, spec->views - 1);
			this->ReInit();
		} else {
			_selected_object_view = 0;
		}

		this->GetWidget<NWidgetMatrix>(BOW_OBJECT_MATRIX)->SetClicked(_selected_object_view);
		this->GetWidget<NWidgetMatrix>(BOW_SELECT_MATRIX)->SetClicked(_selected_object_index);
		this->UpdateSelectSize();
		this->SetDirty();
	}

	void UpdateSelectSize()
	{
		if (_selected_object_index == -1) {
			SetTileSelectSize(1, 1);
		} else {
			const ObjectSpec *spec = ObjectClass::Get(_selected_object_class, _selected_object_index);
			int w = GB(spec->size, HasBit(_selected_object_view, 0) ? 4 : 0, 4);
			int h = GB(spec->size, HasBit(_selected_object_view, 0) ? 0 : 4, 4);
			SetTileSelectSize(w, h);
		}
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, BOW_CLASS_LIST);
		this->GetWidget<NWidgetCore>(BOW_CLASS_LIST)->widget_data = (this->vscroll->GetCapacity() << MAT_ROW_START) + (1 << MAT_COL_START);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (GB(widget, 0, 16)) {
			case BOW_CLASS_LIST: {
				int num_clicked = this->vscroll->GetPosition() + (pt.y - this->nested_array[widget]->pos_y) / this->line_height;
				if (num_clicked >= (int)ObjectClass::GetCount()) break;

				_selected_object_class = (ObjectClassID)num_clicked;
				this->GetWidget<NWidgetMatrix>(BOW_SELECT_MATRIX)->SetCount(ObjectClass::GetCount(_selected_object_class));
				this->SelectFirstAvailableObject(false);
				break;
			}

			case BOW_SELECT_IMAGE: {
				int num_clicked = GB(widget, 16, 16);
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class, num_clicked);
				if (spec->IsAvailable()) this->SelectOtherObject(num_clicked);
				break;
			}

			case BOW_OBJECT_SPRITE:
				if (_selected_object_index != -1) {
					_selected_object_view = GB(widget, 16, 16);
					this->GetWidget<NWidgetMatrix>(BOW_OBJECT_MATRIX)->SetClicked(_selected_object_view);
					this->UpdateSelectSize();
					this->SetDirty();
				}
				break;
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
};

static const NWidgetPart _nested_build_object_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_OBJECT_BUILD_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_HORIZONTAL), SetPadding(2, 0, 0, 0),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_HORIZONTAL), SetPadding(0, 5, 2, 5),
					NWidget(WWT_MATRIX, COLOUR_GREY, BOW_CLASS_LIST), SetFill(1, 0), SetDataTip(0x501, STR_OBJECT_BUILD_CLASS_TOOLTIP), SetScrollbar(BOW_SCROLLBAR),
					NWidget(NWID_VSCROLLBAR, COLOUR_GREY, BOW_SCROLLBAR),
				EndContainer(),
				NWidget(NWID_HORIZONTAL), SetPadding(0, 5, 0, 5),
					NWidget(NWID_MATRIX, COLOUR_DARK_GREEN, BOW_OBJECT_MATRIX), SetPIP(0, 2, 0),
						NWidget(WWT_PANEL, COLOUR_GREY, BOW_OBJECT_SPRITE), SetDataTip(0x0, STR_OBJECT_BUILD_PREVIEW_TOOLTIP), EndContainer(),
					EndContainer(),
				EndContainer(),
				NWidget(WWT_TEXT, COLOUR_DARK_GREEN, BOW_OBJECT_SIZE), SetDataTip(STR_OBJECT_BUILD_SIZE, STR_NULL), SetPadding(2, 5, 2, 5),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetScrollbar(BOW_SELECT_SCROLL),
				NWidget(NWID_HORIZONTAL),
					NWidget(NWID_MATRIX, COLOUR_DARK_GREEN, BOW_SELECT_MATRIX), SetFill(0, 1), SetPIP(0, 2, 0),
						NWidget(WWT_PANEL, COLOUR_DARK_GREEN, BOW_SELECT_IMAGE), SetMinimalSize(66, 60), SetDataTip(0x0, STR_OBJECT_BUILD_TOOLTIP),
								SetFill(0, 0), SetResize(0, 0), SetScrollbar(BOW_SELECT_SCROLL),
						EndContainer(),
					EndContainer(),
					NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, BOW_SELECT_SCROLL),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, INVALID_COLOUR, BOW_INFO), SetPadding(2, 5, 0, 5), SetFill(1, 0), SetResize(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetFill(0, 1), EndContainer(),
				NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
			EndContainer(),
		EndContainer(),
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
	DoCommandP(tile, ObjectClass::Get(_selected_object_class, _selected_object_index)->Index(), _selected_object_view, CMD_BUILD_OBJECT | CMD_MSG(STR_ERROR_CAN_T_BUILD_OBJECT), CcTerraform);
}
