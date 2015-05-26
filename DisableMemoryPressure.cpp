#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include "DisableMemoryPressure.hpp"
#include "RegSvrHelpers.h"

#include <objbase.h> // IUnknown

#include <Cor.h>
#include <CorProf.h>

void __declspec(naked) FunctionEnterNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func, COR_PRF_FUNCTION_ARGUMENT_INFO *argumentInfo)
{

}

void __declspec(naked) FunctionLeaveNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func, COR_PRF_FUNCTION_ARGUMENT_RANGE *retvalRange)
{

}

void __declspec(naked) FunctionTailcallNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func)
{

}

MyProfiler *g_pThisMyProfiler = NULL;
LONG g_cServerLocks = 0; // to count server locks


class MyProfiler : public ICorProfilerCallback2
{
public:
    MyProfiler()
        : m_ref_count(1) // TODO: why not zero?
    {
        OutputDebugString(L"MyProfiler::MyProfiler");
        g_pThisMyProfiler = this;
    }

    ~MyProfiler()
    {
        OutputDebugString(L"MyProfiler::~MyProfiler");
        g_pThisMyProfiler = NULL;
    }

    //
    // IUnknown
    //
    STDMETHOD_(HRESULT, QueryInterface)(REFIID riid, void** ppv)
    {
        OutputDebugString(L"MyProfiler::QueryInterface");

        if ( riid == IID_IUnknown )
            *ppv = static_cast<IUnknown*>( this );
        else if ( riid == IID_ICorProfilerCallback )
            *ppv = static_cast<ICorProfilerCallback*>( this );
        else
        {
            *ppv = NULL;
            return E_NOINTERFACE;
        }
        reinterpret_cast<IUnknown*>(*ppv)->AddRef();
        return S_OK;
    }

    ULONG __stdcall AddRef()
    {
        return ::InterlockedIncrement(&m_ref_count);
    }

    ULONG __stdcall Release()
    {
        if ( ::InterlockedDecrement(&m_ref_count) == 0 )
        {
            delete this;
            return 0;
        }
        return m_ref_count;
    }

    //
    // ICorProfilerCallback
    //
    HRESULT __stdcall Initialize(IUnknown *pICorProfilerInfoUnk)
    {
        OutputDebugString(__FUNCTIONW__);

        ICorProfilerInfo *pCorProfilerInfo = NULL;
        HRESULT hr = pICorProfilerInfoUnk->QueryInterface(
            IID_ICorProfilerInfo,
            (LPVOID* )&pCorProfilerInfo );

        if ( FAILED(hr) )
        {
            OutputDebugString( L"pICorProfilerInfoUnk->QueryInterface FAILED\r\n" );
            return E_INVALIDARG;
        }

        // set the event mask
        DWORD eventMask = (DWORD)(COR_PRF_MONITOR_ENTERLEAVE);
        pCorProfilerInfo->SetEventMask(eventMask);

        // set the enter, leave and tailcall hooks
        hr = pCorProfilerInfo->SetEnterLeaveFunctionHooks(
            (FunctionEnter*)&FunctionEnterNaked,
            (FunctionLeave*)&FunctionLeaveNaked,
            (FunctionTailcall*)&FunctionTailcallNaked);

         if ( FAILED(hr) )
        {
            OutputDebugString( L"pICorProfilerInf->SetEnterLeaveFunctionHooks FAILED\r\n" );
            return hr;
        }

#if 0
        ICorProfilerInfo2 *pICorProfilerInfo2 = NULL;
        hr = pICorProfilerInfoUnk->QueryInterface(
            IID_ICorProfilerInfo2,
            (LPVOID*)&pICorProfilerInfo2);
        if (FAILED(hr))
        {
           // we still want to work if this call fails, might be an older .NET version
           pICorProfilerInfo2.p = NULL;
        }
#endif

        return S_OK;
    }

    HRESULT __stdcall Shutdown(void)
    {
        OutputDebugString(__FUNCTIONW__);
        return S_OK;
    }


