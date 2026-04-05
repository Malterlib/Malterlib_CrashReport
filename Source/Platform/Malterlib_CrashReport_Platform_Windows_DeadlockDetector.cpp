// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/CrashReport/DeadlockDetector>
#include <Mib/Core/PlatformSpecific/WindowsError>
#include <Windows.h>

namespace NMib::NCrashReport::NPlatform
{
	class CWindowsDeadlockDetector : public CDeadlockDetector
	{
	public:

		CWindowsDeadlockDetector();
		~CWindowsDeadlockDetector();

	protected:

		HFONT fp_CreatePointFontIndirect(const LOGFONT* lpLogFont);
		HFONT fp_CreatePointFont(int nPointSize, LPCTSTR lpszFaceName);

		static INT_PTR CALLBACK fsp_HandleMessages(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
		LRESULT fp_DisplayMyMessage(HINSTANCE hinst, HWND hwndOwner, LPCSTR lpszMessage, LPCSTR lpszTitle);
		inline_never bool fp_DisplayMessage(NStr::CStr const& _Title, NStr::CStr const& _Message);

		enum
		{
			ID_HELP = 150,
			ID_TEXT = 200,
			IDB_Yes,
			IDB_No
		};

		HWND mp_CurrentDialogWindow;
		HFONT mp_NormalFont;
	};

	struct CSubSystem_CrashReport_Platform_Windows_DeadlockDetector : public CSubSystem
	{
		CWindowsDeadlockDetector m_DeadlockDetector;
		NSys::FDeadlockUserNotify *m_pDeadlockNotifyFunction = nullptr;

		void f_Debug_SetDeadlockUserNotifyFunction(NSys::FDeadlockUserNotify *_pDeadlockNotifyFunction)
		{
			m_pDeadlockNotifyFunction = _pDeadlockNotifyFunction;
		}

		void f_Debug_StartDeadlockDetector(fp64 _Timeout)
		{
			m_DeadlockDetector.f_SetTimeout(_Timeout);
			if (m_DeadlockDetector.f_GetState() != NThread::EThreadState_Running)
				m_DeadlockDetector.f_Start();
		}

		void f_Debug_NotDeadlocked()
		{
			m_DeadlockDetector.f_Pulse();
		}

		void f_Debug_StopDeadlockDetector()
		{
			m_DeadlockDetector.f_Stop(true);
		}

		void f_Debug_PauseDeadlockDetector()
		{
			m_DeadlockDetector.f_AddPauseValue();
		}

		void f_Debug_ResumeDeadlockDetector()
		{
			aint LastValue = m_DeadlockDetector.f_LastPauseValue();
			if (LastValue == 1)
				m_DeadlockDetector.f_Pulse();

			DMibSafeCheck(LastValue > 0, "Pause error");
		}

		bool f_Debug_IsDeadlocked()
		{
			return m_DeadlockDetector.f_IsDeadlocked();
		}
	};

	constinit TCSubSystem<CSubSystem_CrashReport_Platform_Windows_DeadlockDetector, ESubSystemDestruction_BeforeNonTrackedMemoryManager>
		g_SubSystem_CrashReport_Platform_Windows_DeadlockDetector = {DAggregateInit}
	;

	CWindowsDeadlockDetector::CWindowsDeadlockDetector()
	{
		// This needs to be named excatly like this to be compatibly with old versions of the library (when Malterlib was named Ids)
		if (!FindAtom(str_utf16("IdsDeadLockAtom")))
		{
			AddAtom(str_utf16("IdsDeadLockAtom"));
			mp_pPause = (NAtomic::TCAtomic<aint> *)HeapAlloc(GetProcessHeap(), 0, sizeof(NAtomic::TCAtomic<aint>));
			mp_pPause->f_Store(0);
			umint PausePointer = (umint)mp_pPause;
			for (umint i = 0; i < sizeof(umint) * 8; ++i)
			{
				if (PausePointer & (umint(1) << i))
				{
					AddAtom(NStr::CFWStr128(NStr::CFWStr128::CFormat(str_utf16("IdsDeadLockAtom{}")) << i));
				}
			}
		}
		else
		{
			umint PausePointer = 0;
			for (umint i = 0; i < sizeof(umint) * 8; ++i)
			{
				if (FindAtom(NStr::CFWStr128(NStr::CFWStr128::CFormat(str_utf16("IdsDeadLockAtom{}")) << i)))
				{
					PausePointer |= (umint(1) << i);

				}
			}
			mp_pPause = (NAtomic::TCAtomic<aint> *)PausePointer;
		}

		mp_CurrentDialogWindow = nullptr;
	}

