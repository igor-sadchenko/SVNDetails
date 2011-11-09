#include "stdafx.h"

// SVN / Tortoise
#include "CacheInterface.h"
#include "ShellCache.h"
#include "SVNFolderStatus.h"
#include "RemoteCacheLink.h"

// TC
#include "contentplug.h"

#ifdef _MANAGED
#pragma managed(push, off)
#endif

HINSTANCE g_hmodThisDll = NULL;

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
  switch (dwReason) {
    case DLL_PROCESS_ATTACH:
      g_hmodThisDll = hInstance;
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



//////////////////////////////////////////////////////////////////////////


ShellCache g_ShellCache;
bool g_unversionedovlloaded = false;

CAtlMap<INT8, const char*> g_statusMap;
CRemoteCacheLink* g_remoteCacheLink = NULL;
SVNFolderStatus* g_cachedStatus = NULL;


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

const char* GetSVNStatus(INT8 status)
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
  fileName[0] = toupper(fileName[0]);
  const ATL::CString sFilename(fileName);

  if (flags & CONTENT_DELAYIFSLOW) {
    strlcpy((char*)fieldValue, "\0", maxlen-1);
    return ft_delayed;
  }

  WIN32_FIND_DATA fd = {NULL};
  HANDLE fh = FindFirstFile(sFilename, &fd);

  if (fh != INVALID_HANDLE_VALUE) {
    FindClose(fh);

    if (g_remoteCacheLink && g_cachedStatus) {
      CTSVNPath svnPath(fileName);
      TSVNCacheResponse returnedStatus = {NULL};
      if (g_remoteCacheLink->GetStatusFromRemoteCache(svnPath, &returnedStatus, true)) {
        const FileStatusCacheEntry* status = g_cachedStatus->GetFullStatus(svnPath, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY), TRUE);

        switch (fieldIndex) {
          case 0: // "SVN Author"
            strlcpy((char*)fieldValue, status->author, maxlen-1);
            break;

          case 1: // "SVN Lock owner"
            strlcpy((char*)fieldValue, status->owner, maxlen-1);
            break;

          case 2: // "SVN Prop Status"
            strlcpy((char*)fieldValue, GetSVNStatus(returnedStatus.m_propStatus), maxlen-1);
            break;

          case 3: // "SVN Revision"
            *(int*)fieldValue = (int)returnedStatus.m_cmt_rev;
            break;

          case 4: // "SVN Text Status"
            strlcpy((char*)fieldValue, GetSVNStatus(returnedStatus.m_textStatus), maxlen-1);
            break;

          case 5: // "SVN Short URL"
            strlcpy((char*)fieldValue, status->url, maxlen-1);
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

  g_remoteCacheLink = new CRemoteCacheLink();
  g_cachedStatus = new SVNFolderStatus();

  return;
}

void __stdcall ContentStopGetValue(char* fileName)
{
  return;
}

void __stdcall ContentPluginUnloading(void)
{
  delete g_cachedStatus;
  g_cachedStatus = NULL;

  delete g_remoteCacheLink;
  g_remoteCacheLink = NULL;

  ClearStatusMap();

  return;
}
