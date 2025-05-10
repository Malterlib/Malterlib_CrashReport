// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>

#include "Malterlib_CrashReport_DebugDatabase.h"
#include "Malterlib_CrashReport_DebugDatabase_Internal.h"
#include "Malterlib_CrashReport_DebugDatabase_Filter.h"

namespace NMib::NCrashReport
{
	auto CDebugDatabase::f_CrashDump_List(CCrashDumpFilter _Filter) -> NConcurrency::TCAsyncGenerator<NContainer::TCVector<CCrashDump>>
	{
		auto &Internal = *mp_pInternal;

		auto OnResume = co_await f_CheckDestroyedOnResume();

		auto ReadTransactionWapped = co_await Internal.m_Database(&NDatabase::CDatabaseActor::f_OpenTransactionRead).f_Wrap();

		auto CrashDumpPath = Internal.m_Options.m_DatabaseRoot / "CrashDumps";

		auto BlockingActorCheckout = NConcurrency::fg_BlockingActor();
		co_await fg_ContinueRunningOnActor(BlockingActorCheckout);
		// From here on you can't access Internal

		auto CaptureScope = co_await
			(
				NConcurrency::g_CaptureExceptions %
				(
					NConcurrency::fg_LogError("DebugDatabase", "Error reading data from database (f_CrashDump_List)")
					% "Internal error reading from debug database, see log"
				)
			)
		;

		auto &ReadTransaction = *ReadTransactionWapped; // To force exception to be thrown here

		NContainer::TCVector<CCrashDump> Batch;

		constexpr mint c_BatchSize = 128;

		auto CursorPrefix = NPrivate::fg_GetCrashDumpFilterPrefix(_Filter);
		for (auto iCrashDump = ReadTransaction.m_Transaction.f_ReadCursor(NDatabase::CDatabaseValue(CursorPrefix)); iCrashDump; ++iCrashDump)
		{
			auto Key = iCrashDump.f_Key<NDebugDatabase::CCrashDumpKey>();
			auto Value = iCrashDump.f_Value<NDebugDatabase::CCrashDumpValue>();

			if (!NPrivate::fg_FilterCrashDump(Key, Value, _Filter))
				continue;

			Batch.f_Insert
				(
					CCrashDump
					{
						.m_ID = Key.m_UniqueKey.m_ID
						, .m_FileInfo =
						{
							.m_FileName = Key.m_UniqueKey.m_FileName
							, .m_Timestamp = Value.m_Timestamp
							, .m_Digest = Value.m_Digest
							, .m_Size = Value.m_Size
							, .m_CompressedSize = Value.m_CompressedSize
						}
						, .m_Metadata = Value.m_Metadata
						, .m_ExceptionInfo = Value.m_ExceptionInfo
						, .m_StoredPath = CrashDumpPath / Value.m_Digest.f_GetString()
					}
				)
			;

			if (Batch.f_GetLen() >= c_BatchSize)
				co_yield fg_Move(Batch);
		}

		if (!Batch.f_IsEmpty())
			co_yield fg_Move(Batch);

		co_return {};
	}
}
