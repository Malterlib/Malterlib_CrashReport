// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>

#include "Malterlib_CrashReport_DebugDatabase.h"
#include "Malterlib_CrashReport_DebugDatabase_Internal.h"
#include "Malterlib_CrashReport_DebugDatabase_Filter.h"

namespace NMib::NCrashReport
{
	auto CDebugDatabase::f_Asset_List(CAssetFilter _Filter) -> NConcurrency::TCAsyncGenerator<NContainer::TCVector<CAsset>>
	{
		auto &Internal = *mp_pInternal;

		auto OnResume = co_await f_CheckDestroyedOnResume();

		auto ReadTransactionWapped = co_await Internal.m_Database(&NDatabase::CDatabaseActor::f_OpenTransactionRead).f_Wrap();

		auto AssetPath = Internal.m_Options.m_DatabaseRoot / "Assets";

		auto BlockingActorCheckout = NConcurrency::fg_BlockingActor();
		co_await fg_ContinueRunningOnActor(BlockingActorCheckout);
		// From here on you can't access Internal

		auto CaptureScope = co_await
			(
				NConcurrency::g_CaptureExceptions %
				(
					NConcurrency::fg_LogError("DebugDatabase", "Error reading data from database (f_Asset_List)")
					% "Internal error reading from debug database, see log"
				)
			)
		;

		auto &ReadTransaction = *ReadTransactionWapped; // To force exception to be thrown here

		NContainer::TCVector<CAsset> Batch;

		constexpr umint c_BatchSize = 128;

		auto CursorPrefix = NPrivate::fg_GetAssetFilterPrefix(_Filter);
		for (auto iAsset = ReadTransaction.m_Transaction.f_ReadCursor(NDatabase::CDatabaseValue(CursorPrefix)); iAsset; ++iAsset)
		{
			auto Key = iAsset.f_Key<NDebugDatabase::CAssetKey>();
			auto Value = iAsset.f_Value<NDebugDatabase::CAssetValue>();

			if (!NPrivate::fg_FilterAsset(Key, Value, _Filter))
				continue;

			Batch.f_Insert
				(
					CAsset
					{
						.m_AssetType = Value.m_AssetType
						, .m_BuildID = Key.m_UniqueKey.m_BuildID
						, .m_FileInfo =
						{
							.m_FileName = Key.m_UniqueKey.m_FileName
							, .m_Timestamp = Value.m_Timestamp
							, .m_Digest = Value.m_Digest
							, .m_Size = Value.m_Size
							, .m_CompressedSize = Value.m_CompressedSize
						}
						, .m_Metadata = Value.m_Metadata
						, .m_MainAssetFile = Value.m_MainAssetFile
						, .m_StoredPath = AssetPath / Value.m_Digest.f_GetString()
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
