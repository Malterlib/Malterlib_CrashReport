// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/CrashReport/DeadlockDetector>
#include <Mib/Core/PlatformSpecific/WindowsError>
#include <Windows.h>

namespace NMib::NCrashReport::NPlatform
{
	struct CWindowsDeadlockDetector : public CDeadlockDetector
	{
		CWindowsDeadlockDetector();
		~CWindowsDeadlockDetector();

	protected:
		HFONT fp_CreatePointFontIndirect(LOGFONT const *_pLogFont);
		HFONT fp_CreatePointFont(int _PointSize, LPCTSTR _pFaceName);

		static INT_PTR CALLBACK fsp_HandleMessages(HWND _Dialog, UINT _Message, WPARAM _WParam, LPARAM _LParam);
		LRESULT fp_DisplayMyMessage(HINSTANCE _Instance, HWND _Owner, LPCSTR _pMessage, LPCSTR _pTitle);
		inline_never bool fp_DisplayMessage(NStr::CStr const &_Title, NStr::CStr const &_Message);

		enum EDialogID
		{
			EDialogID_Help   = 150
			, EDialogID_Text = 200
			, EDialogID_Yes
			, EDialogID_No
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
			for (umint iBit = 0; iBit < sizeof(umint) * 8; ++iBit)
			{
				if (PausePointer & (umint(1) << iBit))
					AddAtom(NStr::CFWStr128(NStr::CFWStr128::CFormat(str_utf16("IdsDeadLockAtom{}")) << iBit));
			}
		}
		else
		{
			umint PausePointer = 0;
			for (umint iBit = 0; iBit < sizeof(umint) * 8; ++iBit)
			{
				if (FindAtom(NStr::CFWStr128(NStr::CFWStr128::CFormat(str_utf16("IdsDeadLockAtom{}")) << iBit)))
					PausePointer |= (umint(1) << iBit);
			}
			mp_pPause = (NAtomic::TCAtomic<aint> *)PausePointer;
		}

		mp_CurrentDialogWindow = nullptr;
	}

	CWindowsDeadlockDetector::~CWindowsDeadlockDetector()
	{
	}

	HFONT CWindowsDeadlockDetector::fp_CreatePointFontIndirect(LOGFONT const *_pLogFont)
	{
		HDC DeviceContext;
		DeviceContext = ::GetDC(nullptr);

		// convert nPointSize to logical units based on pDC
		LOGFONT LogFont = *_pLogFont;
		POINT Point;
		Point.y = ::GetDeviceCaps(DeviceContext, LOGPIXELSY) * LogFont.lfHeight;
		Point.y /= 720; // 72 points/inch, 10 decipoints/point
		Point.x = 0;
		::DPtoLP(DeviceContext, &Point, 1);
		POINT Origin = {0, 0};
		::DPtoLP(DeviceContext, &Origin, 1);
		LogFont.lfHeight = -fg_Abs(Point.y - Origin.y);

		ReleaseDC(nullptr, DeviceContext);

		return CreateFontIndirect(&LogFont);
	}

	HFONT CWindowsDeadlockDetector::fp_CreatePointFont(int _PointSize, LPCTSTR _pFaceName)
	{
		LOGFONT LogFont;
		memset(&LogFont, 0, sizeof(LOGFONT));
		LogFont.lfCharSet = DEFAULT_CHARSET;
		LogFont.lfHeight = _PointSize;
		NStr::fg_StrCopy(LogFont.lfFaceName, _pFaceName, sizeof(LogFont.lfFaceName) / sizeof(LogFont.lfFaceName[0]));

		return fp_CreatePointFontIndirect(&LogFont);
	}

