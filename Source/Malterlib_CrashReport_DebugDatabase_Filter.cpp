// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_CrashReport_DebugDatabase.h"
#include "Malterlib_CrashReport_DebugDatabase_Internal.h"
#include "Malterlib_CrashReport_DebugDatabase_Filter.h"

namespace NMib::NCrashReport::NPrivate
{
	bool fg_FilterString(NStr::CStr const &_String, NStorage::TCOptional<NStr::CStr> const &_FilterPattern)
	{
		if (!_FilterPattern)
			return true;

		return NStr::fg_StrMatchWildcard(_String.f_GetStr(), _FilterPattern->f_GetStr()) == NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted;
	}

	bool fg_FilterString(NStorage::TCOptional<NStr::CStr> const &_String, NStorage::TCOptional<NStr::CStr> const &_FilterPattern)
	{
		if (!_FilterPattern)
			return true;

		if (!_String)
			return false;

		return NStr::fg_StrMatchWildcard(_String->f_GetStr(), _FilterPattern->f_GetStr()) == NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted;
	}
}
