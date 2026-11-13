/*
	Quick-access sidebar for the DeSmuME Windows frontend (personal build) - glass look, version 2.

	A narrow bar docked on the left of the main window, holding the buttons used most often.
	It is a child window of the main window, so the DirectDraw clipper keeps the picture out of it.
	In fullscreen the bar is hidden, and slides over the picture when the mouse touches the left edge.
	It is drawn with GDI+ (gradients, soft glow, vector icons) and comes in 4 themes.

	This file is included once, from main.cpp.
*/

#ifndef DESMUME_SIDEBAR_H
#define DESMUME_SIDEBAR_H

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

extern int lastSaveState;

// Button ids. The order shown on screen is stored separately (sidebarOrder), and must stay single digits (< 10 buttons).
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

#define SIDEBAR_THEME_COUNT 4

struct SidebarThemeDef
{
	const char* name;
	DWORD panelTop, panelBottom;   // background gradient (0xAARRGGBB)
	DWORD btnTop, btnBottom;       // button glass gradient
	DWORD hotOverlay, downOverlay; // mouse over / pressed
	DWORD border, highlight;       // outline and inner top highlight
	DWORD accent, accent2;         // active button outline / active icon
	DWORD icon, text;
};

static const SidebarThemeDef sidebarThemes[SIDEBAR_THEME_COUNT] =
{
	{ "Frosted Dark",  0xF0262A36, 0xF012141C, 0x34FFFFFF, 0x0EFFFFFF, 0x22FFFFFF, 0x3CFFFFFF, 0x44FFFFFF, 0x78FFFFFF, 0xFF78AAFF, 0xFF78AAFF, 0xFFEAEFFA, 0xFFC8D0E0 },
	{ "Frosted Light", 0xF2EEF2FA, 0xF2D4DAE8, 0xB4FFFFFF, 0x78FFFFFF, 0x64FFFFFF, 0x30000000, 0x40304060, 0xF0FFFFFF, 0xFF2A64E6, 0xFF2A64E6, 0xFF2A3246, 0xFF3A4458 },
	{ "Aurora",        0xF01C1640, 0xF00C2C48, 0x38FFFFFF, 0x10FFFFFF, 0x24FFFFFF, 0x40FFFFFF, 0x50C8B4FF, 0x80FFFFFF, 0xFFFF78C8, 0xFF5AE6FF, 0xFFEEE8FF, 0xFFC8C0F0 },
	{ "Neo-DS",        0xF4E6EAF0, 0xF4B8C2D2, 0xD2FFFFFF, 0x8CF0F4FA, 0x50FFFFFF, 0x32000000, 0x60708098, 0xFFFFFFFF, 0xFF0A78D7, 0xFF0A78D7, 0xFF3C4A64, 0xFF44506A }
};

static const char* sidebarNames[SB_COUNT] =
{
	"Save", "Load", "Pause", "Speed", "Volume", "Swap screens", "Layout", "Focus top screen", "Fullscreen", "Settings"
};

static HWND sidebarHwnd = NULL;
static HWND sidebarTip = NULL;
static ULONG_PTR sidebarGdiToken = 0;
static bool sidebarEnabled = true;          // user setting (View > Show Sidebar)
static bool sidebarOverlayShown = false;    // fullscreen only: bar shown over the picture
static bool sidebarMenuOpen = false;
static bool sidebarTracking = false;
static int sidebarSpeedMode = 0;            // 0 = x1, 1 = x2, 2 = x4, 3 = x8 (no limiter)
static int sidebarHot = -1;                 // button id under the mouse
static int sidebarDown = -1;                // button id being pressed
static int sidebarVolumeBeforeMute = 100;
static int sidebarTheme = 0;
static int sidebarOrder[SB_COUNT];          // button ids, in display order
static unsigned sidebarHiddenMask = 0;      // bit per button id
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
	return MulDiv(60, dpi, 96);
}

// ---- button order and visibility ----

static void SidebarResetButtons()
{
	for (int i = 0; i < SB_COUNT; i++)
		sidebarOrder[i] = i;
	sidebarHiddenMask = 0;
}

