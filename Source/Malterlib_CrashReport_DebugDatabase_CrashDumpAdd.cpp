// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>
#include <Mib/CrashReport/BuildID>
#include <Mib/Cryptography/RandomID>

#include "Malterlib_CrashReport_DebugDatabase.h"
#include "Malterlib_CrashReport_DebugDatabase_Internal.h"
#include "Malterlib_CrashReport_DebugDatabase_Filter.h"

namespace NMib::NCrashReport
{
	NConcurrency::TCFuture<bool> CDebugDatabase::f_CrashDump_Add(CCrashDumpAdd _Params)
	{
		using namespace NStr;
		{
			CStr Error;
			if (!NFile::CFile::fs_IsSafeRelativePath(_Params.m_FileName, Error))
				co_return DMibErrorInstance("File name {}"_f << Error);
		}

		co_await NPrivate::fg_CheckMetadataValidity(_Params.m_Metadata);

		if (_Params.m_ID.f_IsEmpty())
			co_return DMibErrorInstance("Crash dump ID cannot be empty");

		co_await NPrivate::fg_CheckStringValidity(gc_Str<"Crash dump ID">, _Params.m_ID);

		auto &Internal = *mp_pInternal;

		auto OnResume = co_await f_CheckDestroyedOnResume();

		CStr TempDirectory = Internal.m_Options.m_DatabaseRoot / "Temp";

		if (!_Params.m_Path.f_StartsWith(TempDirectory + "/"))
			co_return DMibErrorInstance("Expected file to be stored inside debug database temp directory");

		auto UncompressedPath = Internal.m_Options.m_DatabaseRoot / "TempUncompressed" / NCryptography::fg_FastRandomID() / _Params.m_FileName;

		NPrivate::CFileInfo FileInfo;
		{
			auto BlockingActorCheckout = NConcurrency::fg_BlockingActor();

			FileInfo = co_await
				(
					NConcurrency::g_Dispatch(BlockingActorCheckout) / [Path = _Params.m_Path, UncompressedPath]() -> NConcurrency::TCFuture<NPrivate::CFileInfo>
					{
						auto CaptureExceptions = co_await (NConcurrency::g_CaptureExceptions % "Error trying to open file");

						co_return co_await NPrivate::fg_GetFileInfoAndUncompress(Path, UncompressedPath);
					}
				)
			;
		}

		auto Cleanup = NConcurrency::g_BlockingActorSubscription / [UncompressedPath]
			{
				NFile::CFile::fs_DeleteDirectoryRecursive(NFile::CFile::fs_GetPath(UncompressedPath));
			}
		;

		NStorage::TCSharedPointer<bool> pWasAdded = fg_Construct(false);

		co_await Internal.m_Database
			(
				&NDatabase::CDatabaseActor::f_WriteWithCompaction
				, NConcurrency::g_ActorFunctorWeak /
				[
					RootDir = Internal.m_Options.m_DatabaseRoot
					, Params = _Params
					, FileInfo
					, bRestoreFileContents = Internal.m_Options.m_bRestoreFileContents
					, pWasAdded
				]
				(NDatabase::CDatabaseActor::CTransactionWrite _Transaction, bool _bCompacting)
				-> NConcurrency::TCFuture<NDatabase::CDatabaseActor::CTransactionWrite>
				{
					co_await NConcurrency::ECoroutineFlag_CaptureMalterlibExceptions;

					auto WriteTransaction = fg_Move(_Transaction);

					co_await fg_ContinueRunningOnActor(WriteTransaction.f_Checkout());

					CStr DestinationPath = RootDir / "CrashDumps" / FileInfo.m_Digest.f_GetString();

					auto ReadCursor = WriteTransaction.m_Transaction.f_ReadCursor(NDebugDatabase::CCrashDumpKey::mc_Prefix, Params.m_ID, Params.m_FileName);
					if (ReadCursor)
					{
						auto CrashDump = ReadCursor.f_Value<NDebugDatabase::CCrashDumpValue>();
						if (FileInfo.m_Digest != CrashDump.m_Digest)
						{
							if (Params.m_bForceOverwrite)
							{
								auto KeyToRemove = ReadCursor.f_Key<NDebugDatabase::CCrashDumpKey>();
								WriteTransaction = co_await NDebugDatabase::fg_DeleteCrashDump(fg_Move(WriteTransaction), KeyToRemove, RootDir);
							}
							else
								co_return DMibErrorInstance("There already exists a crash dump for ID '{}' (file '{}') with different contents"_f << Params.m_ID << Params.m_FileName);
						}
					}

					auto Timestamp = NFile::CFile::fs_GetWriteTime(Params.m_Path);

					if (NFile::CFile::fs_FileExists(DestinationPath))
					{
						if (bRestoreFileContents && (co_await NPrivate::fg_GetFileInfoAndUncompress(DestinationPath, {})).m_Digest != FileInfo.m_Digest)
						{
							DMibLogWithCategory(Malterlib/CrashReport/DebugDatabase, Warning, "Restoring corrupted contents for {} {}", Params.m_ID, Params.m_FileName);
							NFile::CFile::fs_AtomicReplaceFile(Params.m_Path, DestinationPath);
						}
						else
						{
							DMibLogWithCategory(Malterlib/CrashReport/DebugDatabase, Info, "Existing crash dump file was reused {} {}", Params.m_ID, Params.m_FileName);
							NFile::CFile::fs_DeleteDirectoryRecursive(Params.m_Path);
						}
					}
					else
					{
						if (WriteTransaction.m_Transaction.f_ReadCursor(NDebugDatabase::CCrashDumpStoredFileKey::mc_Prefix, FileInfo.m_Digest))
							DMibLogWithCategory(Malterlib/CrashReport/DebugDatabase, Warning, "Restoring missing crash dump contents for {} {}", Params.m_ID, Params.m_FileName);

						NFile::CFile::fs_RenameFile(Params.m_Path, DestinationPath);
					}

					NDebugDatabase::CCrashDumpUniqueKey UniqueKey
						{
							.m_ID = Params.m_ID
							, .m_FileName = Params.m_FileName
						}
					;

					NDebugDatabase::CCrashDumpStoredFileKey StoredCrashDumpKey
						{
							.m_Digest = FileInfo.m_Digest
							, .m_UniqueKey = UniqueKey
						}
					;

					NDebugDatabase::CCrashDumpKey CrashDumpKey
						{
							.m_UniqueKey = UniqueKey
						}
					;

					NDebugDatabase::CCrashDumpValue CrashDumpValue
						{
							.m_Timestamp = Timestamp
							, .m_Digest = FileInfo.m_Digest
							, .m_Size = FileInfo.m_Size
							, .m_CompressedSize = FileInfo.m_CompressedSize
							, .m_Metadata = Params.m_Metadata
							, .m_ExceptionInfo = Params.m_ExceptionInfo
						}
					;

					WriteTransaction.m_Transaction.f_Upsert(StoredCrashDumpKey, NDatabase::CDatabaseValue());

					if (!WriteTransaction.m_Transaction.f_Exists(CrashDumpKey))
						*pWasAdded = true;

					WriteTransaction.m_Transaction.f_Upsert(CrashDumpKey, CrashDumpValue);

					co_return fg_Move(WriteTransaction);
				}
			)
		;

		co_return *pWasAdded;
	}
}
