/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file error_gui.cpp GUI related to errors. */

#include "stdafx.h"
#include "core/geometry_func.hpp"
#include "core/mem_func.hpp"
#include "landscape.h"
#include "newgrf_text.h"
#include "error.h"
#include "viewport_func.h"
#include "gfx_func.h"
#include "string_func.h"
#include "company_base.h"
#include "company_func.h"
#include "company_manager_face.h"
#include "strings_func.h"
#include "zoom_func.h"
#include "window_func.h"
#include "console_func.h"
#include "window_gui.h"
#include "timer/timer.h"
#include "timer/timer_window.h"

#include "widgets/error_widget.h"

#include "table/strings.h"

#include "safeguards.h"

static constexpr NWidgetPart _nested_errmsg_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_RED),
		NWidget(WWT_CAPTION, COLOUR_RED, WID_EM_CAPTION), SetStringTip(STR_ERROR_MESSAGE_CAPTION),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_RED),
		NWidget(WWT_EMPTY, INVALID_COLOUR, WID_EM_MESSAGE), SetPadding(WidgetDimensions::unscaled.modalpopup), SetFill(1, 0), SetMinimalSize(236, 0),
	EndContainer(),
};

static WindowDesc _errmsg_desc(
	WDP_MANUAL, nullptr, 0, 0,
	WC_ERRMSG, WC_NONE,
	{},
	_nested_errmsg_widgets
);

static constexpr NWidgetPart _nested_errmsg_face_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_RED),
		NWidget(WWT_CAPTION, COLOUR_RED, WID_EM_CAPTION),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_RED),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_EM_FACE), SetPadding(2, 0, 2, 2), SetFill(0, 1), SetMinimalSize(92, 119),
			NWidget(WWT_EMPTY, INVALID_COLOUR, WID_EM_MESSAGE), SetPadding(WidgetDimensions::unscaled.modalpopup), SetFill(1, 1), SetMinimalSize(236, 0),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _errmsg_face_desc(
	WDP_MANUAL, nullptr, 0, 0,
	WC_ERRMSG, WC_NONE,
	{},
	_nested_errmsg_face_widgets
);

/**
 * Display an error message in a window.
 * @param summary_msg  General error message showed in first line. Must be valid.
 * @param detailed_msg Detailed error message showed in second line. Can be empty.
 * @param is_critical  Whether the error is critical. Critical messages never go away on their own.
 * @param x            World X position (TileVirtX) of the error location. Set both x and y to 0 to just center the message when there is no related error tile.
 * @param y            World Y position (TileVirtY) of the error location. Set both x and y to 0 to just center the message when there is no related error tile.
 * @param extra_msg    Extra error message showed in third line. Can be empty.
 */
ErrorMessageData::ErrorMessageData(EncodedString &&summary_msg, EncodedString &&detailed_msg, bool is_critical, int x, int y, EncodedString &&extra_msg, CompanyID company) :
	is_critical(is_critical),
	summary_msg(std::move(summary_msg)),
	detailed_msg(std::move(detailed_msg)),
	extra_msg(std::move(extra_msg)),
	position(x, y),
	company(company)
{
	assert(!this->summary_msg.empty());
}

/** The actual queue with errors. */
static ErrorList _error_list;
/** Whether the window system is initialized or not. */
bool _window_system_initialized = false;

