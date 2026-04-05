// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>
#include <Mib/Cryptography/UUID>

#include "Malterlib_CrashReport_BuildID_Internal.h"

namespace NMib::NCrashReport
{
	namespace NPdb
	{
		constexpr ch8 gc_PdbSignature[] = "Microsoft C/C++ MSF 7.00\r\n\x1a\x44\x53";

		struct CPdbSuperBlock
		{
			uint8 m_Signature[30];
			uint8 m_Padding[2];
			uint32 m_BlockSize;
			uint32 m_FreeBlockMapIndex;
			uint32 m_BlockCount;
			uint32 m_DirectorySize;
			uint32 m_Reserved;
		};
	}

	NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>> fg_GetBuildIDsFromPdb(NStr::CStr _FileName);
}
