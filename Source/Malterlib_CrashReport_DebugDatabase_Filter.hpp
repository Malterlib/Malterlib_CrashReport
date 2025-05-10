// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NCrashReport::NPrivate
{
	template <typename tf_CValue>
	bool fg_IsExactFilter(NStorage::TCOptional<tf_CValue> const &_Value)
	{
		if (!_Value)
			return false;

		if (_Value->f_FindChars("*?") >= 0)
			return false;

		return true;
	}
}
