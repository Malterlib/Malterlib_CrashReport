// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include "Malterlib_CrashReport_BuildID.h"
#include "Malterlib_CrashReport_BuildID_MachO.h"

namespace NMib::NCrashReport
{
	template <typename tf_CStreamParent, typename tf_CHeader>
	auto fg_GetBuildIDFromExecutable_MachO_GetFromLoadCommands(NStorage::TCSharedPointer<NFile::CFile> _pFile, NStream::CFilePos _Position)
		-> NConcurrency::TCFuture<NStorage::TCOptional<NStr::CStr>>
	{
		auto CaptureExceptions = co_await (NConcurrency::g_CaptureExceptions % "Error trying to parse MachO load commands");

		NFile::TCBinaryStreamFilePtr<tf_CStreamParent> BinaryStream;
		BinaryStream.f_Open(_pFile.f_Get());
		BinaryStream.f_SetPosition(_Position);

		tf_CHeader Header;
		BinaryStream >> Header.m_Magic;
		BinaryStream >> Header.m_CpuType;
		BinaryStream >> Header.m_CpuSubtype;
		BinaryStream >> Header.m_FileType;
		BinaryStream >> Header.m_nCommands;
		BinaryStream >> Header.m_SizeOfCommands;
		BinaryStream >> Header.m_Flags;

		if constexpr
		(
			requires ()
			{
				Header.m_Reserved;
			}
		)
		{
			BinaryStream >> Header.m_Reserved;
		}

		for (uint32 iCommand = 0; iCommand < Header.m_nCommands; ++iCommand)
		{
			uint32 Position = BinaryStream.f_GetPosition();

			NMachO::CLoadCommand LoadCommand;
			BinaryStream >> LoadCommand.m_Command;
			BinaryStream >> LoadCommand.m_CommandSize;

			if (LoadCommand.m_Command == NMachO::gc_LC_UUID && LoadCommand.m_CommandSize >= sizeof(NMachO::CLoadCommand_Uuid))
			{
				auto Uuid = NCryptography::CUniversallyUniqueIdentifier::fs_Empty();

				{
					NFile::TCBinaryStreamFilePtr<NStream::CBinaryStreamBigEndian> BinaryStreamBigEndian;
					BinaryStreamBigEndian.f_Open(_pFile.f_Get());
					BinaryStreamBigEndian.f_SetPosition(BinaryStream.f_GetPosition());

					BinaryStreamBigEndian >> Uuid.m_TimeLow;
					BinaryStreamBigEndian >> Uuid.m_TimeMid;
					BinaryStreamBigEndian >> Uuid.m_TimeHiAndVersion;
					BinaryStreamBigEndian >> Uuid.m_ClockSequenceHiAndReserved;
					BinaryStreamBigEndian >> Uuid.m_ClockSquenceLow;

					static_assert(sizeof(Uuid.m_Node) == 6);
					BinaryStreamBigEndian.f_ConsumeBytes(Uuid.m_Node, sizeof(Uuid.m_Node));

					BinaryStream.f_SetPosition(BinaryStreamBigEndian.f_GetPosition());
				}

				co_return Uuid.f_GetAsString(NCryptography::EUniversallyUniqueIdentifierFormat_AlphaNum).f_LowerCase();
			}

			BinaryStream.f_SetPosition(Position + LoadCommand.m_CommandSize);
		}

		co_return {};
	}

