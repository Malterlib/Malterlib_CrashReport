// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "stdlib.h"

#ifdef DPlatformFamily_macOS
	#include <memory>
	#include <malloc/malloc.h>
#endif

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
#ifndef DMibClangAnalyzerWorkaround
				int *pTest = 0;
				*pTest = 0;
#endif
			};
			DMibTestSuite(CTestCategory("Breakpoint") << CTestGroup("Manual"))
			{
				DMibTrace("Crashing\n", 0);
				DMibPDebugBreak;
			};
			DMibTestSuite(CTestCategory("Deadlock") << CTestGroup("Manual"))
			{
				NMib::NSys::fg_Debug_StartDeadlockDetector(5.0f);
				volatile bool bValue = true;
				while (bValue)
					bValue = true;
			};
			DMibTestSuite(CTestCategory("Free Perf") << CTestGroup("Manual"))
			{
				auto Checkout = NMib::fg_GetSys()->f_MemoryManager_Checkout();
				
				mint TestSize = 16;
				
				{
					NMib::NTime::CCycles Cycles;
					mint Size2 = TestSize;
					void *pPointer2 = NMib::NMemory::fg_Alloc(Size2);
					
					Cycles.f_Start();
					void *pPointer;
					for (mint i = 0; i < 10000000; ++i)
					{
						pPointer = NMib::NMemory::fg_Alloc(TestSize);
						NMib::NMemory::fg_Free(pPointer, TestSize);
					}
					Cycles.f_Stop();

					NMib::NMemory::fg_Free(pPointer2, Size2);
					
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
#ifdef DPlatformFamily_macOS
			DMibTestSuite("TrySize")
			{
				void *pMemory = malloc(5);
				auto Cleanup = NMib::g_OnScopeExit / [&]
					{
						free(pMemory);
					}
				;
				auto Size = malloc_size(pMemory);
				DMibExpect(Size, >, 0);

				auto Size2 = malloc_size(&pMemory);
				DMibExpect(Size2, ==, 0);

				auto *pInvalidMemory = (void* *)(mint)4096;

				auto SizeInvalidMemory = malloc_size(pInvalidMemory);
				DMibExpect(SizeInvalidMemory, ==, 0);
			};
#endif
		}
	};

	DMibTestRegister(CCrashDumping_Tests, Malterlib::CrashReport);
}

