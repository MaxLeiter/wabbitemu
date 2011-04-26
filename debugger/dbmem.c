#include "stdafx.h"

#include "core.h"
#include "dbmem.h"
#include "dbcommon.h"
#include "resource.h"
#include "expandpane.h"

#define COLUMN_X_OFFSET 7

extern HWND hwndLastFocus;
extern HINSTANCE g_hInst;
extern unsigned short goto_addr;
extern int find_value;
extern HFONT hfontLucida, hfontLucidaBold, hfontSegoe;

static int AddrFromPoint(HWND hwnd, POINT pt, RECT *r) {
	mp_settings *mps = (mp_settings *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	RECT hr;
	GetWindowRect(mps->hwndHeader, &hr);

	int cyHeader = hr.bottom - hr.top;

	TEXTMETRIC tm;
	HDC hdc = GetDC(hwnd);
	SelectObject(hdc, hfontLucida);
	GetTextMetrics(hdc, &tm);
	ReleaseDC(hwnd, hdc);


	int x = pt.x - tm.tmAveCharWidth * 7 - COLUMN_X_OFFSET - (int) (mps->diff - mps->cxMem - 2 * tm.tmAveCharWidth) / 2;
	int y = pt.y - cyHeader;

	int row = y/mps->cyRow;
	int col = (int) (x/mps->diff);

	int addr = mps->addr + ((col + (mps->nCols * row)) * mps->mode);

	if (r != NULL) {
		r->left = tm.tmAveCharWidth*7 + COLUMN_X_OFFSET + (int) (mps->diff * col);
		r->right = r->left + mps->cxMem - 2*tm.tmAveCharWidth;
		r->top = cyHeader + (row*mps->cyRow);
		r->bottom = r->top + tm.tmHeight;
	}

	return addr;
}


static void ScrollUp(HWND hwnd) {
	mp_settings *mps = (mp_settings*) GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (mps->addr > 0)
		mps->addr -= mps->nCols * mps->mode;

	InvalidateRect(hwnd, NULL, FALSE);
	UpdateWindow(hwnd);
}

static void ScrollDown(HWND hwnd) {
	mp_settings *mps = (mp_settings*) GetWindowLongPtr(hwnd, GWLP_USERDATA);

	int data_length = mps->nCols * mps->nRows * mps->mode;
	if (mps->addr + data_length - mps->nCols * mps->mode <= 0xFFFF)
		mps->addr += mps->nCols * mps->mode;

	InvalidateRect(hwnd, NULL, FALSE);
	UpdateWindow(hwnd);
}

static VALUE_FORMAT GetValueFormat(mp_settings *mps) {
	VALUE_FORMAT format = CHAR1;
	if (mps->bText)
		return format;
	switch (mps->mode) {
	case 1:
		switch (mps->display) {
			case HEX:
				format = HEX2;
				break;
			case DEC:
				format = DEC3;
				break;
			case BIN:
				format = BIN8;
				break;
		}
		break;
	case 2:
		switch (mps->display) {
			case HEX:
				format = HEX4;
				break;
			case DEC:
				format = DEC3;
				break;
			case BIN:
				format = BIN16;
				break;
		}
		break;
	}
	return format;
}


LRESULT CALLBACK MemProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	static TEXTMETRIC tm;
	static HWND hwndVal;
	int kMemWidth;
	static int cyHeader;
	switch (Message) {
		case WM_SETFOCUS:
			hwndLastFocus = hwnd;
			return 0;
		case WM_CREATE:
		{
			mp_settings *mps;
			RECT rc;
			GetClientRect(hwnd, &rc);

			HDC hdc = GetDC(hwnd);
			SelectObject(hdc, hfontLucida);
			GetTextMetrics(hdc, &tm);

			mps = (mp_settings *) ((CREATESTRUCT*)lParam)->lpCreateParams;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) mps);

			SelectObject(hdc, hfontSegoe);

			InitCommonControls();

			mps->hwndHeader = CreateWindowEx(0, WC_HEADER, (LPCTSTR) NULL,
				WS_CHILD | WS_VISIBLE | HDS_HORZ |/* HDS_FULLDRAG | HDS_DRAGDROP |*/ WS_CLIPSIBLINGS,
                0, 0, 1, 1, hwnd, (HMENU) ID_SIZE, g_hInst,
                (LPVOID) NULL);

			SendMessage(mps->hwndHeader, WM_SETFONT, (WPARAM) hfontSegoe, TRUE);

			WINDOWPOS wp;
			HDLAYOUT hdl;

			hdl.prc = &rc;
			hdl.pwpos = &wp;
			SendMessage(mps->hwndHeader, HDM_LAYOUT, 0, (LPARAM) &hdl);
			SetWindowPos(mps->hwndHeader, wp.hwndInsertAfter, wp.x, wp.y,
				wp.cx, wp.cy, wp.flags);

		    HDITEM hdi;

		    hdi.mask = HDI_TEXT | HDI_FORMAT | HDI_WIDTH;
			TCHAR pszText[32];
			GetWindowText(hwnd, pszText, sizeof(pszText));
		    hdi.pszText = pszText;
		    hdi.cxy = 1;
		    hdi.cchTextMax = sizeof(hdi.pszText)/sizeof(hdi.pszText[0]);
		    hdi.fmt = HDF_LEFT | HDF_STRING;
		    mps->iData = 1;

		    Header_InsertItem(mps->hwndHeader, 0, &hdi);


		    hdi.pszText = _T("Addr");
		    hdi.cxy = tm.tmAveCharWidth*7;
		    hdi.cchTextMax = sizeof(hdi.pszText)/sizeof(hdi.pszText[0]);
		    mps->iAddr = 0;

		    Header_InsertItem(mps->hwndHeader, 0, &hdi);

			if (mps->track == -1) {
				mps->cmbMode = CreateWindow(
					_T("COMBOBOX"),
					_T(""),
					CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE,
					0, 0, 1, 1,
					//mps->hwndHeader,
					hwnd,
					(HMENU) 1001, g_hInst, NULL);

				ComboBox_AddString(mps->cmbMode, _T("Byte"));
				ComboBox_AddString(mps->cmbMode, _T("Word"));
				ComboBox_AddString(mps->cmbMode, _T("Text"));

				ComboBox_SetCurSel(mps->cmbMode, 0);
				SendMessage(mps->cmbMode, WM_SETFONT, (WPARAM) hfontSegoe, (LPARAM) TRUE);

			}

			/*
			mps->hwndTip = CreateWindowEx(
					NULL,
					TOOLTIPS_CLASS,
					NULL, WS_POPUP | TTS_ALWAYSTIP,
					CW_USEDEFAULT, CW_USEDEFAULT,
					CW_USEDEFAULT, CW_USEDEFAULT,
					hwnd, NULL, g_hInst, NULL);

			SetWindowPos(mps->hwndTip, HWND_TOPMOST,0, 0, 0, 0,
			             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			SendMessage(mps->hwndTip, TTM_ACTIVATE, TRUE, 0);

			TOOLINFO toolInfo = {0};
			toolInfo.cbSize = sizeof(toolInfo);
		    toolInfo.hwnd = hwnd;
		    toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
		    toolInfo.uId = (UINT_PTR) hwnd;
		    toolInfo.lpszText = LPSTR_TEXTCALLBACK;
		    SendMessage(mps->hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
			*/


		    RECT r;
		    GetWindowRect(mps->hwndHeader, &r);
		    cyHeader = r.bottom - r.top;

		    mps->hfontData = hfontLucida;
			mps->hfontAddr = hfontLucidaBold;

			mps->cyRow = 4 * tm.tmHeight / 3;
			SendMessage(hwnd, WM_SIZE, 0, 0);

			TCHAR buffer[64];
#ifdef WINVER
			StringCbPrintf(buffer, sizeof(buffer), _T("Mem%i"), mps->memNum);
#else
			sprintf(buffer, "Mem%i", mps->memNum);
#endif
			int value = (int) QueryDebugKey(buffer);
			mps->addr = value;
			return 0;
		}
		case WM_SIZE:
		{
			RECT rc;
			mp_settings *mps = (mp_settings*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			GetClientRect(hwnd, &rc);

			WINDOWPOS wp;
			HDLAYOUT hdl;

			hdl.prc = &rc;
			hdl.pwpos = &wp;
			SendMessage(mps->hwndHeader, HDM_LAYOUT, 0, (LPARAM) &hdl);
			SetWindowPos(mps->hwndHeader, wp.hwndInsertAfter, wp.x, wp.y,
				wp.cx, wp.cy, wp.flags);


			TCHAR szHeader[64];
			StringCbPrintf(szHeader, sizeof(szHeader), _T("Memory (%d Columns)"), mps->nCols);
			HDITEM hdi;
			hdi.mask = HDI_WIDTH;
			//| HDI_TEXT;
			hdi.pszText = szHeader;
			hdi.cxy = rc.right - tm.tmAveCharWidth*6;
			Header_SetItem(mps->hwndHeader, mps->iData, &hdi);

			if (mps->cmbMode) {
				DWORD dwBaseUnits = GetDialogBaseUnits();
				SetWindowPos(mps->cmbMode,
						HWND_TOP, //mps->hwndHeader,
						wp.cx - COLUMN_X_OFFSET - (26 * LOWORD(dwBaseUnits))/4 + 2,
						0,
						(26 * LOWORD(dwBaseUnits))/4,
						cyHeader,
						0);
			}
			return 0;
		}
		case WM_MOUSEACTIVATE:
			SetFocus(hwnd);
			return 0;
		case WM_PAINT:
		{
			HDC hdc, hdcDest;
			PAINTSTRUCT ps;
			RECT r, dr;
			GetClientRect(hwnd, &r);

			hdcDest = BeginPaint(hwnd, &ps);

			mp_settings *mps = (mp_settings *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

			hdc = CreateCompatibleDC(hdcDest);
			HBITMAP hbm = CreateCompatibleBitmap(hdcDest, r.right, r.bottom);
			SelectObject(hdc, hbm);
			SetBkMode(hdc, TRANSPARENT);

			r.top = cyHeader;
			FillRect(hdc, &r, GetStockBrush(WHITE_BRUSH));

			SelectObject(hdc, GetStockObject(DC_PEN));
			SetDCPenColor(hdc, GetSysColor(COLOR_BTNFACE));

			BOOL isBinary = FALSE;
			TCHAR memfmt[8];
			VALUE_FORMAT format = GetValueFormat(mps);
			switch (format) {
				case DEC3:
				case DEC5:
					StringCbPrintf(memfmt, sizeof(memfmt), _T("%%d"), mps->mode * 2);
					kMemWidth = tm.tmAveCharWidth * mps->mode * 3;
					break;
				case HEX2:
				case HEX4:
					StringCbPrintf(memfmt, sizeof(memfmt), _T("%%0%dx"), mps->mode * 2);
					kMemWidth = tm.tmAveCharWidth * mps->mode * 2;
					break;
				case BIN8:
				case BIN16:
					isBinary = TRUE;
					kMemWidth = tm.tmAveCharWidth * mps->mode * 8;
					break;
				case CHAR1:
					StringCbCopy(memfmt, sizeof(memfmt), _T("%c"));
					kMemWidth = tm.tmAveCharWidth;
					break;
			}

			MoveToEx(hdc, tm.tmAveCharWidth * 7 - 1, cyHeader, NULL);
			LineTo(hdc, tm.tmAveCharWidth * 7 - 1, r.bottom);

			int i, j, 	rows = (r.bottom - r.top + mps->cyRow - 1)/mps->cyRow,
						cols =
						(r.right - r.left - tm.tmAveCharWidth * 7) /
						(kMemWidth + 2 * tm.tmAveCharWidth);

			mps->nRows = rows;
			mps->nCols = cols;
			mps->cxMem = kMemWidth + 2 * tm.tmAveCharWidth;

			double diff = r.right - r.left - tm.tmAveCharWidth*7 - (cols * mps->cxMem);
			if (cols != 1) {
				diff /= cols - 1;
			}

			diff += mps->cxMem;
			mps->diff = diff;
			int addr = mps->addr;

			unsigned int value;
			r.left = COLUMN_X_OFFSET;

			for (	i = 0, r.bottom = r.top + mps->cyRow;
					i < rows;
					i++, OffsetRect(&r, 0, mps->cyRow)) {
				TCHAR szVal[32];
				if (addr < 0)
					StringCbCopy(szVal, sizeof(szVal), _T("0000"));
				else
					StringCbPrintf(szVal, sizeof(szVal), _T("%04X"), addr);

				if (addr < 0x10000) {
					SelectObject(hdc, mps->hfontAddr);
					DrawText(hdc, szVal, -1, &r, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
				}

				if (addr < 0) {
					RECT tr;
					CopyRect(&tr, &r);
					DrawText(hdc, szVal, -1, &tr, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_CALCRECT);
					OffsetRect(&tr, tm.tmAveCharWidth / 3, 0);
					InflateRect(&tr, 0, -tm.tmAveCharWidth / 2);
					if ((tr.bottom - tr.top) % 2 != 0)
						tr.bottom++;

					POINT pts[] = {
							{tr.right, tr.top},
							{tr.right + (tr.bottom - 1 - tr.top)/2+1, tr.top + (tr.bottom - 1 - tr.top)/2+1},
							{tr.right, tr.bottom}};

					SelectObject(hdc, GetStockObject(DC_PEN));
					SetDCPenColor(hdc, RGB(128, 128, 128));
					SelectObject(hdc, GetStockObject(BLACK_BRUSH));
					Polygon(hdc, pts, 3);
				}

				for (j = 0; j < cols; j++) {

					CopyRect(&dr, &r);
					dr.left = tm.tmAveCharWidth * 7 + COLUMN_X_OFFSET + (int) (diff * j);
					dr.right = dr.left + kMemWidth;

					int b;
					BOOL isSel = FALSE;
					if (addr == mps->sel) isSel = TRUE;
					if (isSel == TRUE) {
						mps->xSel = dr.left;
						mps->ySel = dr.top;
						InflateRect(&dr, 2, 0);
						DrawFocusRect(hdc, &dr);
						//DrawItemSelection(hdc, &dr, hwnd == GetFocus());
						InflateRect(&dr, -2, 0);
					}

					value = 0;

					if (addr >= 0x10000) break;
					if (addr >= 0) {
						int shift;
						for (b = 0, shift = 0; b < mps->mode; b++, shift += 8) {
							value += mem_read(lpDebuggerCalc->cpu.mem_c, addr + b) << shift;
						}
						if (isBinary)
							StringCbCopy(szVal, sizeof(szVal), byte_to_binary(value, mps->mode - 1));
						else
							StringCbPrintf(szVal, sizeof(szVal), memfmt, value);

#define COLOR_MEMPOINT_WRITE	(RGB(255, 177, 100))
#define COLOR_MEMPOINT_READ		(RGB(255, 250, 145))
						if (check_mem_write_break(lpDebuggerCalc->cpu.mem_c, addr)) {
							InflateRect(&dr, 2, 0);
							DrawItemSelection(hdc, &dr, hwnd == GetFocus(), COLOR_MEMPOINT_WRITE, 255);
							if (isSel)
								DrawFocusRect(hdc, &dr);
							InflateRect(&dr, -2, 0);
						}
						if (check_mem_read_break(lpDebuggerCalc->cpu.mem_c, addr)) {
							InflateRect(&dr, 2, 0);
							DrawItemSelection(hdc, &dr, hwnd == GetFocus(), COLOR_MEMPOINT_READ , 255);
							if (isSel)
								DrawFocusRect(hdc, &dr);
							InflateRect(&dr, -2, 0);
						}
						addr += mps->mode;

						SelectObject(hdc, mps->hfontData);
						DrawText(hdc, szVal, -1, &dr, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
					} else {
						addr += mps->mode;
					}
				}
			}


			GetClientRect(hwnd, &r);
			BitBlt(hdcDest, 0, cyHeader, r.right, r.bottom, hdc, 0, cyHeader, SRCCOPY);

			EndPaint(hwnd, &ps);

			DeleteDC(hdc);
			DeleteObject(hbm);
			return 0;
		}

		case WM_NOTIFY: {
			switch (((NMHDR*) lParam)->code) {
				case HDN_BEGINTRACK:
				case HDN_ENDTRACK:
					return TRUE;
				case TTN_GETDISPINFO:
				{
					NMTTDISPINFO *nmtdi = (NMTTDISPINFO *) lParam;
					mp_settings *mps = (mp_settings*) GetWindowLongPtr(hwnd, GWLP_USERDATA);

					StringCbPrintf(nmtdi->szText, sizeof(nmtdi->szText), _T("%d"), mem_read(&lpDebuggerCalc->mem_c, mps->addrTrack));					return TRUE;
				}
			}
			return FALSE;
		}
		case WM_MOUSEWHEEL:
		{
			int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

			if (zDelta > 0) ScrollUp(hwnd);
			else ScrollDown(hwnd);

			if (LOWORD(wParam) & MK_CONTROL) {
				if (zDelta > 0) ScrollUp(hwnd);
				else ScrollDown(hwnd);
			}

			return 0;
		}
		case WM_KEYDOWN:
		{
			RECT r;
			GetClientRect(hwnd, &r);
			mp_settings *mps = (mp_settings*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			printf("Wparam: %d (%c)\n", wParam, wParam);
			int data_length = mps->nCols * mps->nRows * mps->mode;
			switch (wParam) {
				case VK_DOWN:
					if (mps->sel + mps->nCols * mps->mode < 0x10000)
						mps->sel += mps->nCols * mps->mode;
					break;
				case VK_UP:
					if (mps->sel - mps->nCols * mps->mode >= 0)
						mps->sel -= mps->nCols * mps->mode;
					break;
				case VK_RIGHT:

					if ((mps->sel - mps->addr)/mps->mode % mps->nCols == mps->nCols-1) {
						mps->sel -= (mps->nCols - 1) * mps->mode;
						break;
					}
				case VK_TAB:
					if (mps->sel <= 0x10000 - mps->mode)
						mps->sel+=mps->mode;
					break;
				case VK_LEFT:
					if ((mps->sel - mps->addr)/mps->mode % mps->nCols == 0)
						mps->sel += (mps->nCols - 1) * mps->mode;
					else if (mps->sel >= mps->mode)
						mps->sel-=mps->mode;
					break;
				case VK_NEXT:
					if (mps->addr + data_length <= 0xFFFF)
						mps->addr += mps->nCols * mps->mode * 4;
					break;
				case VK_PRIOR:
					if (mps->addr - mps->nCols * mps->mode * 4 >= 0)
						mps->addr -= mps->nCols * mps->mode * 4;
					break;
				case VK_RETURN:
					SendMessage(hwnd, WM_LBUTTONDBLCLK, 0, MAKELPARAM(mps->xSel, mps->ySel));
					break;
				case VK_F3:
					SendMessage(hwnd, WM_COMMAND, DB_MEMPOINT_WRITE, 0);
					break;
				case 'G':
					SendMessage(hwnd, WM_COMMAND, DB_GOTO, 0);
					break;
				case 'F':
					SendMessage(hwnd, WM_COMMAND, DB_OPEN_FIND, 0);
					break;
				default:
					return 0;
			}
			SendMessage(hwnd, WM_USER, DB_UPDATE, 1);
			return 0;
		}
		case WM_COMMAND:
		{
			mp_settings *mps = (mp_settings*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			switch (HIWORD(wParam)) {
				case EN_KILLFOCUS:
					if (GetFocus() == hwnd) break;
				case EN_SUBMIT:
				{
					_TUCHAR data[8];
					ValueSubmit(hwndVal, (TCHAR *) data, mps->mode + (2 * mps->bText));
					int i;
					for (i = 0; i < mps->mode; i++) {
						mem_write(&lpDebuggerCalc->mem_c, mps->sel + i, data[i]);
					}
					SendMessage(GetParent(hwnd), WM_USER, DB_UPDATE, 0);
					hwndVal = NULL;
					if (HIWORD(wParam) != EN_KILLFOCUS) {
						SendMessage(hwnd, WM_KEYDOWN, VK_TAB, 0);
						SetFocus(hwnd);
					}
					break;
				}
				case CBN_SELCHANGE:
				{
					int mode = ComboBox_GetCurSel((HWND) lParam) + 1;
					if (mode == 3) {
						mps->bText = TRUE;
						mode = 1;
					} else {
						mps->bText = FALSE;
					}
					mps->mode = mode;
					SendMessage(hwnd, WM_USER, DB_UPDATE, 0);
					SetFocus(hwnd);
					SendMessage(hwnd, WM_SIZE, 0, 0);
					break;
				}
			}
			HWND hwndDialog;
			switch(LOWORD(wParam))
			{
				case DB_OPEN_FIND:
					hwndDialog = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_DLGFIND), hwnd, (DLGPROC) FindDialogProc);
					ShowWindow(hwndDialog, SW_SHOW);
					break;
				case DB_FIND_NEXT: {
					int addr = mps->addr;
					while (addr < 0xFFFF && mem_read(&lpDebuggerCalc->mem_c, addr) != find_value)
						addr++;
					if (addr > 0xFFFF) {
						MessageBox(NULL, _T("Value not found"), _T("Find results"), MB_OK);
						break;
					}
					mps->addr = addr;
					SetFocus(hwnd);
					SendMessage(hwnd, WM_USER, DB_UPDATE, 0);
					break;
				}
				case DB_GOTO: {
					int result;
					result = (int) DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DLGGOTO), hwnd, (DLGPROC) GotoDialogProc);
					if (result == IDOK) mps->addr = goto_addr;
					SetFocus(hwnd);
					SendMessage(hwnd, WM_USER, DB_UPDATE, 0);
					break;
				}
				case DB_MEMPOINT_WRITE: {
					bank_t *bank = &lpDebuggerCalc->mem_c.banks[mc_bank(mps->sel)];
					if (check_mem_write_break(&lpDebuggerCalc->mem_c, mps->sel))
						clear_mem_write_break(&lpDebuggerCalc->mem_c, bank->ram, bank->page, mps->sel);
					else
						set_mem_write_break(&lpDebuggerCalc->mem_c, bank->ram, bank->page, mps->sel);

					SendMessage(GetParent(hwnd), WM_USER, DB_UPDATE, 0);
					break;
				}
				case DB_MEMPOINT_READ: {
					bank_t *bank = &lpDebuggerCalc->mem_c.banks[mc_bank(mps->sel)];
					if (check_mem_read_break(&lpDebuggerCalc->mem_c, mps->sel))
						clear_mem_read_break(&lpDebuggerCalc->mem_c, bank->ram, bank->page, mps->sel);
					else
						set_mem_read_break(&lpDebuggerCalc->mem_c, bank->ram, bank->page, mps->sel);

					SendMessage(GetParent(hwnd), WM_USER, DB_UPDATE, 0);
					break;
				}
			}
			return 0;
		}
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONDOWN:
		{
			SetFocus(hwnd);

			mp_settings *mps = (mp_settings*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			RECT rc;
			GetClientRect(hwnd, &rc);

			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);

			HWND oldVal = FindWindowEx(hwnd, NULL, _T("EDIT"), NULL);
			if (oldVal) {
				SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(0, EN_SUBMIT), (LPARAM) oldVal);
			}

			POINT pt = {x, y};
			RECT r;
			int addr = AddrFromPoint(hwnd, pt, &r);

			if (addr != -1) {
				mps->sel = addr;
				if (Message == WM_LBUTTONDBLCLK) {
					TCHAR szInitial[8];

					int value = 0;
					int shift, b;
					for (b = 0, shift = 0; b < mps->mode; b++, shift += 8) {
						value += mem_read(lpDebuggerCalc->cpu.mem_c, mps->sel+b) << shift;
					}

					TCHAR szFmt[8];
					if (mps->bText)
						StringCbCopy(szFmt, sizeof(szFmt), _T("%c"));
					else
						StringCbPrintf(szFmt, sizeof(szFmt), _T("%%0%dx"), mps->mode*2);
					StringCbPrintf(szInitial, sizeof(szFmt), szFmt, value);

					hwndVal =
					CreateWindow(_T("EDIT"), szInitial,
						WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_MULTILINE,
						r.left-2,
						r.top,
						r.right-r.left+4,
						r.bottom - r.top +4,
						hwnd, 0, g_hInst, NULL);

					VALUE_FORMAT format = GetValueFormat(mps);
					SubclassEdit(hwndVal, (mps->mode == 3) ? 1 : mps->mode * 2, format);
				}
			}
			SendMessage(hwnd, WM_USER, DB_UPDATE, 1);
			return 0;
		}
		/*
		case WM_MOUSEMOVE:
		{
			mp_settings *mps = (mp_settings*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			static int addrTrackPrev = -1;

			POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
			int thisAddr = AddrFromPoint(hwnd, pt, NULL);
			if (thisAddr != addrTrackPrev) {
				mps->addrTrack = thisAddr;
				// Force the tooltip to redisplay
#define TTM_POPUP WM_USER+34
				if (IsWindowVisible(mps->hwndTip))
					SendMessage(mps->hwndTip, TTM_POPUP, 0, 0);
			}
			addrTrackPrev = thisAddr;
			return 0;
		}*/
		case WM_USER:
			switch (wParam) {
				case DB_UPDATE:
				{
					mp_settings *mps = (mp_settings*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
					if (mps->track != -1 && lParam == 0) {
						mps->addr = ((unsigned short*) &lpDebuggerCalc->cpu)[mps->track/2];
					}
					InvalidateRect(hwnd, NULL, FALSE);
					UpdateWindow(hwnd);
					break;
				}
			}
			return 0;
		case WM_DESTROY: {
			mp_settings *mps = (mp_settings *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			if (mps->memNum != -1) {
				TCHAR buffer[64];
				StringCbPrintf(buffer, sizeof(buffer), _T("Mem%i"), mps->memNum);
				DWORD value = mps->addr;
				SaveDebugKey(buffer, (DWORD *) value);
			}
			return 0;
		}
		default:
			return DefWindowProc(hwnd, Message, wParam, lParam);
	}
}