// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/LogError>
#include <Mib/Database/DatabaseActor>

#include "Malterlib_CrashReport_DebugDatabase.h"
#include "Malterlib_CrashReport_DebugDatabase_Internal.h"

namespace NMib::NCrashReport
{
	CDebugDatabase::CDebugDatabase(COptions const &_Options)
		: mp_pInternal(fg_Construct(this, _Options))
	{
	}

	CDebugDatabase::~CDebugDatabase()
	{
	}

	bool CDebugDatabase::fs_IsValidString(NStr::CStr const &_String)
	{
		return _String.f_FindChars("*?") < 0;
	}

	auto CDebugDatabase::f_Init() -> NConcurrency::TCFuture<CInitResult>
	{
		auto &Internal = *mp_pInternal;

		auto OnResume = co_await f_CheckDestroyedOnResume();

		co_await Internal.f_SetupDatabase();
		co_await Internal.f_CleanupTemp();

		co_return CInitResult
			{
				.m_TempDirectory = Internal.m_Options.m_DatabaseRoot / "Temp"
			}
		;
	}

	NConcurrency::TCFuture<void> CDebugDatabase::CInternal::f_SetupDatabase()
	{
		auto OnResume = co_await m_pThis->f_CheckDestroyedOnResume();

		m_Database = fg_Construct();

		co_await
			(
				m_Database
				(
					&NDatabase::CDatabaseActor::f_OpenDatabase
					, m_Options.m_DatabaseRoot / "Database"
					, m_Options.m_MaxDatabaseSize
				)
				% "Failed to open database"
			)
		;

		auto Stats = co_await m_Database(&NDatabase::CDatabaseActor::f_GetAggregateStatistics);
		[[maybe_unused]] auto TotalSizeUsed = Stats.f_GetTotalUsedSize();

		DMibLogWithCategory
			(
				Malterlib/CrashReport/DebugDatabase
				, Info
				, "Debug database uses {fe2}% of allotted space ({ns } / {ns } bytes). {ns } records."
				, fp64(TotalSizeUsed) / fp64(m_Options.m_MaxDatabaseSize) * 100.0
				, TotalSizeUsed
				, m_Options.m_MaxDatabaseSize
				, Stats.m_nDataItems
			)
		;

		{
			auto BlockingActorCheckout = NConcurrency::fg_BlockingActor();

			co_await
				(
					NConcurrency::g_Dispatch(BlockingActorCheckout) / [RootPath = m_Options.m_DatabaseRoot]() -> NConcurrency::TCFuture<void>
					{
						auto CaptureExceptions = co_await (NConcurrency::g_CaptureExceptions % "Error trying to setup database directories");

						NFile::CFile::fs_CreateDirectory(RootPath / "Assets");
						NFile::CFile::fs_CreateDirectory(RootPath / "CrashDumps");
						NFile::CFile::fs_CreateDirectory(RootPath / "Temp");
						NFile::CFile::fs_CreateDirectory(RootPath / "TempUncompressed");

						co_return {};
					}
				)
			;
		}

		co_return {};
	}

	NConcurrency::TCFuture<void> CDebugDatabase::CInternal::f_CleanupTemp()
	{
		auto OnResume = co_await m_pThis->f_CheckDestroyedOnResume();

		auto BlockingActorCheckout = NConcurrency::fg_BlockingActor();
		co_await
			(
				NConcurrency::g_Dispatch(BlockingActorCheckout) / [DatabaseRoot = m_Options.m_DatabaseRoot]() -> void
				{
					auto TempDirectory = DatabaseRoot / "Temp";
					if (NFile::CFile::fs_FileExists(TempDirectory))
					{
						for (auto &Path : NFile::CFile::fs_FindFiles(TempDirectory / "*"))
							NFile::CFile::fs_DeleteDirectoryRecursive(Path);
					}

					auto TempUncompressedDirectory = DatabaseRoot / "TempUncompressed";
					if (NFile::CFile::fs_FileExists(TempUncompressedDirectory))
					{
						for (auto &Path : NFile::CFile::fs_FindFiles(TempUncompressedDirectory / "*"))
							NFile::CFile::fs_DeleteDirectoryRecursive(Path);
					}
				}
			)
		;

		co_return {};
	}

	NConcurrency::TCFuture<void> CDebugDatabase::fp_Destroy()
	{
		NConcurrency::CLogError LogError("Malterlib/CrashReport/DebugDatabase");

		auto &Internal = *mp_pInternal;

		if (Internal.m_Database)
			co_await fg_Move(Internal.m_Database).f_Destroy().f_Wrap() > LogError.f_Warning("Failed to destroy database actor");;

		co_return {};
	}
}
