// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_CrashReport_BreakpadUploader.h"
#include <curl/curl.h>

namespace NMib
{

	namespace NCrashReporter
	{	
		size_t fg_WriteCallback(char* _pBuffer, size_t _Size, size_t _nMemb, void* _pUp)
		{
			return _Size * _nMemb; // We just tell Curl we have handled the bytes (we don't care about the response from the error-reporter server)
		}

		class CUploader::CDetails
		{
		protected:
						
			NMib::NFile::CLockFile mp_UploadLockFile;
			
		public:
			
			CDetails()
			{
				curl_global_init(CURL_GLOBAL_ALL);
			}
			
			~CDetails()
			{
				curl_global_cleanup();
			}
			
			bint f_SendCrashReports(NMib::NStr::CStr const& _DumpDirectory, CUploadContext const& _Context, NMib::NStr::CStr& _oErrors)
			{
				NMib::NStr::CStr DumpDirectory = _DumpDirectory;
				if (!DumpDirectory.f_GetLen() || DumpDirectory[DumpDirectory.f_GetLen()-1] != '/')
					DumpDirectory += "/";
				
				mp_UploadLockFile.f_SetLockFile(DumpDirectory + "UploadLock", NFile::EFileOpen_Write);
				
				NMib::NFile::CLockFile::ELockResult LockResult = mp_UploadLockFile.f_Lock(0.25);
				switch(LockResult)
				{
				case NFile::CLockFile::ELockResult_NoAccess:
					_oErrors += "You do not have the required permissions for the dump directory\r\n";
					return false;
				case NFile::CLockFile::ELockResult_DoesNotExist:
					_oErrors += NMib::NStr::CStr::CFormat("Dump directory ({}) does not exist\r\n") << DumpDirectory;
					return false;
				case NFile::CLockFile::ELockResult_TimedOut:
					_oErrors += "Unable to upload dumps. Either another process is already uploading the dumps or you have insufficient permissions.\r\n";
					return false;
				case NFile::CLockFile::ELockResult_Locked:
					break;
				default:
					DNeverGetHere;
				}
				
				auto LockUnlock = fg_OnScopeExit
												(
												 [this]()
												 {
													mp_UploadLockFile.f_Unlock();
												 }
												)
				;
				
				CUploadContext PerDumpContext = _Context;
				
				NMib::NContainer::TCVector<CStr> lDumpFiles = NMib::NFile::CFile::fs_FindFiles(DumpDirectory + "*", EFileAttrib_File, true);
				for (auto iFile = lDumpFiles.f_GetIterator(); iFile; ++iFile)
				{
					PerDumpContext.m_PathToDumpFile = *iFile;
					if (NMib::NFile::CFile::fs_GetExtension(PerDumpContext.m_PathToDumpFile) == "dmp")
						f_SendCrashReport(PerDumpContext, _oErrors);
				}
				
				if (_oErrors.f_IsEmpty())
					return true;
				
				return false;
			}
				
