/* $Id$ */

/** @file main_gui.cpp Handling of the main viewport. */

#include "stdafx.h"
#include "openttd.h"
#include "currency.h"
#include "spritecache.h"
#include "window_gui.h"
#include "window_func.h"
#include "textbuf_gui.h"
#include "viewport_func.h"
#include "command_func.h"
#include "console_gui.h"
#include "map_func.h"
#include "genworld.h"
#include "transparency_gui.h"
#include "functions.h"
#include "sound_func.h"
#include "transparency.h"
#include "strings_func.h"
#include "zoom_func.h"
#include "company_base.h"
#include "company_func.h"
#include "settings_type.h"
#include "toolbar_gui.h"
#include "statusbar_gui.h"
#include "tilehighlight_func.h"

#include "network/network.h"
#include "network/network_func.h"
#include "network/network_gui.h"
#include "network/network_base.h"

#include "table/sprites.h"
#include "table/strings.h"

static int _rename_id = 1;
static int _rename_what = -1;

void CcGiveMoney(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
#ifdef ENABLE_NETWORK
	if (!success || !_settings_game.economy.give_money) return;

	/* Inform the company of the action of one of it's clients (controllers). */
	char msg[64];
	SetDParam(0, p2);
	GetString(msg, STR_COMPANY_NAME, lastof(msg));

	if (!_network_server) {
		NetworkClientSendChat(NETWORK_ACTION_GIVE_MONEY, DESTTYPE_TEAM, p2, msg, p1);
	} else {
		NetworkServerSendChat(NETWORK_ACTION_GIVE_MONEY, DESTTYPE_TEAM, p2, msg, CLIENT_ID_SERVER, p1);
	}
#endif /* ENABLE_NETWORK */
}

void HandleOnEditText(const char *str)
{
	switch (_rename_what) {
#ifdef ENABLE_NETWORK
	case 3: { // Give money, you can only give money in excess of loan
		const Company *c = GetCompany(_local_company);
		Money money = min(c->money - c->current_loan, (Money)(atoi(str) / _currency->rate));

		uint32 money_c = Clamp(ClampToI32(money), 0, 20000000); // Clamp between 20 million and 0

		/* Give 'id' the money, and substract it from ourself */
		DoCommandP(0, money_c, _rename_id, CMD_GIVE_MONEY | CMD_MSG(STR_INSUFFICIENT_FUNDS), CcGiveMoney, str);
	} break;
#endif /* ENABLE_NETWORK */
		default: NOT_REACHED();
	}

	_rename_id = _rename_what = -1;
}

/**
 * This code is shared for the majority of the pushbuttons.
 * Handles e.g. the pressing of a button (to build things), playing of click sound and sets certain parameters
 *
 * @param w Window which called the function
 * @param widget ID of the widget (=button) that called this function
 * @param cursor How should the cursor image change? E.g. cursor with depot image in it
 * @param mode Tile highlighting mode, e.g. drawing a rectangle or a dot on the ground
 * @param placeproc Procedure which will be called when someone clicks on the map
 * @return true if the button is clicked, false if it's unclicked
 */
bool HandlePlacePushButton(Window *w, int widget, CursorID cursor, ViewportHighlightMode mode, PlaceProc *placeproc)
{
	if (w->IsWidgetDisabled(widget)) return false;

	SndPlayFx(SND_15_BEEP);
	w->SetDirty();

	if (w->IsWidgetLowered(widget)) {
		ResetObjectToPlace();
		return false;
	}

	SetObjectToPlace(cursor, PAL_NONE, mode, w->window_class, w->window_number);
	w->LowerWidget(widget);
	_place_proc = placeproc;
	return true;
}


void CcPlaySound10(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	if (success) SndPlayTileFx(SND_12_EXPLOSION, tile);
}

#ifdef ENABLE_NETWORK
void ShowNetworkGiveMoneyWindow(CompanyID company)
{
	_rename_id = company;
	_rename_what = 3;
	ShowQueryString(STR_EMPTY, STR_NETWORK_GIVE_MONEY_CAPTION, 30, 180, NULL, CS_NUMERAL, QSF_NONE);
}
#endif /* ENABLE_NETWORK */


/* Zooms a viewport in a window in or out
 * No button handling or what so ever */
