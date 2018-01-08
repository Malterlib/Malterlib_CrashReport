// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "stdlib.h"

namespace
{
	class CCrashDumping_Tests : public CTest
	{
	public:

		CCrashDumping_Tests()
		{
		}

		void f_DoTests()
		{
			DMibTestSuite(CTestCategory("Crash") << CTestGroup("Manual"))
			{
				DMibTrace("Crashing\n", 0);
				DMibPDebugBreak;
			};
			DMibTestSuite(CTestCategory("Deadlock") << CTestGroup("Manual"))
			{
				NMib::NSys::fg_Debug_StartDeadlockDetector(5.0f);
				while (true)
				{
				}
			};
			DMibTestSuite(CTestCategory("free perf") << CTestGroup("Manual"))
			{
				auto Checkout = NMib::fg_GetSys()->f_MemoryManager_Checkout();
				
				mint TestSize = 16;
				
				{
					NMib::NTime::CCycles Cycles;
					mint Size2 = TestSize;
					void *pPointer2 = NMib::NMem::fg_Alloc(Size2);
					
					Cycles.f_Start();
					void *pPointer;
					for (mint i = 0; i < 10000000; ++i)
					{
						pPointer = NMib::NMem::fg_Alloc(TestSize);
						NMib::NMem::fg_Free(pPointer, TestSize);
					}
					Cycles.f_Stop();

					NMib::NMem::fg_Free(pPointer2, Size2);
					
					DMibConOut("{} cycles per fg_Alloc/fg_Free pair{\n}", fp64(Cycles.f_GetCycles()) / fp64(10000000.0) << pPointer << pPointer2);
				}
				//NMib::NSys::fg_Thread_Sleep(20.0);
				{
					NMib::NTime::CCycles Cycles;
					Cycles.f_Start();
					void *pPointer2 = malloc(TestSize);
					void *pPointer;
					for (mint i = 0; i < 10000000; ++i)
					{
						pPointer = malloc(TestSize);
						free(pPointer);
					}
					free(pPointer2);
					
					Cycles.f_Stop();
					
					DMibConOut("{} cycles per malloc/free pair{\n}", fp64(Cycles.f_GetCycles()) / fp64(10000000.0) << pPointer << pPointer2);
				}
			};
		}
	};

	DMibTestRegister(CCrashDumping_Tests, Malterlib::CrashReport);
}

