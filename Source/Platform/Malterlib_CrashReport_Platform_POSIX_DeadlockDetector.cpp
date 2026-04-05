// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/CrashReport/DeadlockDetector>

namespace NMib::NCrashReport::NPlatform
{
	class CPosixDeadlockDetector : public CDeadlockDetector
	{
	public:

		CPosixDeadlockDetector()
		{
			mp_pPause = &mp_Pause;
			mp_pPause->f_Store(0);
		}

		~CPosixDeadlockDetector()
		{
		}

	protected:

		NAtomic::TCAtomic<aint> mp_Pause;

		inline_never bool fp_DisplayMessage(NStr::CStr const& _Title, NStr::CStr const& _Message);
	};

	struct CSubSystem_CrashReport_Platform_MacOS_DeadlockDetector : public CSubSystem
	{
		CPosixDeadlockDetector m_DeadlockDetector;
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

	constinit TCSubSystem<CSubSystem_CrashReport_Platform_MacOS_DeadlockDetector, ESubSystemDestruction_BeforeNonTrackedMemoryManager>
		g_SubSystem_CrashReport_Platform_MacOS_DeadlockDetector = {DAggregateInit}
	;

	inline_never bool CPosixDeadlockDetector::fp_DisplayMessage(NStr::CStr const& _Title, NStr::CStr const& _Message)
	{
		auto &SubSystem = *g_SubSystem_CrashReport_Platform_MacOS_DeadlockDetector;
		if (SubSystem.m_pDeadlockNotifyFunction)
			return !SubSystem.m_pDeadlockNotifyFunction();

		return false;
	}
}

bool NMib::NSys::fg_Debug_IsDeadlocked()
{
	return NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_MacOS_DeadlockDetector->f_Debug_IsDeadlocked();
}

void NMib::NSys::fg_Debug_SetDeadlockNotifyFunction(FDeadlockUserNotify *_pCrashDumpUserNotify)
{
	NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_MacOS_DeadlockDetector->f_Debug_SetDeadlockUserNotifyFunction(_pCrashDumpUserNotify);
}

void NMib::NSys::fg_Debug_PauseDeadlockDetector()
{
	NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_MacOS_DeadlockDetector->f_Debug_PauseDeadlockDetector();
}

void NMib::NSys::fg_Debug_ResumeDeadlockDetector()
{
	NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_MacOS_DeadlockDetector->f_Debug_ResumeDeadlockDetector();
}

void NMib::NSys::fg_Debug_StartDeadlockDetector(fp64 _Timeout)
{
	NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_MacOS_DeadlockDetector->f_Debug_StartDeadlockDetector(_Timeout);
}

void NMib::NSys::fg_Debug_NotDeadlocked()
{
	NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_MacOS_DeadlockDetector->f_Debug_NotDeadlocked();
}

void NMib::NSys::fg_Debug_StopDeadlockDetector()
{
	NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_MacOS_DeadlockDetector->f_Debug_StopDeadlockDetector();
}
