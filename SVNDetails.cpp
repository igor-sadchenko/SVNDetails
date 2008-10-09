#include "stdafx.h"

// SVN / Tortoise
#include "svn_wc.h"
#include "CacheInterface.h"

// TC
#include "contentplug.h"

#ifdef _MANAGED
#pragma managed(push, off)
#endif

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
  switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
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
};

CSVNDetails::CSVNDetails()
  : m_hPipe(INVALID_HANDLE_VALUE), m_hEvent(INVALID_HANDLE_VALUE)
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
  if (!EnsurePipeOpen()) {
    return false;
  }

  AutoLocker lock(m_critSec);

  TSVNCacheRequest request;
  ZeroMemory(&request, sizeof(request));
  request.flags = TSVNCACHE_FLAGS_NONOTIFICATIONS;

  if (bRecursive) {
    request.flags |= TSVNCACHE_FLAGS_RECUSIVE_STATUS;
  }

  wcsncpy_s(request.path, MAX_PATH+1, path, MAX_PATH);

  ZeroMemory(&m_Overlapped, sizeof(OVERLAPPED));
  m_Overlapped.hEvent = m_hEvent;

  // Do the transaction in overlapped mode.
  // That way, if anything happens which might block this call
  // we still can get out of it. We NEVER MUST BLOCK THE SHELL!
  // A blocked shell is a very bad user impression, because users
  // who don't know why it's blocked might find the only solution
  // to such a problem is a reboot and therefore they might loose
  // valuable data.
  // One particular situation where the shell could hang is when
  // the cache crashes and our crashreport dialog comes up.
  // Sure, it would be better to have no situations where the shell
  // even can get blocked, but the timeout of 10 seconds is long enough
  // so that users still recognize that something might be wrong and
  // report back to us so we can investigate further.

  DWORD nBytesRead = 0;
  BOOL fSuccess = TransactNamedPipe(m_hPipe, &request, sizeof(request), pReturnedStatus, sizeof(*pReturnedStatus), &nBytesRead, &m_Overlapped);

  if (!fSuccess) {
    if (GetLastError() != ERROR_IO_PENDING) {
      ClosePipe();
      return false;
    }

    // TransactNamedPipe is working in an overlapped operation.
    // Wait for it to finish
    DWORD dwWait = WaitForSingleObject(m_hEvent, 10000);
    if (dwWait == WAIT_OBJECT_0) {
      fSuccess = GetOverlappedResult(m_hPipe, &m_Overlapped, &nBytesRead, FALSE);
    }
    else {
      // the cache didn't respond!
      fSuccess = FALSE;
    }
  }

  if (fSuccess) {
    if (nBytesRead == sizeof(TSVNCacheResponse)) {
      // This is a full response - we need to fix-up some pointers
      pReturnedStatus->m_status.entry = &pReturnedStatus->m_entry;
      pReturnedStatus->m_entry.url = pReturnedStatus->m_url;
    }
    else {
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

  if (m_hPipe != INVALID_HANDLE_VALUE) {
    return true;
  }

  m_hPipe = CreateFile(GetCachePipeName(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

  if (m_hPipe == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY) {
    // TSVNCache is running but is busy connecting a different client.
    // Do not give up immediately but wait for a few milliseconds until
    // the server has created the next pipe instance
    if (WaitNamedPipe(GetCachePipeName(), 50)) {
      m_hPipe = CreateFile(GetCachePipeName(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    }
  }

  if (m_hPipe != INVALID_HANDLE_VALUE) {
    // The pipe connected; change to message-read mode.
    DWORD dwMode = PIPE_READMODE_MESSAGE;

    if (!SetNamedPipeHandleState(m_hPipe, &dwMode, NULL, NULL)) {
      CloseHandle(m_hPipe);
      m_hPipe = INVALID_HANDLE_VALUE;

      return false;
    }

    // create an unnamed (=local) manual reset event for use in the overlapped structure
    m_hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (m_hEvent) {
      return true;
    }

    ClosePipe();
  }

  return false;
}

void CSVNDetails::ClosePipe()
{
  AutoLocker lock(m_critSec);

  if (m_hPipe != INVALID_HANDLE_VALUE) {
    CloseHandle(m_hPipe);
    m_hPipe = INVALID_HANDLE_VALUE;

    CloseHandle(m_hEvent);
    m_hEvent = INVALID_HANDLE_VALUE;
  }

  return;
}


//////////////////////////////////////////////////////////////////////////


const char* GetSVNStatus(svn_wc_status_kind status)
{
  switch (status) {
    case svn_wc_status_none:
      return "None";

    case svn_wc_status_unversioned:
      return "Unversioned";

    case svn_wc_status_normal:
      return "Normal";

    case svn_wc_status_added:
      return "Added";

    case svn_wc_status_missing:
      return "Missing";

    case svn_wc_status_deleted:
      return "Deleted";

    case svn_wc_status_replaced:
      return "Replaced";

    case svn_wc_status_modified:
      return "Modified";

    case svn_wc_status_merged:
      return "Merged";

    case svn_wc_status_conflicted:
      return "Conflicted";

    case svn_wc_status_ignored:
      return "Ignored";

    case svn_wc_status_obstructed:
      return "Obstructed";

    case svn_wc_status_external:
      return "External";

    case svn_wc_status_incomplete:
      return "Incomplete";

    default:
      break;
  }

  return "";
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
};

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

int __stdcall ContentGetDetectString(char* detectString, int maxlen)
{
  return 0;
}

int __stdcall ContentGetSupportedField(int fieldIndex, char* fieldName, char* units, int maxlen)
{
  if ((fieldIndex < 0) || (fieldIndex >= sizeof(fields) / sizeof(fields[0]))) {
    return ft_nomorefields;
  }

  strlcpy(fieldName, fields[fieldIndex].name, maxlen-1);
  strlcpy(units, fields[fieldIndex].unit, maxlen-1);

  return fields[fieldIndex].type;
}

int __stdcall ContentGetValue(char* fileName, int fieldIndex, int unitIndex, void* fieldValue, int maxlen, int flags)
{
  CString sFilename(fileName);

  if (flags & CONTENT_DELAYIFSLOW) {
    strlcpy((char*)fieldValue, "...", maxlen-1);
    return ft_delayed;
  }

  WIN32_FIND_DATA fd = {NULL};
  HANDLE fh = FindFirstFile(sFilename, &fd);

  if (fh != INVALID_HANDLE_VALUE) {
    FindClose(fh);

    CSVNDetails svnDetails;
    TSVNCacheResponse returnedStatus = {NULL};
    svnDetails.GetStatusFromRemoteCache(sFilename, &returnedStatus, true);

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

      default:
        return ft_nosuchfield;
    }
  }
  else {
    return ft_fileerror;
  }

  return fields[fieldIndex].type;
}

void __stdcall ContentSetDefaultParams(ContentDefaultParamStruct* dps)
{
  return;
}

void __stdcall ContentStopGetValue(char* fileName)
{
  return;
}
