// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Database/DatabaseActor>

namespace NMib::NCrashReport::NDebugDatabase
{
	struct CCrashDumpUniqueKey
	{
		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);

		NStr::CStr m_ID;
		NStr::CStr m_FileName;
	};

	struct CCrashDumpKey;
	
	struct CCrashDumpStoredFileKey
	{
		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);

		CCrashDumpKey f_CrashDumpKey() const;

		static NStr::CStr const mc_Prefix;

		NStr::CStr m_Prefix = mc_Prefix;
		NCryptography::CHashDigest_SHA256 m_Digest;
		CCrashDumpUniqueKey m_UniqueKey;
	};

	struct CCrashDumpKey
	{
		template <typename tf_CStream>
		void f_FeedLexicographic(tf_CStream &_Stream) const;
		template <typename tf_CStream>
		void f_ConsumeLexicographic(tf_CStream &_Stream);

		static NStr::CStr const mc_Prefix;

		NStr::CStr m_Prefix = mc_Prefix;
		CCrashDumpUniqueKey m_UniqueKey;
	};

	struct CCrashDumpValue
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		NTime::CTime m_Timestamp;
		NCryptography::CHashDigest_SHA256 m_Digest;
		uint64 m_Size = 0;
		uint64 m_CompressedSize = 0;
		CDebugDatabase::CMetadata m_Metadata;
		NStorage::TCOptional<NStr::CStr> m_ExceptionInfo;
	};

	NConcurrency::TCFuture<CDatabaseDeleteResult> fg_DeleteCrashDump
		(
			NDatabase::CDatabaseActor::CTransactionWrite _Transaction
			, NDebugDatabase::CCrashDumpKey _Key
			, NDebugDatabase::CCrashDumpValue _CrashDump
			, NStr::CStr _RootDir
			, bool _bPretend
		)
	;

	NConcurrency::TCFuture<NDatabase::CDatabaseActor::CTransactionWrite> fg_DeleteCrashDump
		(
			NDatabase::CDatabaseActor::CTransactionWrite _Transaction
			, NDebugDatabase::CCrashDumpKey _Key
			, NStr::CStr _RootDir
		)
	;
}

#include "Malterlib_CrashReport_DebugDatabase_Database_CrashDump.hpp"
