// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NCrashReport::NPrivate
{
	template <typename tf_CValue>
	bool fg_IsExactFilter(NStorage::TCOptional<tf_CValue> const &_Value);

	bool fg_FilterString(NStr::CStr const &_String, NStorage::TCOptional<NStr::CStr> const &_FilterPattern);
	bool fg_FilterString(NStorage::TCOptional<NStr::CStr> const &_String, NStorage::TCOptional<NStr::CStr> const &_FilterPattern);

	NDatabase::CDatabaseValueCache fg_GetAssetFilterPrefix(CDebugDatabase::CAssetFilter const &_Filter);
	bool fg_FilterAsset(NDebugDatabase::CAssetKey const &_Key, NDebugDatabase::CAssetValue const &_Value, CDebugDatabase::CAssetFilter const &_Filter);

	NDatabase::CDatabaseValueCache fg_GetCrashDumpFilterPrefix(CDebugDatabase::CCrashDumpFilter const &_Filter);
	bool fg_FilterCrashDump(NDebugDatabase::CCrashDumpKey const &_Key, NDebugDatabase::CCrashDumpValue const &_Value, CDebugDatabase::CCrashDumpFilter const &_Filter);
}

#include "Malterlib_CrashReport_DebugDatabase_Filter.hpp"