static void SidebarSaveButtons()
{
	char buf[SB_COUNT + 1];
	for (int i = 0; i < SB_COUNT; i++)
		buf[i] = (char)('0' + sidebarOrder[i]);
	buf[SB_COUNT] = 0;
	WritePrivateProfileStringA("Display", "Sidebar Order", buf, IniName);
	WritePrivateProfileInt("Display", "Sidebar Hidden", (int)sidebarHiddenMask, IniName);
}

static void SidebarLoadButtons()
{
	SidebarResetButtons();

	char buf[32];
	buf[0] = 0;
	GetPrivateProfileStringA("Display", "Sidebar Order", "", buf, sizeof(buf), IniName);
	if ((int)strlen(buf) == SB_COUNT)
	{
		bool seen[SB_COUNT];
		int tmp[SB_COUNT];
		bool ok = true;
		for (int i = 0; i < SB_COUNT; i++)
			seen[i] = false;
		for (int i = 0; i < SB_COUNT && ok; i++)
		{
			const int c = buf[i] - '0';
			if (c < 0 || c >= SB_COUNT || seen[c])
				ok = false;
			else
			{
				seen[c] = true;
				tmp[i] = c;
			}
		}
		if (ok)
		{
			for (int i = 0; i < SB_COUNT; i++)
				sidebarOrder[i] = tmp[i];
		}
	}

	sidebarHiddenMask = (unsigned)GetPrivateProfileInt("Display", "Sidebar Hidden", 0, IniName);
	sidebarHiddenMask &= ((1u << SB_COUNT) - 1u);
	sidebarHiddenMask &= ~(1u << SB_CONFIG); // the settings button always stays
}

// Fills out[] with the ids of the visible buttons, in display order. Returns how many.
static int SidebarVisibleIds(int* out)
{
	int n = 0;
	for (int i = 0; i < SB_COUNT; i++)
	{
		const int id = sidebarOrder[i];
		if (sidebarHiddenMask & (1u << id))
			continue;
		out[n++] = id;
	}
	return n;
}

static void SidebarSlotRect(int slot, int count, const RECT& cr, RECT* out)
{
	const int margin = 5;
	if (count < 1) count = 1;
	int h = ((int)(cr.bottom - cr.top) - margin) / count;
	if (h > 56) h = 56;
	if (h < 26) h = 26;
	const int top = (int)cr.top + margin + slot * h;
	SetRect(out, (int)cr.left + margin, top, (int)cr.right - margin, top + h - margin);
}

static bool SidebarButtonRectById(int id, RECT* out)
{
	RECT cr;
	GetClientRect(sidebarHwnd, &cr);
	int ids[SB_COUNT];
	const int n = SidebarVisibleIds(ids);
	for (int s = 0; s < n; s++)
	{
		if (ids[s] == id)
		{
			SidebarSlotRect(s, n, cr, out);
			return true;
		}
	}
	return false;
}

static int SidebarHitTest(HWND hwnd, int x, int y)
{
	RECT cr;
	GetClientRect(hwnd, &cr);
	int ids[SB_COUNT];
	const int n = SidebarVisibleIds(ids);
	POINT p;
	p.x = x;
	p.y = y;
	for (int s = 0; s < n; s++)
	{
		RECT r;
		SidebarSlotRect(s, n, cr, &r);
		if (PtInRect(&r, p)) return ids[s];
	}
	return -1;
}

static POINT SidebarMenuPoint(int id)
{
	POINT pt;
	pt.x = 0;
	pt.y = 0;
	RECT r;
	if (SidebarButtonRectById(id, &r))
	{
		pt.x = r.right;
		pt.y = r.top;
	}
	ClientToScreen(sidebarHwnd, &pt);
	return pt;
}

// ---- placement ----

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

// ---- actions ----

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

static bool SidebarButtonActive(int id)
{
	switch (id)
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
	s |= ((unsigned)sidebarTheme & 7u) << 22;
	return s;
}

// ---- drawing ----

static Gdiplus::Color SbColor(DWORD argb, int alphaScale = 255)
{
	const int a = (int)((argb >> 24) & 0xFF) * alphaScale / 255;
	return Gdiplus::Color((BYTE)a, (BYTE)((argb >> 16) & 0xFF), (BYTE)((argb >> 8) & 0xFF), (BYTE)(argb & 0xFF));
}

