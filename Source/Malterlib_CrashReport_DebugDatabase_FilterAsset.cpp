// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

#include "Malterlib_CrashReport_DebugDatabase.h"
#include "Malterlib_CrashReport_DebugDatabase_Internal.h"
#include "Malterlib_CrashReport_DebugDatabase_Filter.h"

namespace NMib::NCrashReport::NPrivate
{
	NDatabase::CDatabaseValueCache fg_GetAssetFilterPrefix(CDebugDatabase::CAssetFilter const &_Filter)
	{
		NDatabase::CDatabaseValueCache PrefixCache;

		{
			NStream::CBinaryStreamMemoryRef<NStream::CBinaryStreamBigEndian, NDatabase::CDatabaseValueCache> Stream{PrefixCache, 0};

			NDatabase::CDatabaseValue::fs_FeedLexicographic(Stream, NDebugDatabase::CAssetKey::mc_Prefix);

			do
			{
				if (fg_IsExactFilter(_Filter.m_BuildID))
					NDatabase::CDatabaseValue::fs_FeedLexicographic(Stream, *_Filter.m_BuildID);
				else
					break;

				if (fg_IsExactFilter(_Filter.m_FileName))
					NDatabase::CDatabaseValue::fs_FeedLexicographic(Stream, *_Filter.m_FileName);
				else
					break;

				if (fg_IsExactFilter(_Filter.m_Metadata.m_Product))
					NDatabase::CDatabaseValue::fs_FeedLexicographic(Stream, *_Filter.m_Metadata.m_Product);
				else
					break;

				if (fg_IsExactFilter(_Filter.m_Metadata.m_Application))
					NDatabase::CDatabaseValue::fs_FeedLexicographic(Stream, *_Filter.m_Metadata.m_Application);
				else
					break;

				if (fg_IsExactFilter(_Filter.m_Metadata.m_Configuration))
					NDatabase::CDatabaseValue::fs_FeedLexicographic(Stream, *_Filter.m_Metadata.m_Configuration);
				else
					break;

				if (fg_IsExactFilter(_Filter.m_Metadata.m_GitBranch))
					NDatabase::CDatabaseValue::fs_FeedLexicographic(Stream, *_Filter.m_Metadata.m_GitBranch);
				else
					break;

				if (fg_IsExactFilter(_Filter.m_Metadata.m_GitCommit))
					NDatabase::CDatabaseValue::fs_FeedLexicographic(Stream, *_Filter.m_Metadata.m_GitCommit);
				else
					break;

				if (fg_IsExactFilter(_Filter.m_Metadata.m_Platform))
					NDatabase::CDatabaseValue::fs_FeedLexicographic(Stream, *_Filter.m_Metadata.m_Platform);
				else
					break;

				if (fg_IsExactFilter(_Filter.m_Metadata.m_Version))
					NDatabase::CDatabaseValue::fs_FeedLexicographic(Stream, *_Filter.m_Metadata.m_Version);
				else
					break;
			}
			while (false)
				;
		}

		return PrefixCache;
	}

	bool fg_FilterAsset(NDebugDatabase::CAssetKey const &_Key, NDebugDatabase::CAssetValue const &_Value, CDebugDatabase::CAssetFilter const &_Filter)
	{
		if (_Filter.m_AssetType && _Value.m_AssetType != *_Filter.m_AssetType)
			return false;

		if (!fg_FilterString(_Key.m_UniqueKey.m_BuildID, _Filter.m_BuildID))
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

		return true;
	}
}
