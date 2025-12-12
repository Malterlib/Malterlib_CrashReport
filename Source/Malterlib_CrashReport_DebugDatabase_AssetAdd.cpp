// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>
#include <Mib/CrashReport/BuildID>
#include <Mib/Cryptography/RandomID>

#include "Malterlib_CrashReport_DebugDatabase.h"
#include "Malterlib_CrashReport_DebugDatabase_Internal.h"
#include "Malterlib_CrashReport_DebugDatabase_Filter.h"

namespace NMib::NCrashReport
{
	NConcurrency::TCFuture<void> CDebugDatabase::f_Asset_Add(CAssetAdd _Params)
	{
		using namespace NStr;
		{
			CStr Error;
			if (!NFile::CFile::fs_IsSafeRelativePath(_Params.m_FileName, Error))
				co_return DMibErrorInstance("File name {}"_f << Error);
		}

		co_await NPrivate::fg_CheckMetadataValidity(_Params.m_Metadata);

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

		NContainer::TCMap<NStr::CStr, NStr::CStr> BuildIDs;
		switch (_Params.m_Type)
		{
		case CDebugDatabase::EAssetType::mc_Executable:
			{
				BuildIDs = co_await fg_GetBuildIDsFromExecutable(UncompressedPath);
			}
			break;
		case CDebugDatabase::EAssetType::mc_DebugInfo:
			{
				BuildIDs = co_await fg_GetBuildIDsFromSymbols(UncompressedPath);
			}
			break;
		}

		if (BuildIDs.f_IsEmpty())
			co_return DMibErrorInstance("No build ID(s) found in file(s)");

		for (auto &BuildID : BuildIDs)
			co_await NPrivate::fg_CheckStringValidity(gc_Str<"Build ID">, BuildID);

		co_await Internal.m_Database
			(
				&NDatabase::CDatabaseActor::f_WriteWithCompaction
				, NConcurrency::g_ActorFunctorWeak /
				[
					RootDir = Internal.m_Options.m_DatabaseRoot
					, Params = _Params
					, BuildIDs
					, FileInfo
					, bRestoreFileContents = Internal.m_Options.m_bRestoreFileContents
				]
				(NDatabase::CDatabaseActor::CTransactionWrite _Transaction, bool _bCompacting)
				-> NConcurrency::TCFuture<NDatabase::CDatabaseActor::CTransactionWrite>
				{
					co_await NConcurrency::ECoroutineFlag_CaptureMalterlibExceptions;

					auto WriteTransaction = fg_Move(_Transaction);

					co_await fg_ContinueRunningOnActor(WriteTransaction.f_Checkout());

					CStr DestinationPath = RootDir / "Assets" / FileInfo.m_Digest.f_GetString();

					for (auto &BuildID : BuildIDs)
					{
						auto ReadCursor = WriteTransaction.m_Transaction.f_ReadCursor(NDebugDatabase::CAssetKey::mc_Prefix, BuildID, Params.m_FileName);
						if (!ReadCursor)
							continue;

						auto Asset = ReadCursor.f_Value<NDebugDatabase::CAssetValue>();
						if (FileInfo.m_Digest != Asset.m_Digest)
						{
							if (Params.m_bForceOverwrite)
							{
								auto KeyToRemove = ReadCursor.f_Key<NDebugDatabase::CAssetKey>();
								WriteTransaction = co_await NDebugDatabase::fg_DeleteAsset(fg_Move(WriteTransaction), KeyToRemove, RootDir);
							}
							else
								co_return DMibErrorInstance("There already exists an asset for build ID '{}' (file '{}') with different contents"_f << BuildID << Params.m_FileName);
						}
					}

					auto Timestamp = NFile::CFile::fs_GetWriteTime(Params.m_Path);

					if (NFile::CFile::fs_FileExists(DestinationPath))
					{
						if (bRestoreFileContents && (co_await NPrivate::fg_GetFileInfoAndUncompress(DestinationPath, {})).m_Digest != FileInfo.m_Digest)
						{
							DMibLogWithCategory(Malterlib/CrashReport/DebugDatabase, Warning, "Restoring corrupted contents for {vs} {}", BuildIDs, Params.m_FileName);
							NFile::CFile::fs_AtomicReplaceFile(Params.m_Path, DestinationPath);
						}
						else
						{
							DMibLogWithCategory(Malterlib/CrashReport/DebugDatabase, Info, "Existing asset file was reused {vs} {}", BuildIDs, Params.m_FileName);
							NFile::CFile::fs_DeleteDirectoryRecursive(Params.m_Path);
						}
					}
					else
					{
						if (WriteTransaction.m_Transaction.f_ReadCursor(NDebugDatabase::CAssetStoredFileKey::mc_Prefix, FileInfo.m_Digest))
							DMibLogWithCategory(Malterlib/CrashReport/DebugDatabase, Warning, "Restoring missing asset contents for {vs} {}", BuildIDs, Params.m_FileName);

						NFile::CFile::fs_RenameFile(Params.m_Path, DestinationPath);
					}

					for (auto &BuildID : BuildIDs.f_Entries())
					{
						NDebugDatabase::CAssetUniqueKey UniqueKey
							{
								.m_BuildID = BuildID.f_Key()
								, .m_FileName = Params.m_FileName
								, .m_Product = Params.m_Metadata.m_Product ? *Params.m_Metadata.m_Product : CStr()
								, .m_Application = Params.m_Metadata.m_Application ? *Params.m_Metadata.m_Application : CStr()
								, .m_Configuration = Params.m_Metadata.m_Configuration ? *Params.m_Metadata.m_Configuration : CStr()
								, .m_GitBranch = Params.m_Metadata.m_GitBranch ? *Params.m_Metadata.m_GitBranch : CStr()
								, .m_GitCommit = Params.m_Metadata.m_GitCommit ? *Params.m_Metadata.m_GitCommit : CStr()
								, .m_Platform = Params.m_Metadata.m_Platform ? *Params.m_Metadata.m_Platform : CStr()
								, .m_Version = Params.m_Metadata.m_Version ? *Params.m_Metadata.m_Version : CStr()
							}
						;

						NDebugDatabase::CAssetStoredFileKey StoredAssetKey
							{
								.m_Digest = FileInfo.m_Digest
								, .m_UniqueKey = UniqueKey
							}
						;

						NDebugDatabase::CAssetKey AssetKey
							{
								.m_UniqueKey = UniqueKey
							}
						;

						NDebugDatabase::CAssetValue AssetValue
							{
								.m_AssetType = Params.m_Type
								, .m_Timestamp = Timestamp
								, .m_Digest = FileInfo.m_Digest
								, .m_Size = FileInfo.m_Size
								, .m_CompressedSize = FileInfo.m_CompressedSize
								, .m_Metadata = Params.m_Metadata
								, .m_MainAssetFile = BuildID.f_Value()
							}
						;

						WriteTransaction.m_Transaction.f_Upsert(StoredAssetKey, NDatabase::CDatabaseValue());
						WriteTransaction.m_Transaction.f_Upsert(AssetKey, AssetValue);
					}

					co_return fg_Move(WriteTransaction);
				}
			)
		;

		co_return {};
	}
}