static Gdiplus::Color SbOpaque(DWORD argb)
{
	return Gdiplus::Color(255, (BYTE)((argb >> 16) & 0xFF), (BYTE)((argb >> 8) & 0xFF), (BYTE)(argb & 0xFF));
}

static void SbRoundRectPath(Gdiplus::GraphicsPath& path, float x, float y, float w, float h, float r)
{
	const float d = r * 2.0f;
	path.AddArc(x, y, d, d, 180.0f, 90.0f);
	path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
	path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
	path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
	path.CloseFigure();
}

// Small text under the icon (slot number, speed, volume). Returns false when the button has none.
static bool SidebarCaption(int id, wchar_t* buf, size_t count)
{
	switch (id)
	{
	case SB_SAVE:
	case SB_LOAD:
		_snwprintf(buf, count, L"#%d", lastSaveState);
		break;
	case SB_SPEED:
		{
			static const int mult[4] = { 1, 2, 4, 8 };
			_snwprintf(buf, count, L"x%d", mult[sidebarSpeedMode & 3]);
		}
		break;
	case SB_VOLUME:
		_snwprintf(buf, count, L"%d", sndvolume);
		break;
	default:
		return false;
	}
	buf[count - 1] = 0;
	return true;
}

// Vector icons, drawn on a 24x24 grid centered on (cx, cy) and scaled to size s.
static void SidebarDrawIcon(Gdiplus::Graphics& g, int id, float cx, float cy, float s, DWORD color)
{
	const float f = s / 24.0f;
	const float ox = cx - 12.0f * f;
	const float oy = cy - 12.0f * f;
#define SBP(px, py) Gdiplus::PointF(ox + (px) * f, oy + (py) * f)

	float pw = 2.0f * f;
	if (pw < 1.5f) pw = 1.5f;
	Gdiplus::Pen pen(SbColor(color), pw);
	pen.SetStartCap(Gdiplus::LineCapRound);
	pen.SetEndCap(Gdiplus::LineCapRound);
	pen.SetLineJoin(Gdiplus::LineJoinRound);
	Gdiplus::SolidBrush fill(SbColor(color));

	switch (id)
	{
	case SB_SAVE: // arrow going down into a tray
		{
			Gdiplus::PointF tray[4] = { SBP(4, 14), SBP(4, 20), SBP(20, 20), SBP(20, 14) };
			g.DrawLines(&pen, tray, 4);
			g.DrawLine(&pen, SBP(12, 4), SBP(12, 15));
			Gdiplus::PointF head[3] = { SBP(7, 10), SBP(12, 15), SBP(17, 10) };
			g.DrawLines(&pen, head, 3);
		}
		break;
	case SB_LOAD: // arrow coming up out of a tray
		{
			Gdiplus::PointF tray[4] = { SBP(4, 14), SBP(4, 20), SBP(20, 20), SBP(20, 14) };
			g.DrawLines(&pen, tray, 4);
			g.DrawLine(&pen, SBP(12, 16), SBP(12, 5));
			Gdiplus::PointF head[3] = { SBP(7, 10), SBP(12, 5), SBP(17, 10) };
			g.DrawLines(&pen, head, 3);
		}
		break;
	case SB_PAUSE:
		if (romloaded && paused)
		{
			Gdiplus::PointF tri[3] = { SBP(7, 4), SBP(20, 12), SBP(7, 20) };
			g.FillPolygon(&fill, tri, 3);
		}
		else
		{
			g.FillRectangle(&fill, ox + 6.0f * f, oy + 4.0f * f, 4.0f * f, 16.0f * f);
			g.FillRectangle(&fill, ox + 14.0f * f, oy + 4.0f * f, 4.0f * f, 16.0f * f);
		}
		break;
	case SB_SPEED: // two chevrons
		{
			Gdiplus::PointF a[3] = { SBP(4, 5), SBP(11, 12), SBP(4, 19) };
			Gdiplus::PointF b[3] = { SBP(12, 5), SBP(19, 12), SBP(12, 19) };
			g.DrawLines(&pen, a, 3);
			g.DrawLines(&pen, b, 3);
		}
		break;
	case SB_VOLUME: // speaker with waves, or crossed when muted
		{
			Gdiplus::PointF sp[6] = { SBP(3, 9), SBP(7, 9), SBP(12, 4.5f), SBP(12, 19.5f), SBP(7, 15), SBP(3, 15) };
			g.FillPolygon(&fill, sp, 6);
			if (sndvolume == 0)
			{
				g.DrawLine(&pen, SBP(15.5f, 9), SBP(21.5f, 15));
				g.DrawLine(&pen, SBP(21.5f, 9), SBP(15.5f, 15));
			}
			else
			{
				g.DrawArc(&pen, ox + 10.0f * f, oy + 7.0f * f, 7.0f * f, 10.0f * f, -60.0f, 120.0f);
				if (sndvolume > 40)
					g.DrawArc(&pen, ox + 8.0f * f, oy + 3.5f * f, 12.0f * f, 17.0f * f, -60.0f, 120.0f);
			}
		}
		break;
	case SB_SWAP: // two opposite arrows
		{
			g.DrawLine(&pen, SBP(8, 19), SBP(8, 5));
			Gdiplus::PointF h1[3] = { SBP(4, 9), SBP(8, 5), SBP(12, 9) };
			g.DrawLines(&pen, h1, 3);
			g.DrawLine(&pen, SBP(16, 5), SBP(16, 19));
			Gdiplus::PointF h2[3] = { SBP(12, 15), SBP(16, 19), SBP(20, 15) };
			g.DrawLines(&pen, h2, 3);
		}
		break;
	case SB_LAYOUT: // a window split in panes
		{
			Gdiplus::GraphicsPath p;
			SbRoundRectPath(p, ox + 3.0f * f, oy + 4.0f * f, 18.0f * f, 16.0f * f, 2.5f * f);
			g.DrawPath(&pen, &p);
			g.DrawLine(&pen, SBP(3, 12), SBP(21, 12));
			g.DrawLine(&pen, SBP(13, 12), SBP(13, 20));
		}
		break;
	case SB_FOCUS: // a frame with the top screen filling it
		{
			Gdiplus::GraphicsPath p;
			SbRoundRectPath(p, ox + 3.0f * f, oy + 3.0f * f, 18.0f * f, 18.0f * f, 2.5f * f);
			g.DrawPath(&pen, &p);
			g.FillRectangle(&fill, ox + 6.5f * f, oy + 6.5f * f, 11.0f * f, 6.0f * f);
		}
		break;
	case SB_FULL: // four corners
		{
			Gdiplus::PointF c1[3] = { SBP(4, 9), SBP(4, 4), SBP(9, 4) };
			Gdiplus::PointF c2[3] = { SBP(15, 4), SBP(20, 4), SBP(20, 9) };
			Gdiplus::PointF c3[3] = { SBP(20, 15), SBP(20, 20), SBP(15, 20) };
			Gdiplus::PointF c4[3] = { SBP(9, 20), SBP(4, 20), SBP(4, 15) };
			g.DrawLines(&pen, c1, 3);
			g.DrawLines(&pen, c2, 3);
			g.DrawLines(&pen, c3, 3);
			g.DrawLines(&pen, c4, 3);
		}
		break;
	case SB_CONFIG: // three sliders
		{
			g.DrawLine(&pen, SBP(4, 7), SBP(20, 7));
			g.DrawLine(&pen, SBP(4, 12), SBP(20, 12));
			g.DrawLine(&pen, SBP(4, 17), SBP(20, 17));
			g.FillEllipse(&fill, ox + (8.0f - 2.6f) * f, oy + (7.0f - 2.6f) * f, 5.2f * f, 5.2f * f);
			g.FillEllipse(&fill, ox + (15.0f - 2.6f) * f, oy + (12.0f - 2.6f) * f, 5.2f * f, 5.2f * f);
			g.FillEllipse(&fill, ox + (9.0f - 2.6f) * f, oy + (17.0f - 2.6f) * f, 5.2f * f, 5.2f * f);
		}
		break;
	}
#undef SBP
}