bool DoZoomInOutWindow(int how, Window *w)
{
	ViewPort *vp;

	assert(w != NULL);
	vp = w->viewport;

	switch (how) {
		case ZOOM_IN:
			if (vp->zoom == ZOOM_LVL_MIN) return false;
			vp->zoom = (ZoomLevel)((int)vp->zoom - 1);
			vp->virtual_width >>= 1;
			vp->virtual_height >>= 1;

			w->viewport->scrollpos_x += vp->virtual_width >> 1;
			w->viewport->scrollpos_y += vp->virtual_height >> 1;
			w->viewport->dest_scrollpos_x = w->viewport->scrollpos_x;
			w->viewport->dest_scrollpos_y = w->viewport->scrollpos_y;
			break;
		case ZOOM_OUT:
			if (vp->zoom == ZOOM_LVL_MAX) return false;
			vp->zoom = (ZoomLevel)((int)vp->zoom + 1);

			w->viewport->scrollpos_x -= vp->virtual_width >> 1;
			w->viewport->scrollpos_y -= vp->virtual_height >> 1;
			w->viewport->dest_scrollpos_x = w->viewport->scrollpos_x;
			w->viewport->dest_scrollpos_y = w->viewport->scrollpos_y;

			vp->virtual_width <<= 1;
			vp->virtual_height <<= 1;
			break;
	}
	if (vp != NULL) { // the vp can be null when how == ZOOM_NONE
		vp->virtual_left = w->viewport->scrollpos_x;
		vp->virtual_top = w->viewport->scrollpos_y;
	}
	w->SetDirty();
	/* Update the windows that have zoom-buttons to perhaps disable their buttons */
	InvalidateThisWindowData(w);
	return true;
}

void ZoomInOrOutToCursorWindow(bool in, Window *w)
{
	assert(w != NULL);

	if (_game_mode != GM_MENU) {
		ViewPort *vp = w->viewport;
		if ((in && vp->zoom == ZOOM_LVL_MIN) || (!in && vp->zoom == ZOOM_LVL_MAX))
			return;

		Point pt = GetTileZoomCenterWindow(in, w);
		if (pt.x != -1) {
			ScrollWindowTo(pt.x, pt.y, -1, w, true);

			DoZoomInOutWindow(in ? ZOOM_IN : ZOOM_OUT, w);
		}
	}
}

extern void UpdateAllStationVirtCoord();

struct MainWindow : Window
{
	MainWindow(int width, int height) : Window(0, 0, width, height, WC_MAIN_WINDOW, NULL)
	{
		InitializeWindowViewport(this, 0, 0, width, height, TileXY(32, 32), ZOOM_LVL_VIEWPORT);
	}

	virtual void OnPaint()
	{
		this->DrawViewport();
		if (_game_mode == GM_MENU) {
			int off_x = _screen.width / 2;

			DrawSprite(SPR_OTTD_O, PAL_NONE, off_x - 120, 50);
			DrawSprite(SPR_OTTD_P, PAL_NONE, off_x -  86, 50);
			DrawSprite(SPR_OTTD_E, PAL_NONE, off_x -  53, 50);
			DrawSprite(SPR_OTTD_N, PAL_NONE, off_x -  22, 50);

			DrawSprite(SPR_OTTD_T, PAL_NONE, off_x +  34, 50);
			DrawSprite(SPR_OTTD_T, PAL_NONE, off_x +  65, 50);
			DrawSprite(SPR_OTTD_D, PAL_NONE, off_x +  96, 50);
		}
	}

