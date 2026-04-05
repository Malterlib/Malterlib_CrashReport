// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NCrashReport::NDebugDatabase
{
	template <typename tf_CStream>
	void CAssetUniqueKey::f_FeedLexicographic(tf_CStream &_Stream) const
	{
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_BuildID);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_FileName);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Product);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Application);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Configuration);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_GitBranch);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_GitCommit);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Platform);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Version);
	}

	template <typename tf_CStream>
	void CAssetUniqueKey::f_ConsumeLexicographic(tf_CStream &_Stream)
	{
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_BuildID);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_FileName);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Product);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Application);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Configuration);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_GitBranch);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_GitCommit);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Platform);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Version);
	}

	template <typename tf_CStream>
	void CAssetStoredFileKey::f_FeedLexicographic(tf_CStream &_Stream) const
	{
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Prefix);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Digest);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_UniqueKey);
	}

	template <typename tf_CStream>
	void CAssetStoredFileKey::f_ConsumeLexicographic(tf_CStream &_Stream)
	{
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Prefix);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Digest);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_UniqueKey);
	}

	template <typename tf_CStream>
	void CAssetKey::f_FeedLexicographic(tf_CStream &_Stream) const
	{
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Prefix);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_UniqueKey);
	}

	template <typename tf_CStream>
	void CAssetKey::f_ConsumeLexicographic(tf_CStream &_Stream)
	{
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Prefix);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_UniqueKey);
	}

	template <typename tf_CStream>
	void CAssetValue::f_Stream(tf_CStream &_Stream)
	{
		uint32 Version = gc_Version;
		_Stream % Version;
		DMibBinaryStreamVersion(_Stream, Version);

		_Stream % m_AssetType;
		_Stream % m_Timestamp;
		_Stream % m_Digest;
		_Stream % m_Size;
		_Stream % m_CompressedSize;
		_Stream % m_Metadata;
		_Stream % m_MainAssetFile;
	}
}