static void SidebarDrawButton(Gdiplus::Graphics& g, const SidebarThemeDef& th, int id, const RECT& r, Gdiplus::Font* font)
{
	const float x = (float)r.left;
	const float y = (float)r.top;
	const float w = (float)(r.right - r.left);
	const float h = (float)(r.bottom - r.top);
	const bool active = SidebarButtonActive(id);
	const bool hot = (sidebarHot == id);
	const bool down = hot && (sidebarDown == id);

	Gdiplus::GraphicsPath path;
	SbRoundRectPath(path, x, y, w, h, 10.0f);

	if (active) // soft glow around the outline
	{
		Gdiplus::Pen g3(SbColor(th.accent, 30), 7.0f);
		Gdiplus::Pen g2(SbColor(th.accent, 55), 5.0f);
		Gdiplus::Pen g1(SbColor(th.accent, 90), 3.0f);
		g.DrawPath(&g3, &path);
		g.DrawPath(&g2, &path);
		g.DrawPath(&g1, &path);
	}

	{
		Gdiplus::LinearGradientBrush glass(Gdiplus::PointF(0.0f, y), Gdiplus::PointF(0.0f, y + h), SbColor(th.btnTop), SbColor(th.btnBottom));
		g.FillPath(&glass, &path);
	}
	if (hot)
	{
		Gdiplus::SolidBrush hb(SbColor(th.hotOverlay));
		g.FillPath(&hb, &path);
	}
	if (down)
	{
		Gdiplus::SolidBrush db(SbColor(th.downOverlay));
		g.FillPath(&db, &path);
	}
	{
		Gdiplus::Pen bp(active ? SbColor(th.accent) : SbColor(th.border), active ? 1.6f : 1.0f);
		g.DrawPath(&bp, &path);
	}
	{
		Gdiplus::Pen hp(SbColor(th.highlight), 1.0f);
		g.DrawLine(&hp, x + 9.0f, y + 1.5f, x + w - 9.0f, y + 1.5f);
	}

	wchar_t caption[16];
	const bool hasCaption = SidebarCaption(id, caption, 16);

	float iconSize = w * 0.5f;
	const float maxByHeight = h * (hasCaption ? 0.44f : 0.56f);
	if (iconSize > maxByHeight) iconSize = maxByHeight;
	if (iconSize > 26.0f) iconSize = 26.0f;
	const float iconCy = y + h * (hasCaption ? 0.38f : 0.5f);

	SidebarDrawIcon(g, id, x + w * 0.5f, iconCy, iconSize, active ? th.accent2 : th.icon);

	if (hasCaption && font != NULL)
	{
		Gdiplus::StringFormat fmt;
		fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
		fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
		fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
		Gdiplus::SolidBrush tb(SbColor(active ? th.accent2 : th.text));
		g.DrawString(caption, -1, font, Gdiplus::RectF(x, y + h * 0.62f, w, h * 0.34f), &fmt, &tb);
	}
}

