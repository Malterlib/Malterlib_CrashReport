// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Database/DatabaseActor>

namespace NMib::NCrashReport::NDebugDatabase
{
	enum
	{
		EDebugDatabaseVersion_Current = 0x101
	};

	static constexpr uint32 gc_Version = EDebugDatabaseVersion_Current;

	struct CDatabaseDeleteResult
	{
		NDatabase::CDatabaseActor::CTransactionWrite m_WriteTransaction;
		uint64 m_nFilesDeleted = 0;
		uint64 m_nBytesDeleted = 0;
	};
}

#include "Malterlib_CrashReport_DebugDatabase_Database.hpp"
#include "Malterlib_CrashReport_DebugDatabase_Database_Asset.h"
#include "Malterlib_CrashReport_DebugDatabase_Database_CrashDump.h"
