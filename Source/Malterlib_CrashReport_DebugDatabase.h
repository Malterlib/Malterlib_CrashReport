// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/AsyncGenerator>

namespace NMib::NCrashReport
{
	struct CDebugDatabase : public NConcurrency::CActor
	{
		enum class EAssetType : uint32
		{
			mc_Executable = 0
			, mc_DebugInfo = 1
		};

		struct COptions
		{
			NStr::CStr m_DatabaseRoot;
			uint64 m_MaxDatabaseSize = 128 * 1024 * 1024;
			bool m_bRestoreFileContents = false;
		};

		struct CInitResult
		{
			NStr::CStr m_TempDirectory;
		};

		struct CMetadata
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			NStorage::TCOptional<NStr::CStr> m_Product;
			NStorage::TCOptional<NStr::CStr> m_Application;
			NStorage::TCOptional<NStr::CStr> m_Configuration;
			NStorage::TCOptional<NStr::CStr> m_GitBranch;
			NStorage::TCOptional<NStr::CStr> m_GitCommit;
			NStorage::TCOptional<NStr::CStr> m_Platform;
			NStorage::TCOptional<NStr::CStr> m_Version;
			NStorage::TCOptional<NContainer::TCSet<NStr::CStr>> m_Tags;
		};

		struct CAssetAdd
		{
			EAssetType m_Type = EAssetType::mc_Executable;
			NStr::CStr m_FileName;
			NStr::CStr m_Path;
			CMetadata m_Metadata;
			bool m_bForceOverwrite = false;
		};

		struct CAssetFilter
		{
			NStorage::TCOptional<EAssetType> m_AssetType;
			NStorage::TCOptional<NStr::CStr> m_BuildID;
			NStorage::TCOptional<NStr::CStr> m_FileName;
			NStorage::TCOptional<NTime::CTime> m_TimestampStart;
			NStorage::TCOptional<NTime::CTime> m_TimestampEnd;
			CMetadata m_Metadata;
		};

		struct CFileInfo
		{
			NStr::CStr m_FileName;
			NTime::CTime m_Timestamp;
			NCryptography::CHashDigest_SHA256 m_Digest;
			uint64 m_Size = 0;
			uint64 m_CompressedSize = 0;
		};

		struct CAsset
		{
			EAssetType m_AssetType;
			NStr::CStr m_BuildID;
			CFileInfo m_FileInfo;
			CMetadata m_Metadata;
			NStr::CStr m_MainAssetFile;

			NStr::CStr m_StoredPath;
		};

		struct CDeleteResult
		{
			uint64 m_nItemsDeleted = 0;
			uint64 m_nFilesDeleted = 0;
			uint64 m_nBytesDeleted = 0;
		};

		struct CCrashDumpAdd
		{
			NStr::CStr m_ID;
			NStr::CStr m_FileName;
			NStr::CStr m_Path;
			CMetadata m_Metadata;
			NStorage::TCOptional<NStr::CStr> m_ExceptionInfo;
			bool m_bForceOverwrite = false;
		};

		struct CCrashDumpFilter
		{
			NStorage::TCOptional<NStr::CStr> m_ID;
			NStorage::TCOptional<NStr::CStr> m_FileName;
			NStorage::TCOptional<NTime::CTime> m_TimestampStart;
			NStorage::TCOptional<NTime::CTime> m_TimestampEnd;
			CMetadata m_Metadata;
			NStorage::TCOptional<NStr::CStr> m_ExceptionInfo;
		};

		struct CCrashDump
		{
			NStr::CStr m_ID;
			CFileInfo m_FileInfo;
			CMetadata m_Metadata;
			NStorage::TCOptional<NStr::CStr> m_ExceptionInfo;

			NStr::CStr m_StoredPath;
		};

		CDebugDatabase(COptions const &_Options);
		~CDebugDatabase();

		NConcurrency::TCFuture<CInitResult> f_Init();

		NConcurrency::TCFuture<void> f_Asset_Add(CAssetAdd _Params);
		NConcurrency::TCFuture<CDeleteResult> f_Asset_Delete(CAssetFilter _Params, uint64 _nMaxToDelete, bool _bPretend);
		NConcurrency::TCAsyncGenerator<NContainer::TCVector<CAsset>> f_Asset_List(CAssetFilter _Filter);

		NConcurrency::TCFuture<bool> f_CrashDump_Add(CCrashDumpAdd _Params);
		NConcurrency::TCFuture<CDeleteResult> f_CrashDump_Delete(CCrashDumpFilter _Params, uint64 _nMaxToDelete, bool _bPretend);
		NConcurrency::TCAsyncGenerator<NContainer::TCVector<CCrashDump>> f_CrashDump_List(CCrashDumpFilter _Filter);

		static bool fs_IsValidString(NStr::CStr const &_String);

	private:
		NConcurrency::TCFuture<void> fp_Destroy() override;

		struct CInternal;

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NCrashReport;
#endif
