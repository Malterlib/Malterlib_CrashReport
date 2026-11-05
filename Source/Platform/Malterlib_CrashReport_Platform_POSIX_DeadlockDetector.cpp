// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/CrashReport/DeadlockDetector>

namespace NMib::NCrashReport::NPlatform
{
	struct CPosixDeadlockDetector : public CDeadlockDetector
	{
		CPosixDeadlockDetector();
		~CPosixDeadlockDetector();

	protected:
		inline_never bool fp_DisplayMessage(NStr::CStr const &_Title, NStr::CStr const &_Message);

		NAtomic::TCAtomic<aint> mp_Pause;
	};

	CPosixDeadlockDetector::CPosixDeadlockDetector()
	{
		mp_pPause = &mp_Pause;
		mp_pPause->f_Store(0);
	}

	CPosixDeadlockDetector::~CPosixDeadlockDetector()
	{
	}

	struct CSubSystem_CrashReport_Platform_MacOS_DeadlockDetector : public CSubSystem
	{
		void f_Debug_SetDeadlockUserNotifyFunction(NSys::FDeadlockUserNotify *_fDeadlockNotifyFunction);
		void f_Debug_StartDeadlockDetector(fp64 _Timeout);
		void f_Debug_NotDeadlocked();
		void f_Debug_StopDeadlockDetector();
		void f_Debug_PauseDeadlockDetector();
		void f_Debug_ResumeDeadlockDetector();
		bool f_Debug_IsDeadlocked();

		CPosixDeadlockDetector m_DeadlockDetector;
		NSys::FDeadlockUserNotify *m_fDeadlockNotifyFunction = nullptr;
	};

	void CSubSystem_CrashReport_Platform_MacOS_DeadlockDetector::f_Debug_SetDeadlockUserNotifyFunction(NSys::FDeadlockUserNotify *_fDeadlockNotifyFunction)
	{
		m_fDeadlockNotifyFunction = _fDeadlockNotifyFunction;
	}

	void CSubSystem_CrashReport_Platform_MacOS_DeadlockDetector::f_Debug_StartDeadlockDetector(fp64 _Timeout)
	{
		m_DeadlockDetector.f_SetTimeout(_Timeout);
		if (m_DeadlockDetector.f_GetState() != NThread::EThreadState_Running)
			m_DeadlockDetector.f_Start();
	}

	void CSubSystem_CrashReport_Platform_MacOS_DeadlockDetector::f_Debug_NotDeadlocked()
	{
		m_DeadlockDetector.f_Pulse();
	}

	void CSubSystem_CrashReport_Platform_MacOS_DeadlockDetector::f_Debug_StopDeadlockDetector()
	{
		m_DeadlockDetector.f_Stop(true);
	}

	void CSubSystem_CrashReport_Platform_MacOS_DeadlockDetector::f_Debug_PauseDeadlockDetector()
	{
		m_DeadlockDetector.f_AddPauseValue();
	}

	void CSubSystem_CrashReport_Platform_MacOS_DeadlockDetector::f_Debug_ResumeDeadlockDetector()
	{
		aint LastValue = m_DeadlockDetector.f_LastPauseValue();
		if (LastValue == 1)
			m_DeadlockDetector.f_Pulse();

		DMibSafeCheck(LastValue > 0, "Pause error");
	}

	bool CSubSystem_CrashReport_Platform_MacOS_DeadlockDetector::f_Debug_IsDeadlocked()
	{
		return m_DeadlockDetector.f_IsDeadlocked();
	}

	constinit TCSubSystem<CSubSystem_CrashReport_Platform_MacOS_DeadlockDetector, ESubSystemDestruction_BeforeNonTrackedMemoryManager>
		g_SubSystem_CrashReport_Platform_MacOS_DeadlockDetector = {DAggregateInit}
	;

	inline_never bool CPosixDeadlockDetector::fp_DisplayMessage(NStr::CStr const &_Title, NStr::CStr const &_Message)
	{
		auto &SubSystem = *g_SubSystem_CrashReport_Platform_MacOS_DeadlockDetector;
		if (SubSystem.m_fDeadlockNotifyFunction)
			return !SubSystem.m_fDeadlockNotifyFunction();

		return false;
	}
}

bool NMib::NSys::fg_Debug_IsDeadlocked()
{
	return NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_MacOS_DeadlockDetector->f_Debug_IsDeadlocked();
}

void NMib::NSys::fg_Debug_SetDeadlockNotifyFunction(FDeadlockUserNotify *_fCrashDumpUserNotify)
{
	NMib::NCrashReport::NPlatform::g_SubSystem_CrashReport_Platform_MacOS_DeadlockDetector->f_Debug_SetDeadlockUserNotifyFunction(_fCrashDumpUserNotify);
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
