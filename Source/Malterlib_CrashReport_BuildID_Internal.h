// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

namespace NMib::NCrashReport
{
	enum class EExecutableType : uint32
	{
		mc_ExecutableLinkableFormat = 0
		, mc_MachObject
		, mc_PortableExecutable
	};

	struct CExecutableFormat
	{
		EExecutableType m_Type = EExecutableType::mc_ExecutableLinkableFormat;
		umint m_ArchitectureBitness = 64;
		EEndian m_Endian = EEndian_Little;
	};

	NConcurrency::TCFuture<CExecutableFormat> fg_GetExecutableFormat(NStorage::TCSharedPointer<NFile::CFile> _pFile);
}
