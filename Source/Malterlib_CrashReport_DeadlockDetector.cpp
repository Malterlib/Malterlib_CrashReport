// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include "Malterlib_CrashReport_DeadlockDetector.h"

namespace NMib::NCrashReport
{
	CDeadlockDetector::CDeadlockDetector()
	{
		mp_Stopwatch.f_Start();
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
		mp_LastPulse = mp_Stopwatch.f_GetTime();
		mp_SavedSpanCheck = 0;
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

	bool CDeadlockDetector::f_IsDeadlocked()
	{
		fp64 LastPulse;

		{
			DMibLock(mp_Lock);
			LastPulse = mp_LastPulse;
		}

		fp64 Now = mp_Stopwatch.f_GetTime();
		fp64 Timeout = mp_Timeout * 0.5;

		fp64 SpanPulse = Now - LastPulse;
		fp64 SpanAdjusted = SpanPulse;

		if (SpanAdjusted < Timeout)
			return false;

		return true;
	}

	aint CDeadlockDetector::f_Main()
	{
		mp_LastCheck = mp_LastPulse = mp_Stopwatch.f_GetTime();
		mp_SavedSpanCheck = 0;

		while (f_GetState() != NThread::EThreadState_EventWantQuit)
		{
			fp64 Now = mp_Stopwatch.f_GetTime();

			fp64 LastPulse;
			fp64 LastSpanCheck;

			{
				DMibLock(mp_Lock);
				LastPulse = mp_LastPulse;
				LastSpanCheck = mp_SavedSpanCheck;
			}

			fp64 SpanCheck = Now - mp_LastCheck;
			fp64 SpanPulse = Now - LastPulse;


			if (SpanCheck > mp_Timeout * 0.5 * NTime::CSystem_Time::fs_GetTimeSpeed())
			{
				DMibLock(mp_Lock);
				mp_SavedSpanCheck += SpanCheck;
			}

			SpanCheck += LastSpanCheck;

			mp_LastCheck = Now;

			fp64 SpanAdjusted = SpanPulse - SpanCheck;

			if (SpanAdjusted > mp_Timeout * NTime::CSystem_Time::fs_GetTimeSpeed())
			{
				if (!*mp_pPause)
				{
					NStr::CStr ProgramName = fg_GetSys()->f_GetProgramName();

					NStr::CStr Title = NStr::CStr::CFormat("{} is not responding") << ProgramName;
					NStr::CStr Message = NStr::CStr::CFormat("{} is not responding. This might be temporary.\n\nWould you like to continue waiting for {0} to respond?\n\nTo report this problem, please press 'Create error report' to create an error report.") << ProgramName;


#if defined(DMibDebug) && 0
					DMibDTrace("Deadlock detector would have activated" DMibNewLine, 0);
#else
					bool bDaemon = fg_GetSys()->f_GetRunningAsDaemon();

					if (bDaemon || !fp_DisplayMessage(Title, Message))
					{
						DMibPDebugBreak;
					}
#endif
				}
				{
					DMibLock(mp_Lock);
					mp_LastPulse = mp_Stopwatch.f_GetTime();
					mp_SavedSpanCheck = 0.0;
				}
			}

			m_EventWantQuit.f_WaitTimeout(2.0);
		}

		return 0;
	}
}