	CWindowsDeadlockDetector::~CWindowsDeadlockDetector()
	{
	}

	HFONT CWindowsDeadlockDetector::fp_CreatePointFontIndirect(const LOGFONT* lpLogFont)
	{
		HDC hDC;
		hDC = ::GetDC(nullptr);

		// convert nPointSize to logical units based on pDC
		LOGFONT logFont = *lpLogFont;
		POINT pt;
		pt.y = ::GetDeviceCaps(hDC, LOGPIXELSY) * logFont.lfHeight;
		pt.y /= 720;    // 72 points/inch, 10 decipoints/point
		pt.x = 0;
		::DPtoLP(hDC, &pt, 1);
		POINT ptOrg = { 0, 0 };
		::DPtoLP(hDC, &ptOrg, 1);
		logFont.lfHeight = -fg_Abs(pt.y - ptOrg.y);

		ReleaseDC(nullptr, hDC);

		return CreateFontIndirect(&logFont);
	}

	HFONT CWindowsDeadlockDetector::fp_CreatePointFont(int nPointSize, LPCTSTR lpszFaceName)
	{

		LOGFONT logFont;
		memset(&logFont, 0, sizeof(LOGFONT));
		logFont.lfCharSet = DEFAULT_CHARSET;
		logFont.lfHeight = nPointSize;
		NStr::fg_StrCopy(logFont.lfFaceName, lpszFaceName, sizeof(logFont.lfFaceName) / sizeof(logFont.lfFaceName[0]));

		return fp_CreatePointFontIndirect(&logFont);
	}

	INT_PTR CALLBACK CWindowsDeadlockDetector::fsp_HandleMessages(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto &SubSystem = *g_SubSystem_CrashReport_Platform_Windows_DeadlockDetector;
		switch(uMsg)
		{
		case WM_SYSCOMMAND:
			{
				switch (wParam)
				{
				case SC_CLOSE:

					EndDialog(hwndDlg, 1);
					return 1;
				};
			}
			break;
		case WM_TIMER:
			{
				bool bDeadlocked = SubSystem.m_DeadlockDetector.f_IsDeadlocked();
				if (!bDeadlocked)
				{
					EndDialog(hwndDlg, 1);
				}
				return 1;
			}
			break;
		case WM_INITDIALOG:
			{

				POINT Pnt;
				GetCursorPos(&Pnt);
				HMONITOR hMon = MonitorFromPoint(Pnt, MONITOR_DEFAULTTONEAREST);
				MONITORINFO MonInfo;
				NMemory::fg_MemClear(MonInfo);
				MonInfo.cbSize = sizeof(MonInfo);
				GetMonitorInfo(hMon, &MonInfo);

				RECT Rect;
				GetWindowRect(hwndDlg, &Rect);

				int Width = Rect.right - Rect.left;
				int Height = Rect.bottom - Rect.top;
				int MonWidth = MonInfo.rcWork.right - MonInfo.rcWork.left;
				int MonHeight = MonInfo.rcWork.bottom - MonInfo.rcWork.top;

				SendMessage(hwndDlg, WM_SETFONT, (WPARAM)SubSystem.m_DeadlockDetector.mp_NormalFont, 0);
				HWND hTmp = GetDlgItem(hwndDlg, IDB_Yes);
				SendMessage(hTmp, WM_SETFONT, (WPARAM)SubSystem.m_DeadlockDetector.mp_NormalFont, 0);
				hTmp = GetDlgItem(hwndDlg, IDB_No);
				SendMessage(hTmp, WM_SETFONT, (WPARAM)SubSystem.m_DeadlockDetector.mp_NormalFont, 0);
				hTmp = GetDlgItem(hwndDlg, ID_TEXT);
				SendMessage(hTmp, WM_SETFONT, (WPARAM)SubSystem.m_DeadlockDetector.mp_NormalFont, 0);

//					SendDlgItemMessage(hwndDlg, IDB_Yes, BM_SETSTYLE, BS_PUSHBUTTON, (LONG)TRUE);
//					SendDlgItemMessage(hwndDlg, IDB_No, BM_SETSTYLE, BS_DEFPUSHBUTTON, (LONG)TRUE);
				SendMessage(hwndDlg, DM_SETDEFID, IDB_Yes,0L);

				SetWindowPos(hwndDlg, nullptr, MonInfo.rcWork.left + MonWidth / 2 - Width / 2, MonInfo.rcWork.top + MonHeight / 2 - Height / 2, 0, 0, SWP_NOSIZE|SWP_NOZORDER);

				SetTimer(hwndDlg, 0, 100, nullptr);
				BringWindowToTop(hwndDlg);
				SubSystem.m_DeadlockDetector.mp_CurrentDialogWindow = hwndDlg;
				return 1;
			}
			break;
		case WM_COMMAND:
			{
				int Control = LOWORD(wParam);
				int Message = HIWORD(wParam);
//					HWND Window = (HWND)lParam;
				if (Message == BN_CLICKED)
				{
					if (Control == IDB_Yes)
					{
						EndDialog(hwndDlg, 1);
						return 1;
					}
					else if (Control == IDB_No)
					{
						EndDialog(hwndDlg, 335);
						return 1;
					}
				}

			}
			break;

		}
		return 0;
	}