static void SidebarDraw(Gdiplus::Graphics& g, int W, int H)
{
	const SidebarThemeDef& th = sidebarThemes[sidebarTheme];
	g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

	g.Clear(SbOpaque(th.panelBottom));
	{
		Gdiplus::LinearGradientBrush bg(Gdiplus::PointF(0.0f, 0.0f), Gdiplus::PointF(0.0f, (float)H), SbColor(th.panelTop), SbColor(th.panelBottom));
		g.FillRectangle(&bg, 0, 0, W, H);
	}
	{
		// glass sheen across the upper part of the bar
		Gdiplus::PointF sheen[4] =
		{
			Gdiplus::PointF(0.0f, 0.0f),
			Gdiplus::PointF((float)W, 0.0f),
			Gdiplus::PointF((float)W, (float)H * 0.16f),
			Gdiplus::PointF(0.0f, (float)H * 0.30f)
		};
		Gdiplus::SolidBrush sb(Gdiplus::Color(20, 255, 255, 255));
		g.FillPolygon(&sb, sheen, 4);
	}
	{
		Gdiplus::Pen edge(SbColor(th.border), 1.0f);
		g.DrawLine(&edge, (float)(W - 1), 0.0f, (float)(W - 1), (float)H);
	}

	Gdiplus::FontFamily family(L"Segoe UI");
	Gdiplus::Font font(&family, 7.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);

	RECT cr;
	SetRect(&cr, 0, 0, W, H);
	int ids[SB_COUNT];
	const int n = SidebarVisibleIds(ids);
	for (int s = 0; s < n; s++)
	{
		RECT r;
		SidebarSlotRect(s, n, cr, &r);
		SidebarDrawButton(g, th, ids[s], r, &font);
	}
}

