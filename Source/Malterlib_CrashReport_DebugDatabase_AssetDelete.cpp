// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_CrashReport_DebugDatabase.h"
#include "Malterlib_CrashReport_DebugDatabase_Internal.h"
#include "Malterlib_CrashReport_DebugDatabase_Filter.h"

namespace NMib::NCrashReport
{
	auto CDebugDatabase::f_Asset_Delete(CAssetFilter _Filter, uint64 _nMaxToDelete, bool _bPretend) -> NConcurrency::TCFuture<CDeleteResult>
	{
		if (_nMaxToDelete == 0)
			co_return {};

		auto &Internal = *mp_pInternal;

		auto OnResume = co_await f_CheckDestroyedOnResume();

		NStorage::TCSharedPointer<CDeleteResult> pResult = fg_Construct();

		co_await Internal.m_Database
			(
				&NDatabase::CDatabaseActor::f_WriteWithCompaction
				, NConcurrency::g_ActorFunctorWeak /
				[
					RootDir = Internal.m_Options.m_DatabaseRoot
					, Filter = _Filter
					, pResult
					, nMaxToDelete = _nMaxToDelete
					, bPretend = _bPretend
				]
				(NDatabase::CDatabaseActor::CTransactionWrite _Transaction, bool _bCompacting) mutable
				-> NConcurrency::TCFuture<NDatabase::CDatabaseActor::CTransactionWrite>
				{
					co_await NConcurrency::ECoroutineFlag_CaptureMalterlibExceptions;

					auto WriteTransaction = fg_Move(_Transaction);

					co_await fg_ContinueRunningOnActor(WriteTransaction.f_Checkout());

					auto CursorPrefix = NPrivate::fg_GetAssetFilterPrefix(Filter);
					for (auto iAsset = WriteTransaction.m_Transaction.f_WriteCursor(NDatabase::CDatabaseValue(CursorPrefix)); iAsset;)
					{
						auto Key = iAsset.f_Key<NDebugDatabase::CAssetKey>();
						auto Value = iAsset.f_Value<NDebugDatabase::CAssetValue>();

						if (!NPrivate::fg_FilterAsset(Key, Value, Filter))
						{
							++iAsset;
							continue;
						}

						iAsset.f_Delete();
						{
							auto Cleanup = g_OnScopeExit / [&iAsset, NextKey = iAsset ? iAsset.f_Key<NDebugDatabase::CAssetKey>() : NDebugDatabase::CAssetKey{}]
								{
									if (iAsset)
										iAsset.f_FindEqual(NextKey);
								}
							;

							auto Result = co_await NDebugDatabase::fg_DeleteAsset(fg_Move(WriteTransaction), fg_Move(Key), fg_Move(Value), RootDir, bPretend);

							WriteTransaction = fg_Move(Result.m_WriteTransaction);

							pResult->m_nFilesDeleted += Result.m_nFilesDeleted;
							pResult->m_nBytesDeleted += Result.m_nBytesDeleted;
							++pResult->m_nItemsDeleted;
						}

						if (--nMaxToDelete == 0)
							break;
					}

					if (bPretend)
						co_return NDatabase::CDatabaseActor::CTransactionWrite::fs_Empty(); // Abort transaction

					co_return fg_Move(WriteTransaction);
				}
			)
		;
		
		co_return *pResult;
	}
}
