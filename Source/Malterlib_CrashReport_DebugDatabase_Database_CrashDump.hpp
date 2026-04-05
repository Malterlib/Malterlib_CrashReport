// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NCrashReport::NDebugDatabase
{
	template <typename tf_CStream>
	void CCrashDumpUniqueKey::f_FeedLexicographic(tf_CStream &_Stream) const
	{
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_ID);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_FileName);
	}

	template <typename tf_CStream>
	void CCrashDumpUniqueKey::f_ConsumeLexicographic(tf_CStream &_Stream)
	{
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_ID);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_FileName);
	}

	template <typename tf_CStream>
	void CCrashDumpStoredFileKey::f_FeedLexicographic(tf_CStream &_Stream) const
	{
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Prefix);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Digest);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_UniqueKey);
	}

	template <typename tf_CStream>
	void CCrashDumpStoredFileKey::f_ConsumeLexicographic(tf_CStream &_Stream)
	{
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Prefix);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Digest);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_UniqueKey);
	}

	template <typename tf_CStream>
	void CCrashDumpKey::f_FeedLexicographic(tf_CStream &_Stream) const
	{
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_Prefix);
		NDatabase::CDatabaseValue::fs_FeedLexicographic(_Stream, m_UniqueKey);
	}

	template <typename tf_CStream>
	void CCrashDumpKey::f_ConsumeLexicographic(tf_CStream &_Stream)
	{
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_Prefix);
		NDatabase::CDatabaseValue::fs_ConsumeLexicographic(_Stream, m_UniqueKey);
	}

	template <typename tf_CStream>
	void CCrashDumpValue::f_Stream(tf_CStream &_Stream)
	{
		uint32 Version = gc_Version;
		_Stream % Version;
		DMibBinaryStreamVersion(_Stream, Version);

		_Stream % m_Timestamp;
		_Stream % m_Digest;
		_Stream % m_Size;
		_Stream % m_CompressedSize;
		_Stream % m_Metadata;
		_Stream % m_ExceptionInfo;
	}
}