	INT_PTR CALLBACK CWindowsDeadlockDetector::fsp_HandleMessages(HWND _Dialog, UINT _Message, WPARAM _WParam, LPARAM _LParam)
	{
		auto &SubSystem = *g_SubSystem_CrashReport_Platform_Windows_DeadlockDetector;
		switch (_Message)
		{
		case WM_SYSCOMMAND:
			{
				switch (_WParam)
				{
				case SC_CLOSE:
					EndDialog(_Dialog, 1);
					return 1;
				};
			}
			break;
		case WM_TIMER:
			{
				bool bDeadlocked = SubSystem.m_DeadlockDetector.f_IsDeadlocked();
				if (!bDeadlocked)
					EndDialog(_Dialog, 1);
				return 1;
			}
			break;
		case WM_INITDIALOG:
			{
				POINT Pnt;
				GetCursorPos(&Pnt);
				HMONITOR Monitor = MonitorFromPoint(Pnt, MONITOR_DEFAULTTONEAREST);
				MONITORINFO MonInfo;
				NMemory::fg_MemClear(MonInfo);
				MonInfo.cbSize = sizeof(MonInfo);
				GetMonitorInfo(Monitor, &MonInfo);

				RECT Rect;
				GetWindowRect(_Dialog, &Rect);

				int Width = Rect.right - Rect.left;
				int Height = Rect.bottom - Rect.top;
				int MonWidth = MonInfo.rcWork.right - MonInfo.rcWork.left;
				int MonHeight = MonInfo.rcWork.bottom - MonInfo.rcWork.top;

				SendMessage(_Dialog, WM_SETFONT, (WPARAM)SubSystem.m_DeadlockDetector.mp_NormalFont, 0);
				HWND DialogItem = GetDlgItem(_Dialog, EDialogID_Yes);
				SendMessage(DialogItem, WM_SETFONT, (WPARAM)SubSystem.m_DeadlockDetector.mp_NormalFont, 0);
				DialogItem = GetDlgItem(_Dialog, EDialogID_No);
				SendMessage(DialogItem, WM_SETFONT, (WPARAM)SubSystem.m_DeadlockDetector.mp_NormalFont, 0);
				DialogItem = GetDlgItem(_Dialog, EDialogID_Text);
				SendMessage(DialogItem, WM_SETFONT, (WPARAM)SubSystem.m_DeadlockDetector.mp_NormalFont, 0);

				// SendDlgItemMessage(_Dialog, EDialogID_Yes, BM_SETSTYLE, BS_PUSHBUTTON, (LONG)TRUE);
				// SendDlgItemMessage(_Dialog, EDialogID_No, BM_SETSTYLE, BS_DEFPUSHBUTTON, (LONG)TRUE);
				SendMessage(_Dialog, DM_SETDEFID, EDialogID_Yes, 0L);

				SetWindowPos(_Dialog, nullptr, MonInfo.rcWork.left + MonWidth / 2 - Width / 2, MonInfo.rcWork.top + MonHeight / 2 - Height / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

				SetTimer(_Dialog, 0, 100, nullptr);
				BringWindowToTop(_Dialog);
				SubSystem.m_DeadlockDetector.mp_CurrentDialogWindow = _Dialog;
				return 1;
			}
			break;
		case WM_COMMAND:
			{
				int Control = LOWORD(_WParam);
				int Message = HIWORD(_WParam);
				// HWND Window = (HWND)_LParam;
				if (Message == BN_CLICKED)
				{
					if (Control == EDialogID_Yes)
					{
						EndDialog(_Dialog, 1);
						return 1;
					}
					else if (Control == EDialogID_No)
					{
						EndDialog(_Dialog, 335);
						return 1;
					}
				}
			}
			break;
		}
		return 0;
	}

	LRESULT CWindowsDeadlockDetector::fp_DisplayMyMessage(HINSTANCE _Instance, HWND _Owner, LPCSTR _pMessage, LPCSTR _pTitle)
	{
		HGLOBAL GlobalMemory;
		LPDLGTEMPLATE pDialogTemplate;
		LPDLGITEMTEMPLATE pItemTemplate;
		LPWORD pWord;
		LPWSTR pString;
		LRESULT Result;
		int nChars;

		GlobalMemory = GlobalAlloc(GMEM_ZEROINIT, 2048);
		if (!GlobalMemory)
			return -1;

		pDialogTemplate = (LPDLGTEMPLATE)GlobalLock(GlobalMemory);

		// Define a dialog box.

		pDialogTemplate->style = WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION;
		pDialogTemplate->cdit = 3; // number of controls
		pDialogTemplate->x = 10;
		pDialogTemplate->y = 10;
		pDialogTemplate->cx = 200;
		pDialogTemplate->cy = 75;

		pWord = (LPWORD)(pDialogTemplate + 1);
		*pWord++ = 0; // no menu
		*pWord++ = 0; // predefined dialog box class (by default)

		pString = (LPWSTR) pWord;
		nChars = MultiByteToWideChar(CP_ACP, 0, _pTitle, -1, pString, 50);
		pWord += nChars;

		int32 ButtonWidth = 60;
		int32 ButtonGap = 5;
		int32 ButtonStart = (pDialogTemplate->cx - (ButtonWidth * 2 + ButtonGap)) / 2;
		//-----------------------
		// Define an Yes button.
		//-----------------------
		pWord = fg_AlignUp(pWord, 4); // align DLGITEMTEMPLATE on DWORD boundary
		pItemTemplate = (LPDLGITEMTEMPLATE) pWord;
		pItemTemplate->x = ButtonStart;
		pItemTemplate->y = 55;
		pItemTemplate->cx = ButtonWidth;
		pItemTemplate->cy = 12;
		pItemTemplate->id = EDialogID_Yes; // OK button identifier
		pItemTemplate->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;

		pWord = (LPWORD)(pItemTemplate + 1);
		*pWord++ = 0xFFFF;
		*pWord++ = 0x0080; // button class

		pString = (LPWSTR) pWord;
		nChars = MultiByteToWideChar(CP_ACP, 0, "Continue waiting", -1, pString, 50);
		pWord += nChars;
		*pWord++ = 0x0000; // Creation Data

		//-----------------------
		// Define an No button.
		//-----------------------
		pWord = fg_AlignUp(pWord, 4); // align DLGITEMTEMPLATE on DWORD boundary
		pItemTemplate = (LPDLGITEMTEMPLATE) pWord;
		pItemTemplate->x = ButtonStart + ButtonWidth + ButtonGap;
		pItemTemplate->y = 55;
		pItemTemplate->cx = ButtonWidth;
		pItemTemplate->cy = 12;
		pItemTemplate->id = EDialogID_No; // OK button identifier
		pItemTemplate->style = WS_CHILD | WS_VISIBLE;

		pWord = (LPWORD)(pItemTemplate + 1);
		*pWord++ = 0xFFFF;
		*pWord++ = 0x0080; // button class

		pString = (LPWSTR) pWord;
		nChars = MultiByteToWideChar(CP_ACP, 0, "Create error report", -1, pString, 50);
		pWord += nChars;
		*pWord++ = 0x0000; // Creation Data

		//-----------------------
		// Define a static text control.
		//-----------------------
		pWord = fg_AlignUp(pWord, 4); // align DLGITEMTEMPLATE on DWORD boundary
		pItemTemplate = (LPDLGITEMTEMPLATE) pWord;
		pItemTemplate->x = 7;
		pItemTemplate->y = 7;
		pItemTemplate->cx = 180;
		pItemTemplate->cy = 45;
		pItemTemplate->id = EDialogID_Text; // text identifier
		pItemTemplate->style = WS_CHILD | WS_VISIBLE | SS_LEFT;

		pWord = (LPWORD)(pItemTemplate + 1);
		*pWord++ = 0xFFFF;
		*pWord++ = 0x0082; // static class

		pWord = (LPWORD)NStr::fg_StrCopy((ch16 *)pWord, _pMessage);
		*pWord++ = 0x0000; // Creation Data

		GlobalUnlock(GlobalMemory);

		mp_NormalFont = fp_CreatePointFont(80, str_utf16("MS Sans Serif"));
		Result = DialogBoxIndirect(_Instance, (LPDLGTEMPLATE) GlobalMemory, _Owner, (DLGPROC) fsp_HandleMessages);
		mp_CurrentDialogWindow = nullptr;
		DeleteObject(mp_NormalFont);

		GlobalFree(GlobalMemory);

		return Result;
	}

	inline_never bool CWindowsDeadlockDetector::fp_DisplayMessage(NStr::CStr const &_Title, NStr::CStr const &_Message)
	{
		auto &SubSystem = *g_SubSystem_CrashReport_Platform_Windows_DeadlockDetector;
		if (SubSystem.m_pDeadlockNotifyFunction)
			return !SubSystem.m_pDeadlockNotifyFunction();
		int Return = fp_DisplayMyMessage((HINSTANCE)nullptr, nullptr, _Message, _Title);

		if (Return == -1)
			DMibDTrace("{}\n", NMib::NPlatform::fg_Win32_GetLastErrorStr());

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
