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
#include "strings_func.h"
#include "viewport_func.h"
#include "tilehighlight_func.h"
#include "window_gui.h"
#include "window_func.h"
#include "zoom_func.h"

#include "widgets/object_widget.h"

#include "table/strings.h"

#include "safeguards.h"

static ObjectClassID _selected_object_class; ///< the currently visible object class
static int _selected_object_index;           ///< the index of the selected object in the current class or -1
static uint8 _selected_object_view;          ///< the view of the selected object

/** The window used for building objects. */
class BuildObjectWindow : public Window {
	static const int OBJECT_MARGIN = 4; ///< The margin (in pixels) around an object.
	int line_height;                    ///< The height of a single line.
	int info_height;                    ///< The height of the info box.
	Scrollbar *vscroll;                 ///< The scrollbar.

	/** Scroll #WID_BO_CLASS_LIST so that the selected object class is visible. */
	void EnsureSelectedObjectClassIsVisible()
	{
		uint pos = 0;
		for (int i = 0; i < _selected_object_class; i++) {
			if (ObjectClass::Get((ObjectClassID) i)->GetUISpecCount() == 0) continue;
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

		ObjectClass *sel_objclass = ObjectClass::Get(_selected_object_class);
		if ((int)sel_objclass->GetSpecCount() <= _selected_object_index) return false;

		return sel_objclass->GetSpec(_selected_object_index)->IsAvailable();
	}

	/**
	 * Calculate the number of columns of the #WID_BO_SELECT_MATRIX widget.
	 * @return Number of columns in the matrix.
	 */
	uint GetMatrixColumnCount()
	{
		const NWidgetBase *matrix = this->GetWidget<NWidgetBase>(WID_BO_SELECT_MATRIX);
		return 1 + (matrix->current_x - matrix->smallest_x) / matrix->resize_x;
	}

public:
	BuildObjectWindow(WindowDesc *desc, WindowNumber number) : Window(desc), info_height(1)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_BO_SCROLLBAR);
		this->FinishInitNested(number);

		ResetObjectToPlace();

		this->vscroll->SetPosition(0);
		this->vscroll->SetCount(ObjectClass::GetUIClassCount());

		NWidgetMatrix *matrix = this->GetWidget<NWidgetMatrix>(WID_BO_SELECT_MATRIX);
		matrix->SetScrollbar(this->GetScrollbar(WID_BO_SELECT_SCROLL));

		this->SelectOtherClass(_selected_object_class);
		if (this->CanRestoreSelectedObject()) {
			this->SelectOtherObject(_selected_object_index);
		} else {
			this->SelectFirstAvailableObject(true);
		}
		assert(ObjectClass::Get(_selected_object_class)->GetUISpecCount() > 0); // object GUI should be disables elsewise
		this->EnsureSelectedObjectClassIsVisible();
		this->GetWidget<NWidgetMatrix>(WID_BO_OBJECT_MATRIX)->SetCount(4);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch (widget) {
			case WID_BO_OBJECT_NAME: {
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class)->GetSpec(_selected_object_index);
				SetDParam(0, spec != NULL ? spec->name : STR_EMPTY);
				break;
			}

