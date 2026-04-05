// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

namespace NMib::NCrashReport
{
	class CDeadlockDetector : public NThread::CThread
	{
	public:

		CDeadlockDetector();
		~CDeadlockDetector();

		aint f_Main();

		virtual NStr::CStr f_GetThreadName();
		void f_Pulse();
		void f_SetTimeout(fp64 _Timeout);

		aint f_LastPauseValue();
		void f_AddPauseValue();

		bool f_IsDeadlocked();

	protected:

		virtual bool fp_DisplayMessage(NStr::CStr const& _Title, NStr::CStr const& _Message) = 0;

		fp64 mp_Timeout;

		NAtomic::TCAtomic<aint> *mp_pPause;

		NThread::CMutual mp_Lock;

		NTime::CStopwatch mp_Stopwatch;

		fp64 mp_LastPulse;
		fp64 mp_LastCheck;
		fp64 mp_SavedSpanCheck;
	};
}
