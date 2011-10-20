#include "stdafx.h"

// SVN / Tortoise
#include "svn_wc.h"
#include "CacheInterface.h"
#include "registry.h"

// TC
#include "contentplug.h"

// Forward declaration
void ClearStatusMap();

#ifdef _MANAGED
#pragma managed(push, off)
#endif

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
  switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
      break;

    case DLL_THREAD_ATTACH:
      break;

    case DLL_THREAD_DETACH:
      break;

    case DLL_PROCESS_DETACH:
      break;

    default:
      break;
  }

  return TRUE;
}

#ifdef _MANAGED
#pragma managed(pop)
#endif

typedef CComCritSecLock<CComCriticalSection> AutoLocker;

class CSVNDetails
{
  public:
    CSVNDetails();
    ~CSVNDetails();

    bool GetStatusFromRemoteCache(const CString& path, TSVNCacheResponse* pReturnedStatus, bool bRecursive);

  private:
    bool EnsurePipeOpen();
    void ClosePipe();

    HANDLE m_hPipe;
    OVERLAPPED m_Overlapped;
    HANDLE m_hEvent;
    CComCriticalSection m_critSec;
    long m_lastTimeout;
};

CSVNDetails::CSVNDetails()
  : m_hPipe(INVALID_HANDLE_VALUE), m_hEvent(INVALID_HANDLE_VALUE), m_lastTimeout(0)
{
  m_critSec.Init();

  return;
}

CSVNDetails::~CSVNDetails()
{
  ClosePipe();
  m_critSec.Term();

  return;
}

