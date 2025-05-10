// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Compression/ZstandardAsync>
#include <Mib/File/Generators>

#include "Malterlib_CrashReport_DebugDatabase.h"
#include "Malterlib_CrashReport_DebugDatabase_Internal.h"

namespace NMib::NCrashReport::NPrivate
{
	NConcurrency::TCFuture<CFileInfo> fg_GetFileInfoAndUncompress(NStr::CStr _Path, NStr::CStr _UncompressTo)
	{
		auto CaptureExceptions = co_await (NConcurrency::g_CaptureExceptions % "Error trying to get file digest");

		using namespace NStr;

		if (NFile::CFile::fs_FileExists(_Path, NFile::EFileAttrib_File))
		{
			auto DataGenerator = NCompression::fg_DecompressZstandardAsync(NFile::fg_ReadFileGenerator(NFile::CReadFileGeneratorParams{.m_Path = _Path}));

			NCryptography::CHash_SHA256 Hash;

			bool bWriteOutput = !!_UncompressTo;

			NConcurrency::TCFutureVector<void> Writes;

			NConcurrency::CRoundRobinBlockingActors BlockingActors(4);

			NStorage::TCSharedPointer<NFile::CFile> pOutFile;
			if (bWriteOutput)
			{
				NFile::CFile::fs_CreateDirectory(NFile::CFile::fs_GetPath(_UncompressTo));
				pOutFile = fg_Construct();
				pOutFile->f_Open
					(
						_UncompressTo
						, NFile::EFileOpen_Write | NFile::EFileOpen_ShareAll | NFile::EFileOpen_NoLocalCache
					)
				;
			}

			uint64 FilePosition = 0;
			for (auto iData = co_await fg_Move(DataGenerator).f_GetPipelinedIterator(); iData; co_await ++iData)
			{
				auto &&Data = *iData;

				auto DataLen = Data.f_GetLen();

				Hash.f_AddData(Data.f_GetArray(), DataLen);
				if (bWriteOutput)
				{
					(
						NConcurrency::g_Dispatch(*BlockingActors) /
						[
							pOutFile
							, FilePosition
							, Data = fg_Move(Data)
						]
						() mutable
						{
							pOutFile->f_WriteNoLocalCache(FilePosition, Data.f_GetArray(), Data.f_GetLen());
						}
					)
					> Writes;
				}

				FilePosition += DataLen;
			}

			co_await (fg_AllDone(Writes) % "Writing uncompressed file failed");

			co_return CFileInfo
				{
					.m_Digest = Hash.f_GetDigest()
					, .m_Size = FilePosition
					, .m_CompressedSize = (uint64)NFile::CFile::fs_GetFileSize(_Path)
				}
			;
		}
		else if (NFile::CFile::fs_FileExists(_Path, NFile::EFileAttrib_Directory))
		{
			NFile::CFile::CFindFilesOptions FindFilesOptions(_Path / "*", true);
			FindFilesOptions.m_AttribMask = NFile::EFileAttrib_File | NFile::EFileAttrib_Link;

			auto Files = NFile::CFile::fs_FindFiles(FindFilesOptions);
			Files.f_Sort();

			NCryptography::CHash_SHA256 Hash;
			uint64 Size = 0;
			uint64 CompressedSize = 0;

			for (auto &File : Files)
			{
				CStr RootPath = _Path;
				CStr RelativePath = File.m_Path;

				auto CommonPath = NFile::CFile::fs_GetCommonPathAndMakeRelative(RootPath, RelativePath);

				if (!RootPath.f_IsEmpty())
					co_return DMibErrorInstance("Internal error, could not determine relative path: {}"_f << RootPath);

				Hash.f_AddData(RelativePath.f_GetStr(), RelativePath.f_GetLen());

				if (File.m_Attribs & NFile::EFileAttrib_Link)
				{
					if (_UncompressTo)
					{
						auto DestinationFile = _UncompressTo / RelativePath;
						NFile::CFile::fs_CreateDirectory(NFile::CFile::fs_GetPath(DestinationFile));
						auto LinkDestination = NFile::CFile::fs_ResolveSymbolicLink(File.m_Path);

						NFile::CFile::fs_CreateSymbolicLink(LinkDestination, DestinationFile, NFile::EFileAttrib_Directory, NFile::ESymbolicLinkFlag_Relative);
					}
					continue;
				}

				auto FileInfo = co_await fg_GetFileInfoAndUncompress(File.m_Path, _UncompressTo ? _UncompressTo / RelativePath : CStr());
				Hash.f_AddData(FileInfo.m_Digest.f_GetData(), FileInfo.m_Digest.mc_Size);
				Size += FileInfo.m_Size;
				CompressedSize += FileInfo.m_CompressedSize;
			}

			co_return CFileInfo
				{
					.m_Digest = Hash.f_GetDigest()
					, .m_Size = Size
					, .m_CompressedSize = CompressedSize
				}
			;
		}

		co_return DMibErrorInstance("No valid file found to get digest from at: {}"_f << _Path);
	}

	NConcurrency::TCFuture<void> fg_CheckStringValidity(NStr::CStr _Description, NStr::CStr _String)
	{
		using namespace NStr;

		if (!CDebugDatabase::fs_IsValidString(_String))
			co_return DMibErrorInstance("{} cannot contain any of these characters: '*?'. Provided string: '{}'"_f << _Description << _String);

		co_return {};
	}

	NConcurrency::TCFuture<void> fg_CheckMetadataValidity(CDebugDatabase::CMetadata _Metadata)
	{
		using namespace NStr;

		if (_Metadata.m_Product)
			co_await fg_CheckStringValidity(gc_Str<"Metadata product">, *_Metadata.m_Product);

		if (_Metadata.m_Application)
			co_await fg_CheckStringValidity(gc_Str<"Metadata application">, *_Metadata.m_Application);

		if (_Metadata.m_Configuration)
			co_await fg_CheckStringValidity(gc_Str<"Metadata configuration">, *_Metadata.m_Configuration);

		if (_Metadata.m_GitBranch)
			co_await fg_CheckStringValidity(gc_Str<"Metadata gitbranch">, *_Metadata.m_GitBranch);

		if (_Metadata.m_GitCommit)
			co_await fg_CheckStringValidity(gc_Str<"Metadata gitcommit">, *_Metadata.m_GitCommit);

		if (_Metadata.m_Platform)
			co_await fg_CheckStringValidity(gc_Str<"Metadata platform">, *_Metadata.m_Platform);

		if (_Metadata.m_Version)
			co_await fg_CheckStringValidity(gc_Str<"Metadata version">, *_Metadata.m_Version);

		if (_Metadata.m_Tags)
		{
			for (auto &Tag : *_Metadata.m_Tags)
				co_await fg_CheckStringValidity(gc_Str<"Metadata tags">, Tag);
		}

		co_return {};
	}
}