	LRESULT CWindowsDeadlockDetector::fp_DisplayMyMessage(HINSTANCE hinst, HWND hwndOwner, LPCSTR lpszMessage, LPCSTR lpszTitle)
	{
		HGLOBAL hgbl;
		LPDLGTEMPLATE lpdt;
		LPDLGITEMTEMPLATE lpdit;
		LPWORD lpw;
		LPWSTR lpwsz;
		LRESULT ret;
		int nchar;

		hgbl = GlobalAlloc(GMEM_ZEROINIT, 2048);
		if (!hgbl)
			return -1;

		lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);

		// Define a dialog box.

		lpdt->style = WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION;
		lpdt->cdit = 3;  // number of controls
		lpdt->x  = 10;  lpdt->y  = 10;
		lpdt->cx = 200; lpdt->cy = 75;

		lpw = (LPWORD) (lpdt + 1);
		*lpw++ = 0;   // no menu
		*lpw++ = 0;   // predefined dialog box class (by default)

		lpwsz = (LPWSTR) lpw;
		nchar = MultiByteToWideChar (CP_ACP, 0, lpszTitle,
										-1, lpwsz, 50);
		lpw += nchar;

		int32 ButtonWidth = 60;
		int32 ButtonGap = 5;
		int32 ButtonStart = (lpdt->cx - (ButtonWidth*2 + ButtonGap)) / 2;
		//-----------------------
		// Define an Yes button.
		//-----------------------
		lpw = fg_AlignUp (lpw, 4); // align DLGITEMTEMPLATE on DWORD boundary
		lpdit = (LPDLGITEMTEMPLATE) lpw;
		lpdit->x  = ButtonStart; lpdit->y  = 55;
		lpdit->cx = ButtonWidth; lpdit->cy = 12;
		lpdit->id = IDB_Yes;  // OK button identifier
		lpdit->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;

		lpw = (LPWORD) (lpdit + 1);
		*lpw++ = 0xFFFF;
		*lpw++ = 0x0080;    // button class

		lpwsz = (LPWSTR) lpw;
		nchar = MultiByteToWideChar (CP_ACP, 0, "Continue waiting", -1, lpwsz, 50);
		lpw   += nchar;
		*lpw++ = 0x0000; // Creation Data