bool CSVNDetails::GetStatusFromRemoteCache(const CString& path, TSVNCacheResponse* pReturnedStatus, bool bRecursive)
{
  if(!EnsurePipeOpen())
  {
    // We've failed to open the pipe - try and start the cache
    // but only if the last try to start the cache was a certain time
    // ago. If we just try over and over again without a small pause
    // in between, the explorer is rendered unusable!
    // Failing to start the cache can have different reasons: missing exe,
    // missing registry key, corrupt exe, ...
    if (((long)GetTickCount() - m_lastTimeout) < 0)
      return false;
    STARTUPINFO startup;
    PROCESS_INFORMATION process;
    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    memset(&process, 0, sizeof(process));

    CRegStdString cachePath(_T("Software\\TortoiseSVN\\CachePath"), _T("TSVNCache.exe"), false, HKEY_LOCAL_MACHINE);
    CString sCachePath = ((tstring) cachePath).c_str();
    if (CreateProcess(sCachePath.GetBuffer(sCachePath.GetLength()+1), NULL, NULL, NULL, FALSE, 0, 0, 0, &startup, &process)==0)
    {
      // It's not appropriate to do a message box here, because there may be hundreds of calls
      sCachePath.ReleaseBuffer();
      ATLTRACE("Failed to start cache\n");
      return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    sCachePath.ReleaseBuffer();

    // Wait for the cache to open
    long endTime = (long)GetTickCount()+1000;
    while(!EnsurePipeOpen())
    {
      if(((long)GetTickCount() - endTime) > 0)
      {
        m_lastTimeout = (long)GetTickCount()+10000;
        return false;
      }
    }
  }

  AutoLocker lock(m_critSec);

  DWORD nBytesRead;
  TSVNCacheRequest request;
  request.flags = TSVNCACHE_FLAGS_NONOTIFICATIONS;
  if(bRecursive)
  {
    request.flags |= TSVNCACHE_FLAGS_RECUSIVE_STATUS;
  }
  wcsncpy_s(request.path, MAX_PATH+1, path, MAX_PATH);
  SecureZeroMemory(&m_Overlapped, sizeof(OVERLAPPED));
  m_Overlapped.hEvent = m_hEvent;
  // Do the transaction in overlapped mode.
  // That way, if anything happens which might block this call
  // we still can get out of it. We NEVER MUST BLOCK THE SHELL!
  // A blocked shell is a very bad user impression, because users
  // who don't know why it's blocked might find the only solution
  // to such a problem is a reboot and therefore they might loose
  // valuable data.
  // One particular situation where the shell could hang is when
  // the cache crashes and our crash report dialog comes up.
  // Sure, it would be better to have no situations where the shell
  // even can get blocked, but the timeout of 10 seconds is long enough
  // so that users still recognize that something might be wrong and
  // report back to us so we can investigate further.

  BOOL fSuccess = TransactNamedPipe(m_hPipe,
    &request, sizeof(request),
    pReturnedStatus, sizeof(*pReturnedStatus),
    &nBytesRead, &m_Overlapped);

  if (!fSuccess)
  {
    if (GetLastError()!=ERROR_IO_PENDING)
    {
      //OutputDebugStringA("TortoiseShell: TransactNamedPipe failed\n");
      ClosePipe();
      return false;
    }

    // TransactNamedPipe is working in an overlapped operation.
    // Wait for it to finish
    DWORD dwWait = WaitForSingleObject(m_hEvent, 10000);
    if (dwWait == WAIT_OBJECT_0)
    {
      fSuccess = GetOverlappedResult(m_hPipe, &m_Overlapped, &nBytesRead, FALSE);
    }
    else
    {
      // the cache didn't respond!
      fSuccess = FALSE;
    }
  }

  if (fSuccess)
  {
    if(nBytesRead == sizeof(TSVNCacheResponse))
    {
      // This is a full response - we need to fix-up some pointers
      pReturnedStatus->m_status.entry = &pReturnedStatus->m_entry;
      pReturnedStatus->m_entry.url = pReturnedStatus->m_url;
    }
    else
    {
      pReturnedStatus->m_status.entry = NULL;
    }

    return true;
  }
  ClosePipe();
  return false;
}

bool CSVNDetails::EnsurePipeOpen()
{
  AutoLocker lock(m_critSec);
  if(m_hPipe != INVALID_HANDLE_VALUE)
  {
    return true;
  }

  m_hPipe = CreateFile(
    GetCachePipeName(),       // pipe name
    GENERIC_READ |          // read and write access
    GENERIC_WRITE,
    0,                // no sharing
    NULL,             // default security attributes
    OPEN_EXISTING,          // opens existing pipe
    FILE_FLAG_OVERLAPPED,     // default attributes
    NULL);              // no template file

  if (m_hPipe == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY)
  {
    // TSVNCache is running but is busy connecting a different client.
    // Do not give up immediately but wait for a few milliseconds until
    // the server has created the next pipe instance
    if (WaitNamedPipe(GetCachePipeName(), 50))
    {
      m_hPipe = CreateFile(
        GetCachePipeName(),       // pipe name
        GENERIC_READ |          // read and write access
        GENERIC_WRITE,
        0,                // no sharing
        NULL,             // default security attributes
        OPEN_EXISTING,          // opens existing pipe
        FILE_FLAG_OVERLAPPED,     // default attributes
        NULL);              // no template file
    }
  }

  if (m_hPipe != INVALID_HANDLE_VALUE)
  {
    // The pipe connected; change to message-read mode.
    DWORD dwMode;

    dwMode = PIPE_READMODE_MESSAGE;
    if(!SetNamedPipeHandleState(
      m_hPipe,    // pipe handle
      &dwMode,  // new pipe mode
      NULL,     // don't set maximum bytes
      NULL))    // don't set maximum time
    {
      ATLTRACE("SetNamedPipeHandleState failed");
      CloseHandle(m_hPipe);
      m_hPipe = INVALID_HANDLE_VALUE;
      return false;
    }
    // create an unnamed (=local) manual reset event for use in the overlapped structure
    if ((m_hEvent != INVALID_HANDLE_VALUE)&&(m_hEvent != NULL))
      return true;
    m_hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (m_hEvent)
      return true;
        m_hEvent = INVALID_HANDLE_VALUE;
    ATLTRACE("CreateEvent failed");
    ClosePipe();
    return false;
  }

  return false;
}

void CSVNDetails::ClosePipe()
{
  AutoLocker lock(m_critSec);

  if(m_hPipe != INVALID_HANDLE_VALUE)
  {
    CloseHandle(m_hPipe);
    CloseHandle(m_hEvent);
    m_hPipe = INVALID_HANDLE_VALUE;
    m_hEvent = INVALID_HANDLE_VALUE;
  }
}


//////////////////////////////////////////////////////////////////////////


CAtlMap<svn_wc_status_kind, const char*> g_statusMap;
CSVNDetails* svnDetails = NULL;

char* strlcpy(char* p, const char* p2, int maxlen)
{
  if ((int)strlen(p2) >= maxlen) {
    strncpy(p, p2, maxlen);
    p[maxlen] = NULL;
  }
  else {
    strcpy(p, p2);
  }

  return p;
}

WCHAR* awlcopy(WCHAR* outname, const char* inname, int maxlen)
{
  if (inname) {
    int ret = MultiByteToWideChar(CP_ACP, 0, inname, -1, outname, maxlen);
	if (ret) {
      outname[maxlen] = 0;
    }
	else {
      outname = NULL;
	}
    return outname;
  }
  else
    return NULL;
}

#ifndef countof
#define countof(str) (sizeof(str)/sizeof(str[0]))
#endif
#define awfilenamecopy(outname, inname) awlcopy(outname, inname, countof(outname) - 1)

void FillStatusMap(const char* iniFilename)
{
  const int maxLen = 32;
  const char* section = "SVNDetails";

  #define AddStatusValue(key, default) { \
    char* buf = new char[maxLen]; \
    if (GetPrivateProfileStringA(section, #key, "", buf, maxLen, iniFilename) == 0) { \
      strlcpy(buf, default, maxLen); \
      WritePrivateProfileStringA(section, #key, default, iniFilename); \
    } \
    g_statusMap.SetAt(key, buf); \
  }

  AddStatusValue(svn_wc_status_none,         "None");
  AddStatusValue(svn_wc_status_unversioned,  "Unversioned");
  AddStatusValue(svn_wc_status_normal,       "Normal");
  AddStatusValue(svn_wc_status_added,        "Added");
  AddStatusValue(svn_wc_status_missing,      "Missing");
  AddStatusValue(svn_wc_status_deleted,      "Deleted");
  AddStatusValue(svn_wc_status_replaced,     "Replaced");
  AddStatusValue(svn_wc_status_modified,     "Modified");
  AddStatusValue(svn_wc_status_merged,       "Merged");
  AddStatusValue(svn_wc_status_conflicted,   "Conflicted");
  AddStatusValue(svn_wc_status_ignored,      "Ignored");
  AddStatusValue(svn_wc_status_obstructed,   "Obstructed");
  AddStatusValue(svn_wc_status_external,     "External");
  AddStatusValue(svn_wc_status_incomplete,   "Incomplete");

  return;
}

const char* GetSVNStatus(svn_wc_status_kind status)
{
  const char* result = NULL;

  if (g_statusMap.Lookup(status, result)) {
    return result;
  }

  return "Unknown";
}

void ClearStatusMap()
{
  POSITION pos = g_statusMap.GetStartPosition();

  while (pos) {
    delete g_statusMap.GetNext(pos)->m_value;
  }

  return;
}


//////////////////////////////////////////////////////////////////////////


struct stFields
{
  char* name;
  int type;
  char* unit;
};

stFields fields[] = {
  {"SVN Author",      ft_string,      ""},
  {"SVN Lock owner",  ft_string,      ""},
  {"SVN Prop Status", ft_string,      ""},
  {"SVN Revision",    ft_numeric_32,  ""},
  {"SVN Text Status", ft_string,      ""},
  {"SVN URL",         ft_string,      ""},
  {"SVN Short URL",   ft_string,      ""},

};

int __stdcall ContentGetDetectString(char* detectString, int maxlen)
{
  return 0;
}

int __stdcall ContentGetSupportedField(int fieldIndex, char* fieldName, char* units, int maxlen)
{
  if ((fieldIndex < 0) || (fieldIndex >= countof(fields))) {
    return ft_nomorefields;
  }

  strlcpy(fieldName, fields[fieldIndex].name, maxlen-1);
  strlcpy(units, fields[fieldIndex].unit, maxlen-1);

  return fields[fieldIndex].type;
}

int __stdcall ContentGetValue(char* fileName, int fieldIndex, int unitIndex, void* fieldValue, int maxlen, int flags)
{
	WCHAR fileNameW[MAX_PATH+1];
	return ContentGetValueW(awfilenamecopy(fileNameW, fileName), fieldIndex, unitIndex, fieldValue, maxlen, flags);
}

int __stdcall ContentGetValueW(WCHAR* fileName, int fieldIndex, int unitIndex, void* fieldValue, int maxlen, int flags)
{
  const ATL::CString sFilename(fileName, MAX_PATH+1);

  if (flags & CONTENT_DELAYIFSLOW) {
    strlcpy((char*)fieldValue, "\0", maxlen-1);
    return ft_delayed;
  }

  WIN32_FIND_DATA fd = {NULL};
  HANDLE fh = FindFirstFile(sFilename, &fd);

  if (fh != INVALID_HANDLE_VALUE) {
    FindClose(fh);

    if (svnDetails) {
      TSVNCacheResponse returnedStatus = {NULL};
      if (svnDetails->GetStatusFromRemoteCache(sFilename, &returnedStatus, true)) {
        switch (fieldIndex) {
          case 0: // "SVN Author"
            strlcpy((char*)fieldValue, returnedStatus.m_author, maxlen-1);
            break;

          case 1: // "SVN Lock owner"
            strlcpy((char*)fieldValue, returnedStatus.m_owner, maxlen-1);
            break;

          case 2: // "SVN Prop Status"
            strlcpy((char*)fieldValue, GetSVNStatus(returnedStatus.m_status.prop_status), maxlen-1);
            break;

          case 3: // "SVN Revision"
            *(int*)fieldValue = returnedStatus.m_entry.cmt_rev;
            break;

          case 4: // "SVN Text Status"
            strlcpy((char*)fieldValue, GetSVNStatus(returnedStatus.m_status.text_status), maxlen-1);
            break;

          case 5: // "SVN URL"
            strlcpy((char*)fieldValue, returnedStatus.m_url, maxlen-1);
            break;

          case 6: // "SVN Short URL"
            char* tok;
            strlcpy((char*)fieldValue, returnedStatus.m_url, maxlen-1);
            if ((tok = strstr((char*)fieldValue,"trunk")) != NULL)
              strncpy((char*)fieldValue, --tok, maxlen-1);
            if ((tok = strstr((char*)fieldValue,"branches")) != NULL)
              strncpy((char*)fieldValue, --tok, maxlen-1);
            if ((tok = strstr((char*)fieldValue,"tags")) != NULL)
              strncpy((char*)fieldValue, --tok, maxlen-1);
            break;

          default:
            return ft_nosuchfield;
        }
      }
      else {
        return ft_fileerror;
      }
    }
    else {
      return ft_fileerror;
    }
  }
  else {
    return ft_fileerror;
  }

  return fields[fieldIndex].type;
}

void __stdcall ContentSetDefaultParams(ContentDefaultParamStruct* dps)
{
  FillStatusMap(dps->DefaultIniName);
  svnDetails = new CSVNDetails();
  return;
}

void __stdcall ContentStopGetValue(char* fileName)
{
  return;
}

void __stdcall ContentPluginUnloading(void)
{
    delete svnDetails;
    svnDetails = NULL;
    ClearStatusMap();
    return;
}
