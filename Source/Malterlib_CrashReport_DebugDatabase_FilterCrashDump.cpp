// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include "Malterlib_CrashReport_DebugDatabase.h"
#include "Malterlib_CrashReport_DebugDatabase_Internal.h"
#include "Malterlib_CrashReport_DebugDatabase_Filter.h"

namespace NMib::NCrashReport::NPrivate
{
	NDatabase::CDatabaseValueCache fg_GetCrashDumpFilterPrefix(CDebugDatabase::CCrashDumpFilter const &_Filter)
	{
		NDatabase::CDatabaseValueCache PrefixCache;

		{
			NStream::CBinaryStreamMemoryRef<NStream::CBinaryStreamBigEndian, NDatabase::CDatabaseValueCache> Stream{PrefixCache, 0};

			NDatabase::CDatabaseValue::fs_FeedLexicographic(Stream, NDebugDatabase::CCrashDumpKey::mc_Prefix);

			do
			{
				if (fg_IsExactFilter(_Filter.m_ID))
					NDatabase::CDatabaseValue::fs_FeedLexicographic(Stream, *_Filter.m_ID);
				else
					break;

				if (fg_IsExactFilter(_Filter.m_FileName))
					NDatabase::CDatabaseValue::fs_FeedLexicographic(Stream, *_Filter.m_FileName);
				else
					break;
			}
			while (false)
				;
		}

		return PrefixCache;
	}

	bool fg_FilterCrashDump(NDebugDatabase::CCrashDumpKey const &_Key, NDebugDatabase::CCrashDumpValue const &_Value, CDebugDatabase::CCrashDumpFilter const &_Filter)
	{
		if (!fg_FilterString(_Key.m_UniqueKey.m_ID, _Filter.m_ID))
			return false;

		if (!fg_FilterString(_Key.m_UniqueKey.m_FileName, _Filter.m_FileName))
			return false;

		if (_Filter.m_TimestampStart && _Value.m_Timestamp < *_Filter.m_TimestampStart)
			return false;

		if (_Filter.m_TimestampEnd && _Value.m_Timestamp > *_Filter.m_TimestampEnd)
			return false;

		if (!fg_FilterString(_Value.m_Metadata.m_Product, _Filter.m_Metadata.m_Product))
			return false;

		if (!fg_FilterString(_Value.m_Metadata.m_Application, _Filter.m_Metadata.m_Application))
			return false;

		if (!fg_FilterString(_Value.m_Metadata.m_Configuration, _Filter.m_Metadata.m_Configuration))
			return false;

		if (!fg_FilterString(_Value.m_Metadata.m_GitBranch, _Filter.m_Metadata.m_GitBranch))
			return false;

		if (!fg_FilterString(_Value.m_Metadata.m_GitCommit, _Filter.m_Metadata.m_GitCommit))
			return false;

		if (!fg_FilterString(_Value.m_Metadata.m_Platform, _Filter.m_Metadata.m_Platform))
			return false;

		if (!fg_FilterString(_Value.m_Metadata.m_Version, _Filter.m_Metadata.m_Version))
			return false;

		if (_Filter.m_Metadata.m_Tags && !_Filter.m_Metadata.m_Tags->f_IsEmpty())
		{
			if (!_Value.m_Metadata.m_Tags)
				return false;

			auto &ExectedTags = *_Filter.m_Metadata.m_Tags;
			auto &Tags = *_Value.m_Metadata.m_Tags;

			for (auto &Tag : ExectedTags)
			{
				if (!Tags.f_FindEqual(Tag))
					return false;
			}
		}

		if (!fg_FilterString(_Value.m_ExceptionInfo, _Filter.m_ExceptionInfo))
			return false;

		return true;
	}
}
