// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>
#include <Mib/Cryptography/UUID>

#include "Malterlib_CrashReport_BuildID_Internal.h"

namespace NMib::NCrashReport
{
	namespace NMachO
	{
		static constexpr uint32 gc_MachoMagic64LittleEndian = 0xfeedfacf;
		static constexpr uint32 gc_MachoMagic64BigEndian = 0xcffaedfe;

		static constexpr uint32 gc_MachoMagicLittleEndian = 0xfeedface;
		static constexpr uint32 gc_MachoMagicBigEndian = 0xcefaedfe;

		static constexpr uint32 gc_MachoMagicFatBigEndian = 0xbebafeca;
		static constexpr uint32 gc_MachoMagicFatLittleEndian = 0xcafebabe;
		static constexpr uint32 gc_MachoMagicFat64BigEndian = 0xbfbafeca;
		static constexpr uint32 gc_MachoMagicFat64LittleEndian = 0xcafebabf;
		static constexpr uint32 gc_LC_UUID = 0x1b;

		struct CMachHeader
		{
			uint32 m_Magic;
			uint32 m_CpuType;
			uint32 m_CpuSubtype;
			uint32 m_FileType;
			uint32 m_nCommands;
			uint32 m_SizeOfCommands;
			uint32 m_Flags;
		};

		struct CMachHeader64
		{
			uint32 m_Magic;
			uint32 m_CpuType;
			uint32 m_CpuSubtype;
			uint32 m_FileType;
			uint32 m_nCommands;
			uint32 m_SizeOfCommands;
			uint32 m_Flags;
			uint32 m_Reserved;
		};

		struct CLoadCommand
		{
			uint32 m_Command;
			uint32 m_CommandSize;
		};

		struct CLoadCommand_Uuid
		{
			uint32 m_Command;
			uint32 m_CommandSize;
			NCryptography::CUniversallyUniqueIdentifier m_Uuid;
		};

		struct CFatHeader
		{
			uint32 m_Magic;
			uint32 m_nArchitectures;
		};

		struct CFatArchitecture
		{
			uint32 m_CpuType;
			uint32 m_CpuSubtype;
			uint32 m_Offset;
			uint32 m_Size;
			uint32 m_Align;
		};

		struct CFat64Architecture
		{
			uint32 m_CpuType;
			uint32 m_CpuSubtype;
			uint64 m_Offset;
			uint64 m_Size;
			uint32 m_Align;
			uint32 m_Reserved;
		};
	}

	auto fg_GetBuildIDsFromExecutable_MachO(NStorage::TCSharedPointer<NFile::CFile> _pFile, CExecutableFormat _Format, NStr::CStr _MainAssetFile)
		-> NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>>
	;
	NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>> fg_GetBuildIDsFromExecutable_MacOSBundle(NStr::CStr _BundleDirectory);
	NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>> fg_GetBuildIDsFromDsym(NStr::CStr _BundleDirectory);
}