    // TODO: Implement the unneeded stubs in a separate class
    virtual HRESULT STDMETHODCALLTYPE AppDomainCreationStarted(
        /* [in] */ AppDomainID appDomainId)
    {
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE AppDomainCreationFinished(
        /* [in] */ AppDomainID appDomainId,
        /* [in] */ HRESULT hrStatus)
    {
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE AppDomainShutdownStarted(
            /* [in] */ AppDomainID appDomainId)
    {
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE AppDomainShutdownFinished(
            /* [in] */ AppDomainID appDomainId,
            /* [in] */ HRESULT hrStatus)
    {
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE AssemblyLoadStarted(
        /* [in] */ AssemblyID assemblyId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE AssemblyLoadFinished(
        /* [in] */ AssemblyID assemblyId,
        /* [in] */ HRESULT hrStatus) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE AssemblyUnloadStarted(
        /* [in] */ AssemblyID assemblyId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE AssemblyUnloadFinished(
        /* [in] */ AssemblyID assemblyId,
        /* [in] */ HRESULT hrStatus) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ModuleLoadStarted(
        /* [in] */ ModuleID moduleId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ModuleLoadFinished(
        /* [in] */ ModuleID moduleId,
        /* [in] */ HRESULT hrStatus) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ModuleUnloadStarted(
        /* [in] */ ModuleID moduleId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ModuleUnloadFinished(
        /* [in] */ ModuleID moduleId,
        /* [in] */ HRESULT hrStatus) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ModuleAttachedToAssembly(
        /* [in] */ ModuleID moduleId,
        /* [in] */ AssemblyID AssemblyId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ClassLoadStarted(
        /* [in] */ ClassID classId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ClassLoadFinished(
        /* [in] */ ClassID classId,
        /* [in] */ HRESULT hrStatus) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ClassUnloadStarted(
        /* [in] */ ClassID classId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ClassUnloadFinished(
        /* [in] */ ClassID classId,
        /* [in] */ HRESULT hrStatus) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE FunctionUnloadStarted(
        /* [in] */ FunctionID functionId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE JITCompilationStarted(
        /* [in] */ FunctionID functionId,
        /* [in] */ BOOL fIsSafeToBlock) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE JITCompilationFinished(
        /* [in] */ FunctionID functionId,
        /* [in] */ HRESULT hrStatus,
        /* [in] */ BOOL fIsSafeToBlock) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE JITCachedFunctionSearchStarted(
        /* [in] */ FunctionID functionId,
        /* [out] */ BOOL *pbUseCachedFunction) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE JITCachedFunctionSearchFinished(
        /* [in] */ FunctionID functionId,
        /* [in] */ COR_PRF_JIT_CACHE result) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE JITFunctionPitched(
        /* [in] */ FunctionID functionId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE JITInlining(
        /* [in] */ FunctionID callerId,
        /* [in] */ FunctionID calleeId,
        /* [out] */ BOOL *pfShouldInline) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ThreadCreated(
        /* [in] */ ThreadID threadId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ThreadDestroyed(
        /* [in] */ ThreadID threadId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ThreadAssignedToOSThread(
        /* [in] */ ThreadID managedThreadId,
        /* [in] */ DWORD osThreadId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RemotingClientInvocationStarted( void) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RemotingClientSendingMessage(
        /* [in] */ GUID *pCookie,
        /* [in] */ BOOL fIsAsync) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RemotingClientReceivingReply(
        /* [in] */ GUID *pCookie,
        /* [in] */ BOOL fIsAsync) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RemotingClientInvocationFinished( void) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RemotingServerReceivingMessage(
        /* [in] */ GUID *pCookie,
        /* [in] */ BOOL fIsAsync) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RemotingServerInvocationStarted( void) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RemotingServerInvocationReturned( void) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RemotingServerSendingReply(
        /* [in] */ GUID *pCookie,
        /* [in] */ BOOL fIsAsync) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE UnmanagedToManagedTransition(
        /* [in] */ FunctionID functionId,
        /* [in] */ COR_PRF_TRANSITION_REASON reason) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ManagedToUnmanagedTransition(
        /* [in] */ FunctionID functionId,
        /* [in] */ COR_PRF_TRANSITION_REASON reason) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RuntimeSuspendStarted(
        /* [in] */ COR_PRF_SUSPEND_REASON suspendReason) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RuntimeSuspendFinished( void) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RuntimeSuspendAborted( void) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RuntimeResumeStarted( void) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RuntimeResumeFinished( void) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RuntimeThreadSuspended(
        /* [in] */ ThreadID threadId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RuntimeThreadResumed(
        /* [in] */ ThreadID threadId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE MovedReferences(
        /* [in] */ ULONG cMovedObjectIDRanges,
        /* [size_is][in] */ ObjectID oldObjectIDRangeStart[  ],
        /* [size_is][in] */ ObjectID newObjectIDRangeStart[  ],
        /* [size_is][in] */ ULONG cObjectIDRangeLength[  ]) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ObjectAllocated(
        /* [in] */ ObjectID objectId,
        /* [in] */ ClassID classId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ObjectsAllocatedByClass(
        /* [in] */ ULONG cClassCount,
        /* [size_is][in] */ ClassID classIds[  ],
        /* [size_is][in] */ ULONG cObjects[  ]) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ObjectReferences(
        /* [in] */ ObjectID objectId,
        /* [in] */ ClassID classId,
        /* [in] */ ULONG cObjectRefs,
        /* [size_is][in] */ ObjectID objectRefIds[  ]) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE RootReferences(
        /* [in] */ ULONG cRootRefs,
        /* [size_is][in] */ ObjectID rootRefIds[  ]) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionThrown(
        /* [in] */ ObjectID thrownObjectId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionSearchFunctionEnter(
        /* [in] */ FunctionID functionId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionSearchFunctionLeave( void) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionSearchFilterEnter(
        /* [in] */ FunctionID functionId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionSearchFilterLeave( void) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionSearchCatcherFound(
        /* [in] */ FunctionID functionId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionOSHandlerEnter(
        /* [in] */ UINT_PTR __unused) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionOSHandlerLeave(
        /* [in] */ UINT_PTR __unused) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionUnwindFunctionEnter(
        /* [in] */ FunctionID functionId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionUnwindFunctionLeave( void) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionUnwindFinallyEnter(
        /* [in] */ FunctionID functionId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionUnwindFinallyLeave( void) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionCatcherEnter(
        /* [in] */ FunctionID functionId,
        /* [in] */ ObjectID objectId) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionCatcherLeave( void) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE COMClassicVTableCreated(
        /* [in] */ ClassID wrappedClassId,
        /* [in] */ REFGUID implementedIID,
        /* [in] */ void *pVTable,
        /* [in] */ ULONG cSlots) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE COMClassicVTableDestroyed(
        /* [in] */ ClassID wrappedClassId,
        /* [in] */ REFGUID implementedIID,
        /* [in] */ void *pVTable) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionCLRCatcherFound( void) { return S_OK; }

    virtual HRESULT STDMETHODCALLTYPE ExceptionCLRCatcherExecute( void) { return S_OK; }





     virtual HRESULT STDMETHODCALLTYPE ThreadNameChanged( 
            /* [in] */ ThreadID threadId,
            /* [in] */ ULONG cchName,
            /* [annotation][in] */ 
            _In_reads_opt_(cchName)  WCHAR name[  ]) { return S_OK; }
        
        virtual HRESULT STDMETHODCALLTYPE GarbageCollectionStarted( 
            /* [in] */ int cGenerations,
            /* [size_is][in] */ BOOL generationCollected[  ],
            /* [in] */ COR_PRF_GC_REASON reason) { return S_OK; }
        
        virtual HRESULT STDMETHODCALLTYPE SurvivingReferences( 
            /* [in] */ ULONG cSurvivingObjectIDRanges,
            /* [size_is][in] */ ObjectID objectIDRangeStart[  ],
            /* [size_is][in] */ ULONG cObjectIDRangeLength[  ]) { return S_OK; }
        
        virtual HRESULT STDMETHODCALLTYPE GarbageCollectionFinished( void) { return S_OK; }
        
        virtual HRESULT STDMETHODCALLTYPE FinalizeableObjectQueued( 
            /* [in] */ DWORD finalizerFlags,
            /* [in] */ ObjectID objectID) { return S_OK; }
        
        virtual HRESULT STDMETHODCALLTYPE RootReferences2( 
            /* [in] */ ULONG cRootRefs,
            /* [size_is][in] */ ObjectID rootRefIds[  ],
            /* [size_is][in] */ COR_PRF_GC_ROOT_KIND rootKinds[  ],
            /* [size_is][in] */ COR_PRF_GC_ROOT_FLAGS rootFlags[  ],
            /* [size_is][in] */ UINT_PTR rootIds[  ]) { return S_OK; }
        
        virtual HRESULT STDMETHODCALLTYPE HandleCreated( 
            /* [in] */ GCHandleID handleId,
            /* [in] */ ObjectID initialObjectId) { return S_OK; }
        
        virtual HRESULT STDMETHODCALLTYPE HandleDestroyed( 
            /* [in] */ GCHandleID handleId) { return S_OK; }
        


private:
    ULONG m_ref_count;
};


HRESULT __stdcall MyProfilerFactory::QueryInterface(const IID& iid, void** ppv)
{
    OutputDebugString(L"MyProfilerFactory::QueryInterface");

    if (iid == IID_IUnknown || iid == IID_IClassFactory)
    {
        *ppv = static_cast<IClassFactory*>( this );
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    reinterpret_cast<IUnknown*>(*ppv)->AddRef();
    return S_OK;
}

ULONG __stdcall MyProfilerFactory::AddRef()
{
    return ::InterlockedIncrement(&m_ref_count);
}

ULONG __stdcall MyProfilerFactory::Release()
{
    if ( ::InterlockedDecrement(&m_ref_count) == 0 )
    {
        delete this;
        return 0;
    }
    return m_ref_count;
}

// IClassFactory implementation
HRESULT __stdcall MyProfilerFactory::CreateInstance(IUnknown* pUnknownOuter,
                                    const IID& iid,
                                    void** ppv )
{
    OutputDebugString(L"MyProfilerFactory::CreateInstance");

    // Cannot aggregate.
    if ( pUnknownOuter != NULL )
    {
        return CLASS_E_NOAGGREGATION;
    }

    MyProfiler* pMyProfiler = new MyProfiler();
    if (pMyProfiler == NULL) // TODO: static assert on disabled exceptions
    {
        return E_OUTOFMEMORY;
    }

    // Get the requested interface.
    HRESULT hr = pMyProfiler->QueryInterface( iid, ppv );

    // Release the IUnknown pointer.
    pMyProfiler->Release() ;

    return hr ;
}
