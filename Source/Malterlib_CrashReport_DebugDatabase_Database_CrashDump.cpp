// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>

#include "Malterlib_CrashReport_DebugDatabase.h"
#include "Malterlib_CrashReport_DebugDatabase_Internal.h"

namespace NMib::NCrashReport::NDebugDatabase
{
	constexpr NStr::CStr CCrashDumpStoredFileKey::mc_Prefix = NStr::gc_Str<"DbgDbCDSF">;
	constexpr NStr::CStr CCrashDumpKey::mc_Prefix = NStr::gc_Str<"DbgDbCD">;

	CCrashDumpKey CCrashDumpStoredFileKey::f_CrashDumpKey() const
	{
		return CCrashDumpKey{.m_UniqueKey = m_UniqueKey};
	}

	NConcurrency::TCFuture<CDatabaseDeleteResult> fg_DeleteCrashDump
		(
			NDatabase::CDatabaseActor::CTransactionWrite _Transaction
			, NDebugDatabase::CCrashDumpKey _Key
			, CCrashDumpValue _CrashDump
			, NStr::CStr _RootDir
			, bool _bPretend
		)
	{
		// This is running in the blocking actor context
		CDatabaseDeleteResult Result{.m_WriteTransaction = fg_Move(_Transaction)};

		CCrashDumpStoredFileKey StoredFileKey{.m_Digest = _CrashDump.m_Digest, .m_UniqueKey = _Key.m_UniqueKey};

		Result.m_WriteTransaction.m_Transaction.f_Delete(StoredFileKey);

		if (!Result.m_WriteTransaction.m_Transaction.f_ReadCursor(CCrashDumpStoredFileKey::mc_Prefix, _CrashDump.m_Digest))
		{
			auto CaptureExceptions = co_await (NConcurrency::g_CaptureExceptions % "Error trying to delete crash dump contents");

			NStr::CStr FileOrDirectoryToDelete = _RootDir / "CrashDumps" / _CrashDump.m_Digest.f_GetString();
			if (NFile::CFile::fs_FileExists(FileOrDirectoryToDelete))
			{
				if (NFile::CFile::fs_FileExists(FileOrDirectoryToDelete, NFile::EFileAttrib_File))
				{
					++Result.m_nFilesDeleted;
					Result.m_nBytesDeleted += NFile::CFile::fs_GetFileSize(FileOrDirectoryToDelete);
				}
				else
				{
					NFile::CFile::CFindFilesOptions FindFilesOptions(FileOrDirectoryToDelete / "*", true);
					FindFilesOptions.m_AttribMask = NFile::EFileAttrib_File;

					auto Files = NFile::CFile::fs_FindFiles(FindFilesOptions);
					for (auto &File : Files)
					{
						++Result.m_nFilesDeleted;
						Result.m_nBytesDeleted += NFile::CFile::fs_GetFileSize(File.m_Path);
					}
				}

				if (!_bPretend)
					NFile::CFile::fs_DeleteDirectoryRecursive(FileOrDirectoryToDelete);
			}
		}

		co_return fg_Move(Result);
	}

	NConcurrency::TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> fg_DeleteCrashDump
		(
			NDatabase::CDatabaseActor::CTransactionWrite _Transaction
			, CCrashDumpKey _Key
			, NStr::CStr _RootDir
		)
	{
		// This is running in the blocking actor context
		auto WriteTransaction = fg_Move(_Transaction);

		NDebugDatabase::CCrashDumpValue CrashDump;
		if (!WriteTransaction.m_Transaction.f_Get(_Key, CrashDump))
			co_return DMibErrorInstance("Internal error: Crash dump not found when trying to delete it");

		WriteTransaction.m_Transaction.f_Delete(_Key);

		co_return fg_Move((co_await fg_DeleteCrashDump(fg_Move(WriteTransaction), fg_Move(_Key), fg_Move(CrashDump), fg_Move(_RootDir), false)).m_WriteTransaction);
	}
}
