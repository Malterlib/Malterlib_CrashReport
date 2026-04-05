// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>
#include <Mib/Cryptography/UUID>

#include "Malterlib_CrashReport_BuildID_Internal.h"

namespace NMib::NCrashReport
{
	namespace NElf
	{
		enum class ESegmentType : uint32
		{
			mc_NULL = 0
			, mc_LOAD = 1
			, mc_DYNAMIC = 2
			, mc_INTERP = 3
			, mc_NOTE = 4
			, mc_SHLIB = 5
			, mc_PHDR = 6
			, mc_LOSUNW = 0x6ffffffa
			, mc_SUNWBSS = 0x6ffffffb
			, mc_SUNWSTACK = 0x6ffffffa
			, mc_HISUNW = 0x6fffffff
			, mc_LOPROC = 0x70000000
			, mc_HIPROC = 0x7fffffff
		};
	}

	NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>> fg_GetBuildIDsFromExecutable_Elf(NStorage::TCSharedPointer<NFile::CFile> _pFile, CExecutableFormat _Format);
}