	virtual EventState OnKeyPress(uint16 key, uint16 keycode)
	{
		switch (keycode) {
			case 'Q' | WKC_CTRL:
			case 'Q' | WKC_META:
				HandleExitGameRequest();
				return ES_HANDLED;
		}

		/* Disable all key shortcuts, except quit shortcuts when
		 * generating the world, otherwise they create threading
		 * problem during the generating, resulting in random
		 * assertions that are hard to trigger and debug */
		if (IsGeneratingWorld()) return ES_NOT_HANDLED;

		if (keycode == WKC_BACKQUOTE) {
			IConsoleSwitch();
			return ES_HANDLED;
		}

		if (keycode == ('B' | WKC_CTRL)) {
			extern bool _draw_bounding_boxes;
			_draw_bounding_boxes = !_draw_bounding_boxes;
			MarkWholeScreenDirty();
			return ES_HANDLED;
		}

		if (_game_mode == GM_MENU) return ES_NOT_HANDLED;

		switch (keycode) {
			case 'C':
			case 'Z': {
				Point pt = GetTileBelowCursor();
				if (pt.x != -1) {
					if (keycode == 'Z') MaxZoomInOut(ZOOM_IN, this);
					ScrollMainWindowTo(pt.x, pt.y);
				}
				break;
			}

			case WKC_ESC: ResetObjectToPlace(); break;
			case WKC_DELETE: DeleteNonVitalWindows(); break;
			case WKC_DELETE | WKC_SHIFT: DeleteAllNonVitalWindows(); break;
			case 'R' | WKC_CTRL: MarkWholeScreenDirty(); break;

#if defined(_DEBUG)
			case '0' | WKC_ALT: // Crash the game
				*(byte*)0 = 0;
				break;

			case '1' | WKC_ALT: // Gimme money
				/* Server can not cheat in advertise mode either! */
#ifdef ENABLE_NETWORK
				if (!_networking || !_network_server || !_settings_client.network.server_advertise)
#endif /* ENABLE_NETWORK */
					DoCommandP(0, 10000000, 0, CMD_MONEY_CHEAT);
				break;

			case '2' | WKC_ALT: // Update the coordinates of all station signs
				UpdateAllStationVirtCoord();
				break;
#endif

			case '1' | WKC_CTRL:
			case '2' | WKC_CTRL:
			case '3' | WKC_CTRL:
			case '4' | WKC_CTRL:
			case '5' | WKC_CTRL:
			case '6' | WKC_CTRL:
			case '7' | WKC_CTRL:
			case '8' | WKC_CTRL:
			case '9' | WKC_CTRL:
				/* Transparency toggle hot keys */
				ToggleTransparency((TransparencyOption)(keycode - ('1' | WKC_CTRL)));
				MarkWholeScreenDirty();
				break;

			case '1' | WKC_CTRL | WKC_SHIFT:
			case '2' | WKC_CTRL | WKC_SHIFT:
			case '3' | WKC_CTRL | WKC_SHIFT:
			case '4' | WKC_CTRL | WKC_SHIFT:
			case '5' | WKC_CTRL | WKC_SHIFT:
			case '6' | WKC_CTRL | WKC_SHIFT:
			case '7' | WKC_CTRL | WKC_SHIFT:
			case '8' | WKC_CTRL | WKC_SHIFT:
				/* Invisibility toggle hot keys */
				ToggleInvisibilityWithTransparency((TransparencyOption)(keycode - ('1' | WKC_CTRL | WKC_SHIFT)));
				MarkWholeScreenDirty();
				break;

			case 'X' | WKC_CTRL:
				ShowTransparencyToolbar();
				break;

			case 'X':
				ResetRestoreAllTransparency();
				break;

#ifdef ENABLE_NETWORK
			case WKC_RETURN: case 'T': // smart chat; send to team if any, otherwise to all
				if (_networking) {
					const NetworkClientInfo *cio = NetworkFindClientInfoFromClientID(_network_own_client_id);
					if (cio == NULL) break;

					ShowNetworkChatQueryWindow(NetworkClientPreferTeamChat(cio) ? DESTTYPE_TEAM : DESTTYPE_BROADCAST, cio->client_playas);
				}
				break;

			case WKC_SHIFT | WKC_RETURN: case WKC_SHIFT | 'T': // send text message to all clients
				if (_networking) ShowNetworkChatQueryWindow(DESTTYPE_BROADCAST, 0);
				break;

			case WKC_CTRL | WKC_RETURN: case WKC_CTRL | 'T': // send text to all team mates
				if (_networking) {
					const NetworkClientInfo *cio = NetworkFindClientInfoFromClientID(_network_own_client_id);
					if (cio == NULL) break;

					ShowNetworkChatQueryWindow(DESTTYPE_TEAM, cio->client_playas);
				}
				break;
#endif

			default: return ES_NOT_HANDLED;
		}
		return ES_HANDLED;
	}

	virtual void OnScroll(Point delta)
	{
		ViewPort *vp = IsPtInWindowViewport(this, _cursor.pos.x, _cursor.pos.y);

		if (vp == NULL) {
			_cursor.fix_at = false;
			_scrolling_viewport = false;
		}

		this->viewport->scrollpos_x += ScaleByZoom(delta.x, vp->zoom);
		this->viewport->scrollpos_y += ScaleByZoom(delta.y, vp->zoom);
		this->viewport->dest_scrollpos_x = this->viewport->scrollpos_x;
		this->viewport->dest_scrollpos_y = this->viewport->scrollpos_y;
	};

	virtual void OnMouseWheel(int wheel)
	{
		ZoomInOrOutToCursorWindow(wheel < 0, this);
	}

	virtual void OnInvalidateData(int data)
	{
		/* Forward the message to the appropiate toolbar (ingame or scenario editor) */
		InvalidateWindowData(WC_MAIN_TOOLBAR, 0, data);
	}
};


void ShowSelectGameWindow();

void SetupColoursAndInitialWindow()
{
	for (uint i = 0; i != 16; i++) {
		const byte *b = GetNonSprite(PALETTE_RECOLOUR_START + i, ST_RECOLOUR);

		assert(b);
		memcpy(_colour_gradient[i], b + 0xC6, sizeof(_colour_gradient[i]));
	}

	new MainWindow(_screen.width, _screen.height);

	/* XXX: these are not done */
	switch (_game_mode) {
		default: NOT_REACHED();
		case GM_MENU:
			ShowSelectGameWindow();
			break;

		case GM_NORMAL:
		case GM_EDITOR:
			ShowVitalWindows();
			break;
	}
}

void ShowVitalWindows()
{
	AllocateToolbar();

	/* Status bad only for normal games */
	if (_game_mode == GM_EDITOR) return;

	ShowStatusBar();
}

/**
 * Size of the application screen changed.
 * Adapt the game screen-size, re-allocate the open windows, and repaint everything
 */
void GameSizeChanged()
{
	_cur_resolution.width  = _screen.width;
	_cur_resolution.height = _screen.height;
	ScreenSizeChanged();
	RelocateAllWindows(_screen.width, _screen.height);
	MarkWholeScreenDirty();
}