		//-----------------------
		// Define an No button.
		//-----------------------
		lpw = fg_AlignUp (lpw, 4); // align DLGITEMTEMPLATE on DWORD boundary
		lpdit = (LPDLGITEMTEMPLATE) lpw;
		lpdit->x  = ButtonStart + ButtonWidth + ButtonGap; lpdit->y  = 55;
		lpdit->cx = ButtonWidth; lpdit->cy = 12;
		lpdit->id = IDB_No;  // OK button identifier
		lpdit->style = WS_CHILD | WS_VISIBLE;

		lpw = (LPWORD) (lpdit + 1);
		*lpw++ = 0xFFFF;
		*lpw++ = 0x0080;    // button class

		lpwsz = (LPWSTR) lpw;
		nchar = MultiByteToWideChar (CP_ACP, 0, "Create error report", -1, lpwsz, 50);
		lpw   += nchar;
		*lpw++ = 0x0000; // Creation Data

		//-----------------------
		// Define a static text control.
		//-----------------------
		lpw = fg_AlignUp (lpw, 4); // align DLGITEMTEMPLATE on DWORD boundary
		lpdit = (LPDLGITEMTEMPLATE) lpw;
		lpdit->x  = 7; lpdit->y  = 7;
		lpdit->cx = 180; lpdit->cy = 45;
		lpdit->id = ID_TEXT;  // text identifier
		lpdit->style = WS_CHILD | WS_VISIBLE | SS_LEFT;

		lpw = (LPWORD) (lpdit + 1);
		*lpw++ = 0xFFFF;
		*lpw++ = 0x0082;                         // static class

		lpw = (LPWORD)NStr::fg_StrCopy((ch16 *)lpw, lpszMessage);
		*lpw++ = 0x0000; // Creation Data

		GlobalUnlock(hgbl);

		mp_NormalFont = fp_CreatePointFont(80, str_utf16("MS Sans Serif"));
		ret = DialogBoxIndirect(hinst, (LPDLGTEMPLATE) hgbl, hwndOwner, (DLGPROC) fsp_HandleMessages);
		mp_CurrentDialogWindow = nullptr;
		DeleteObject(mp_NormalFont);

		GlobalFree(hgbl);

		return ret;
	}

	inline_never bool CWindowsDeadlockDetector::fp_DisplayMessage(NStr::CStr const& _Title, NStr::CStr const& _Message)
	{
		auto &SubSystem = *g_SubSystem_CrashReport_Platform_Windows_DeadlockDetector;
		if (SubSystem.m_pDeadlockNotifyFunction)
			return !SubSystem.m_pDeadlockNotifyFunction();
		int Return = fp_DisplayMyMessage((HINSTANCE)nullptr, nullptr, _Message, _Title);

		if (Return == -1)
		{
			DMibDTrace("{}\n", NMib::NPlatform::fg_Win32_GetLastErrorStr());
		}

		if (Return == 335)
			return 0;
		else
			return 1;
	}
}

bool NMib::NSys::fg_Debug_IsDeadlocked()
{
	return NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_Windows_DeadlockDetector->f_Debug_IsDeadlocked();
}

void NMib::NSys::fg_Debug_SetDeadlockNotifyFunction(FDeadlockUserNotify *_pCrashDumpUserNotify)
{
	NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_Windows_DeadlockDetector->f_Debug_SetDeadlockUserNotifyFunction(_pCrashDumpUserNotify);
}

void NMib::NSys::fg_Debug_PauseDeadlockDetector()
{
	NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_Windows_DeadlockDetector->f_Debug_PauseDeadlockDetector();
}

void NMib::NSys::fg_Debug_ResumeDeadlockDetector()
{
	NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_Windows_DeadlockDetector->f_Debug_ResumeDeadlockDetector();
}

void NMib::NSys::fg_Debug_StartDeadlockDetector(fp64 _Timeout)
{
	NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_Windows_DeadlockDetector->f_Debug_StartDeadlockDetector(_Timeout);
}

void NMib::NSys::fg_Debug_NotDeadlocked()
{
	NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_Windows_DeadlockDetector->f_Debug_NotDeadlocked();
}

void NMib::NSys::fg_Debug_StopDeadlockDetector()
{
	NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_Windows_DeadlockDetector->f_Debug_StopDeadlockDetector();
}
