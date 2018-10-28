/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file error_gui.cpp GUI related to errors. */

#include "stdafx.h"
#include "landscape.h"
#include "newgrf_text.h"
#include "error.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "string_func.h"
#include "company_base.h"
#include "company_manager_face.h"
#include "strings_func.h"
#include "zoom_func.h"
#include "window_func.h"
#include "console_func.h"
#include "window_gui.h"

#include "widgets/error_widget.h"

#include "table/strings.h"
#include <list>

#include "safeguards.h"

static const NWidgetPart _nested_errmsg_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_RED),
		NWidget(WWT_CAPTION, COLOUR_RED, WID_EM_CAPTION), SetDataTip(STR_ERROR_MESSAGE_CAPTION, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_RED),
		NWidget(WWT_EMPTY, COLOUR_RED, WID_EM_MESSAGE), SetPadding(0, 2, 0, 2), SetMinimalSize(236, 32),
	EndContainer(),
};

static WindowDesc _errmsg_desc(
	WDP_MANUAL, "error", 0, 0,
	WC_ERRMSG, WC_NONE,
	0,
	_nested_errmsg_widgets, lengthof(_nested_errmsg_widgets)
);

static const NWidgetPart _nested_errmsg_face_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_RED),
		NWidget(WWT_CAPTION, COLOUR_RED, WID_EM_CAPTION), SetDataTip(STR_ERROR_MESSAGE_CAPTION_OTHER_COMPANY, STR_NULL),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_RED),
		NWidget(NWID_HORIZONTAL), SetPIP(2, 1, 2),
			NWidget(WWT_EMPTY, COLOUR_RED, WID_EM_FACE), SetMinimalSize(92, 119), SetFill(0, 1), SetPadding(2, 0, 1, 0),
			NWidget(WWT_EMPTY, COLOUR_RED, WID_EM_MESSAGE), SetFill(0, 1), SetMinimalSize(238, 123),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _errmsg_face_desc(
	WDP_MANUAL, "error_face", 0, 0,
	WC_ERRMSG, WC_NONE,
	0,
	_nested_errmsg_face_widgets, lengthof(_nested_errmsg_face_widgets)
);

/**
 * Copy the given data into our instance.
 * @param data The data to copy.
 */
ErrorMessageData::ErrorMessageData(const ErrorMessageData &data)
{
	*this = data;
	for (size_t i = 0; i < lengthof(this->strings); i++) {
		if (this->strings[i] != NULL) {
			this->strings[i] = stredup(this->strings[i]);
			this->decode_params[i] = (size_t)this->strings[i];
		}
	}
}

/** Free all the strings. */
ErrorMessageData::~ErrorMessageData()
{
	for (size_t i = 0; i < lengthof(this->strings); i++) free(this->strings[i]);
}

/**
 * Display an error message in a window.
 * @param summary_msg  General error message showed in first line. Must be valid.
 * @param detailed_msg Detailed error message showed in second line. Can be INVALID_STRING_ID.
 * @param duration     The amount of time to show this error message.
 * @param x            World X position (TileVirtX) of the error location. Set both x and y to 0 to just center the message when there is no related error tile.
 * @param y            World Y position (TileVirtY) of the error location. Set both x and y to 0 to just center the message when there is no related error tile.
 * @param textref_stack_grffile NewGRF that provides the #TextRefStack for the error message.
 * @param textref_stack_size Number of uint32 values to put on the #TextRefStack for the error message; 0 if the #TextRefStack shall not be used.
 * @param textref_stack Values to put on the #TextRefStack.
 */
ErrorMessageData::ErrorMessageData(StringID summary_msg, StringID detailed_msg, uint duration, int x, int y, const GRFFile *textref_stack_grffile, uint textref_stack_size, const uint32 *textref_stack) :
	duration(duration),
	textref_stack_grffile(textref_stack_grffile),
	textref_stack_size(textref_stack_size),
	summary_msg(summary_msg),
	detailed_msg(detailed_msg),
	face(INVALID_COMPANY)
{
	this->position.x = x;
	this->position.y = y;

	memset(this->decode_params, 0, sizeof(this->decode_params));
	memset(this->strings, 0, sizeof(this->strings));

	if (textref_stack_size > 0) MemCpyT(this->textref_stack, textref_stack, textref_stack_size);

	assert(summary_msg != INVALID_STRING_ID);
}

