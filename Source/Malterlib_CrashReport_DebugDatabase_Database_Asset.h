// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Database/DatabaseActor>

namespace NMib::NCrashReport::NDebugDatabase
{
	struct CAssetUniqueKey
	{
		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);

		NStr::CStr m_BuildID;
		NStr::CStr m_FileName;
		NStr::CStr m_Product;
		NStr::CStr m_Application;
		NStr::CStr m_Configuration;
		NStr::CStr m_GitBranch;
		NStr::CStr m_GitCommit;
		NStr::CStr m_Platform;
		NStr::CStr m_Version;
	};

	struct CAssetKey;
	
	struct CAssetStoredFileKey
	{
		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);

		CAssetKey f_AssetKey() const;

		static NStr::CStr const mc_Prefix;

		NStr::CStr m_Prefix = mc_Prefix;
		NCryptography::CHashDigest_SHA256 m_Digest;
		CAssetUniqueKey m_UniqueKey;
	};

	struct CAssetKey
	{
		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);

		static NStr::CStr const mc_Prefix;

		NStr::CStr m_Prefix = mc_Prefix;
		CAssetUniqueKey m_UniqueKey;
	};

	struct CAssetValue
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		CDebugDatabase::EAssetType m_AssetType = CDebugDatabase::EAssetType::mc_Executable;
		NTime::CTime m_Timestamp;
		NCryptography::CHashDigest_SHA256 m_Digest;
		uint64 m_Size = 0;
		uint64 m_CompressedSize = 0;
		CDebugDatabase::CMetadata m_Metadata;
		NStr::CStr m_MainAssetFile;
	};

	NConcurrency::TCFuture<CDatabaseDeleteResult> fg_DeleteAsset
		(
			NDatabase::CDatabaseActor::CTransactionWrite _Transaction
			, NDebugDatabase::CAssetKey _Key
			, NDebugDatabase::CAssetValue _Asset
			, NStr::CStr _RootDir
			, bool _bPretend
		)
	;

	NConcurrency::TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> fg_DeleteAsset
		(
			NDatabase::CDatabaseActor::CTransactionWrite _Transaction
			, NDebugDatabase::CAssetKey _Key
			, NStr::CStr _RootDir
		)
	;
}

#include "Malterlib_CrashReport_DebugDatabase_Database_Asset.hpp"
