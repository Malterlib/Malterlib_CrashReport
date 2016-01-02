// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include "Malterlib_CrashReport_DeadlockDetector.h"

namespace NMib
{
	namespace NSystem
	{
		CDeadlockDetector::CDeadlockDetector()
		{

		}

		CDeadlockDetector::~CDeadlockDetector()
		{

		}

		NStr::CStr CDeadlockDetector::f_GetThreadName()
		{
			return "Malterlib_Core_PlatformImp_DeadlockDetector";
		}

		void CDeadlockDetector::f_Pulse()
		{
			DMibLock(mp_Lock);
			mp_LastPulse = NTime::CTime::fs_NowUTC();
			mp_SavedSpanCheck = NTime::CTimeSpanConvert::fs_CreateSpan(0,0);
		}

		void CDeadlockDetector::f_SetTimeout(fp64 _Timeout)
		{
			mp_Timeout = _Timeout;
		}

		aint CDeadlockDetector::f_LastPauseValue()
		{
			return mp_pPause->f_FetchSub(1);
		}

		void CDeadlockDetector::f_AddPauseValue()
		{
			mp_pPause->f_FetchAdd(1);
		}

		bint CDeadlockDetector::f_IsDeadlocked()
		{
			NTime::CTime LastPulse;

			{
				DMibLock(mp_Lock);
				LastPulse = mp_LastPulse;
			}

			NTime::CTime Now = NTime::CTime::fs_NowUTC();
			fp64 Timeout = mp_Timeout * 0.5;

			NTime::CTimeSpan SpanPulse = Now - LastPulse;
			NTime::CTimeSpan SpanAdjusted = SpanPulse;

			if (SpanAdjusted.f_GetSecondsFraction() < Timeout)
				return false;

			return true;
		}

		aint CDeadlockDetector::f_Main()
		{
			m_EventWantQuit.f_ReportTo(&mp_Event);
			mp_LastCheck = mp_LastPulse = NTime::CTime::fs_NowUTC();
			mp_SavedSpanCheck = NTime::CTimeSpanConvert::fs_CreateSpan(0,0);

			while (f_GetState() != NThread::EThreadState_EventWantQuit)
			{
				NTime::CTime Now = NTime::CTime::fs_NowUTC();

				NTime::CTime LastPulse;
				NTime::CTimeSpan LastSpanCheck;

				{
					DMibLock(mp_Lock);
					LastPulse = mp_LastPulse;
					LastSpanCheck = mp_SavedSpanCheck;
				}

				NTime::CTimeSpan SpanCheck = Now - mp_LastCheck;
				NTime::CTimeSpan SpanPulse = Now - LastPulse;

				
				if (SpanCheck.f_GetSecondsFraction() > mp_Timeout * 0.5 * NTime::CSystem_Time::fs_GetTimeSpeed())
				{
					DMibLock(mp_Lock);
					mp_SavedSpanCheck += SpanCheck;
				}

				SpanCheck += LastSpanCheck;

				mp_LastCheck = Now;

				NTime::CTimeSpan SpanAdjusted = SpanPulse - SpanCheck;

				if (SpanAdjusted.f_GetSecondsFraction() > mp_Timeout * NTime::CSystem_Time::fs_GetTimeSpeed())
				{
					if (!*mp_pPause)
					{
						NStr::CStr ProgramName = fg_GetSys()->f_GetProgramName();

						NStr::CStr Title = NStr::CStr::CFormat("{} is not responding") << ProgramName; 
						NStr::CStr Message = NStr::CStr::CFormat("{} is not responding. This might be temporary.\n\nWould you like to continue waiting for {0} to respond?\n\nTo report this problem, please press 'Create error report' to create an error report.") << ProgramName; 


#if defined(DMibDebug) && 0
						DMibDTrace("Deadlock detector would have activated" DMibNewLine, 0);
#else
						bint bService = fg_GetSys()->f_GetRunningAsService();

						if (bService || !fp_DisplayMessage(Title, Message))
						{
							DMibPDebugBreak;
						}
#endif
					}
					{
						DMibLock(mp_Lock);
						mp_LastPulse = NTime::CTime::fs_NowUTC();
						mp_SavedSpanCheck = NTime::CTimeSpanConvert::fs_CreateSpan(0,0);
					}
				}

				mp_Event.f_WaitTimeout(2.0);
			}

			return 0;
		}

	}
}