/** Window class for displaying an error message window. */
struct ErrmsgWindow : public Window, ErrorMessageData {
private:
	uint height_summary = 0; ///< Height of the #summary_msg string in pixels in the #WID_EM_MESSAGE widget.
	uint height_detailed = 0; ///< Height of the #detailed_msg string in pixels in the #WID_EM_MESSAGE widget.
	uint height_extra = 0; ///< Height of the #extra_msg string in pixels in the #WID_EM_MESSAGE widget.
	TimeoutTimer<TimerWindow> display_timeout;

public:
	ErrmsgWindow(const ErrorMessageData &data) :
		Window(data.HasFace() ? _errmsg_face_desc : _errmsg_desc),
		ErrorMessageData(data),
		display_timeout(std::chrono::seconds(_settings_client.gui.errmsg_duration), [this]() {
			this->Close();
		})
	{
		this->InitNested();

		/* Only start the timeout if the message is not critical. */
		if (!this->is_critical) {
			this->display_timeout.Reset();
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_EM_MESSAGE: {
				this->height_summary = GetStringHeight(this->summary_msg.GetDecodedString(), size.width);
				this->height_detailed = (this->detailed_msg.empty()) ? 0 : GetStringHeight(this->detailed_msg.GetDecodedString(), size.width);
				this->height_extra = (this->extra_msg.empty()) ? 0 : GetStringHeight(this->extra_msg.GetDecodedString(), size.width);

				uint panel_height = this->height_summary;
				if (!this->detailed_msg.empty()) panel_height += this->height_detailed + WidgetDimensions::scaled.vsep_wide;
				if (!this->extra_msg.empty()) panel_height += this->height_extra + WidgetDimensions::scaled.vsep_wide;

				size.height = std::max(size.height, panel_height);
				break;
			}
			case WID_EM_FACE:
				size = maxdim(size, GetScaledSpriteSize(SPR_GRADIENT));
				break;
		}
	}

	Point OnInitialPosition([[maybe_unused]] int16_t sm_width, [[maybe_unused]] int16_t sm_height, [[maybe_unused]] int window_number) override
	{
		/* Position (0, 0) given, center the window. */
		if (this->position.x == 0 && this->position.y == 0) {
			Point pt = {(_screen.width - sm_width) >> 1, (_screen.height - sm_height) >> 1};
			return pt;
		}

		constexpr int distance_to_cursor = 200;

		/* Position the error window just above the cursor. This makes the
		 * error window clearly visible, without being in the way of what
		 * the user is doing. */
		Point pt;
		pt.x = _cursor.pos.x - sm_width / 2;
		pt.y = _cursor.pos.y - (distance_to_cursor + sm_height);

		if (pt.y < GetMainViewTop()) {
			/* Window didn't fit above cursor, so place it below. */
			pt.y = _cursor.pos.y + distance_to_cursor;
		}

		return pt;
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		/* If company gets shut down, while displaying an error about it, remove the error message. */
		if (this->company != CompanyID::Invalid() && !Company::IsValidID(this->company)) this->Close();
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_EM_CAPTION && this->company != CompanyID::Invalid()) return GetString(STR_ERROR_MESSAGE_CAPTION_OTHER_COMPANY, this->company);

		return this->Window::GetWidgetString(widget, stringid);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_EM_FACE: {
				const Company *c = Company::Get(this->company);
				DrawCompanyManagerFace(c->face, c->colour, r);
				break;
			}

			case WID_EM_MESSAGE:
				if (this->detailed_msg.empty()) {
					DrawStringMultiLine(r, this->summary_msg.GetDecodedString(), TC_FROMSTRING, SA_CENTER);
				} else if (this->extra_msg.empty()) {
					/* Extra space when message is shorter than company face window */
					int extra = (r.Height() - this->height_summary - this->height_detailed - WidgetDimensions::scaled.vsep_wide) / 2;

					/* Note: NewGRF supplied error message often do not start with a colour code, so default to white. */
					DrawStringMultiLine(r.WithHeight(this->height_summary + extra, false), this->summary_msg.GetDecodedString(), TC_WHITE, SA_CENTER);
					DrawStringMultiLine(r.WithHeight(this->height_detailed + extra, true), this->detailed_msg.GetDecodedString(), TC_WHITE, SA_CENTER);
				} else {
					/* Extra space when message is shorter than company face window */
					int extra = (r.Height() - this->height_summary - this->height_detailed - this->height_extra - (WidgetDimensions::scaled.vsep_wide * 2)) / 3;

					/* Note: NewGRF supplied error message often do not start with a colour code, so default to white. */
					Rect top_section = r.WithHeight(this->height_summary + extra, false);
					Rect bottom_section = r.WithHeight(this->height_extra + extra, true);
					Rect middle_section = { top_section.left, top_section.bottom, top_section.right, bottom_section.top };
					DrawStringMultiLine(top_section, this->summary_msg.GetDecodedString(), TC_WHITE, SA_CENTER);
					DrawStringMultiLine(middle_section, this->detailed_msg.GetDecodedString(), TC_WHITE, SA_CENTER);
					DrawStringMultiLine(bottom_section, this->extra_msg.GetDecodedString(), TC_WHITE, SA_CENTER);
				}