			case WID_BO_OBJECT_SIZE: {
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class)->GetSpec(_selected_object_index);
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
			case WID_BO_CLASS_LIST: {
				for (uint i = 0; i < ObjectClass::GetClassCount(); i++) {
					ObjectClass *objclass = ObjectClass::Get((ObjectClassID)i);
					if (objclass->GetUISpecCount() == 0) continue;
					size->width = max(size->width, GetStringBoundingBox(objclass->name).width);
				}
				size->width += padding.width;
				this->line_height = FONT_HEIGHT_NORMAL + WD_MATRIX_TOP + WD_MATRIX_BOTTOM;
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
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class)->GetSpec(_selected_object_index);
				if (spec != NULL) {
					if (spec->views >= 2) size->width  += resize->width;
					if (spec->views >= 4) size->height += resize->height;
				}
				resize->width = 0;
				resize->height = 0;
				break;
			}

			case WID_BO_OBJECT_SPRITE: {
				bool two_wide = false;  // Whether there will be two widgets next to each other in the matrix or not.
				int height[2] = {0, 0}; // The height for the different views; in this case views 1/2 and 4.

				/* Get the height and view information. */
				for (int i = 0; i < NUM_OBJECTS; i++) {
					const ObjectSpec *spec = ObjectSpec::Get(i);
					if (!spec->IsEverAvailable()) continue;
					two_wide |= spec->views >= 2;
					height[spec->views / 4] = max<int>(ObjectSpec::Get(i)->height, height[spec->views / 4]);
				}

				/* Determine the pixel heights. */
				for (size_t i = 0; i < lengthof(height); i++) {
					height[i] *= ScaleGUITrad(TILE_HEIGHT);
					height[i] += ScaleGUITrad(TILE_PIXELS) + 2 * OBJECT_MARGIN;
				}

				/* Now determine the size of the minimum widgets. When there are two columns, then
				 * we want these columns to be slightly less wide. When there are two rows, then
				 * determine the size of the widgets based on the maximum size for a single row
				 * of widgets, or just the twice the widget height of the two row ones. */
				size->height = max(height[0], height[1] * 2 + 2);
				if (two_wide) {
					size->width  = (3 * ScaleGUITrad(TILE_PIXELS) + 2 * OBJECT_MARGIN) * 2 + 2;
				} else {
					size->width  = 4 * ScaleGUITrad(TILE_PIXELS) + 2 * OBJECT_MARGIN;
				}

				/* Get the right size for the single widget based on the current spec. */
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class)->GetSpec(_selected_object_index);
				if (spec != NULL) {
					if (spec->views >= 2) size->width  = size->width  / 2 - 1;
					if (spec->views >= 4) size->height = size->height / 2 - 1;
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
				size->width  = ScaleGUITrad(64) + 2;
				size->height = ScaleGUITrad(58) + 2;
				break;

			default: break;
		}
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (GB(widget, 0, 16)) {
			case WID_BO_CLASS_LIST: {
				int y = r.top;
				uint pos = 0;
				for (uint i = 0; i < ObjectClass::GetClassCount(); i++) {
					ObjectClass *objclass = ObjectClass::Get((ObjectClassID)i);
					if (objclass->GetUISpecCount() == 0) continue;
					if (!this->vscroll->IsVisible(pos++)) continue;
					DrawString(r.left + WD_MATRIX_LEFT, r.right - WD_MATRIX_RIGHT, y + WD_MATRIX_TOP, objclass->name,
							((int)i == _selected_object_class) ? TC_WHITE : TC_BLACK);
					y += this->line_height;
				}
				break;
			}

			case WID_BO_OBJECT_SPRITE: {
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class)->GetSpec(_selected_object_index);
				if (spec == NULL) break;

				/* Height of the selection matrix.
				 * Depending on the number of views, the matrix has a 1x1, 1x2, 2x1 or 2x2 layout. To make the previews
				 * look nice in all layouts, we use the 4x4 layout (smallest previews) as starting point. For the bigger
				 * previews in the layouts with less views we add space homogeneously on all sides, so the 4x4 preview-rectangle
				 * is centered in the 2x1, 1x2 resp. 1x1 buttons. */
				uint matrix_height = this->GetWidget<NWidgetMatrix>(WID_BO_OBJECT_MATRIX)->current_y;

				DrawPixelInfo tmp_dpi;
				/* Set up a clipping area for the preview. */
				if (FillDrawPixelInfo(&tmp_dpi, r.left, r.top, r.right - r.left + 1, r.bottom - r.top + 1)) {
					DrawPixelInfo *old_dpi = _cur_dpi;
					_cur_dpi = &tmp_dpi;
					if (spec->grf_prop.grffile == NULL) {
						extern const DrawTileSprites _objects[];
						const DrawTileSprites *dts = &_objects[spec->grf_prop.local_id];
						DrawOrigTileSeqInGUI((r.right - r.left) / 2 - 1, (r.bottom - r.top + matrix_height / 2) / 2 - OBJECT_MARGIN - ScaleGUITrad(TILE_PIXELS), dts, PAL_NONE);
					} else {
						DrawNewObjectTileInGUI((r.right - r.left) / 2 - 1, (r.bottom - r.top + matrix_height / 2) / 2 - OBJECT_MARGIN - ScaleGUITrad(TILE_PIXELS), spec, GB(widget, 16, 16));
					}
					_cur_dpi = old_dpi;
				}
				break;
			}

			case WID_BO_SELECT_IMAGE: {
				ObjectClass *objclass = ObjectClass::Get(_selected_object_class);
				int obj_index = objclass->GetIndexFromUI(GB(widget, 16, 16));
				if (obj_index < 0) break;
				const ObjectSpec *spec = objclass->GetSpec(obj_index);
				if (spec == NULL) break;

				if (!spec->IsAvailable()) {
					GfxFillRect(r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, PC_BLACK, FILLRECT_CHECKER);
				}
				DrawPixelInfo tmp_dpi;
				/* Set up a clipping area for the preview. */
				if (FillDrawPixelInfo(&tmp_dpi, r.left + 1, r.top, (r.right - 1) - (r.left + 1) + 1, r.bottom - r.top + 1)) {
					DrawPixelInfo *old_dpi = _cur_dpi;
					_cur_dpi = &tmp_dpi;
					if (spec->grf_prop.grffile == NULL) {
						extern const DrawTileSprites _objects[];
						const DrawTileSprites *dts = &_objects[spec->grf_prop.local_id];
						DrawOrigTileSeqInGUI((r.right - r.left) / 2 - 1, r.bottom - r.top - OBJECT_MARGIN - ScaleGUITrad(TILE_PIXELS), dts, PAL_NONE);
					} else {
						DrawNewObjectTileInGUI((r.right - r.left) / 2 - 1, r.bottom - r.top - OBJECT_MARGIN - ScaleGUITrad(TILE_PIXELS), spec,
								min(_selected_object_view, spec->views - 1));
					}
					_cur_dpi = old_dpi;
				}
				break;
			}

			case WID_BO_INFO: {
				const ObjectSpec *spec = ObjectClass::Get(_selected_object_class)->GetSpec(_selected_object_index);
				if (spec == NULL) break;

				/* Get the extra message for the GUI */
				if (HasBit(spec->callback_mask, CBM_OBJ_FUND_MORE_TEXT)) {
					uint16 callback_res = GetObjectCallback(CBID_OBJECT_FUND_MORE_TEXT, 0, 0, spec, NULL, INVALID_TILE, _selected_object_view);
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
								int y = DrawStringMultiLine(r.left, r.right, r.top, UINT16_MAX, message, TC_ORANGE) - r.top;
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
	}

	/**
	 * Select the specified object class.
	 * @param object_class_index Object class index to select.
	 */
	void SelectOtherClass(ObjectClassID object_class_index)
	{
		_selected_object_class = object_class_index;
		this->GetWidget<NWidgetMatrix>(WID_BO_SELECT_MATRIX)->SetCount(ObjectClass::Get(_selected_object_class)->GetUISpecCount());
	}

	/**
	 * Select the specified object in #_selected_object_class class.
	 * @param object_index Object index to select, \c -1 means select nothing.
	 */
	void SelectOtherObject(int object_index)
	{
		_selected_object_index = object_index;
		if (_selected_object_index != -1) {
			const ObjectSpec *spec = ObjectClass::Get(_selected_object_class)->GetSpec(_selected_object_index);
			_selected_object_view = min(_selected_object_view, spec->views - 1);
			this->ReInit();
		} else {
			_selected_object_view = 0;
		}

		if (_selected_object_index != -1) {
			SetObjectToPlaceWnd(SPR_CURSOR_TRANSMITTER, PAL_NONE, HT_RECT, this);
		}

		this->UpdateButtons(_selected_object_class, _selected_object_index, _selected_object_view);
	}

	void UpdateSelectSize()
	{
		if (_selected_object_index == -1) {
			SetTileSelectSize(1, 1);
		} else {
			const ObjectSpec *spec = ObjectClass::Get(_selected_object_class)->GetSpec(_selected_object_index);
			int w = GB(spec->size, HasBit(_selected_object_view, 0) ? 4 : 0, 4);
			int h = GB(spec->size, HasBit(_selected_object_view, 0) ? 0 : 4, 4);
			SetTileSelectSize(w, h);
		}
	}

	/**
	 * Update buttons to show the selection to the user.
	 * @param sel_class The class of the selected object.
	 * @param sel_index Index of the object to select, or \c -1 .
	 * @param sel_view View of the object to select.
	 */
	void UpdateButtons(ObjectClassID sel_class, int sel_index, uint sel_view)
	{
		int view_number, object_number;
		if (sel_index == -1) {
			view_number = -1; // If no object selected, also hide the selected view.
			object_number = -1;
		} else {
			view_number = sel_view;
			object_number = ObjectClass::Get(sel_class)->GetUIFromIndex(sel_index);
		}

		this->GetWidget<NWidgetMatrix>(WID_BO_OBJECT_MATRIX)->SetClicked(view_number);
		this->GetWidget<NWidgetMatrix>(WID_BO_SELECT_MATRIX)->SetClicked(object_number);
		this->UpdateSelectSize();
		this->SetDirty();
	}

	virtual void OnResize()
	{
		this->vscroll->SetCapacityFromWidget(this, WID_BO_CLASS_LIST);
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (GB(widget, 0, 16)) {
			case WID_BO_CLASS_LIST: {
				int num_clicked = this->vscroll->GetPosition() + (pt.y - this->nested_array[widget]->pos_y) / this->line_height;
				if (num_clicked >= (int)ObjectClass::GetUIClassCount()) break;

				this->SelectOtherClass(ObjectClass::GetUIClass(num_clicked));
				this->SelectFirstAvailableObject(false);
				break;
			}

			case WID_BO_SELECT_IMAGE: {
				ObjectClass *objclass = ObjectClass::Get(_selected_object_class);
				int num_clicked = objclass->GetIndexFromUI(GB(widget, 16, 16));
				if (num_clicked >= 0 && objclass->GetSpec(num_clicked)->IsAvailable()) this->SelectOtherObject(num_clicked);
				break;
			}

			case WID_BO_OBJECT_SPRITE:
				if (_selected_object_index != -1) {
					_selected_object_view = GB(widget, 16, 16);
					this->SelectOtherObject(_selected_object_index); // Re-select the object for a different view.
				}
				break;
		}
	}

	virtual void OnPlaceObject(Point pt, TileIndex tile)
	{
		DoCommandP(tile, ObjectClass::Get(_selected_object_class)->GetSpec(_selected_object_index)->Index(),
				_selected_object_view, CMD_BUILD_OBJECT | CMD_MSG(STR_ERROR_CAN_T_BUILD_OBJECT), CcTerraform);
	}

	virtual void OnPlaceObjectAbort()
	{
		this->UpdateButtons(_selected_object_class, -1, _selected_object_view);
	}

	/**
	 * Select the first available object.
	 * @param change_class If true, change the class if no object in the current
	 *   class is available.
	 */
	void SelectFirstAvailableObject(bool change_class)
	{
		/* First try to select an object in the selected class. */
		ObjectClass *sel_objclass = ObjectClass::Get(_selected_object_class);
		for (uint i = 0; i < sel_objclass->GetSpecCount(); i++) {
			const ObjectSpec *spec = sel_objclass->GetSpec(i);
			if (spec->IsAvailable()) {
				this->SelectOtherObject(i);
				return;
			}
		}
		if (change_class) {
			/* If that fails, select the first available object
			 * from a random class. */
			for (ObjectClassID j = OBJECT_CLASS_BEGIN; j < OBJECT_CLASS_MAX; j++) {
				ObjectClass *objclass = ObjectClass::Get(j);
				for (uint i = 0; i < objclass->GetSpecCount(); i++) {
					const ObjectSpec *spec = objclass->GetSpec(i);
					if (spec->IsAvailable()) {
						this->SelectOtherClass(j);
						this->SelectOtherObject(i);
						return;
					}
				}
			}
		}
		/* If all objects are unavailable, select nothing... */
		if (ObjectClass::Get(_selected_object_class)->GetUISpecCount() == 0) {
			/* ... but make sure that the class is not empty. */
			for (ObjectClassID j = OBJECT_CLASS_BEGIN; j < OBJECT_CLASS_MAX; j++) {
				if (ObjectClass::Get(j)->GetUISpecCount() > 0) {
					this->SelectOtherClass(j);
					break;
				}
			}
		}
		this->SelectOtherObject(-1);
	}
};

