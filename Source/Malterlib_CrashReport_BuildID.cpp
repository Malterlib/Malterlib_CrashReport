// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include "Malterlib_CrashReport_BuildID.h"
#include "Malterlib_CrashReport_BuildID_MachO.h"
#include "Malterlib_CrashReport_BuildID_Elf.h"
#include "Malterlib_CrashReport_BuildID_PortableExecutable.h"
#include "Malterlib_CrashReport_BuildID_Pdb.h"

namespace NMib::NCrashReport
{
	NConcurrency::TCFuture<CExecutableFormat> fg_GetExecutableFormat(NStorage::TCSharedPointer<NFile::CFile> _pFile)
	{
		auto BlockingActorCheckout = NConcurrency::fg_BlockingActor();

		using namespace NStr;

		co_return co_await
			(
				NConcurrency::g_Dispatch(BlockingActorCheckout) / [_pFile]() -> NConcurrency::TCFuture<CExecutableFormat>
				{
					auto CaptureExceptions = co_await (NConcurrency::g_CaptureExceptions % "Error trying to get executable type");

					NFile::TCBinaryStreamFilePtr<> BinaryStream;
					BinaryStream.f_Open(_pFile.f_Get());
					BinaryStream.f_SetPosition(0);

					uint32 Magic = 0;
					BinaryStream >> Magic;

					// Mach-O detection
					if (Magic == NMachO::gc_MachoMagic64LittleEndian)
						co_return CExecutableFormat{.m_Type = EExecutableType::mc_MachObject, .m_ArchitectureBitness = 64, .m_Endian = EEndian_Little};
					else if (Magic == NMachO::gc_MachoMagic64BigEndian)
						co_return CExecutableFormat{.m_Type = EExecutableType::mc_MachObject, .m_ArchitectureBitness = 64, .m_Endian = EEndian_Big};
					else if (Magic == NMachO::gc_MachoMagicLittleEndian)
						co_return CExecutableFormat{.m_Type = EExecutableType::mc_MachObject, .m_ArchitectureBitness = 32, .m_Endian = EEndian_Little};
					else if (Magic == NMachO::gc_MachoMagicBigEndian)
						co_return CExecutableFormat{.m_Type = EExecutableType::mc_MachObject, .m_ArchitectureBitness = 32, .m_Endian = EEndian_Big};
					else if (Magic == NMachO::gc_MachoMagicFatBigEndian || Magic == NMachO::gc_MachoMagicFat64BigEndian)
						co_return CExecutableFormat{.m_Type = EExecutableType::mc_MachObject, .m_ArchitectureBitness = 0, .m_Endian = EEndian_Big};
					else if (Magic == NMachO::gc_MachoMagicFatLittleEndian || Magic == NMachO::gc_MachoMagicFat64LittleEndian)
						co_return CExecutableFormat{.m_Type = EExecutableType::mc_MachObject, .m_ArchitectureBitness = 0, .m_Endian = EEndian_Little};

					// ELF detection
					uint8 ElfIdent[6] = {};
					BinaryStream.f_SetPosition(0);
					BinaryStream.f_ConsumeBytes(ElfIdent, sizeof(ElfIdent));
					if (ElfIdent[0] == 0x7F && ElfIdent[1] == 'E' && ElfIdent[2] == 'L' && ElfIdent[3] == 'F')
					{
						umint Bitness = (ElfIdent[4] == 1) ? 32 : (ElfIdent[4] == 2) ? 64 : 0;
						EEndian Endian = (ElfIdent[5] == 1) ? EEndian_Little : (ElfIdent[5] == 2) ? EEndian_Big : EEndian_Native;

						if (Bitness != 0 && Endian != EEndian_Native)
							co_return CExecutableFormat{.m_Type = EExecutableType::mc_ExecutableLinkableFormat, .m_ArchitectureBitness = Bitness, .m_Endian = Endian};
					}

					// PE detection
					BinaryStream.f_SetPosition(0);

					uint16 DosMagic = 0;
					BinaryStream >> DosMagic;
					if (DosMagic == 0x5A4D) // 'MZ'
					{
						BinaryStream.f_SetPosition(0x3C);
						uint32 PEOffset = 0;
						BinaryStream >> PEOffset;

						BinaryStream.f_SetPosition(PEOffset);
						uint32 PESignature = 0;
						BinaryStream >> PESignature;

						if (PESignature == 0x00004550) // 'PE\0\0'
						{
							uint16 Machine = 0;
							BinaryStream >> Machine;

							switch (NPortableExecutable::EImageMachine(Machine))
							{
							case NPortableExecutable::EImageMachine::mc_AM33:
							case NPortableExecutable::EImageMachine::mc_ARM:
							case NPortableExecutable::EImageMachine::mc_ARMV7:
							case NPortableExecutable::EImageMachine::mc_I386:
							case NPortableExecutable::EImageMachine::mc_MIPSFPU:
							case NPortableExecutable::EImageMachine::mc_SH3:
							case NPortableExecutable::EImageMachine::mc_SH3E:
							case NPortableExecutable::EImageMachine::mc_SH3DSP:
							case NPortableExecutable::EImageMachine::mc_SH4:
							case NPortableExecutable::EImageMachine::mc_WCEMIPSV2:
							case NPortableExecutable::EImageMachine::mc_R3000:
							case NPortableExecutable::EImageMachine::mc_ALPHA:
							case NPortableExecutable::EImageMachine::mc_TRICORE:
							case NPortableExecutable::EImageMachine::mc_M32R:
							case NPortableExecutable::EImageMachine::mc_POWERPC:
							case NPortableExecutable::EImageMachine::mc_POWERPCFP:
 								co_return CExecutableFormat{.m_Type = EExecutableType::mc_PortableExecutable, .m_ArchitectureBitness = 32, .m_Endian = EEndian_Little};

							case NPortableExecutable::EImageMachine::mc_AMD64:
							case NPortableExecutable::EImageMachine::mc_IA64:
							case NPortableExecutable::EImageMachine::mc_SH5:
							case NPortableExecutable::EImageMachine::mc_R4000:
							case NPortableExecutable::EImageMachine::mc_R10000:
							case NPortableExecutable::EImageMachine::mc_ALPHA64:
							case NPortableExecutable::EImageMachine::mc_ARM64:
							case NPortableExecutable::EImageMachine::mc_POWERPC64:
								co_return CExecutableFormat{.m_Type = EExecutableType::mc_PortableExecutable, .m_ArchitectureBitness = 64, .m_Endian = EEndian_Little};

							case NPortableExecutable::EImageMachine::mc_EBC:
							case NPortableExecutable::EImageMachine::mc_MIPS16:
							case NPortableExecutable::EImageMachine::mc_MIPSFPU16:
							case NPortableExecutable::EImageMachine::mc_THUMB:
							case NPortableExecutable::EImageMachine::mc_CEE:
							case NPortableExecutable::EImageMachine::mc_CEF:
							default:
								co_return DMibErrorInstance("Unsupported machine type: 0x{nfh,sj4,sf0}"_f << Machine);
							}
						}
					}

					co_return DMibErrorInstance("Unknown or unsupported executable format");
				}
			)
		;
	}

	NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>> fg_GetBuildIDsFromExecutable(NStr::CStr _FileName)
	{
		NStorage::TCSharedPointer<NFile::CFile> pFile = fg_Construct();
		{
			auto BlockingActorCheckout = NConcurrency::fg_BlockingActor();

			co_await
				(
					NConcurrency::g_Dispatch(BlockingActorCheckout) / [pFile, _FileName]() -> NConcurrency::TCFuture<NStr::CStr>
					{
						auto CaptureExceptions = co_await (NConcurrency::g_CaptureExceptions % "Error trying to open file");

						if (NFile::CFile::fs_FileExists(_FileName, NFile::EFileAttrib_Directory))
							co_return {};

						pFile->f_Open(_FileName, NFile::EFileOpen_Read | NFile::EFileOpen_ShareAll);

						co_return {};
					}
				)
			;
		}

		if (!pFile->f_IsValid())
			co_return co_await fg_GetBuildIDsFromExecutable_MacOSBundle(_FileName);

		auto ExecutableFormat = co_await fg_GetExecutableFormat(pFile);

		switch (ExecutableFormat.m_Type)
		{
		case EExecutableType::mc_ExecutableLinkableFormat:
			co_return co_await fg_GetBuildIDsFromExecutable_Elf(pFile, ExecutableFormat);
		case EExecutableType::mc_MachObject:
			co_return co_await fg_GetBuildIDsFromExecutable_MachO(pFile, ExecutableFormat, {});
		case EExecutableType::mc_PortableExecutable:
			co_return co_await fg_GetBuildIDsFromExecutable_PortableExecutable(pFile, ExecutableFormat);
		}

		co_return DMibErrorInstance("Internal error: Invalid executable type");
	}

	NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>> fg_GetBuildIDsFromSymbols(NStr::CStr _FileName)
	{
		auto Extension = NFile::CFile::fs_GetExtension(_FileName);

		if (Extension.f_CmpNoCase("dsym") == 0)
			co_return co_await fg_GetBuildIDsFromDsym(_FileName);
		else if (Extension.f_CmpNoCase("pdb") == 0)
			co_return co_await fg_GetBuildIDsFromPdb(_FileName);
		else if (Extension.f_CmpNoCase("debug") == 0)
			co_return co_await fg_GetBuildIDsFromExecutable(_FileName);

		co_return DMibErrorInstance("Unknown symbol format");
	}

	NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>> fg_GetBuildIDsFromFile(NStr::CStr _FileName)
	{
		auto Extension = NFile::CFile::fs_GetExtension(_FileName);

		if (Extension.f_CmpNoCase("dsym") == 0)
			co_return co_await fg_GetBuildIDsFromDsym(_FileName);
		else if (Extension.f_CmpNoCase("pdb") == 0)
			co_return co_await fg_GetBuildIDsFromPdb(_FileName);

		co_return co_await fg_GetBuildIDsFromExecutable(_FileName);
	}
}