			bint f_SendCrashReport(CUploadContext const& _Context, NMib::NStr::CStr& _oErrors)
			{
				if (!NMib::NFile::CFile::fs_FileExists(_Context.m_PathToDumpFile, NMib::NFile::EFileAttrib_File))
				{
					_oErrors += NMib::NStr::CStr::CFormat("No dump file with the name {} exists.\r\n") << _Context.m_PathToDumpFile;
					return false;
				}

				if (_Context.m_PathToDumpFile.f_FindNoCase("FullDump") >= 0)
				{
					_oErrors += NMib::NStr::CStr::CFormat("Sending full dumps is not supported. Please contact support for details on how to send {} manually.\r\n") << _Context.m_PathToDumpFile;
					return false;
				}
				
				try
				{
					static const int nMaxBytes = 1024*1024*4; // Max 4MB
					
					aint nBytes = NMib::NFile::CFile::fs_GetFileSize(_Context.m_PathToDumpFile);
					if (nBytes > nMaxBytes)
					{
						_oErrors += NMib::NStr::CStr::CFormat("The dump ({}) size exceeds the maximum allowed. Please contact support for details on how to send this dump manually.\r\n") << _Context.m_PathToDumpFile;
						return false;
					}
				}
				catch (NMib::NFile::CExceptionFile const& _Exception)
				{
					_oErrors += NMib::NStr::CStr::CFormat("An error occured whilst verifying dump size: {}\r\n") << _Exception.f_GetErrorStr();
					return false;
				}
				
				CURL* pCurl = curl_easy_init();
				if (!pCurl)
				{
					_oErrors += "libcurl was not initialised\r\n";
					return false;
				}
				
				struct curl_httppost* pFormPost = nullptr;
				struct curl_httppost* pLastPtr = nullptr;
				
				curl_easy_setopt(pCurl, CURLOPT_URL, _Context.m_CrashServer.f_GetStr());
				
				// Add the parameters
				{
					for (auto iParam = _Context.m_Parameters.f_GetIterator(); iParam; ++iParam)
					{
						NMib::NStr::CStr Key = fg_ForceStrUTF8(iParam.f_GetKey());
						NMib::NStr::CStr Value = fg_ForceStrUTF8(*iParam);
						
						curl_formadd
									(
										&pFormPost
										, &pLastPtr
										, CURLFORM_COPYNAME
										, Key.f_GetStr()
										, CURLFORM_COPYCONTENTS
										, Value.f_GetStr()
										, CURLFORM_END
									)
						;
					}
				}
				
				// Add the dump file
				{
					curl_formadd
								(
									&pFormPost
									, &pLastPtr
									, CURLFORM_COPYNAME
									, "upload_file_minidump"
									, CURLFORM_FILE
									, _Context.m_PathToDumpFile.f_GetStr()
									, CURLFORM_END
								 )
					;
				}
				
				struct curl_slist* pHeaderList = nullptr;
				char Buffer[] = "Expect:";
				pHeaderList = curl_slist_append(pHeaderList, Buffer);
				curl_easy_setopt(pCurl, CURLOPT_HTTPHEADER, pHeaderList);
				
				curl_easy_setopt(pCurl, CURLOPT_HTTPPOST, pFormPost);
				curl_easy_setopt(pCurl, CURLOPT_FAILONERROR, 1);
				curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, &fg_WriteCallback);
				
				char ErrorBuffer[CURL_ERROR_SIZE] = {0};
				curl_easy_setopt(pCurl, CURLOPT_ERRORBUFFER, &ErrorBuffer);

			
				CURLcode Result = curl_easy_perform(pCurl);
				bint bSuccess = Result == CURLE_OK;
				
				if (!bSuccess)
				{
					_oErrors += NMib::NStr::CStr::CFormat("Error occured while sending dump ({}): {}\n") << _Context.m_PathToDumpFile << curl_easy_strerror(Result);
					if (ErrorBuffer[0])
						_oErrors += NMib::NStr::CStr::CFormat("Error message: {}\n") << ErrorBuffer;
				}
				else
				{
					try
					{
						NMib::NFile::CFile::fs_DeleteFile(_Context.m_PathToDumpFile);
					}
					catch (NMib::NFile::CExceptionFile const& _Exception)
					{
						_oErrors += NMib::NStr::CStr::CFormat("An error occured whilst attempting to delete dump file ({}) after sending. The error reported was: {}\r\n") << _Context.m_PathToDumpFile << _Exception.f_GetErrorStr();
					}
				}

				curl_easy_cleanup(pCurl);
				curl_formfree(pFormPost);
				curl_slist_free_all(pHeaderList);
				
				return bSuccess;
			}
			
		};
		
		
		CUploader::CUploader()
			: mp_pD(fg_Construct())
		{
			
		}
		
		CUploader::~CUploader()
		{
			
		}
		
		bint CUploader::f_SendCrashReports(NMib::NStr::CStr const& _DumpDirectory, CUploadContext const& _Context, NMib::NStr::CStr& _oErrors)
		{
			return mp_pD->f_SendCrashReports(_DumpDirectory, _Context, _oErrors);
		}
		
		bint CUploader::f_SendCrashReport(CUploadContext const& _Context, NStr::CStr& _oErrors)
		{
			return mp_pD->f_SendCrashReport(_Context, _oErrors);
		}
		
