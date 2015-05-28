#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include "RegSvrHelpers.h"

#include <objbase.h> // IUnknown

#include <Cor.h>
#include <CorProf.h>

class MyProfiler;
extern MyProfiler *g_pThisMyProfiler;
extern LONG g_cServerLocks;
inline LONG LockModule()	{ return ::InterlockedIncrement(&g_cServerLocks); }
inline LONG UnlockModule()	{ return ::InterlockedDecrement(&g_cServerLocks); }

class MyProfilerFactory : public IClassFactory
{
public:
    HRESULT __stdcall QueryInterface(const IID& iid, void** ppv);

    ULONG __stdcall AddRef();

    ULONG __stdcall Release();

    HRESULT __stdcall CreateInstance(IUnknown* pUnknownOuter,
                                     const IID& iid,
                                     void** ppv );

    HRESULT __stdcall LockServer(BOOL bLock)
    {
        if (bLock)
        {
            LockModule();
        }
        else
        {
            UnlockModule();
        }
        return S_OK;
    }

    MyProfilerFactory()
        : m_ref_count(1) // TODO: why not zero?
    {
    }

    ~MyProfilerFactory()
    {
        // do nothing
    }

private:
    ULONG m_ref_count;
};

