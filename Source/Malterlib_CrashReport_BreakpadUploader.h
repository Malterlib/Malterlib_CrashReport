// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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

	class CUploader
	{
	public:

		CUploader();
		~CUploader();

		bint f_SendCrashReports(NMib::NStr::CStr const& _DumpDirectory, CUploadContext const& _Context, NMib::NStr::CStr& _oErrors);
		bint f_SendCrashReport(CUploadContext const& _Context, NMib::NStr::CStr& _oErrors);

	protected:

		class CDetails;
		NMib::NStorage::TCUniquePointer<CDetails> mp_pD;

	};

	bint fg_SendCrashReports(NMib::NStr::CStr const& _Email, NMib::NStr::CStr& _oErrors);
	bint fg_SendCrashReports(NMib::NStr::CStr const& _DumpDirectory, NMib::NStr::CStr const& _CrashServer, NMib::NStr::CStr const& _Email, NMib::NStr::CStr& _oErrors);
	bint fg_SendCrashReport(CUploadContext const& _Context, NMib::NStr::CStr& _oErrors);
	bint fg_CheckForCrashDumps();
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NCrashReport;
#endif