		bint fg_SendCrashReports(NMib::NStr::CStr const& _Email, NMib::NStr::CStr& _oErrors)
		{		
			auto fUploadDumps = [&] (NMib::NStr::CStr const& _DumpDir)
			{
				if (!NMib::NFile::CFile::fs_FileExists(_DumpDir, NMib::NFile::EFileAttrib_Directory))
					return;
				
				fg_SendCrashReports(_DumpDir, DMibBreakpadUploadServer, _Email, _oErrors);
			};
			
			NMib::NStr::CStr MalterlibCrashDumpDir = NSys::fg_Process_GetEnvironmentVariable(CStr("MalterlibCrashDumpDir"));
			if (!MalterlibCrashDumpDir.f_IsEmpty())
				fUploadDumps(MalterlibCrashDumpDir);

			fUploadDumps(NMib::NFile::CFile::fs_AppendPath(NMib::NFile::CFile::fs_GetProgramDirectory(), "CrashDumps"));
			if (NMib::NFile::CFile::fs_GetProgramRoot() != NMib::NFile::CFile::fs_GetProgramDirectory())
				fUploadDumps(NMib::NFile::CFile::fs_AppendPath(NMib::NFile::CFile::fs_GetProgramRoot(), "CrashDumps"));
			fUploadDumps(NMib::NFile::CFile::fs_AppendPath(NMib::NFile::CFile::fs_GetUserLocalProgramDirectory(), "CrashDumps"));
			
			return _oErrors.f_IsEmpty();
		}
		
		bint fg_SendCrashReports(NMib::NStr::CStr const& _DumpDirectory, NMib::NStr::CStr const& _CrashServer, NMib::NStr::CStr const& _Email, NMib::NStr::CStr& _oErrors)
		{
			CUploadContext Context;
			Context.m_Parameters["email"] = _Email;
			Context.m_CrashServer = _CrashServer;
			
			CUploader Uploader;
			return Uploader.f_SendCrashReports(_DumpDirectory, Context, _oErrors);
		}

		bint fg_SendCrashReport(CUploadContext const& _Context, NMib::NStr::CStr& _oErrors)
		{
			CUploader Uploader;
			return Uploader.f_SendCrashReport(_Context, _oErrors);
		}

		bint fg_CheckForCrashDumps()
		{
			auto fl_DirectoryHasDumps = [](NMib::NStr::CStr const& _DumpDir) -> bint
			{
				NMib::NContainer::TCVector<CStr> lDumpFiles = NMib::NFile::CFile::fs_FindFiles(_DumpDir + "/*", EFileAttrib_File, true);
				for (auto iFile = lDumpFiles.f_GetIterator(); iFile; ++iFile)
				{
					if (NMib::NFile::CFile::fs_GetExtension(*iFile) == "dmp")
						return true;
				}

				return false;
			};

			NMib::NStr::CStr MalterlibCrashDumpDir = NSys::fg_Process_GetEnvironmentVariable(CStr("MalterlibCrashDumpDir"));
			if (!MalterlibCrashDumpDir.f_IsEmpty())
			{
				if (fl_DirectoryHasDumps(MalterlibCrashDumpDir))
					return true;
			}

			if (fl_DirectoryHasDumps(NMib::NFile::CFile::fs_AppendPath(NMib::NFile::CFile::fs_GetProgramRoot(), "CrashDumps")))
				return true;

			if (NMib::NFile::CFile::fs_GetProgramRoot() != NMib::NFile::CFile::fs_GetProgramDirectory() && fl_DirectoryHasDumps(NMib::NFile::CFile::fs_AppendPath(NMib::NFile::CFile::fs_GetProgramDirectory(), "CrashDumps")))
				return true;

			if (fl_DirectoryHasDumps(NMib::NFile::CFile::fs_AppendPath(NMib::NFile::CFile::fs_GetUserLocalProgramDirectory(), "CrashDumps")))
				return true;

			return false;
		}
		
	} // Namespace NCrashReporter

} // Namespace NMib

