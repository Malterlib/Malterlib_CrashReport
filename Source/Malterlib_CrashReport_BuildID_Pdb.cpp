// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include "Malterlib_CrashReport_BuildID.h"
#include "Malterlib_CrashReport_BuildID_Pdb.h"

namespace NMib::NCrashReport
{
	NConcurrency::TCFuture<NContainer::TCMap<NStr::CStr, NStr::CStr>> fg_GetBuildIDsFromPdb(NStr::CStr _FileName)
	{
		auto BlockingActorCheckout = NConcurrency::fg_BlockingActor();

		using namespace NStr;

		co_return co_await
			(
				NConcurrency::g_Dispatch(BlockingActorCheckout) / [_FileName]() -> NConcurrency::TCFuture<NContainer::TCSet<CStr>>
				{
					auto CaptureExceptions = co_await (NConcurrency::g_CaptureExceptions % "Error reading PDB Build ID");

					NStorage::TCSharedPointer<NFile::CFile> pFile = fg_Construct();
					pFile->f_Open(_FileName, NFile::EFileOpen_Read | NFile::EFileOpen_ShareAll);

					NFile::TCBinaryStreamFilePtr<> BinaryStream;
					BinaryStream.f_Open(pFile.f_Get());
					BinaryStream.f_SetPosition(0);

					uint8 Header[sizeof(NPdb::gc_PdbSignature)] = {};
					BinaryStream.f_ConsumeBytes(Header, sizeof(NPdb::gc_PdbSignature) - 1);

					if (NMemory::fg_MemCmp(Header, (uint8 const *)NPdb::gc_PdbSignature, sizeof(NPdb::gc_PdbSignature) - 1) != 0)
						co_return DMibErrorInstance("Invalid MSF header signature");

					BinaryStream.f_SetPosition(0x20);

					uint32 BlockSize = 0;
					uint32 FreeBlockMapBlockIndex = 0;
					uint32 nBlocks = 0;
					uint32 nDirectoryBytes = 0;
					uint32 Reserved = 0;

					BinaryStream >> BlockSize;
					BinaryStream >> FreeBlockMapBlockIndex;
					BinaryStream >> nBlocks;
					BinaryStream >> nDirectoryBytes;
					BinaryStream >> Reserved;

					uint32 nDirectoryBlocks = fg_AlignUp(nDirectoryBytes, BlockSize) / BlockSize;

					NContainer::TCVector<uint32> DirectoryBlockIndices;
					for (uint32 iBlock = 0; iBlock < nDirectoryBlocks; ++iBlock)
					{
						uint32 BlockIndex = 0;
						BinaryStream >> BlockIndex;
						if (BlockIndex != 0)
							DirectoryBlockIndices.f_Insert(BlockIndex);
					}

					NContainer::CByteVector DirectoryIndexData;
					{
						NFile::TCBinaryStreamFilePtr<> DirectoryStream;
						DirectoryStream.f_Open(pFile.f_Get());

						DirectoryIndexData.f_SetLen(DirectoryBlockIndices.f_GetLen() * BlockSize);
						uint8 *pOut = DirectoryIndexData.f_GetArray();

						for (uint32 iBlock : DirectoryBlockIndices)
						{
							DirectoryStream.f_SetPosition(iBlock * BlockSize);
							DirectoryStream.f_ConsumeBytes(pOut, BlockSize);
							pOut += BlockSize;
						}
					}

					NContainer::TCVector<uint32> DirectoryIndices;
					{
						NStream::CBinaryStreamMemoryRef<> MemoryStream(DirectoryIndexData);

						for (uint32 iBlock = 0; iBlock < nDirectoryBlocks; ++iBlock)
						{
							uint32 BlockIndex = 0;
							MemoryStream >> BlockIndex;
							if (BlockIndex != 0)
								DirectoryIndices.f_Insert(BlockIndex);
						}
					}

					NContainer::CByteVector DirectoryData;
					{
						NFile::TCBinaryStreamFilePtr<> DirectoryStream;
						DirectoryStream.f_Open(pFile.f_Get());

						DirectoryData.f_SetLen(DirectoryIndices.f_GetLen() * BlockSize);
						uint8 *pOut = DirectoryData.f_GetArray();

						for (uint32 iBlock : DirectoryIndices)
						{
							DirectoryStream.f_SetPosition(iBlock * BlockSize);
							DirectoryStream.f_ConsumeBytes(pOut, BlockSize);
							pOut += BlockSize;
						}
					}

					NStream::CBinaryStreamMemoryRef<> DirectoryMemory(DirectoryData);

					uint32 NumStreams = 0;
					DirectoryMemory >> NumStreams;

					NContainer::TCVector<uint32> StreamSizes;
					StreamSizes.f_SetLen(NumStreams);
					for (uint32 iStream = 0; iStream < NumStreams; ++iStream)
						DirectoryMemory >> StreamSizes[iStream];

					NContainer::TCVector<NContainer::TCVector<uint32>> StreamBlockIndices;
					StreamBlockIndices.f_SetLen(NumStreams);

					for (uint32 iStream = 0; iStream < NumStreams; ++iStream)
					{
						uint32 NumStreamBlocks = fg_AlignUp(StreamSizes[iStream], BlockSize) / BlockSize;
						StreamBlockIndices[iStream].f_SetLen(NumStreamBlocks);

						for (uint32 iBlock = 0; iBlock < NumStreamBlocks; ++iBlock)
							DirectoryMemory >> StreamBlockIndices[iStream][iBlock];
					}

					if (StreamSizes.f_GetLen() <= 1 || StreamBlockIndices.f_GetLen() <= 1)
						co_return DMibErrorInstance("PDB is missing stream 1");

					NContainer::CByteVector Stream1Data;
					{
						NFile::TCBinaryStreamFilePtr<> Stream1;
						Stream1.f_Open(pFile.f_Get());

						for (uint32 iBlock : StreamBlockIndices[1])
						{
							Stream1.f_SetPosition(iBlock * BlockSize);

							NContainer::CByteVector Temp;
							Temp.f_SetLen(BlockSize);
							Stream1.f_ConsumeBytes(Temp.f_GetArray(), BlockSize);

							Stream1Data.f_Insert(Temp);
						}
					}

					NStream::CBinaryStreamMemoryRef<> StreamMemory(Stream1Data);

					if (Stream1Data.f_GetLen() < 24)
						co_return DMibErrorInstance("PDB stream 1 isn't long enough to extract build id");

					uint32 Version = 0;
					uint32 TimeDateStamp = 0;
					uint32 Age = 0;
					uint32 DataTimeLow = 0;
					uint16 DataTimeMid = 0;
					uint16 DataTimeHiAndVersion = 0;
					uint8 Node[8] = {};

					StreamMemory >> Version;
					StreamMemory >> TimeDateStamp;
					StreamMemory >> Age;
					StreamMemory >> DataTimeLow;
					StreamMemory >> DataTimeMid;
					StreamMemory >> DataTimeHiAndVersion;
					StreamMemory.f_ConsumeBytes(Node, 8);

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
