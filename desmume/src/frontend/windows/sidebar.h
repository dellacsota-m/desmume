/*
	Quick-access sidebar for the DeSmuME Windows frontend (personal build).

	A narrow bar docked on the left of the main window, holding the buttons used most often.
	It is a child window of the main window, so the DirectDraw clipper keeps the picture out of it.
	In fullscreen the bar is hidden, and slides over the picture when the mouse touches the left edge.

	This file is included once, from main.cpp.
*/

#ifndef DESMUME_SIDEBAR_H
#define DESMUME_SIDEBAR_H

extern int lastSaveState;

enum
{
	SB_SAVE = 0,
	SB_LOAD,
	SB_PAUSE,
	SB_SPEED,
	SB_VOLUME,
	SB_SWAP,
	SB_LAYOUT,
	SB_FOCUS,
	SB_FULL,
	SB_CONFIG,
	SB_COUNT
};

static HWND sidebarHwnd = NULL;
static bool sidebarEnabled = true;          // user setting (View > Show Sidebar)
static bool sidebarOverlayShown = false;    // fullscreen only: bar shown over the picture
static bool sidebarMenuOpen = false;
static bool sidebarTracking = false;
static int sidebarSpeedMode = 0;            // 0 = x1, 1 = x2, 2 = x4, 3 = x8 (no limiter)
static int sidebarHot = -1;
static int sidebarDown = -1;
static int sidebarVolumeBeforeMute = 100;
static unsigned sidebarStateSig = 0xFFFFFFFF;

static bool SidebarIsFullscreen()
{
	return ((GetStyle() & DWS_FULLSCREEN) != 0) || fsWindow;
}

static int SidebarWidth()
{
	HDC dc = GetDC(NULL);
	int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
	if (dc) ReleaseDC(NULL, dc);
	if (dpi < 96) dpi = 96;
	return MulDiv(58, dpi, 96);
}