/**
 * Copy error parameters from current DParams.
 */
void ErrorMessageData::CopyOutDParams()
{
	/* Reset parameters */
	for (size_t i = 0; i < lengthof(this->strings); i++) free(this->strings[i]);
	memset(this->decode_params, 0, sizeof(this->decode_params));
	memset(this->strings, 0, sizeof(this->strings));

	/* Get parameters using type information */
	if (this->textref_stack_size > 0) StartTextRefStackUsage(this->textref_stack_grffile, this->textref_stack_size, this->textref_stack);
	CopyOutDParam(this->decode_params, this->strings, this->detailed_msg == INVALID_STRING_ID ? this->summary_msg : this->detailed_msg, lengthof(this->decode_params));
	if (this->textref_stack_size > 0) StopTextRefStackUsage();

	if (this->detailed_msg == STR_ERROR_OWNED_BY) {
		CompanyID company = (CompanyID)GetDParamX(this->decode_params, 2);
		if (company < MAX_COMPANIES) face = company;
	}
}

/**
 * Set a error string parameter.
 * @param n Parameter index
 * @param v Parameter value
 */
void ErrorMessageData::SetDParam(uint n, uint64 v)
{
	this->decode_params[n] = v;
}

/**
 * Set a rawstring parameter.
 * @param n Parameter index
 * @param str Raw string
 */
void ErrorMessageData::SetDParamStr(uint n, const char *str)
{
	free(this->strings[n]);
	this->strings[n] = stredup(str);
}

/** Define a queue with errors. */
typedef std::list<ErrorMessageData> ErrorList;
/** The actual queue with errors. */
ErrorList _error_list;
/** Whether the window system is initialized or not. */
bool _window_system_initialized = false;

