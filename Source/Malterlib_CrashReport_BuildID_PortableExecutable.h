// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>
#include <Mib/Cryptography/UUID>

#include "Malterlib_CrashReport_BuildID_Internal.h"

namespace NMib::NCrashReport
{
	namespace NPortableExecutable
	{
		enum class EImageMachine : uint16
		{
			mc_AM33 = 0x1d3
			, mc_AMD64 = 0x8664
			, mc_ARM = 0x1c0
			, mc_ARMV7 = 0x1c4
			, mc_EBC = 0xebc
			, mc_I386 = 0x14c
			, mc_IA64 = 0x200
			, mc_M32R = 0x9041
			, mc_MIPS16 = 0x266
			, mc_MIPSFPU = 0x366
			, mc_MIPSFPU16 = 0x466
			, mc_POWERPC = 0x1f0
			, mc_POWERPCFP = 0x1f1
			, mc_POWERPC64 = 0x1f2
			, mc_R4000 = 0x166
			, mc_SH3 = 0x1a2
			, mc_SH3E = 0x01a4
			, mc_SH3DSP = 0x1a3
			, mc_SH4 = 0x1a6
			, mc_SH5 = 0x1a8
			, mc_THUMB = 0x1c2
			, mc_WCEMIPSV2 = 0x169
			, mc_R3000 = 0x162
			, mc_R10000 = 0x168
			, mc_ALPHA = 0x184
			, mc_ALPHA64 = 0x0284
			, mc_AXP64 = mc_ALPHA64
			, mc_CEE = 0xC0EE
			, mc_TRICORE = 0x0520
			, mc_CEF = 0x0CEF
			, mc_ARM64 = 0xAA64
		};
	}

	auto fg_GetBuildIDsFromExecutable_PortableExecutable(NStorage::TCSharedPointer<NFile::CFile> _pFile, CExecutableFormat _Format)
		-> NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>>
	;
}
