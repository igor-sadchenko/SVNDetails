// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once
// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#ifndef WINVER                  // Specifies that the minimum required platform is Windows Vista.
#define WINVER 0x0600           // Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINNT            // Specifies that the minimum required platform is Windows Vista.
#define _WIN32_WINNT 0x0600     // Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINDOWS          // Specifies that the minimum required platform is Windows 98.
#define _WIN32_WINDOWS 0x0410   // Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef _WIN32_IE               // Specifies that the minimum required platform is Internet Explorer 7.0.
#define _WIN32_IE 0x0700        // Change this to the appropriate value to target other versions of IE.
#endif

#define ISOLATION_AWARE_ENABLED 1

#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Wspiapi.h>
#include <windows.h>

#include <commctrl.h>
#include <Shlobj.h>
#include <Shlwapi.h>
#include <tchar.h>
#include <wininet.h>
#include <Aclapi.h>

#include <atlbase.h>
#include <atlcoll.h>
#include <atlexcept.h>
#include <atlstr.h>

#pragma warning(push)
#pragma warning(disable: 4702)  // Unreachable code warnings in xtree
#include <string>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
#pragma warning(pop)

#ifndef WIN64
  // libapr_tsvn32
  #undef apr_array_make
  #define apr_array_make _apr_array_make

  #undef apr_array_pop
  #define apr_array_pop _apr_array_pop

  #undef apr_array_push
  #define apr_array_push _apr_array_push

  #undef apr_hash_count
  #define apr_hash_count _apr_hash_count

  #undef apr_hash_first
  #define apr_hash_first _apr_hash_first

  #undef apr_hash_get
  #define apr_hash_get _apr_hash_get

  #undef apr_hash_make
  #define apr_hash_make _apr_hash_make

  #undef apr_hash_next
  #define apr_hash_next _apr_hash_next

  #undef apr_hash_set
  #define apr_hash_set _apr_hash_set

  #undef apr_hash_this
  #define apr_hash_this _apr_hash_this

  #undef apr_initialize
  #define apr_initialize _apr_initialize

  #undef apr_pool_clear
  #define apr_pool_clear _apr_pool_clear

  #undef apr_pool_destroy
  #define apr_pool_destroy _apr_pool_destroy

  #undef apr_pstrdup
  #define apr_pstrdup _apr_pstrdup

  #undef apr_pstrmemdup
  #define apr_pstrmemdup _apr_pstrmemdup

  #undef apr_strerror
  #define apr_strerror _apr_strerror

  // libaprutil_tsvn32
  #undef apr_uri_parse
  #define apr_uri_parse _apr_uri_parse

  #undef apr_uri_unparse
  #define apr_uri_unparse _apr_uri_unparse
#endif

#pragma warning(push)
#include "apr_general.h"
#include "svn_pools.h"
#include "svn_client.h"
#include "svn_path.h"
#include "svn_wc.h"
#include "svn_utf.h"
#include "svn_config.h"
#include "svn_subst.h"
#include "svn_props.h"
#pragma warning(pop)

#include "SysInfo.h"
#include "DebugOutput.h"

#define CSTRING_AVAILABLE