				break;

			default:
				break;
		}
	}

	void OnMouseLoop() override
	{
		/* Disallow closing the window too easily, if timeout is disabled */
		if (_right_button_down && !this->is_critical) this->Close();
	}

	void Close([[maybe_unused]] int data = 0) override
	{
		SetRedErrorSquare(INVALID_TILE);
		if (_window_system_initialized) ShowFirstError();
		this->Window::Close();
	}

	/**
	 * Check whether the currently shown error message was critical or not.
	 * @return True iff the message was critical.
	 */
	bool IsCritical()
	{
		return this->is_critical;
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
	ErrmsgWindow *w = dynamic_cast<ErrmsgWindow *>(FindWindowById(WC_ERRMSG, 0));
	if (_window_system_initialized && w != nullptr) {
		if (w->IsCritical()) _error_list.push_front(*w);
		_window_system_initialized = false;
		w->Close();
	}
}

/**
 * Display an error message in a window.
 * Note: CommandCost errors are always severity level WL_INFO.
 * @param summary_msg  General error message showed in first line. Must be valid.
 * @param x            World X position (TileVirtX) of the error location. Set both x and y to 0 to just center the message when there is no related error tile.
 * @param y            World Y position (TileVirtY) of the error location. Set both x and y to 0 to just center the message when there is no related error tile.
 * @param cc           CommandCost containing the optional detailed and extra error messages shown in the second and third lines (can be empty).
 */
void ShowErrorMessage(EncodedString &&summary_msg, int x, int y, CommandCost &cc)
{
	EncodedString error = std::move(cc.GetEncodedMessage());
	if (error.empty()) error = GetEncodedStringIfValid(cc.GetErrorMessage());

	ShowErrorMessage(std::move(summary_msg), std::move(error), WL_INFO, x, y,
		GetEncodedStringIfValid(cc.GetExtraErrorMessage()), cc.GetErrorOwner());
}

/**
 * Display an error message in a window.
 * @param summary_msg  General error message showed in first line. Must be valid.
 * @param detailed_msg Detailed error message showed in second line. Can be empty.
 * @param wl           Message severity.
 * @param x            World X position (TileVirtX) of the error location. Set both x and y to 0 to just center the message when there is no related error tile.
 * @param y            World Y position (TileVirtY) of the error location. Set both x and y to 0 to just center the message when there is no related error tile.
 * @param extra_msg    Extra error message shown in third line. Can be empty.
 */
void ShowErrorMessage(EncodedString &&summary_msg, EncodedString &&detailed_msg, WarningLevel wl, int x, int y, EncodedString &&extra_msg, CompanyID company)
{
	if (wl != WL_INFO) {
		/* Print message to console */

		std::string message = summary_msg.GetDecodedString();
		if (!detailed_msg.empty()) {
			message += " ";
			message += detailed_msg.GetDecodedString();
		}
		if (!extra_msg.empty()) {
			message += " ";
			message += extra_msg.GetDecodedString();
		}

		IConsolePrint(wl == WL_WARNING ? CC_WARNING : CC_ERROR, message);
	}

	bool is_critical = wl == WL_CRITICAL;

	if (_game_mode == GM_BOOTSTRAP) return;
	if (_settings_client.gui.errmsg_duration == 0 && !is_critical) return;

	ErrorMessageData data(std::move(summary_msg), std::move(detailed_msg), is_critical, x, y, std::move(extra_msg), company);

	ErrmsgWindow *w = dynamic_cast<ErrmsgWindow *>(FindWindowById(WC_ERRMSG, 0));
	if (w != nullptr) {
		if (w->IsCritical()) {
			/* A critical error is currently shown. */
			if (wl == WL_CRITICAL) {
				/* Push another critical error in the queue of errors,
				 * but do not put other errors in the queue. */
				_error_list.push_back(std::move(data));
			}
			return;
		}
		/* A non-critical error was shown. */
		w->Close();
	}
	new ErrmsgWindow(data);
}


/**
 * Close active error message window
 * @return true if a window was closed.
 */
bool HideActiveErrorMessage()
{
	ErrmsgWindow *w = dynamic_cast<ErrmsgWindow *>(FindWindowById(WC_ERRMSG, 0));
	if (w == nullptr) return false;
	w->Close();
	return true;
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
