// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include "Malterlib_CrashReport_BuildID.h"
#include "Malterlib_CrashReport_BuildID_Elf.h"

namespace NMib::NCrashReport
{
	template <typename tf_CStreamParent>
	NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>> fg_GetBuildIDsFromExecutable_Elf_Streamed(NStorage::TCSharedPointer<NFile::CFile> _pFile, bool _bIs64Bit)
	{
		using namespace NStr;

		auto CaptureExceptions = co_await (NConcurrency::g_CaptureExceptions % "Error reading ELF Build ID");

		NFile::TCBinaryStreamFilePtr<tf_CStreamParent> BinaryStream;
		BinaryStream.f_Open(_pFile.f_Get());
		BinaryStream.f_SetPosition(0);

		uint8 Ident[16] = {};
		BinaryStream.f_ConsumeBytes(Ident, 16);
		if (!(Ident[0] == 0x7F && Ident[1] == 'E' && Ident[2] == 'L' && Ident[3] == 'F'))
			co_return DMibErrorInstance("Invalid ELF header");

		uint64 ProgramHeaderOffset = 0;
		uint16 EntrySize = 0;
		uint16 NumEntries = 0;

		if (_bIs64Bit)
		{
			BinaryStream.f_SetPosition(32); // e_phoff
			BinaryStream >> ProgramHeaderOffset;

			BinaryStream.f_SetPosition(54); // e_phentsize
			BinaryStream >> EntrySize;
			BinaryStream >> NumEntries;
		}
		else
		{
			uint32 Offset32 = 0;
			BinaryStream.f_SetPosition(28); // e_phoff
			BinaryStream >> Offset32;
			ProgramHeaderOffset = Offset32;

			BinaryStream.f_SetPosition(42); // e_phentsize
			BinaryStream >> EntrySize;
			BinaryStream >> NumEntries;
		}

		bool bFoundInvalidNote = false;

		for (uint16 iEntry = 0; iEntry < NumEntries; ++iEntry)
		{
			BinaryStream.f_SetPosition(ProgramHeaderOffset + iEntry * EntrySize);

			NElf::ESegmentType Type = NElf::ESegmentType::mc_NULL;
			uint64 Offset = 0;
			uint64 FileSize = 0;

			if (_bIs64Bit)
			{
				BinaryStream >> Type;
				BinaryStream.f_AddPosition(4); // Skip flags
				BinaryStream >> Offset;
				BinaryStream.f_AddPosition(16); // Skip vaddr, paddr
				BinaryStream >> FileSize;
			}
			else
			{
				uint32 Offset32 = 0;
				uint32 Size32 = 0;

				BinaryStream >> Type;
				BinaryStream >> Offset32;
				Offset = Offset32;
				BinaryStream.f_AddPosition(8); // Skip vaddr, paddr
				BinaryStream >> Size32;
				FileSize = Size32;
			}

			if (Type != NElf::ESegmentType::mc_NOTE || FileSize < 16)
				continue;

			NStream::CFilePos Max = Offset + FileSize;
			BinaryStream.f_SetPosition(Offset);

			while (BinaryStream.f_GetPosition() + 12 <= Max)
			{
				uint32 NameSize = 0;
				uint32 DescSize = 0;
				uint32 NoteType = 0;

				BinaryStream >> NameSize;
				BinaryStream >> DescSize;
				BinaryStream >> NoteType;

				if (NameSize > 128 || DescSize > 512)
				{
					bFoundInvalidNote = true;
					break;
				}

				char Name[128] = {};
				BinaryStream.f_ConsumeBytes(Name, NameSize);

				uint64 NameEnd = fg_AlignUp(BinaryStream.f_GetPosition(), 4);
				BinaryStream.f_SetPosition(NameEnd);

				if (NoteType == 3 && fg_StrCmp(Name, "GNU") == 0)
				{
					uint8 Desc[64] = {};
					if (DescSize > sizeof(Desc))
					{
						bFoundInvalidNote = true;
						break;
					}

					BinaryStream.f_ConsumeBytes(Desc, DescSize);

					CStr BuildID;
					for (uint32 iDesc = 0; iDesc < DescSize; ++iDesc)
						BuildID += "{nfh,sj2,sf0}"_f << Desc[iDesc];

					NContainer::TCMap<NStr::CStr, NStr::CStr> Result;
					Result[BuildID];
					co_return Result;
				}

				uint64 DescEnd = fg_AlignUp(BinaryStream.f_GetPosition() + DescSize, 4);
				BinaryStream.f_SetPosition(DescEnd);
			}
		}

		if (bFoundInvalidNote)
			co_return DMibErrorInstance("No Build ID found in ELF notes (note entries with invalid size were found)");
		else
			co_return DMibErrorInstance("No Build ID found in ELF notes");
	}

	NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>> fg_GetBuildIDsFromExecutable_Elf(NStorage::TCSharedPointer<NFile::CFile> _pFile, CExecutableFormat _Format)
	{
		auto BlockingActorCheckout = NConcurrency::fg_BlockingActor();

		if (_Format.m_ArchitectureBitness != 64 && _Format.m_ArchitectureBitness != 32)
			co_return DMibErrorInstance("Only 64 or 32 bit ELF executables are supported");

		co_return co_await
			(
				NConcurrency::g_Dispatch(BlockingActorCheckout) / [_pFile, _Format]() -> NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>>
				{
					if (_Format.m_Endian == EEndian_Little)
						co_return co_await fg_GetBuildIDsFromExecutable_Elf_Streamed<NStream::CBinaryStreamDefault>(_pFile, _Format.m_ArchitectureBitness == 64);
					else if (_Format.m_Endian == EEndian_Big)
						co_return co_await fg_GetBuildIDsFromExecutable_Elf_Streamed<NStream::CBinaryStreamBigEndian>(_pFile, _Format.m_ArchitectureBitness == 64);

					co_return DMibErrorInstance("Unsupported ELF format");
				}
			)
		;
	}
}