// Places the bar and tells UpdateWndRects() how much width it takes from the picture area.
void SidebarReposition(HWND hwnd, int tbheight)
{
	if (sidebarHwnd == NULL)
	{
		sidebarReservedWidth = 0;
		return;
	}

	RECT cr;
	GetClientRect(hwnd, &cr);
	const int w = SidebarWidth();
	int h = (int)(cr.bottom - cr.top) - tbheight;
	if (h < 1) h = 1;

	bool visible;
	if (!sidebarEnabled)
	{
		sidebarReservedWidth = 0;
		visible = false;
	}
	else if (SidebarIsFullscreen())
	{
		sidebarReservedWidth = 0;
		visible = sidebarOverlayShown;
	}
	else
	{
		sidebarReservedWidth = w;
		sidebarOverlayShown = false;
		visible = true;
	}

	static int lastW = -1, lastH = -1, lastTb = -1;
	static bool lastVisible = false;
	if (visible == lastVisible && w == lastW && h == lastH && tbheight == lastTb)
		return;
	lastW = w; lastH = h; lastTb = tbheight; lastVisible = visible;

	SetWindowPos(sidebarHwnd, HWND_TOP, 0, tbheight, w, h, SWP_NOACTIVATE | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
}

static void SidebarSetVolume(int v)
{
	if (v < 0) v = 0;
	if (v > 100) v = 100;
	if (v == 0 && sndvolume > 0) sidebarVolumeBeforeMute = sndvolume;
	sndvolume = v;
	SPU_SetVolume(sndvolume);
	WritePrivateProfileInt("Sound", "Volume", sndvolume, IniName);
}

static void SidebarApplySpeed()
{
	switch (sidebarSpeedMode)
	{
	case 1:  FastForward = 0; SetSpeedScalerIndex(1); break; // 200%
	case 2:  FastForward = 0; SetSpeedScalerIndex(0); break; // 400%
	case 3:  FastForward = 1; break;                         // no limiter
	default: FastForward = 0; SetSpeedScalerIndex(5); break; // 100%
	}
}

static void SidebarLabel(int i, char* buf, size_t size)
{
	switch (i)
	{
	case SB_SAVE:   _snprintf(buf, size, "SAVE\n#%d", lastSaveState); break;
	case SB_LOAD:   _snprintf(buf, size, "LOAD\n#%d", lastSaveState); break;
	case SB_PAUSE:  _snprintf(buf, size, "%s", (romloaded && paused) ? "PLAY" : "PAUSE"); break;
	case SB_SPEED:
		{
			static const int mult[4] = { 1, 2, 4, 8 };
			_snprintf(buf, size, "x%d", mult[sidebarSpeedMode & 3]);
		}
		break;
	case SB_VOLUME:
		if (sndvolume == 0) _snprintf(buf, size, "MUTE");
		else _snprintf(buf, size, "VOL\n%d", sndvolume);
		break;
	case SB_SWAP:   _snprintf(buf, size, "SWAP"); break;
	case SB_LAYOUT: _snprintf(buf, size, "LAYOUT"); break;
	case SB_FOCUS:  _snprintf(buf, size, "FOCUS"); break;
	case SB_FULL:   _snprintf(buf, size, "FULL"); break;
	case SB_CONFIG: _snprintf(buf, size, "CFG"); break;
	default:        _snprintf(buf, size, "?"); break;
	}
	buf[size - 1] = 0;
}

static bool SidebarButtonActive(int i)
{
	switch (i)
	{
	case SB_PAUSE:  return romloaded && paused;
	case SB_SPEED:  return sidebarSpeedMode != 0;
	case SB_VOLUME: return sndvolume == 0;
	case SB_SWAP:   return video.swap != 0;
	case SB_FOCUS:  return video.layout == 3 && customTopFocus;
	case SB_FULL:   return SidebarIsFullscreen();
	default:        return false;
	}
}

static void SidebarButtonRect(int i, const RECT& cr, RECT* out)
{
	const int margin = 4;
	int h = ((int)(cr.bottom - cr.top) - margin) / SB_COUNT;
	if (h > 52) h = 52;
	if (h < 20) h = 20;
	const int top = (int)cr.top + margin + i * h;
	SetRect(out, (int)cr.left + margin, top, (int)cr.right - margin, top + h - margin);
}

static int SidebarHitTest(HWND hwnd, int x, int y)
{
	RECT cr;
	GetClientRect(hwnd, &cr);
	POINT p;
	p.x = x; p.y = y;
	for (int i = 0; i < SB_COUNT; i++)
	{
		RECT r;
		SidebarButtonRect(i, cr, &r);
		if (PtInRect(&r, p)) return i;
	}
	return -1;
}

static unsigned SidebarStateSignature()
{
	unsigned s = 0;
	s |= (romloaded && paused) ? 1u : 0u;
	s |= ((unsigned)sidebarSpeedMode & 3u) << 1;
	s |= ((unsigned)sndvolume & 0x7Fu) << 3;
	s |= ((unsigned)video.swap & 3u) << 10;
	s |= ((unsigned)video.layout & 3u) << 12;
	s |= ((unsigned)customLayoutPreset & 3u) << 14;
	s |= (customTopFocus ? 1u : 0u) << 16;
	s |= (SidebarIsFullscreen() ? 1u : 0u) << 17;
	s |= ((unsigned)lastSaveState & 15u) << 18;
	return s;
}

static void SidebarPaint(HWND hwnd)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	RECT cr;
	GetClientRect(hwnd, &cr);

	HDC mem = CreateCompatibleDC(hdc);
	HBITMAP bmp = CreateCompatibleBitmap(hdc, cr.right, cr.bottom);
	HGDIOBJ oldBmp = SelectObject(mem, bmp);

	HBRUSH bg = CreateSolidBrush(RGB(22, 24, 30));
	FillRect(mem, &cr, bg);
	DeleteObject(bg);

	int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
	if (dpi < 96) dpi = 96;
	HFONT font = CreateFontA(-MulDiv(8, dpi, 72), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
	HGDIOBJ oldFont = SelectObject(mem, font);
	SetBkMode(mem, TRANSPARENT);

	for (int i = 0; i < SB_COUNT; i++)
	{
		RECT r;
		SidebarButtonRect(i, cr, &r);

		const bool active = SidebarButtonActive(i);
		const bool hot = (sidebarHot == i);
		const bool down = hot && (sidebarDown == i);
		const COLORREF fill = down ? RGB(66, 74, 96) : (hot ? RGB(46, 52, 68) : RGB(32, 36, 46));
		const COLORREF border = active ? RGB(96, 148, 255) : RGB(48, 54, 68);

		HBRUSH brush = CreateSolidBrush(fill);
		HPEN pen = CreatePen(PS_SOLID, 1, border);
		HGDIOBJ oldBrush = SelectObject(mem, brush);
		HGDIOBJ oldPen = SelectObject(mem, pen);
		RoundRect(mem, r.left, r.top, r.right, r.bottom, 8, 8);
		SelectObject(mem, oldBrush);
		SelectObject(mem, oldPen);
		DeleteObject(brush);
		DeleteObject(pen);

		char label[32];
		SidebarLabel(i, label, sizeof(label));
		SetTextColor(mem, active ? RGB(140, 184, 255) : RGB(226, 230, 240));

		RECT calc;
		SetRect(&calc, 0, 0, (int)(r.right - r.left) - 4, 0);
		DrawTextA(mem, label, -1, &calc, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
		const int th = (int)(calc.bottom - calc.top);
		RECT tr = r;
		tr.top = r.top + (((int)(r.bottom - r.top) - th) / 2);
		tr.bottom = tr.top + th;
		DrawTextA(mem, label, -1, &tr, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
	}

	BitBlt(hdc, 0, 0, cr.right, cr.bottom, mem, 0, 0, SRCCOPY);

	SelectObject(mem, oldFont);
	DeleteObject(font);
	SelectObject(mem, oldBmp);
	DeleteObject(bmp);
	DeleteDC(mem);
	EndPaint(hwnd, &ps);
}

// Shows a popup menu next to a button. Returns the chosen command, or 0.
static int SidebarTrackMenu(HMENU menu, int buttonIndex)
{
	RECT cr, r;
	GetClientRect(sidebarHwnd, &cr);
	SidebarButtonRect(buttonIndex, cr, &r);
	POINT pt;
	pt.x = r.right;
	pt.y = r.top;
	ClientToScreen(sidebarHwnd, &pt);

	sidebarMenuOpen = true;
	SetForegroundWindow(MainWindow->getHWnd());
	int cmd = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, sidebarHwnd, NULL);
	sidebarMenuOpen = false;
	DestroyMenu(menu);
	return cmd;
}

static void SidebarSlotMenu(int buttonIndex)
{
	HMENU menu = CreatePopupMenu();
	for (int n = 0; n < 10; n++)
	{
		char text[32];
		_snprintf(text, sizeof(text), "Slot %d", n);
		text[sizeof(text) - 1] = 0;
		AppendMenuA(menu, MF_STRING | (n == lastSaveState ? MF_CHECKED : 0), 100 + n, text);
	}
	int cmd = SidebarTrackMenu(menu, buttonIndex);
	if (cmd >= 100 && cmd < 110)
		HK_StateSetSlot(cmd - 100, true);
}

static void SidebarVolumeMenu(int buttonIndex)
{
	static const int levels[5] = { 0, 25, 50, 75, 100 };
	static const char* names[5] = { "Mute", "25%", "50%", "75%", "100%" };
	HMENU menu = CreatePopupMenu();
	for (int k = 0; k < 5; k++)
		AppendMenuA(menu, MF_STRING | (sndvolume == levels[k] ? MF_CHECKED : 0), 200 + k, names[k]);
	int cmd = SidebarTrackMenu(menu, buttonIndex);
	if (cmd >= 200 && cmd < 205)
		SidebarSetVolume(levels[cmd - 200]);
}

static void SidebarLayoutMenu(int buttonIndex)
{
	const bool custom = (video.layout == 3);
	HMENU menu = CreatePopupMenu();
	AppendMenuA(menu, MF_STRING | (video.layout == 0 ? MF_CHECKED : 0), ID_LCDS_VERTICAL, "Vertical");
	AppendMenuA(menu, MF_STRING | (video.layout == 1 ? MF_CHECKED : 0), ID_LCDS_HORIZONTAL, "Horizontal");
	AppendMenuA(menu, MF_STRING | (video.layout == 2 ? MF_CHECKED : 0), ID_LCDS_ONE, "One screen");
	AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuA(menu, MF_STRING | (custom && customLayoutPreset == 0 ? MF_CHECKED : 0), ID_LCDS_CUSTOM_TOPBIG, "Large top, small bottom");
	AppendMenuA(menu, MF_STRING | (custom && customLayoutPreset == 1 ? MF_CHECKED : 0), ID_LCDS_CUSTOM_HYBRID, "Hybrid (side by side)");
	AppendMenuA(menu, MF_STRING | (custom && customLayoutPreset == 2 ? MF_CHECKED : 0), ID_LCDS_CUSTOM_PIP, "Bottom picture-in-picture");
	AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuA(menu, MF_STRING, ID_LCDS_CUSTOM_BIGGER, "Bottom screen bigger");
	AppendMenuA(menu, MF_STRING, ID_LCDS_CUSTOM_SMALLER, "Bottom screen smaller");
	int cmd = SidebarTrackMenu(menu, buttonIndex);
	if (cmd != 0)
		PostMessage(MainWindow->getHWnd(), WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
}

static void SidebarSetEnabled(bool on)
{
	sidebarEnabled = on;
	sidebarOverlayShown = false;
	WritePrivateProfileBool("Display", "Show Sidebar", on, IniName);
	MainWindow->checkMenu(ID_VIEW_SIDEBAR, on);
	UpdateWndRects(MainWindow->getHWnd());
	if (romloaded) Display();
}

static void SidebarConfigMenu(int buttonIndex)
{
	HMENU slots = CreatePopupMenu();
	for (int n = 0; n < 10; n++)
	{
		char text[32];
		_snprintf(text, sizeof(text), "Slot %d", n);
		text[sizeof(text) - 1] = 0;
		AppendMenuA(slots, MF_STRING | (n == lastSaveState ? MF_CHECKED : 0), 100 + n, text);
	}
	HMENU menu = CreatePopupMenu();
	AppendMenuA(menu, MF_POPUP, (UINT_PTR)slots, "Save slot");
	AppendMenuA(menu, MF_STRING, 300, "Control config...");
	AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuA(menu, MF_STRING, 301, "Hide sidebar (View > Show Sidebar)");
	int cmd = SidebarTrackMenu(menu, buttonIndex); // also destroys the submenu
	if (cmd >= 100 && cmd < 110)
		HK_StateSetSlot(cmd - 100, true);
	else if (cmd == 300)
		PostMessage(MainWindow->getHWnd(), WM_COMMAND, MAKEWPARAM(IDM_CONFIG, 0), 0);
	else if (cmd == 301)
		SidebarSetEnabled(false);
}

// vertical, horizontal, one screen, then the three custom layouts
static void SidebarCycleLayout()
{
	const int current = (video.layout == 3) ? (3 + customLayoutPreset) : video.layout;
	const int next = (current + 1) % 6;
	if (next < 3)
	{
		customTopFocus = false;
		customReturnLayout = -1;
		doLCDsLayout(next);
	}
	else
		SetCustomLayoutPreset(next - 3);
}

static void SidebarAction(int i, bool rightClick)
{
	switch (i)
	{
	case SB_SAVE:
		if (rightClick) SidebarSlotMenu(i);
		else HK_StateSaveSlot(lastSaveState, true);
		break;
	case SB_LOAD:
		if (rightClick) SidebarSlotMenu(i);
		else HK_StateLoadSlot(lastSaveState, true);
		break;
	case SB_PAUSE:
		PostMessage(MainWindow->getHWnd(), WM_COMMAND, MAKEWPARAM(IDM_PAUSE, 0), 0);
		break;
	case SB_SPEED:
		sidebarSpeedMode = rightClick ? 0 : ((sidebarSpeedMode + 1) % 4);
		SidebarApplySpeed();
		break;
	case SB_VOLUME:
		SidebarVolumeMenu(i);
		break;
	case SB_SWAP:
		LCDsSwap(-1);
		if (romloaded) Display();
		break;
	case SB_LAYOUT:
		if (rightClick) SidebarLayoutMenu(i);
		else SidebarCycleLayout();
		break;
	case SB_FOCUS:
		ToggleTopScreenFocus();
		break;
	case SB_FULL:
		ToggleFullscreen();
		break;
	case SB_CONFIG:
		SidebarConfigMenu(i);
		break;
	}
	if (sidebarHwnd) InvalidateRect(sidebarHwnd, NULL, FALSE);
}

static void SidebarTimerTick()
{
	const unsigned sig = SidebarStateSignature();
	if (sig != sidebarStateSig)
	{
		sidebarStateSig = sig;
		InvalidateRect(sidebarHwnd, NULL, FALSE);
	}

	if (!sidebarEnabled || !SidebarIsFullscreen())
		return;

	HWND mainWnd = MainWindow->getHWnd();
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(mainWnd, &p);
	RECT cr;
	GetClientRect(mainWnd, &cr);
	const int tb = MainWindowToolbar->GetHeight();
	const bool insideVertically = (p.y >= tb && p.y < cr.bottom);

	if (!sidebarOverlayShown)
	{
		if (insideVertically && p.x >= 0 && p.x <= 3 && GetForegroundWindow() == mainWnd)
		{
			sidebarOverlayShown = true;
			SidebarReposition(mainWnd, tb);
		}
	}
	else if (!sidebarMenuOpen)
	{
		if (!(insideVertically && p.x >= 0 && p.x <= SidebarWidth() + 6))
		{
			sidebarOverlayShown = false;
			SidebarReposition(mainWnd, tb);
			if (romloaded) Display();
		}
	}
}

static LRESULT CALLBACK SidebarProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		SidebarPaint(hwnd);
		return 0;

	case WM_MOUSEACTIVATE:
		return MA_NOACTIVATE;

	case WM_MOUSEMOVE:
		{
			const int hit = SidebarHitTest(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			if (hit != sidebarHot)
			{
				sidebarHot = hit;
				InvalidateRect(hwnd, NULL, FALSE);
			}
			if (!sidebarTracking)
			{
				TRACKMOUSEEVENT tme;
				tme.cbSize = sizeof(tme);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hwnd;
				tme.dwHoverTime = 0;
				TrackMouseEvent(&tme);
				sidebarTracking = true;
			}
		}
		return 0;

	case WM_MOUSELEAVE:
		sidebarTracking = false;
		sidebarHot = -1;
		InvalidateRect(hwnd, NULL, FALSE);
		return 0;

	case WM_LBUTTONDOWN:
		sidebarDown = SidebarHitTest(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		SetCapture(hwnd);
		InvalidateRect(hwnd, NULL, FALSE);
		return 0;

	case WM_LBUTTONUP:
		{
			ReleaseCapture();
			const int hit = SidebarHitTest(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			const int down = sidebarDown;
			sidebarDown = -1;
			InvalidateRect(hwnd, NULL, FALSE);
			if (hit >= 0 && hit == down)
				SidebarAction(hit, false);
			SetFocus(MainWindow->getHWnd());
		}
		return 0;

	case WM_RBUTTONUP:
		{
			const int hit = SidebarHitTest(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			if (hit >= 0)
				SidebarAction(hit, true);
			SetFocus(MainWindow->getHWnd());
		}
		return 0;

	case WM_TIMER:
		SidebarTimerTick();
		return 0;

	case WM_DESTROY:
		KillTimer(hwnd, 1);
		return 0;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

static void SidebarCreate(HWND parent)
{
	sidebarEnabled = GetPrivateProfileBool("Display", "Show Sidebar", true, IniName);

	static bool registered = false;
	if (!registered)
	{
		WNDCLASSEXA wc;
		ZeroMemory(&wc, sizeof(wc));
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = SidebarProc;
		wc.hInstance = hAppInst;
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = NULL;
		wc.lpszClassName = "DeSmuMESidebar";
		if (!RegisterClassExA(&wc)) return;
		registered = true;
	}

	sidebarHwnd = CreateWindowExA(0, "DeSmuMESidebar", "", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 10, 10, parent, NULL, hAppInst, NULL);
	if (sidebarHwnd == NULL) return;

	SetTimer(sidebarHwnd, 1, 120, NULL);
	SidebarApplySpeed(); // start at x1
}

#endif // DESMUME_SIDEBAR_H
