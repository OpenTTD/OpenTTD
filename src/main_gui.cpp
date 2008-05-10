/* $Id$ */

/** @file main_gui.cpp Handling of the main viewport. */

#include "stdafx.h"
#include "openttd.h"
#include "currency.h"
#include "spritecache.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "textbuf_gui.h"
#include "viewport_func.h"
#include "command_func.h"
#include "news_gui.h"
#include "console.h"
#include "waypoint.h"
#include "genworld.h"
#include "transparency_gui.h"
#include "date_func.h"
#include "functions.h"
#include "sound_func.h"
#include "transparency.h"
#include "strings_func.h"
#include "zoom_func.h"
#include "string_func.h"
#include "player_base.h"
#include "player_func.h"
#include "player_gui.h"
#include "settings_type.h"
#include "toolbar_gui.h"
#include "variables.h"
#include "tilehighlight_func.h"

#include "network/network.h"
#include "network/network_data.h"
#include "network/network_client.h"
#include "network/network_server.h"
#include "network/network_gui.h"

#include "table/sprites.h"
#include "table/strings.h"

static int _rename_id = 1;
static int _rename_what = -1;

void CcGiveMoney(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
#ifdef ENABLE_NETWORK
	if (!success || !_patches.give_money) return;

	char msg[20];
	/* Inform the player of this action */
	snprintf(msg, sizeof(msg), "%d", p1);

	if (!_network_server) {
		SEND_COMMAND(PACKET_CLIENT_CHAT)(NETWORK_ACTION_GIVE_MONEY, DESTTYPE_TEAM, p2, msg);
	} else {
		NetworkServer_HandleChat(NETWORK_ACTION_GIVE_MONEY, DESTTYPE_TEAM, p2, msg, NETWORK_SERVER_INDEX);
	}
#endif /* ENABLE_NETWORK */
}