static void SidebarPaint(HWND hwnd)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	RECT cr;
	GetClientRect(hwnd, &cr);
	const int W = (int)(cr.right - cr.left);
	const int H = (int)(cr.bottom - cr.top);
	if (W > 0 && H > 0 && sidebarGdiToken != 0)
	{
		Gdiplus::Bitmap bmp(W, H, PixelFormat32bppPARGB);
		{
			Gdiplus::Graphics g(&bmp);
			SidebarDraw(g, W, H);
		}
		Gdiplus::Graphics wg(hdc);
		wg.DrawImage(&bmp, 0, 0);
	}
	EndPaint(hwnd, &ps);
}

// ---- tooltips ----

static void SidebarTooltipText(int id, char* buf, size_t size)
{
	switch (id)
	{
	case SB_SAVE:   _snprintf(buf, size, "Save state (slot %d) - right-click: choose slot", lastSaveState); break;
	case SB_LOAD:   _snprintf(buf, size, "Load state (slot %d) - right-click: choose slot", lastSaveState); break;
	case SB_PAUSE:  _snprintf(buf, size, "%s", (romloaded && paused) ? "Resume" : "Pause"); break;
	case SB_SPEED:
		{
			static const int mult[4] = { 1, 2, 4, 8 };
			_snprintf(buf, size, "Speed x%d - click: next speed, right-click: x1", mult[sidebarSpeedMode & 3]);
		}
		break;
	case SB_VOLUME: _snprintf(buf, size, "Volume %d%%", sndvolume); break;
	case SB_SWAP:   _snprintf(buf, size, "Swap top and bottom screens"); break;
	case SB_LAYOUT: _snprintf(buf, size, "Next layout - right-click: all layouts"); break;
	case SB_FOCUS:  _snprintf(buf, size, "Focus top screen"); break;
	case SB_FULL:   _snprintf(buf, size, "Fullscreen (Alt+Enter)"); break;
	case SB_CONFIG: _snprintf(buf, size, "Sidebar settings: theme, buttons, save slot"); break;
	default:        _snprintf(buf, size, "%s", ""); break;
	}
	buf[size - 1] = 0;
}

static void SidebarUpdateTooltips()
{
	if (sidebarTip == NULL || sidebarHwnd == NULL)
		return;

	for (int id = 0; id < SB_COUNT; id++)
	{
		TOOLINFOA ti;
		ZeroMemory(&ti, sizeof(ti));
		ti.cbSize = sizeof(ti);
		ti.hwnd = sidebarHwnd;
		ti.uId = (UINT_PTR)id;
		SendMessageA(sidebarTip, TTM_DELTOOLA, 0, (LPARAM)&ti);
	}

	RECT cr;
	GetClientRect(sidebarHwnd, &cr);
	int ids[SB_COUNT];
	const int n = SidebarVisibleIds(ids);
	for (int s = 0; s < n; s++)
	{
		TOOLINFOA ti;
		ZeroMemory(&ti, sizeof(ti));
		ti.cbSize = sizeof(ti);
		ti.uFlags = TTF_SUBCLASS;
		ti.hwnd = sidebarHwnd;
		ti.uId = (UINT_PTR)ids[s];
		SidebarSlotRect(s, n, cr, &ti.rect);
		ti.lpszText = LPSTR_TEXTCALLBACKA;
		SendMessageA(sidebarTip, TTM_ADDTOOLA, 0, (LPARAM)&ti);
	}
}

static void SidebarRefreshLayout()
{
	SidebarUpdateTooltips();
	if (sidebarHwnd) InvalidateRect(sidebarHwnd, NULL, FALSE);
}

// ---- menus ----

// Shows a popup menu at a screen position. Returns the chosen command, or 0.
static int SidebarTrackMenu(HMENU menu, POINT pt)
{
	sidebarMenuOpen = true;
	SetForegroundWindow(MainWindow->getHWnd());
	int cmd = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, sidebarHwnd, NULL);
	sidebarMenuOpen = false;
	DestroyMenu(menu); // also destroys the submenus
	return cmd;
}

