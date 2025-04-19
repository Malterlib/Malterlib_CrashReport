// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_CrashReport_BuildID.h"
#include "Malterlib_CrashReport_BuildID_PortableExecutable.h"

namespace NMib::NCrashReport
{
	auto fg_GetBuildIDsFromExecutable_PortableExecutable(NStorage::TCSharedPointer<NFile::CFile> _pFile, CExecutableFormat _Format)
		-> NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>>
	{
		auto BlockingActorCheckout = NConcurrency::fg_BlockingActor();

		if (_Format.m_ArchitectureBitness != 64 && _Format.m_ArchitectureBitness != 32)
			co_return DMibErrorInstance("Only 64 or 32 bit PE executables are supported");

		if (_Format.m_Endian != EEndian_Little)
			co_return DMibErrorInstance("Only little endian PE executables are supported");

		using namespace NStr;

		co_return co_await
			(
				NConcurrency::g_Dispatch(BlockingActorCheckout) / [_pFile]() -> NConcurrency::TCFuture<NContainer::TCSet<CStr>>
				{
					auto CaptureExceptions = co_await (NConcurrency::g_CaptureExceptions % "Error reading PE Build ID");

					NFile::TCBinaryStreamFilePtr<> BinaryStream;
					BinaryStream.f_Open(_pFile.f_Get());
					BinaryStream.f_SetPosition(0);

					uint16 DosMagic = 0;
					BinaryStream >> DosMagic;
					if (DosMagic != 0x5A4D) // 'MZ'
						co_return DMibErrorInstance("Invalid DOS header");

					BinaryStream.f_SetPosition(0x3C);
					uint32 PEOffset = 0;
					BinaryStream >> PEOffset;

					BinaryStream.f_SetPosition(PEOffset);
					uint32 PESignature = 0;
					BinaryStream >> PESignature;
					if (PESignature != 0x00004550) // 'PE\0\0'
						co_return DMibErrorInstance("Invalid PE signature");

					BinaryStream.f_AddPosition(20); // Skip COFF header
					auto PEHeaderPosition = BinaryStream.f_GetPosition();

					uint16 OptionalMagic = 0;
					BinaryStream >> OptionalMagic;

					bool bIsPE32Plus = OptionalMagic == 0x20B;
					uint32 DataDirectoryOffset = PEHeaderPosition + (bIsPE32Plus ? 112 : 96);

					BinaryStream.f_SetPosition(DataDirectoryOffset + 6 * 8); // Entry 6 = Debug Directory
					uint32 DebugDirRVA = 0;
					uint32 DebugDirSize = 0;
					BinaryStream >> DebugDirRVA;
					BinaryStream >> DebugDirSize;

					if (DebugDirRVA == 0 || DebugDirSize == 0)
						co_return DMibErrorInstance("No debug directory found in PE");

					BinaryStream.f_SetPosition(PEOffset + 0x6);
					uint16 NumSections = 0;
					BinaryStream >> NumSections;

					BinaryStream.f_SetPosition(PEOffset + 0x14);
					uint16 SizeOfOptionalHeader = 0;
					BinaryStream >> SizeOfOptionalHeader;

					uint32 SectionOffset = PEOffset + 0x18 + SizeOfOptionalHeader;
					uint32 DebugFileOffset = 0;

					for (uint16 iSection = 0; iSection < NumSections; ++iSection)
					{
						char SectionName[9] = {};
						BinaryStream.f_SetPosition(SectionOffset + iSection * 40 + 0);
						BinaryStream.f_ConsumeBytes(SectionName, 8);
						SectionName[8] = 0;

						uint32 VirtualSize = 0;
						uint32 VirtualAddress = 0;
						uint32 SizeOfRawData = 0;
						uint32 PointerToRawData = 0;

						BinaryStream.f_SetPosition(SectionOffset + iSection * 40 + 8);
						BinaryStream >> VirtualSize;
						BinaryStream >> VirtualAddress;
						BinaryStream >> SizeOfRawData;
						BinaryStream >> PointerToRawData;

						if (DebugDirRVA >= VirtualAddress && DebugDirRVA < VirtualAddress + VirtualSize)
						{
							DebugFileOffset = PointerToRawData + (DebugDirRVA - VirtualAddress);
							break;
						}
					}

					if (DebugFileOffset == 0)
						co_return DMibErrorInstance("Failed to map RVA to file offset for debug directory");

					BinaryStream.f_SetPosition(DebugFileOffset);

					uint32 Characteristics = 0;
					uint32 TimeDateStamp = 0;
					uint16 MajorVersion = 0;
					uint16 MinorVersion = 0;
					uint32 Type = 0;
					uint32 SizeOfData = 0;
					uint32 AddressOfRawData = 0;
					uint32 PointerToRawData = 0;

					BinaryStream >> Characteristics;
					BinaryStream >> TimeDateStamp;
					BinaryStream >> MajorVersion;
					BinaryStream >> MinorVersion;
					BinaryStream >> Type;
					BinaryStream >> SizeOfData;
					BinaryStream >> AddressOfRawData;
					BinaryStream >> PointerToRawData;

					if (Type != 2) // IMAGE_DEBUG_TYPE_CODEVIEW
						co_return DMibErrorInstance("No CodeView debug info found in PE");

					BinaryStream.f_SetPosition(PointerToRawData);

					char Signature[4] = {};
					BinaryStream.f_ConsumeBytes(Signature, 4);
					if (!(Signature[0] == 'R' && Signature[1] == 'S' && Signature[2] == 'D' && Signature[3] == 'S'))
						co_return DMibErrorInstance("Missing RSDS signature in debug info");

					uint32 DataTimeLow = 0;
					uint16 DataTimeMid = 0;
					uint16 DataTimeHiAndVersion = 0;
					uint8 Node[8] = {};
					uint32 Age = 0;

					BinaryStream >> DataTimeLow;
					BinaryStream >> DataTimeMid;
					BinaryStream >> DataTimeHiAndVersion;
					BinaryStream.f_ConsumeBytes(Node, 8);
					BinaryStream >> Age;

					CStr BuildID;
					BuildID += "{nfh,sj8,sf0}"_f << DataTimeLow;
					BuildID += "{nfh,sj4,sf0}"_f << DataTimeMid;
					BuildID += "{nfh,sj4,sf0}"_f << DataTimeHiAndVersion;
					for (uint32 iNode = 0; iNode < 8; ++iNode)
						BuildID += "{nfh,sj2,sf0}"_f << Node[iNode];
					BuildID += "{nfh,sj8,sf0}"_f << Age;

					NContainer::TCSet<CStr> Result;
					Result[BuildID];
					co_return Result;
				}
			)
		;
	}
}