void HandleOnEditText(const char *str)
{
	int id = _rename_id;
	_cmd_text = str;

	switch (_rename_what) {
	case 1: // Rename a waypoint
		if (*str == '\0') return;
		DoCommandP(0, id, 0, NULL, CMD_RENAME_WAYPOINT | CMD_MSG(STR_CANT_CHANGE_WAYPOINT_NAME));
		break;
#ifdef ENABLE_NETWORK
	case 3: { // Give money, you can only give money in excess of loan
		const Player *p = GetPlayer(_current_player);
		Money money = min(p->player_money - p->current_loan, (Money)(atoi(str) / _currency->rate));

		uint32 money_c = Clamp(ClampToI32(money), 0, 20000000); // Clamp between 20 million and 0

		/* Give 'id' the money, and substract it from ourself */
		DoCommandP(0, money_c, id, CcGiveMoney, CMD_GIVE_MONEY | CMD_MSG(STR_INSUFFICIENT_FUNDS));
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
void ShowNetworkGiveMoneyWindow(PlayerID player)
{
	_rename_id = player;
	_rename_what = 3;
	ShowQueryString(STR_EMPTY, STR_NETWORK_GIVE_MONEY_CAPTION, 30, 180, NULL, CS_NUMERAL);
}
#endif /* ENABLE_NETWORK */

void ShowRenameWaypointWindow(const Waypoint *wp)
{
	int id = wp->index;

	/* Are we allowed to change the name of the waypoint? */
	if (!CheckTileOwnership(wp->xy)) {
		ShowErrorMessage(_error_message, STR_CANT_CHANGE_WAYPOINT_NAME,
			TileX(wp->xy) * TILE_SIZE, TileY(wp->xy) * TILE_SIZE);
		return;
	}

	_rename_id = id;
	_rename_what = 1;
	SetDParam(0, id);
	ShowQueryString(STR_WAYPOINT_RAW, STR_EDIT_WAYPOINT_NAME, 30, 180, NULL, CS_ALPHANUMERAL);
}


/* Zooms a viewport in a window in or out */
/* No button handling or what so ever */
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

			WP(w, vp_d).scrollpos_x += vp->virtual_width >> 1;
			WP(w, vp_d).scrollpos_y += vp->virtual_height >> 1;
			WP(w, vp_d).dest_scrollpos_x = WP(w,vp_d).scrollpos_x;
			WP(w, vp_d).dest_scrollpos_y = WP(w,vp_d).scrollpos_y;
			break;
		case ZOOM_OUT:
			if (vp->zoom == ZOOM_LVL_MAX) return false;
			vp->zoom = (ZoomLevel)((int)vp->zoom + 1);

			WP(w, vp_d).scrollpos_x -= vp->virtual_width >> 1;
			WP(w, vp_d).scrollpos_y -= vp->virtual_height >> 1;
			WP(w, vp_d).dest_scrollpos_x = WP(w,vp_d).scrollpos_x;
			WP(w, vp_d).dest_scrollpos_y = WP(w,vp_d).scrollpos_y;

			vp->virtual_width <<= 1;
			vp->virtual_height <<= 1;
			break;
	}
	if (vp != NULL) { // the vp can be null when how == ZOOM_NONE
		vp->virtual_left = WP(w, vp_d).scrollpos_x;
		vp->virtual_top = WP(w, vp_d).scrollpos_y;
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

		Point pt = GetTileZoomCenterWindow(in,w);
		if (pt.x != -1) {
			ScrollWindowTo(pt.x, pt.y, w, true);

			DoZoomInOutWindow(in ? ZOOM_IN : ZOOM_OUT, w);
		}
	}
}

extern void UpdateAllStationVirtCoord();

static void MainWindowWndProc(Window *w, WindowEvent *e)
{
	switch (e->event) {
		case WE_PAINT:
			DrawWindowViewport(w);
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
			break;

		case WE_KEYPRESS:
			switch (e->we.keypress.keycode) {
				case 'Q' | WKC_CTRL:
				case 'Q' | WKC_META:
					HandleExitGameRequest();
					break;
			}

			/* Disable all key shortcuts, except quit shortcuts when
			* generating the world, otherwise they create threading
			* problem during the generating, resulting in random
			* assertions that are hard to trigger and debug */
			if (IsGeneratingWorld()) break;

			if (e->we.keypress.keycode == WKC_BACKQUOTE) {
				IConsoleSwitch();
				e->we.keypress.cont = false;
				break;
			}

			if (e->we.keypress.keycode == ('B' | WKC_CTRL)) {
				e->we.keypress.cont = false;
				extern bool _draw_bounding_boxes;
				_draw_bounding_boxes = !_draw_bounding_boxes;
				MarkWholeScreenDirty();
				break;
			}

			if (_game_mode == GM_MENU) break;

			switch (e->we.keypress.keycode) {
				case 'C':
				case 'Z': {
					Point pt = GetTileBelowCursor();
					if (pt.x != -1) {
						if (e->we.keypress.keycode == 'Z') MaxZoomInOut(ZOOM_IN, w);
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
					if (!_networking || !_network_server || !_network_advertise)
						DoCommandP(0, 10000000, 0, NULL, CMD_MONEY_CHEAT);
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
					ToggleTransparency((TransparencyOption)(e->we.keypress.keycode - ('1' | WKC_CTRL)));
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
					ToggleInvisibilityWithTransparency((TransparencyOption)(e->we.keypress.keycode - ('1' | WKC_CTRL | WKC_SHIFT)));
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
						const NetworkClientInfo *cio = NetworkFindClientInfoFromIndex(_network_own_client_index);
						bool teamchat = false;

						if (cio == NULL) break;

						/* Only players actually playing can speak to team. Eg spectators cannot */
						if (_patches.prefer_teamchat && IsValidPlayer(cio->client_playas)) {
							const NetworkClientInfo *ci;
							FOR_ALL_ACTIVE_CLIENT_INFOS(ci) {
								if (ci->client_playas == cio->client_playas && ci != cio) {
									teamchat = true;
									break;
								}
							}
						}

						ShowNetworkChatQueryWindow(teamchat ? DESTTYPE_TEAM : DESTTYPE_BROADCAST, cio->client_playas);
					}
					break;

				case WKC_SHIFT | WKC_RETURN: case WKC_SHIFT | 'T': // send text message to all players
					if (_networking) ShowNetworkChatQueryWindow(DESTTYPE_BROADCAST, 0);
					break;

				case WKC_CTRL | WKC_RETURN: case WKC_CTRL | 'T': // send text to all team mates
					if (_networking) {
						const NetworkClientInfo *cio = NetworkFindClientInfoFromIndex(_network_own_client_index);
						if (cio == NULL) break;

						ShowNetworkChatQueryWindow(DESTTYPE_TEAM, cio->client_playas);
					}
					break;
#endif

				default: return;
			}
			e->we.keypress.cont = false;
			break;

		case WE_SCROLL: {
			ViewPort *vp = IsPtInWindowViewport(w, _cursor.pos.x, _cursor.pos.y);

			if (vp == NULL) {
				_cursor.fix_at = false;
				_scrolling_viewport = false;
			}

			WP(w, vp_d).scrollpos_x += ScaleByZoom(e->we.scroll.delta.x, vp->zoom);
			WP(w, vp_d).scrollpos_y += ScaleByZoom(e->we.scroll.delta.y, vp->zoom);
			WP(w, vp_d).dest_scrollpos_x = WP(w, vp_d).scrollpos_x;
			WP(w, vp_d).dest_scrollpos_y = WP(w, vp_d).scrollpos_y;
		} break;

		case WE_MOUSEWHEEL:
			ZoomInOrOutToCursorWindow(e->we.wheel.wheel < 0, w);
			break;

		case WE_INVALIDATE_DATA:
			/* Forward the message to the appropiate toolbar (ingame or scenario editor) */
			InvalidateWindowData(WC_MAIN_TOOLBAR, 0, e->we.invalidate.data);
			break;
	}
}


void ShowSelectGameWindow();

void SetupColorsAndInitialWindow()
{
	for (uint i = 0; i != 16; i++) {
		const byte *b = GetNonSprite(PALETTE_RECOLOR_START + i);

		assert(b);
		memcpy(_colour_gradient[i], b + 0xC6, sizeof(_colour_gradient[i]));
	}

	int width = _screen.width;
	int height = _screen.height;

	Window *w = new Window(0, 0, width, height, MainWindowWndProc, WC_MAIN_WINDOW, NULL);
	InitializeWindowViewport(w, 0, 0, width, height, TileXY(32, 32), ZOOM_LVL_VIEWPORT);

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

extern void ShowStatusBar();

void ShowVitalWindows()
{
	Window *w = AllocateToolbar();
	DoZoomInOutWindow(ZOOM_NONE, w);

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
	_cur_resolution[0] = _screen.width;
	_cur_resolution[1] = _screen.height;
	ScreenSizeChanged();
	RelocateAllWindows(_screen.width, _screen.height);
	MarkWholeScreenDirty();
}
