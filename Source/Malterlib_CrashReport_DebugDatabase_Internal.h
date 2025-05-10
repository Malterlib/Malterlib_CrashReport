// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

#include "Malterlib_CrashReport_DebugDatabase_Database.h"

namespace NMib::NCrashReport
{
	struct CDebugDatabase::CInternal : public NConcurrency::CActorInternal
	{
		CInternal(CDebugDatabase *_pThis, COptions const &_Options)
			: m_pThis(_pThis)
			, m_Options(_Options)
		{
		}

		NConcurrency::TCFuture<void> f_SetupDatabase();
		NConcurrency::TCFuture<void> f_CleanupTemp();

		CDebugDatabase *m_pThis = nullptr;
		COptions m_Options;
		NConcurrency::TCActor<NDatabase::CDatabaseActor> m_Database;
	};
}

namespace NMib::NCrashReport::NPrivate
{
	struct CFileInfo
	{
		NCryptography::CHashDigest_SHA256 m_Digest;
		uint64 m_Size = 0;
		uint64 m_CompressedSize = 0;
	};

	NConcurrency::TCFuture<CFileInfo> fg_GetFileInfoAndUncompress(NStr::CStr _Path, NStr::CStr _UncompressTo);

	NConcurrency::TCFuture<void> fg_CheckStringValidity(NStr::CStr _Description, NStr::CStr _String);
	NConcurrency::TCFuture<void> fg_CheckMetadataValidity(CDebugDatabase::CMetadata _Metadata);
}