static HMENU SidebarBuildSlotMenu()
{
	HMENU menu = CreatePopupMenu();
	for (int n = 0; n < 10; n++)
	{
		char text[32];
		_snprintf(text, sizeof(text), "Slot %d", n);
		text[sizeof(text) - 1] = 0;
		AppendMenuA(menu, MF_STRING | (n == lastSaveState ? MF_CHECKED : 0), 100 + n, text);
	}
	return menu;
}

static void SidebarSlotMenu(int id)
{
	int cmd = SidebarTrackMenu(SidebarBuildSlotMenu(), SidebarMenuPoint(id));
	if (cmd >= 100 && cmd < 110)
		HK_StateSetSlot(cmd - 100, true);
}

static void SidebarVolumeMenu(int id)
{
	static const int levels[5] = { 0, 25, 50, 75, 100 };
	static const char* names[5] = { "Mute", "25%", "50%", "75%", "100%" };
	HMENU menu = CreatePopupMenu();
	for (int k = 0; k < 5; k++)
		AppendMenuA(menu, MF_STRING | (sndvolume == levels[k] ? MF_CHECKED : 0), 200 + k, names[k]);
	int cmd = SidebarTrackMenu(menu, SidebarMenuPoint(id));
	if (cmd >= 200 && cmd < 205)
		SidebarSetVolume(levels[cmd - 200]);
}

static void SidebarLayoutMenu(int id)
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
	int cmd = SidebarTrackMenu(menu, SidebarMenuPoint(id));
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

// Used by the hotkey
void ToggleSidebarVisibility()
{
	SidebarSetEnabled(!sidebarEnabled);
}

static void SidebarSetTheme(int t)
{
	if (t < 0 || t >= SIDEBAR_THEME_COUNT) return;
	sidebarTheme = t;
	WritePrivateProfileInt("Display", "Sidebar Theme", sidebarTheme, IniName);
	if (sidebarHwnd) InvalidateRect(sidebarHwnd, NULL, FALSE);
}

static void SidebarMoveButton(int id, int direction)
{
	int pos = -1;
	for (int i = 0; i < SB_COUNT; i++)
		if (sidebarOrder[i] == id) pos = i;
	const int other = pos + direction;
	if (pos < 0 || other < 0 || other >= SB_COUNT) return;
	const int tmp = sidebarOrder[pos];
	sidebarOrder[pos] = sidebarOrder[other];
	sidebarOrder[other] = tmp;
}