static const NWidgetPart _nested_build_object_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetDataTip(STR_OBJECT_BUILD_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_DEFSIZEBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN),
		NWidget(NWID_HORIZONTAL), SetPadding(2, 0, 0, 0),
			NWidget(NWID_VERTICAL),
				NWidget(NWID_HORIZONTAL), SetPadding(0, 5, 2, 5),
					NWidget(WWT_MATRIX, COLOUR_GREY, WID_BO_CLASS_LIST), SetFill(1, 0), SetMatrixDataTip(1, 0, STR_OBJECT_BUILD_CLASS_TOOLTIP), SetScrollbar(WID_BO_SCROLLBAR),
					NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_BO_SCROLLBAR),
				EndContainer(),
				NWidget(NWID_HORIZONTAL), SetPadding(0, 5, 0, 5),
					NWidget(NWID_MATRIX, COLOUR_DARK_GREEN, WID_BO_OBJECT_MATRIX), SetPIP(0, 2, 0),
						NWidget(WWT_PANEL, COLOUR_GREY, WID_BO_OBJECT_SPRITE), SetDataTip(0x0, STR_OBJECT_BUILD_PREVIEW_TOOLTIP), EndContainer(),
					EndContainer(),
				EndContainer(),
				NWidget(WWT_TEXT, COLOUR_DARK_GREEN, WID_BO_OBJECT_NAME), SetDataTip(STR_ORANGE_STRING, STR_NULL), SetPadding(2, 5, 2, 5),
				NWidget(WWT_TEXT, COLOUR_DARK_GREEN, WID_BO_OBJECT_SIZE), SetDataTip(STR_OBJECT_BUILD_SIZE, STR_NULL), SetPadding(2, 5, 2, 5),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetScrollbar(WID_BO_SELECT_SCROLL),
				NWidget(NWID_HORIZONTAL),
					NWidget(NWID_MATRIX, COLOUR_DARK_GREEN, WID_BO_SELECT_MATRIX), SetFill(0, 1), SetPIP(0, 2, 0),
						NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WID_BO_SELECT_IMAGE), SetMinimalSize(66, 60), SetDataTip(0x0, STR_OBJECT_BUILD_TOOLTIP),
								SetFill(0, 0), SetResize(0, 0), SetScrollbar(WID_BO_SELECT_SCROLL),
						EndContainer(),
					EndContainer(),
					NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WID_BO_SELECT_SCROLL),
				EndContainer(),
			EndContainer(),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_BO_INFO), SetPadding(2, 5, 0, 5), SetFill(1, 0), SetResize(1, 0),
			NWidget(NWID_VERTICAL),
				NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetFill(0, 1), EndContainer(),
				NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _build_object_desc(
	WDP_AUTO, "build_object", 0, 0,
	WC_BUILD_OBJECT, WC_BUILD_TOOLBAR,
	WDF_CONSTRUCTION,
	_nested_build_object_widgets, lengthof(_nested_build_object_widgets)
);

/** Show our object picker.  */
void ShowBuildObjectPicker()
{
	/* Don't show the place object button when there are no objects to place. */
	if (ObjectClass::GetUIClassCount() > 0) {
		AllocateWindowDescFront<BuildObjectWindow>(&_build_object_desc, 0);
	}
}

/** Reset all data of the object GUI. */
void InitializeObjectGui()
{
	_selected_object_class = (ObjectClassID)0;
}
