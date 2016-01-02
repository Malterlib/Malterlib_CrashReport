// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib
{
	namespace NSystem
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

			bint f_IsDeadlocked();

		protected:

			virtual bint fp_DisplayMessage(NStr::CStr const& _Title, NStr::CStr const& _Message) = 0;

			NThread::CEventAutoResetReportable mp_Event;
			fp64 mp_Timeout;

			NAtomic::TCAtomicAggregate<aint> *mp_pPause;

			NThread::CMutual mp_Lock;

			NTime::CTime mp_LastPulse;
			NTime::CTime mp_LastCheck;
			NTime::CTimeSpan mp_SavedSpanCheck;

		};
	}
}

