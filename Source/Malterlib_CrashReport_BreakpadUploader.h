// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>

namespace NMib::NCrashReport
{
	struct CUploadContext
	{
		NMib::NContainer::TCMap<NMib::NStr::CStr, NMib::NStr::CStr> m_Parameters;
		NMib::NStr::CStr m_PathToDumpFile;
		NMib::NStr::CStr m_CrashServer;
	};

	struct CUploader
	{
		CUploader();
		~CUploader();

		bool f_SendCrashReports(NMib::NStr::CStr const &_DumpDirectory, CUploadContext const &_Context, NMib::NStr::CStr &o_Errors);
		bool f_SendCrashReport(CUploadContext const &_Context, NMib::NStr::CStr &o_Errors);

	protected:
		struct CDetails;
		NMib::NStorage::TCUniquePointer<CDetails> mp_pDetails;
	};

	bool fg_SendCrashReports(NMib::NStr::CStr const &_Email, NMib::NStr::CStr &o_Errors);
	bool fg_SendCrashReports(NMib::NStr::CStr const &_DumpDirectory, NMib::NStr::CStr const &_CrashServer, NMib::NStr::CStr const &_Email, NMib::NStr::CStr &o_Errors);
	bool fg_SendCrashReport(CUploadContext const &_Context, NMib::NStr::CStr &o_Errors);
	bool fg_CheckForCrashDumps();
}

#ifndef DMibPNoShortCuts
using namespace NMib::NCrashReport;
#endif
