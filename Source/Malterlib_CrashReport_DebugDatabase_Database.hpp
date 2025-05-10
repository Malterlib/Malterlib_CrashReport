// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NCrashReport
{
	// Versioned as in the database
	template <typename tf_CStream>
	void CDebugDatabase::CMetadata::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Product;
		_Stream % m_Application;
		_Stream % m_Configuration;
		_Stream % m_GitBranch;
		_Stream % m_GitCommit;
		_Stream % m_Platform;
		_Stream % m_Version;
		_Stream % m_Tags;
	}
}