	template <typename tf_CStreamParent, typename tf_CArchitecture>
	NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>> fg_GetBuildIDFromExecutable_MachO_GetFromFat(NStorage::TCSharedPointer<NFile::CFile> _pFile, NStr::CStr _MainAssetFile)
	{
		using namespace NStr;

		auto CaptureExceptions = co_await (NConcurrency::g_CaptureExceptions % "Error trying to parse MachO fat contents");

		NContainer::TCMap<NStr::CStr, NStr::CStr> BuildIDs;

		NFile::TCBinaryStreamFilePtr<tf_CStreamParent> FatStream;
		FatStream.f_Open(_pFile.f_Get());
		FatStream.f_SetPosition(0);

		NMachO::CFatHeader FatHeader;
		FatStream >> FatHeader.m_Magic;
		FatStream >> FatHeader.m_nArchitectures;

		for (uint32 iArchitecture = 0; iArchitecture < FatHeader.m_nArchitectures; ++iArchitecture)
		{
			tf_CArchitecture Arch;
			FatStream >> Arch.m_CpuType;
			FatStream >> Arch.m_CpuSubtype;
			FatStream >> Arch.m_Offset;
			FatStream >> Arch.m_Size;
			FatStream >> Arch.m_Align;

			if constexpr
			(
				requires ()
				{
					Arch.m_Reserved;
				}
			)
			{
				FatStream >> Arch.m_Reserved;
			}

			auto CleanupPosition = g_OnScopeExit / [&, Position = FatStream.f_GetPosition()]
				{
					FatStream.f_SetPosition(Position);
				}
			;

			NFile::TCBinaryStreamFilePtr<> SliceStream;
			SliceStream.f_Open(_pFile.f_Get());
			SliceStream.f_SetPosition(Arch.m_Offset);

			uint32 SliceMagic = 0;
			SliceStream >> SliceMagic;

			NConcurrency::TCFuture<NStorage::TCOptional<NStr::CStr>> Future;

			if (SliceMagic == NMachO::gc_MachoMagic64LittleEndian)
				Future = fg_GetBuildIDFromExecutable_MachO_GetFromLoadCommands<NStream::CBinaryStreamDefault, NMachO::CMachHeader64>(_pFile, Arch.m_Offset);
			else if (SliceMagic == NMachO::gc_MachoMagic64BigEndian)
				Future = fg_GetBuildIDFromExecutable_MachO_GetFromLoadCommands<NStream::CBinaryStreamBigEndian, NMachO::CMachHeader64>(_pFile, Arch.m_Offset);
			else if (SliceMagic == NMachO::gc_MachoMagicLittleEndian)
				Future = fg_GetBuildIDFromExecutable_MachO_GetFromLoadCommands<NStream::CBinaryStreamDefault, NMachO::CMachHeader>(_pFile, Arch.m_Offset);
			else if (SliceMagic == NMachO::gc_MachoMagicBigEndian)
				Future = fg_GetBuildIDFromExecutable_MachO_GetFromLoadCommands<NStream::CBinaryStreamBigEndian, NMachO::CMachHeader>(_pFile, Arch.m_Offset);
			else
				co_return DMibErrorInstance("Unknown fat slice magic: {nfh,sj8,sf0}"_f << SliceMagic);

			auto Result = co_await fg_Move(Future);
			if (Result)
				BuildIDs[*Result] = _MainAssetFile;
		}

		co_return BuildIDs;
	}

	auto fg_GetBuildIDsFromExecutable_MachO(NStorage::TCSharedPointer<NFile::CFile> _pFile, CExecutableFormat _Format, NStr::CStr _MainAssetFile)
		-> NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>>
	{
		DMibRequire(_Format.m_Type == EExecutableType::mc_MachObject);

		if (_Format.m_ArchitectureBitness != 64 && _Format.m_ArchitectureBitness != 0)
			co_return DMibErrorInstance("Only 64 bit MachO or fat executables are supported");

		auto BlockingActorCheckout = NConcurrency::fg_BlockingActor();

		co_return co_await
			(
				NConcurrency::g_Dispatch(BlockingActorCheckout) / [_pFile, _MainAssetFile]() -> NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>>
				{
					auto CaptureExceptions = co_await (NConcurrency::g_CaptureExceptions % "Error trying to get executable type");

					NFile::TCBinaryStreamFilePtr<> BinaryStream;
					BinaryStream.f_Open(_pFile.f_Get());
					BinaryStream.f_SetPosition(0);

					uint32 Magic = 0;
					BinaryStream >> Magic;

					NContainer::TCMap<NStr::CStr, NStr::CStr> BuildIDs;

					NConcurrency::TCFuture<NStorage::TCOptional<NStr::CStr>> Future;
					if (Magic == NMachO::gc_MachoMagic64LittleEndian)
					{
						if (auto Result = co_await fg_GetBuildIDFromExecutable_MachO_GetFromLoadCommands<NStream::CBinaryStreamDefault, NMachO::CMachHeader64>(_pFile, 0))
							BuildIDs[*Result] = _MainAssetFile;
					}
					else if (Magic == NMachO::gc_MachoMagic64BigEndian)
					{
						if (auto Result = co_await fg_GetBuildIDFromExecutable_MachO_GetFromLoadCommands<NStream::CBinaryStreamBigEndian, NMachO::CMachHeader64>(_pFile, 0))
							BuildIDs[*Result] = _MainAssetFile;
					}
					else if (Magic == NMachO::gc_MachoMagicLittleEndian)
					{
						if (auto Result = co_await fg_GetBuildIDFromExecutable_MachO_GetFromLoadCommands<NStream::CBinaryStreamDefault, NMachO::CMachHeader>(_pFile, 0))
							BuildIDs[*Result] = _MainAssetFile;
					}
					else if (Magic == NMachO::gc_MachoMagicBigEndian)
					{
						if (auto Result = co_await fg_GetBuildIDFromExecutable_MachO_GetFromLoadCommands<NStream::CBinaryStreamBigEndian, NMachO::CMachHeader>(_pFile, 0))
							BuildIDs[*Result] = _MainAssetFile;
					}
					else if (Magic == NMachO::gc_MachoMagicFatLittleEndian)
						BuildIDs = co_await fg_GetBuildIDFromExecutable_MachO_GetFromFat<NStream::CBinaryStreamDefault, NMachO::CFatArchitecture>(_pFile, _MainAssetFile);
					else if (Magic == NMachO::gc_MachoMagicFatBigEndian)
						BuildIDs = co_await fg_GetBuildIDFromExecutable_MachO_GetFromFat<NStream::CBinaryStreamBigEndian, NMachO::CFatArchitecture>(_pFile, _MainAssetFile);
					else if (Magic == NMachO::gc_MachoMagicFat64LittleEndian)
						BuildIDs = co_await fg_GetBuildIDFromExecutable_MachO_GetFromFat<NStream::CBinaryStreamDefault, NMachO::CFat64Architecture>(_pFile, _MainAssetFile);
					else if (Magic == NMachO::gc_MachoMagicFat64BigEndian)
						BuildIDs = co_await fg_GetBuildIDFromExecutable_MachO_GetFromFat<NStream::CBinaryStreamBigEndian, NMachO::CFat64Architecture>(_pFile, _MainAssetFile);
					else
						co_return DMibErrorInstance("Unsupported magic MachO file");

					if (BuildIDs.f_IsEmpty())
						co_return DMibErrorInstance("No LC_UUID were found in the MachO file");

					co_return fg_Move(BuildIDs);
				}
			)
		;
	}

	NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>> fg_GetBuildIDsFromExecutable_MacOSBundle(NStr::CStr _BundleDirectory)
	{
		auto BlockingActorCheckout = NConcurrency::fg_BlockingActor();

		using namespace NStr;

		co_return co_await
			(
				NConcurrency::g_Dispatch(BlockingActorCheckout) / [_BundleDirectory]() -> NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>>
				{
					auto CaptureExceptions = co_await (NConcurrency::g_CaptureExceptions % "Error trying to read bundle");

					auto ExpectedExecutableName = NFile::CFile::fs_GetFileNoExt(_BundleDirectory);

					NStr::CStr MainAssetFile = gc_Str<"Contents/MacOS">.m_Str / ExpectedExecutableName;
					NStr::CStr BundleExecutable = _BundleDirectory / MainAssetFile;

					if (!NFile::CFile::fs_FileExists(BundleExecutable, NFile::EFileAttrib_File))
						co_return DMibErrorInstance("Could not find executable '{}' in bundle"_f << ExpectedExecutableName);

					NContainer::TCMap<NStr::CStr, NStr::CStr> AllBuildIDs;

					NStorage::TCSharedPointer<NFile::CFile> pFile = fg_Construct();
					pFile->f_Open(BundleExecutable, NFile::EFileOpen_Read | NFile::EFileOpen_ShareAll);

					auto Format = co_await fg_GetExecutableFormat(pFile);
					if (Format.m_Type != EExecutableType::mc_MachObject)
						co_return DMibErrorInstance("Executable in binary is not MachO");

					AllBuildIDs += co_await fg_GetBuildIDsFromExecutable_MachO(pFile, Format, MainAssetFile);

					if (AllBuildIDs.f_IsEmpty())
						co_return DMibErrorInstance("No LC_UUID found in binary in bundle");

					co_return AllBuildIDs;
				}
			)
		;
	}

	NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>> fg_GetBuildIDsFromDsym(NStr::CStr _BundleDirectory)
	{
		auto BlockingActorCheckout = NConcurrency::fg_BlockingActor();

		co_return co_await
			(
				NConcurrency::g_Dispatch(BlockingActorCheckout) / [_BundleDirectory]() -> NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>>
				{
					auto CaptureExceptions = co_await (NConcurrency::g_CaptureExceptions % "Error trying to read .dSYM bundle");

					// Expected DWARF folder in the bundle
					NStr::CStr BundleExecutablesDirectory = _BundleDirectory / "Contents/Resources/DWARF";

					auto BundleExecutables = NFile::CFile::fs_FindFiles(BundleExecutablesDirectory / "*", NFile::EFileAttrib_File);
					if (BundleExecutables.f_IsEmpty())
						co_return DMibErrorInstance("No DWARF binaries found in .dSYM bundle");

					NContainer::TCMap<NStr::CStr, NStr::CStr> AllBuildIDs;

					for (auto const &FileName : BundleExecutables)
					{
						NStorage::TCSharedPointer<NFile::CFile> pFile = fg_Construct();
						pFile->f_Open(FileName, NFile::EFileOpen_Read | NFile::EFileOpen_ShareAll);

						auto Format = co_await fg_GetExecutableFormat(pFile);
						if (Format.m_Type != EExecutableType::mc_MachObject)
							continue;

						auto MainAssetFile = NFile::CFile::fs_MakePathRelative(FileName, _BundleDirectory);

						AllBuildIDs += co_await fg_GetBuildIDsFromExecutable_MachO(pFile, Format, MainAssetFile);
					}

					if (AllBuildIDs.f_IsEmpty())
						co_return DMibErrorInstance("No LC_UUID found in any DWARF binary in .dSYM");

					co_return AllBuildIDs;
				}
			)
		;
	}
}
