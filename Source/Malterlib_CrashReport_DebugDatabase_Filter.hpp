// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NCrashReport::NPrivate
{
	template <typename tf_CValue, NStorage::EOptionalMoveSemantics tf_MoveSemantics>
	bool fg_IsExactFilter(NStorage::TCOptional<tf_CValue, tf_MoveSemantics> const &_Value)
	{
		if (!_Value)
			return false;

		if (_Value->f_FindChars("*?") >= 0)
			return false;

		return true;
	}
}
