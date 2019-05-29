// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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

		NThread::CEventAutoResetReportable mp_Event;
		fp64 mp_Timeout;

		NAtomic::TCAtomicAggregate<aint> *mp_pPause;

		NThread::CMutual mp_Lock;

		NTime::CClock mp_Clock;

		fp64 mp_LastPulse;
		fp64 mp_LastCheck;
		fp64 mp_SavedSpanCheck;
	};
}
