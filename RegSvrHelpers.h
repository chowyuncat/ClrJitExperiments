#ifndef __UTILS_H__
#define __UTILS_H__

// this function will register a component.
HRESULT RegisterServer( HMODULE hModule, REFCLSID clsid, const char* szFriendlyName, const char* szVerIndProgID, const char* szProgID, const char* szThreadingModel );

// This function will unregister a component.
HRESULT UnregisterServer( REFCLSID clsid, const char* szVerIndProgID, const char* szProgID );

#endif //__UTILS_H__