/** Window class for displaying an error message window. */
struct ErrmsgWindow : public Window, ErrorMessageData {
private:
	uint height_summary;            ///< Height of the #summary_msg string in pixels in the #WID_EM_MESSAGE widget.
	uint height_detailed;           ///< Height of the #detailed_msg string in pixels in the #WID_EM_MESSAGE widget.

public:
	ErrmsgWindow(const ErrorMessageData &data) : Window(data.HasFace() ? &_errmsg_face_desc : &_errmsg_desc), ErrorMessageData(data)
	{
		this->InitNested();
	}

	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case WID_EM_MESSAGE: {
				CopyInDParam(0, this->decode_params, lengthof(this->decode_params));
				if (this->textref_stack_size > 0) StartTextRefStackUsage(this->textref_stack_grffile, this->textref_stack_size, this->textref_stack);

				int text_width = max(0, (int)size->width - WD_FRAMETEXT_LEFT - WD_FRAMETEXT_RIGHT);
				this->height_summary = GetStringHeight(this->summary_msg, text_width);
				this->height_detailed = (this->detailed_msg == INVALID_STRING_ID) ? 0 : GetStringHeight(this->detailed_msg, text_width);

				if (this->textref_stack_size > 0) StopTextRefStackUsage();

				uint panel_height = WD_FRAMERECT_TOP + this->height_summary + WD_FRAMERECT_BOTTOM;
				if (this->detailed_msg != INVALID_STRING_ID) panel_height += this->height_detailed + WD_PAR_VSEP_WIDE;

				size->height = max(size->height, panel_height);
				break;
			}
			case WID_EM_FACE: {
				Dimension face_size = GetSpriteSize(SPR_GRADIENT);
				size->width = max(size->width, face_size.width);
				size->height = max(size->height, face_size.height);
				break;
			}
		}
	}

	virtual Point OnInitialPosition(int16 sm_width, int16 sm_height, int window_number)
	{
		/* Position (0, 0) given, center the window. */
		if (this->position.x == 0 && this->position.y == 0) {
			Point pt = {(_screen.width - sm_width) >> 1, (_screen.height - sm_height) >> 1};
			return pt;
		}

		/* Find the free screen space between the main toolbar at the top, and the statusbar at the bottom.
		 * Add a fixed distance 20 to make it less cluttered.
		 */
		int scr_top = GetMainViewTop() + 20;
		int scr_bot = GetMainViewBottom() - 20;

		Point pt = RemapCoords2(this->position.x, this->position.y);
		const ViewPort *vp = FindWindowById(WC_MAIN_WINDOW, 0)->viewport;
		if (this->face == INVALID_COMPANY) {
			/* move x pos to opposite corner */
			pt.x = UnScaleByZoom(pt.x - vp->virtual_left, vp->zoom) + vp->left;
			pt.x = (pt.x < (_screen.width >> 1)) ? _screen.width - sm_width - 20 : 20; // Stay 20 pixels away from the edge of the screen.

			/* move y pos to opposite corner */
			pt.y = UnScaleByZoom(pt.y - vp->virtual_top, vp->zoom) + vp->top;
			pt.y = (pt.y < (_screen.height >> 1)) ? scr_bot - sm_height : scr_top;
		} else {
			pt.x = Clamp(UnScaleByZoom(pt.x - vp->virtual_left, vp->zoom) + vp->left - (sm_width / 2),  0, _screen.width  - sm_width);
			pt.y = Clamp(UnScaleByZoom(pt.y - vp->virtual_top,  vp->zoom) + vp->top  - (sm_height / 2), scr_top, scr_bot - sm_height);
		}
		return pt;
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	virtual void OnInvalidateData(int data = 0, bool gui_scope = true)
	{
		/* If company gets shut down, while displaying an error about it, remove the error message. */
		if (this->face != INVALID_COMPANY && !Company::IsValidID(this->face)) delete this;
	}

	virtual void SetStringParameters(int widget) const
	{
		if (widget == WID_EM_CAPTION) CopyInDParam(0, this->decode_params, lengthof(this->decode_params));
	}

	virtual void DrawWidget(const Rect &r, int widget) const
	{
		switch (widget) {
			case WID_EM_FACE: {
				const Company *c = Company::Get(this->face);
				DrawCompanyManagerFace(c->face, c->colour, r.left, r.top);
				break;
			}

			case WID_EM_MESSAGE:
				CopyInDParam(0, this->decode_params, lengthof(this->decode_params));
				if (this->textref_stack_size > 0) StartTextRefStackUsage(this->textref_stack_grffile, this->textref_stack_size, this->textref_stack);

				if (this->detailed_msg == INVALID_STRING_ID) {
					DrawStringMultiLine(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, r.top + WD_FRAMERECT_TOP, r.bottom - WD_FRAMERECT_BOTTOM,
							this->summary_msg, TC_FROMSTRING, SA_CENTER);
				} else {
					int extra = (r.bottom - r.top + 1 - this->height_summary - this->height_detailed - WD_PAR_VSEP_WIDE) / 2;

					/* Note: NewGRF supplied error message often do not start with a colour code, so default to white. */
					int top = r.top + WD_FRAMERECT_TOP;
					int bottom = top + this->height_summary + extra;
					DrawStringMultiLine(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, top, bottom, this->summary_msg, TC_WHITE, SA_CENTER);

					bottom = r.bottom - WD_FRAMERECT_BOTTOM;
					top = bottom - this->height_detailed - extra;
					DrawStringMultiLine(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, top, bottom, this->detailed_msg, TC_WHITE, SA_CENTER);
				}

				if (this->textref_stack_size > 0) StopTextRefStackUsage();
				break;

			default:
				break;
		}
	}

	virtual void OnMouseLoop()
	{
		/* Disallow closing the window too easily, if timeout is disabled */
		if (_right_button_down && this->duration != 0) delete this;
	}

	virtual void OnHundredthTick()
	{
		/* Timeout enabled? */
		if (this->duration != 0) {
			this->duration--;
			if (this->duration == 0) delete this;
		}
	}

	~ErrmsgWindow()
	{
		SetRedErrorSquare(INVALID_TILE);
		if (_window_system_initialized) ShowFirstError();
	}

	virtual EventState OnKeyPress(WChar key, uint16 keycode)
	{
		if (keycode != WKC_SPACE) return ES_NOT_HANDLED;
		delete this;
		return ES_HANDLED;
	}

	/**
	 * Check whether the currently shown error message was critical or not.
	 * @return True iff the message was critical.
	 */
	bool IsCritical()
	{
		return this->duration == 0;
	}
};

