#include <Windows.h>
#include "RegSvrHelpers.h"
#include "DisableMemoryPressure.hpp"
#include <tchar.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


// {8062FF69-AB86-4183-AC28-9B83DE4AA6C1}
static const GUID g_CLSID_MyProfiler = { 0x8062ff69, 0xab86, 0x4183, { 0xac, 0x28, 0x9b, 0x83, 0xde, 0x4a, 0xa6, 0xc1 } };

static const wchar_t g_lpszDLLName[] = L"DotNetCatZMyProfiler";

// Friendly name of component
static const char g_szFriendlyName[] = "DotNet CatZ My Profiler";

// Version-independent ProgID
static const char g_szVerIndProgID[] = "DotNet.CatZ.MyProfiler";

// ProgID prefix
static const char g_szProgID[] = "DotNet.CatZ.MyProfiler.1";

// Thread model
static const char g_szThreadingModel[] = "Both";

static HANDLE g_hMutexMethod;

static DWORD g_dwProcessID;

// GetModuleInst
HINSTANCE g_hInst = NULL; // gobal instance handle
static HINSTANCE GetModuleInst() { return g_hInst; }

STDAPI DllCanUnloadNow()
{
    return ( g_cServerLocks == 0) ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer()
{
	return RegisterServer( GetModuleInst(), g_CLSID_MyProfiler, g_szFriendlyName, g_szVerIndProgID, g_szProgID, g_szThreadingModel );
}

STDAPI DllUnregisterServer()
{
	return UnregisterServer( g_CLSID_MyProfiler, g_szVerIndProgID, g_szProgID );
}

STDAPI DllGetClassObject( REFCLSID rclsid, REFIID riid, void** ppv )
{
    OutputDebugString(L"ClrJitExperiments DllGetClassObject");
	if (rclsid != g_CLSID_MyProfiler)
	{
		return CLASS_E_CLASSNOTAVAILABLE;
	}

	// Create class factory.
	MyProfilerFactory* pFactory = new MyProfilerFactory;
    // NB: Reference count set to 1 in constructor

	if (pFactory == NULL)
	{
		return E_OUTOFMEMORY ;
	}

	// Get requested interface.
	HRESULT hr = pFactory->QueryInterface( riid, ppv );
	pFactory->Release();

	return hr ;
}
//
//STDAPI DllRegisterServer();
//STDAPI DllUnregisterServer();
//STDAPI DllGetClassObject( REFCLSID rclsid, REFIID riid,	void** ppv );
//STDAPI DllCanUnloadNow();
// COM stuff <<

TCHAR g_szObjectName[ MAX_PATH ];
LPTSTR GenerateUniqueName( const LPCTSTR szObjectName, DWORD dwProcessID)
{
   //_stprintf( g_szObjectName, _T("_%s_%u_"), szObjectName, g_dwProcessID /*::GetCurrentProcessId()*/ );

   // SAFER:
   _sntprintf(g_szObjectName, ( sizeof (g_szObjectName) / sizeof(g_szObjectName[0]) ), _T("_%s_%u"), szObjectName, dwProcessID );
   return g_szObjectName;
}

// main entry
BOOL APIENTRY DllMain( HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved )
{
	// save off the instance handle for later use
	switch ( dwReason )
	{
		case DLL_PROCESS_ATTACH:
            OutputDebugString(L"ClrJitExperiments DllMain DLL_PROCESS_ATTACH");
			DisableThreadLibraryCalls( hInstance );

			// init instance variables
			g_hInst = hInstance;
			// procID will be used to create process-unique kernel objects
			g_dwProcessID = ::GetCurrentProcessId();

			// init mutex, since it's a kernel-object make it unique
			g_hMutexMethod = ::CreateMutex( NULL, FALSE /* not initially own */, GenerateUniqueName(g_lpszDLLName, g_dwProcessID) );
			break;

		case DLL_PROCESS_DETACH:
            OutputDebugString(L"ClrJitExperiments DllMain DLL_PROCESS_DETACH");
			::CloseHandle( g_hMutexMethod );

			// lpReserved == NULL means that we called FreeLibrary()
			// in that case do nothing
			if (lpReserved != NULL && g_pThisMyProfiler != NULL)
			{
                // TODO:
				// g_pThisMyProfiler->DllDetachShutdown();
			}

			break;

		default:
            OutputDebugString(L"ClrJitExperiments DllMain **UNKNOWN**");
			break;
	}

	return TRUE;
}