// The settings menu: theme, which buttons to show and in what order, save slot, controls.
static void SidebarConfigMenu(POINT pt)
{
	HMENU themes = CreatePopupMenu();
	for (int t = 0; t < SIDEBAR_THEME_COUNT; t++)
		AppendMenuA(themes, MF_STRING | (t == sidebarTheme ? MF_CHECKED : 0), 400 + t, sidebarThemes[t].name);

	HMENU buttons = CreatePopupMenu();
	for (int s = 0; s < SB_COUNT; s++)
	{
		const int id = sidebarOrder[s];
		const bool hidden = (sidebarHiddenMask & (1u << id)) != 0;
		HMENU sub = CreatePopupMenu();
		AppendMenuA(sub, MF_STRING | (hidden ? 0 : MF_CHECKED) | (id == SB_CONFIG ? MF_GRAYED : 0), 500 + id * 10, "Visible");
		AppendMenuA(sub, MF_STRING | (s == 0 ? MF_GRAYED : 0), 501 + id * 10, "Move up");
		AppendMenuA(sub, MF_STRING | (s == SB_COUNT - 1 ? MF_GRAYED : 0), 502 + id * 10, "Move down");
		AppendMenuA(buttons, MF_POPUP, (UINT_PTR)sub, sidebarNames[id]);
	}
	AppendMenuA(buttons, MF_SEPARATOR, 0, NULL);
	AppendMenuA(buttons, MF_STRING, 600, "Reset buttons");

	HMENU menu = CreatePopupMenu();
	AppendMenuA(menu, MF_POPUP, (UINT_PTR)themes, "Theme");
	AppendMenuA(menu, MF_POPUP, (UINT_PTR)buttons, "Buttons");
	AppendMenuA(menu, MF_POPUP, (UINT_PTR)SidebarBuildSlotMenu(), "Save slot");
	AppendMenuA(menu, MF_STRING, 300, "Control config...");
	AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuA(menu, MF_STRING, 301, "Hide sidebar (View > Show Sidebar)");

	const int cmd = SidebarTrackMenu(menu, pt);
	if (cmd >= 100 && cmd < 110)
		HK_StateSetSlot(cmd - 100, true);
	else if (cmd >= 400 && cmd < 400 + SIDEBAR_THEME_COUNT)
		SidebarSetTheme(cmd - 400);
	else if (cmd >= 500 && cmd < 600)
	{
		const int id = (cmd - 500) / 10;
		const int op = (cmd - 500) % 10;
		if (id >= 0 && id < SB_COUNT)
		{
			if (op == 0 && id != SB_CONFIG)
				sidebarHiddenMask ^= (1u << id);
			else if (op == 1)
				SidebarMoveButton(id, -1);
			else if (op == 2)
				SidebarMoveButton(id, 1);
			SidebarSaveButtons();
			SidebarRefreshLayout();
		}
	}
	else if (cmd == 600)
	{
		SidebarResetButtons();
		SidebarSaveButtons();
		SidebarRefreshLayout();
	}
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

static void SidebarAction(int id, bool rightClick)
{
	switch (id)
	{
	case SB_SAVE:
		if (rightClick) SidebarSlotMenu(id);
		else HK_StateSaveSlot(lastSaveState, true);
		break;
	case SB_LOAD:
		if (rightClick) SidebarSlotMenu(id);
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
		SidebarVolumeMenu(id);
		break;
	case SB_SWAP:
		LCDsSwap(-1);
		if (romloaded) Display();
		break;
	case SB_LAYOUT:
		if (rightClick) SidebarLayoutMenu(id);
		else SidebarCycleLayout();
		break;
	case SB_FOCUS:
		ToggleTopScreenFocus();
		break;
	case SB_FULL:
		ToggleFullscreen();
		break;
	case SB_CONFIG:
		SidebarConfigMenu(SidebarMenuPoint(id));
		break;
	}
	if (sidebarHwnd) InvalidateRect(sidebarHwnd, NULL, FALSE);
}

// ---- window ----

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

	case WM_SIZE:
		SidebarRefreshLayout();
		return 0;

	case WM_MOUSEACTIVATE:
		return MA_NOACTIVATE;

	case WM_NOTIFY:
		{
			NMHDR* nm = (NMHDR*)lParam;
			if (nm != NULL && nm->hwndFrom == sidebarTip && nm->code == TTN_GETDISPINFOA)
			{
				NMTTDISPINFOA* di = (NMTTDISPINFOA*)lParam;
				SidebarTooltipText((int)nm->idFrom, di->szText, sizeof(di->szText));
				return 0;
			}
		}
		break;

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
			else
			{
				// right-click on the bar itself: settings menu
				POINT pt;
				GetCursorPos(&pt);
				SidebarConfigMenu(pt);
			}
			SetFocus(MainWindow->getHWnd());
		}
		return 0;

	case WM_TIMER:
		SidebarTimerTick();
		return 0;

	case WM_DESTROY:
		KillTimer(hwnd, 1);
		if (sidebarGdiToken != 0)
		{
			Gdiplus::GdiplusShutdown(sidebarGdiToken);
			sidebarGdiToken = 0;
		}
		return 0;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

static void SidebarCreate(HWND parent)
{
	sidebarEnabled = GetPrivateProfileBool("Display", "Show Sidebar", true, IniName);
	sidebarTheme = GetPrivateProfileInt("Display", "Sidebar Theme", 0, IniName);
	if (sidebarTheme < 0 || sidebarTheme >= SIDEBAR_THEME_COUNT) sidebarTheme = 0;
	SidebarLoadButtons();

	Gdiplus::GdiplusStartupInput gdiInput;
	if (Gdiplus::GdiplusStartup(&sidebarGdiToken, &gdiInput, NULL) != Gdiplus::Ok)
		sidebarGdiToken = 0;

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

	sidebarTip = CreateWindowExA(WS_EX_TOPMOST, "tooltips_class32", NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, sidebarHwnd, NULL, hAppInst, NULL);
	if (sidebarTip != NULL)
		SetWindowPos(sidebarTip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	SetTimer(sidebarHwnd, 1, 120, NULL);
	SidebarApplySpeed(); // start at x1
	SidebarUpdateTooltips();
}

#endif // DESMUME_SIDEBAR_H