/**
 * Clear all errors from the queue.
 */
void ClearErrorMessages()
{
	UnshowCriticalError();
	_error_list.clear();
}

/** Show the first error of the queue. */
void ShowFirstError()
{
	_window_system_initialized = true;
	if (!_error_list.empty()) {
		new ErrmsgWindow(_error_list.front());
		_error_list.pop_front();
	}
}

/**
 * Unshow the critical error. This has to happen when a critical
 * error is shown and we uninitialise the window system, i.e.
 * remove all the windows.
 */
void UnshowCriticalError()
{
	ErrmsgWindow *w = (ErrmsgWindow*)FindWindowById(WC_ERRMSG, 0);
	if (_window_system_initialized && w != NULL) {
		if (w->IsCritical()) _error_list.push_front(*w);
		_window_system_initialized = false;
		delete w;
	}
}

/**
 * Display an error message in a window.
 * @param summary_msg  General error message showed in first line. Must be valid.
 * @param detailed_msg Detailed error message showed in second line. Can be INVALID_STRING_ID.
 * @param wl           Message severity.
 * @param x            World X position (TileVirtX) of the error location. Set both x and y to 0 to just center the message when there is no related error tile.
 * @param y            World Y position (TileVirtY) of the error location. Set both x and y to 0 to just center the message when there is no related error tile.
 * @param textref_stack_grffile NewGRF providing the #TextRefStack for the error message.
 * @param textref_stack_size Number of uint32 values to put on the #TextRefStack for the error message; 0 if the #TextRefStack shall not be used.
 * @param textref_stack Values to put on the #TextRefStack.
 */
void ShowErrorMessage(StringID summary_msg, StringID detailed_msg, WarningLevel wl, int x, int y, const GRFFile *textref_stack_grffile, uint textref_stack_size, const uint32 *textref_stack)
{
	assert(textref_stack_size == 0 || (textref_stack_grffile != NULL && textref_stack != NULL));
	if (summary_msg == STR_NULL) summary_msg = STR_EMPTY;

	if (wl != WL_INFO) {
		/* Print message to console */
		char buf[DRAW_STRING_BUFFER];

		if (textref_stack_size > 0) StartTextRefStackUsage(textref_stack_grffile, textref_stack_size, textref_stack);

		char *b = GetString(buf, summary_msg, lastof(buf));
		if (detailed_msg != INVALID_STRING_ID) {
			b += seprintf(b, lastof(buf), " ");
			GetString(b, detailed_msg, lastof(buf));
		}

		if (textref_stack_size > 0) StopTextRefStackUsage();

		switch (wl) {
			case WL_WARNING: IConsolePrint(CC_WARNING, buf); break;
			default:         IConsoleError(buf); break;
		}
	}

	bool no_timeout = wl == WL_CRITICAL;

	if (_settings_client.gui.errmsg_duration == 0 && !no_timeout) return;

	ErrorMessageData data(summary_msg, detailed_msg, no_timeout ? 0 : _settings_client.gui.errmsg_duration, x, y, textref_stack_grffile, textref_stack_size, textref_stack);
	data.CopyOutDParams();

	ErrmsgWindow *w = (ErrmsgWindow*)FindWindowById(WC_ERRMSG, 0);
	if (w != NULL && w->IsCritical()) {
		/* A critical error is currently shown. */
		if (wl == WL_CRITICAL) {
			/* Push another critical error in the queue of errors,
			 * but do not put other errors in the queue. */
			_error_list.push_back(data);
		}
	} else {
		/* Nothing or a non-critical error was shown. */
		delete w;
		new ErrmsgWindow(data);
	}
}

/**
 * Schedule a list of errors.
 * Note: This does not try to display the error now. This is useful if the window system is not yet running.
 * @param datas Error message datas; cleared afterwards
 */
void ScheduleErrorMessage(ErrorList &datas)
{
	_error_list.splice(_error_list.end(), datas);
}

/**
 * Schedule an error.
 * Note: This does not try to display the error now. This is useful if the window system is not yet running.
 * @param data Error message data; cleared afterwards
 */
void ScheduleErrorMessage(const ErrorMessageData &data)
{
	_error_list.push_back(data);
}
