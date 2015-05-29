#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include "DisableMemoryPressure.hpp"
#include "ILStream.h"
#include "RegSvrHelpers.h"

#include <objbase.h> // IUnknown
#include "CorProfilerCallbackStubImpl.hpp"

#include <stdio.h> // _snprintf

#include <atlcomcli.h> // CComPtr

#include <map>

// TODO: Unify these macros
#define _DIMOF( Array ) (sizeof(Array) / sizeof(Array[0]))

static void DebugPrintfA(const char *format, ...)
{
    const size_t kBufferSize = 1023;
    char buffer[kBufferSize + 1] = { 0 };
    va_list args;
    va_start(args, format);
    _vsnprintf(buffer, kBufferSize, format, args);
    va_end(args);

    OutputDebugStringA(buffer);
}

static void DebugPrintfW(const wchar_t *format, ...)
{
    const size_t kBufferSize = 1023;
    wchar_t buffer[kBufferSize + 1] = { 0 };
    va_list args;
    va_start(args, format);
    _vsnwprintf(buffer, kBufferSize, format, args);
    va_end(args);

    OutputDebugStringW(buffer);
}

// TODO: HACK:
#define NAME_BUFFER_SIZE 1024

#if 0
void __declspec(naked) FunctionEnterNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func, COR_PRF_FUNCTION_ARGUMENT_INFO *argumentInfo)
{

}

void __declspec(naked) FunctionLeaveNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func, COR_PRF_FUNCTION_ARGUMENT_RANGE *retvalRange)
{

}

void __declspec(naked) FunctionTailcallNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func)
{

}
#endif


void __stdcall FunctionEnterGlobal(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO frameInfo, COR_PRF_FUNCTION_ARGUMENT_INFO *argInfo);

// this function simply forwards the FunctionLeave call the global profiler object
void __stdcall FunctionTailcallGlobal(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO frameInfo)
{
    //if (g_pICorProfilerCallback != NULL)
    //    g_pICorProfilerCallback->Tailcall(functionID,clientData,frameInfo);
}

// this function simply forwards the FunctionLeave call the global profiler object
void __stdcall FunctionLeaveGlobal(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO frameInfo, COR_PRF_FUNCTION_ARGUMENT_RANGE *retvalRange)
{
    // make sure the global reference to our profiler is valid
    //if (g_pICorProfilerCallback != NULL)
    //    g_pICorProfilerCallback->Leave(functionID,clientData,frameInfo,retvalRange);
}

#ifdef _M_IX86
// this function is called by the CLR when a function has been entered
void _declspec(naked) FunctionEnterNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func, COR_PRF_FUNCTION_ARGUMENT_INFO *argumentInfo)
{
    __asm
    {
        push    ebp                 // Create a standard frame
        mov     ebp,esp
        pushad                      // Preserve all registers

        mov     eax,[ebp+0x14]      // argumentInfo
        push    eax
        mov     ecx,[ebp+0x10]      // func
        push    ecx
        mov     edx,[ebp+0x0C]      // clientData
        push    edx
        mov     eax,[ebp+0x08]      // functionID
        push    eax
        call    FunctionEnterGlobal

        popad                       // Restore all registers
        pop     ebp                 // Restore EBP
        ret     16
    }
}



// this function is called by the CLR when a function is exiting
void _declspec(naked) FunctionLeaveNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func, COR_PRF_FUNCTION_ARGUMENT_RANGE *retvalRange)
{
    __asm
    {
        push    ebp                 // Create a standard frame
        mov     ebp,esp
        pushad                      // Preserve all registers

        mov     eax,[ebp+0x14]      // argumentInfo
        push    eax
        mov     ecx,[ebp+0x10]      // func
        push    ecx
        mov     edx,[ebp+0x0C]      // clientData
        push    edx
        mov     eax,[ebp+0x08]      // functionID
        push    eax
        call    FunctionLeaveGlobal

        popad                       // Restore all registers
        pop     ebp                 // Restore EBP
        ret     16
    }
}


// this function is called by the CLR when a tailcall occurs.  A tailcall occurs when the 
// last action of a method is a call to another method.
void _declspec(naked) FunctionTailcallNaked(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO func)
{
    __asm
    {
        push    ebp                 // Create a standard frame
        mov     ebp,esp
        pushad                      // Preserve all registers

        mov     eax,[ebp+0x14]      // argumentInfo
        push    eax
        mov     ecx,[ebp+0x10]      // func
        push    ecx
        mov     edx,[ebp+0x0C]      // clientData
        push    edx
        mov     eax,[ebp+0x08]      // functionID
        push    eax
        call    FunctionTailcallGlobal

        popad                       // Restore all registers
        pop     ebp                 // Restore EBP
        ret     16
    }
}
#endif // _M_IX86


// this function is called by the CLR when a function has been mapped to an ID
// static UINT_PTR __stdcall FunctionMapper(FunctionID functionID, BOOL *pbHookFunction);
static FunctionIDMapper FunctionMapper;

// creates the fully scoped name of the method in the provided buffer
static HRESULT GetFullMethodName(ICorProfilerInfo2 *pProfilerInfo, FunctionID functionID, LPWSTR wszMethod, int cMethod)
{
    IMetaDataImport* pIMetaDataImport = 0;
    HRESULT hr = S_OK;
    mdToken funcToken = 0;
    WCHAR szFunction[NAME_BUFFER_SIZE];
    WCHAR szClass[NAME_BUFFER_SIZE];

    // get the token for the function which we will use to get its name
    hr = pProfilerInfo->GetTokenAndMetaDataFromFunction(functionID, IID_IMetaDataImport, (LPUNKNOWN *) &pIMetaDataImport, &funcToken);
    if(SUCCEEDED(hr))
    {
        mdTypeDef classTypeDef;
        ULONG cchFunction;
        ULONG cchClass;

        // retrieve the function properties based on the token
        hr = pIMetaDataImport->GetMethodProps(funcToken, &classTypeDef, szFunction, NAME_BUFFER_SIZE, &cchFunction, 0, 0, 0, 0, 0);
        if (SUCCEEDED(hr))
        {
            // get the function name
            hr = pIMetaDataImport->GetTypeDefProps(classTypeDef, szClass, NAME_BUFFER_SIZE, &cchClass, 0, 0);
            if (SUCCEEDED(hr))
            {
                // create the fully qualified name
                _snwprintf_s(wszMethod,cMethod,cMethod,L"%s.%s",szClass,szFunction);
                // DebugPrintfW(L"GetFullMethodName: %s", wszMethod);
            }
        }


        // release our reference to the metadata
        pIMetaDataImport->Release();
    }
    return hr;
}


MyProfiler *g_pThisMyProfiler = NULL;
LONG g_cServerLocks = 0; // to count server locks

class MyProfiler : public CorProfilerCallbackStubImpl2
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

    const wchar_t * MapFunction(FunctionID functionID)
    {
        // DebugPrintfA("MapFunction: %d", functionID);
        const FunctionIdMapIterator i = m_functionIdToNameMap.find(functionID);
        if (i != m_functionIdToNameMap.end())
        {
            return i->second.c_str();
        }

        WCHAR szMethodName[NAME_BUFFER_SIZE + 1] = { 0 };
        // const WCHAR* p = NULL; //TODO WTF?
        // USES_CONVERSION;

        // get the method name
        HRESULT hr = GetFullMethodName(m_pICorProfilerInfo2, functionID, szMethodName, NAME_BUFFER_SIZE);
        if (FAILED(hr))
        {
            // if we couldn't get the function name, then log it
            char msg[1024] = {0};
            _snprintf(msg, 1023, "Unable to find the name for %i\r\n", functionID);
            OutputDebugStringA(msg);
            return NULL;
        }

        // DebugPrintfW(L"  MapFunction: %d => %s", functionID, szMethodName);

        m_functionIdToNameMap[functionID] = szMethodName;

        // TODO: Free szMethodName

        // OutputDebugStringW(szMethodName);

        return m_functionIdToNameMap[functionID].c_str();
    }

    void FunctionEnter(FunctionID functionID,  UINT_PTR clientData, COR_PRF_FRAME_INFO frameInfo, COR_PRF_FUNCTION_ARGUMENT_INFO *argInfo)
    {
        const FunctionIdMapIterator i = m_functionIdToNameMap.find(functionID);
        DebugPrintfW(L"Enter: %s", i->second.c_str());
    }

    const wchar_t *GetFunctionName(FunctionID functionID)
    {
        const FunctionIdMapIterator i = m_functionIdToNameMap.find(functionID);

        if (i == m_functionIdToNameMap.end())
        {
            return MapFunction(functionID);
        }
        else
        {
            return i->second.c_str();
        }
    }

    const wchar_t *GetFunctionNameFromMemberReference(ModuleID moduleID, mdToken tkMemberRef)
    {
        // DebugPrintfA("GetFunctionNameFromToken %08X ? ", (int)tkMemberRef);
        const MemberRefMap &methods = m_moduleMethodRefs[moduleID];
        const MemberRefMapIterator i = methods.find(tkMemberRef);

        if (i == methods.end())
        {
            return NULL;
        }
        else
        {
            return i->second.c_str();
        }
    }

    //
    //
    // IUnknown
    //
    //

    STDMETHOD_(HRESULT, QueryInterface)(REFIID riid, void** ppv)
    {
        OutputDebugString(L"MyProfiler::QueryInterface");

        if (riid == IID_IUnknown)
        {
            OutputDebugString(L"MyProfiler::QueryInterface(IUnknown)");
            *ppv = static_cast<IUnknown*>( this );
        }
        else if (riid == IID_ICorProfilerCallback3)
        {
            OutputDebugString(L"(Not implemented) MyProfiler::QueryInterface(IID_ICorProfilerCallback3)");
            // This means that a CLR V4 specific environment variable must be set.
             *ppv = NULL;
            return E_NOINTERFACE;
        }
        else if (riid == IID_ICorProfilerCallback2)
        {
            OutputDebugString(L"MyProfiler::QueryInterface(IID_ICorProfilerCallback2)");
            *ppv = static_cast<ICorProfilerCallback2*>( this );
        }
        else if (riid == IID_ICorProfilerCallback)
        {
            OutputDebugString(L"MyProfiler::QueryInterface(IID_ICorProfilerCallback)");
            *ppv = static_cast<ICorProfilerCallback*>( this );
        }
        else
        {
            OutputDebugString(L"MyProfiler::QueryInterface(IID_????????)");
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
    // HRESULT __stdcall Initialize(IUnknown *pICorProfilerInfoUnk)
    STDMETHOD(Initialize)(IUnknown *pICorProfilerInfoUnk)
    {
        OutputDebugString(L"MyProfiler::Initialize (ICorProfilerCallback)");

        m_instSet = CreateInstructionSet();

        ICorProfilerInfo2 *pCorProfilerInfo = NULL;
        HRESULT hr = pICorProfilerInfoUnk->QueryInterface(
            IID_ICorProfilerInfo2,
            (LPVOID* )&pCorProfilerInfo );

        if ( FAILED(hr) )
        {
            OutputDebugString( L"pICorProfilerInfoUnk->QueryInterface FAILED\r\n" );
            return E_INVALIDARG;
        }

        // set the event mask
        DWORD eventMask = COR_PRF_MONITOR_NONE;
        // eventMask |= (DWORD)COR_PRF_MONITOR_ENTERLEAVE;
        eventMask |= (DWORD)COR_PRF_MONITOR_JIT_COMPILATION;
        eventMask |= (DWORD)COR_PRF_MONITOR_MODULE_LOADS;
        pCorProfilerInfo->SetEventMask(eventMask);

#if 0
        // set the enter, leave and tailcall hooks
        hr = pCorProfilerInfo->SetEnterLeaveFunctionHooks2(
            (FunctionEnter2*)&FunctionEnterNaked,
            (FunctionLeave2*)&FunctionLeaveNaked,
            (FunctionTailcall2*)&FunctionTailcallNaked);
#endif
        if ( FAILED(hr) )
        {
            OutputDebugString( L"pICorProfilerInf->SetEnterLeaveFunctionHooks FAILED\r\n" );
            return hr;
        }

        m_pICorProfilerInfo2 = pCorProfilerInfo;

        // TODO: Use SetFunctionMapper2 to pass userdata
        hr = pCorProfilerInfo->SetFunctionIDMapper(FunctionMapper);

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
        DebugPrintfA("%s", __FUNCTION__);
        return S_OK;
    }

#if 1
    virtual HRESULT STDMETHODCALLTYPE ModuleLoadFinished(
        /* [in] */ ModuleID moduleID,
        /* [in] */ HRESULT hrStatus)
    {

        HRESULT hr;
        LPCBYTE pbBaseLoadAddr;
        WCHAR wszModuleName[300];
        ULONG cchNameIn = _DIMOF(wszModuleName);
        ULONG cchNameOut;
        AssemblyID assemblyID;

        hr = m_pICorProfilerInfo2->GetModuleInfo(
            moduleID,
            &pbBaseLoadAddr,
            cchNameIn,
            &cchNameOut,
            wszModuleName,
            &assemblyID);
        if (FAILED(hr))
        {
            return hr;
        }

        DebugPrintfW(L"MODULE (%08X) LOAD FINISHED: %s\n", (int) moduleID, wszModuleName);
     
        if (std::wstring(wszModuleName).find(L"Test.exe") == std::wstring::npos)
            return hr;

        CComPtr<IMetaDataImport> pMDImport;
        hr = m_pICorProfilerInfo2->GetModuleMetaData(moduleID, ofRead, IID_IMetaDataImport, (IUnknown **)&pMDImport);
        if (FAILED(hr))
        {
            DebugPrintfA("m_pICorProfilerInfo2->GetModuleMetaData failed\r\n");
            return hr;
        }

        /*if (false)
        {

            HCORENUM hEnum = NULL;
            BOOL fMoreModuleRefs = TRUE;
            const ULONG cMax = 100;
            mdModuleRef rModuleRefs[cMax];
            ULONG cModuleRefs;
            WCHAR moduleRefName[300];
            ULONG cchNameIn = _DIMOF(moduleRefName);
            ULONG cchNameOut;
            while (fMoreModuleRefs)
            {
                hr = pMDImport->EnumModuleRefs(&hEnum, rModuleRefs, cMax, &cModuleRefs);
                if (FAILED(hr))
                {
		            DebugPrintfW( _T("**FAILURE: pMetaDataImport->EnumModuleRefs: FAILED\r\n") );
                    return hr;
                }
                if (hr == S_FALSE)
                {
                    fMoreModuleRefs = FALSE;
                }

                for (ULONG i = 0; i < cModuleRefs; ++i)
                {
                    const mdModuleRef mur = rModuleRefs[i];
                    pMDImport->GetModuleRefProps(mur, moduleRefName, cchNameIn, &cchNameOut);
                    DebugPrintfW(L"Module Reference: (%08X) %s", mur, moduleRefName);
                }
            }
        }*/

#if 1
        if (true)
        {
            HCORENUM hEnumTypeRefs = NULL;
            BOOL fMoreTypeRefs = TRUE;
            const ULONG cMax = 100;
            mdTypeRef rTypeRefs[cMax];
            ULONG cTypeRefs;
            WCHAR typeRefName[300];
            ULONG cchNameIn = _DIMOF(typeRefName);
            ULONG cchNameOut;
            while (fMoreTypeRefs)
            {
                hr = pMDImport->EnumTypeRefs(&hEnumTypeRefs, rTypeRefs, cMax, &cTypeRefs);
                if (FAILED(hr))
                {
		            DebugPrintfW( _T("**FAILURE: pMetaDataImport->EnumModuleRefs: FAILED\r\n") );
                    return hr;
                }
                if (hr == S_FALSE)
                {
                    fMoreTypeRefs = FALSE;
                }

                for (ULONG typeRefIndex = 0; typeRefIndex < cTypeRefs; ++typeRefIndex)
                {
                    const mdTypeRef typeReference = rTypeRefs[typeRefIndex];

                    mdToken scopeToken;
                    hr = pMDImport->GetTypeRefProps(typeReference, &scopeToken, typeRefName, cchNameIn, &cchNameOut);
                    if (FAILED(hr))
                    {
		                DebugPrintfW( _T("**FAILURE: pMetaDataImport->GetTypeRefProps: FAILED\r\n") );
                        return hr;
                    }
                    DebugPrintfW(L" Type Ref: (%08X) %s", typeReference, typeRefName);


                    BOOL fMoreModuleRefs = TRUE;
                    const ULONG cMax = 100;
                    mdMemberRef rMemberRefs[cMax];
                    ULONG cMemberRefs;
                    HCORENUM hEnumMemberRefs = NULL;
                    hr = pMDImport->EnumMemberRefs(&hEnumMemberRefs, typeReference, rMemberRefs, cMax, &cMemberRefs);
                    if (FAILED(hr))
                    {
		                DebugPrintfW( _T("**FAILURE: pMetaDataImport->EnumMemberRefs: FAILED\r\n") );
                        return hr;
                    }

                    // TODO: Continue enumerating if more than 100

                    for (ULONG memberRefIndex = 0; memberRefIndex < cMemberRefs; ++memberRefIndex)
                    {
                        const mdMemberRef mr = rMemberRefs[memberRefIndex];
                        WCHAR wszMemberRefName[300];
                        mdToken token;
                        PCCOR_SIGNATURE sigBlob;
                        ULONG cbSig;
                        pMDImport->GetMemberRefProps(mr, &token, wszMemberRefName, cchNameIn, &cchNameOut, &sigBlob, &cbSig);
                        DebugPrintfW(L"  Member Ref: (%08X) %s", mr, wszMemberRefName);


                        std::wstring fullMemberReferenceName(typeRefName);
                        fullMemberReferenceName += L".";
                        fullMemberReferenceName += wszMemberRefName;

                        m_moduleMethodRefs[moduleID][mr] = fullMemberReferenceName;
                    }
                    pMDImport->CloseEnum(hEnumMemberRefs);
                } // for current TypeRefs
            } // while more TypeRefs
            pMDImport->CloseEnum(hEnumTypeRefs);
        } // if true
#endif

#if 0
        HCORENUM hEnum = NULL;
        BOOL fMoreTypeDefs = TRUE;
        const ULONG cMax = 100;
        mdTypeDef rTypeDefs[cMax];
        ULONG cTypeDefs;
        while (fMoreTypeDefs)
        {
            hr = pMDImport->EnumTypeDefs(&hEnum, rTypeDefs, cMax, &cTypeDefs);
            if (FAILED(hr))
            {
                return hr;
            }
            if (hr == S_FALSE)
            {
                fMoreTypeDefs = FALSE;
            }

            for (ULONG i = 0; i < cTypeDefs; ++i)
            {
                const mdTypeDef td = rTypeDefs[i];
                WCHAR wszTypeName[300];
                ULONG cchNameIn = _DIMOF(wszTypeName);
                ULONG cchNameOut;
                // TODO: call gettypedefprops with null args to get name length for runtime alloc
                DWORD flags;
                mdToken baseClass;
                hr = pMDImport->GetTypeDefProps(td, wszTypeName, cchNameIn, &cchNameOut, &flags, &baseClass);
                if (FAILED(hr))
                {
                    DebugPrintfA("pMDImport->GetTypeDefProps failed\r\n");
                    return hr;
                }
                const std::wstring typeName(wszTypeName);
                //if (typeName.find(L"System.Console") != std::wstring::npos)
                {
                   DebugPrintfW(L" TypeDef: (%08X) %s", (int) td, wszTypeName);
                }
                /*else
                {
                    continue;
                }*/

                HCORENUM hEnum2 = NULL;
                mdMethodDef aMethodDefs[100];
                ULONG cMethodDefs;
                BOOL fMoreMethodDefs = TRUE;
                while (fMoreMethodDefs)
                {
                    hr = pMDImport->EnumMemberRefs(
                        &hEnum2,
                        td,
                        aMethodDefs,
                        _DIMOF(aMethodDefs),
                        &cMethodDefs);
                    if (FAILED(hr))
                    {
		                DebugPrintfW( _T("**FAILURE: pMetaDataImport->EnumMemberRefs: IsNilToken(classTypeDef)\r\n") );
                        return hr;
                    }
                    if (hr == S_FALSE)
                    {
                        fMoreMethodDefs = FALSE;
                    }

                    for (ULONG j = 0; j < cMethodDefs; ++j)
                    {
                        const mdMethodDef mb = aMethodDefs[j];
                        mdTypeDef classTypeDef; // should match
                        WCHAR wszMethodName[300];
                        ULONG cchMethodNameIn = _DIMOF(wszMethodName);
                        ULONG cchMethodNameOut;
                        // TODO: call gettypedefprops with null args to get name length for runtime alloc
                        // get detailed method info
	                    DWORD dwMethodAttr = 0;
                        PCCOR_SIGNATURE sigBlob;
                        ULONG cbSigBlob;
                        ULONG codeRVA;
                        DWORD implFlags;
                        /*hr = pMDImport->GetMethodProps(
                            mb, &classTypeDef, wszMethodName,
                            cchMethodNameIn, &cchMethodNameOut, &dwMethodAttr, &sigBlob, &cbSigBlob, &codeRVA, &implFlags );*/

                        hr = pMDImport->GetMemberProps(mb, &classTypeDef, wszMethodName, cchMethodNameIn, &cchMethodNameOut,
                            &dwMethodAttr, &sigBlob, &cbSigBlob,
                            NULL, NULL, NULL, NULL, NULL);
                        if ( FAILED(hr) )
	                    {
		                    DebugPrintfW( _T("**FAILURE: pMetaDataImport->GetMethodProps failed\r\n") );
		                    return hr;
	                    }
	                    if ( (classTypeDef == 0) || IsNilToken(classTypeDef) )
                        {
		                    DebugPrintfW( _T("**FAILURE: pMetaDataImport->GetMethodProps: IsNilToken(classTypeDef)\r\n") );
                            return E_FAIL;
                        }

                        const std::wstring methodName(wszMethodName);
                        // if (methodName.find(L"WriteLine") != std::wstring::npos)
                        {
                            DebugPrintfW(L" %08X    Member: %s", (int)mb, wszMethodName);
                        }

                        std::wstring fullMethodName(wszTypeName);
                        fullMethodName += L".";
                        fullMethodName += wszMethodName;

                        m_moduleMethodRefs[moduleID][mb] = fullMethodName;
                    }
                } // while more methods
                if (hEnum2 != NULL)
                {
                    pMDImport->CloseEnum(hEnum2);
                }
            }
        }

        if (hEnum != NULL)
        {
            pMDImport->CloseEnum(hEnum);
        }
#endif

        return hr;
    }

#endif


    void DebugPrintILFunction(ModuleID moduleID, LPCBYTE pMethodHeader, ULONG cbMethodSizeIncludingHeader)
    {
        DebugPrintfW(L"DebugPrintILFunction:");
        const IMAGE_COR_ILMETHOD* pMethod = reinterpret_cast<const IMAGE_COR_ILMETHOD*>(pMethodHeader);
 	    const IMAGE_COR_ILMETHOD_FAT* fat = &pMethod->Fat;
        size_t codeOffset;
        if ((fat->Flags & CorILMethod_FormatMask) == CorILMethod_FatFormat)
        {
            DebugPrintfA(" Method is fat of size %d.", (int)cbMethodSizeIncludingHeader);

            const size_t headerSizeInDwords = fat->Size;
            codeOffset = sizeof(DWORD) * headerSizeInDwords;
        }
        else
        {
            DebugPrintfA(" Method is tiny of size %d.", (int)cbMethodSizeIncludingHeader);
            const IMAGE_COR_ILMETHOD_TINY* tiny = &pMethod->Tiny;
            const size_t cbTinyHeaderSize = 1;
            codeOffset = cbTinyHeaderSize;
            // shift right 2 bits
            DWORD recompute =  ( ULONG ) ( ((unsigned) pMethod->Tiny.Flags_CodeSize) >> (CorILMethod_FormatShift - 1)/*0x2*/ );
            DebugPrintfA(" Recomputed tiny size (%02x) %d.", (int)pMethod->Tiny.Flags_CodeSize, (int)recompute);
        }

        const ULONG cbMethodSizeWithoutHeader = cbMethodSizeIncludingHeader - codeOffset;


        std::string all_bytes_as_string;
        for (ULONG i = 0; i < cbMethodSizeWithoutHeader; ++i)
        {
            unsigned char il_byte = pMethodHeader[i + codeOffset];
            char byte_as_string[8] = { 0 };
            _snprintf(byte_as_string, 7, "%02x ", il_byte);
            all_bytes_as_string += byte_as_string;
            if (i > 0 && ((i % 8) == 0))
            {
                DebugPrintfA(all_bytes_as_string.c_str());
                all_bytes_as_string = "";
            }
        }

        DebugPrintfA(all_bytes_as_string.c_str());

        std::vector<ULONG> bytes_to_nop;

        for (ULONG i = 0; i < cbMethodSizeWithoutHeader; ++i)
        {
            const unsigned char il_byte = pMethodHeader[i + codeOffset];
            const ULONG instruction_start_address = i;
            bool instr_found = false;
            for (size_t j = 0; j < m_instSet.size(); ++j)
            {
                const Instruction &inst = m_instSet[j];
                if (il_byte == inst.opcode2)
                {
                    instr_found = true;
                    if (inst.opcode_length > 1)
                        i += inst.opcode_length - 1;

                    const size_t kInstructionStringLength = 128;
                    wchar_t instruction_string[kInstructionStringLength] = { 0 };
                    if (inst.param == ParamKind::InlineNone)
                    {
                        _snwprintf(instruction_string, 63, L"IL_%04x:  /* %02x   |                  */ %s\r\n", instruction_start_address, il_byte, inst.name.c_str());
                    }
                    else
                    {
                        const uint8_t il_metadata_token_first_byte = pMethodHeader[i+codeOffset+inst.param];

                        wchar_t extra[128] = {0};
                        // TODO: check instruction inline argument type intead of hardcoding opcode
                        if (inst.opcode2 == 0x28 || inst.opcode2 == 0x6F)
                        {
                            uint32_t inline_argument_metadatatoken = 0;
                            memcpy(&inline_argument_metadatatoken, pMethodHeader+i+codeOffset+1, inst.param);

                            const wchar_t *name = GetFunctionNameFromMemberReference(moduleID, inline_argument_metadatatoken);
                            _snwprintf(extra, 127, L"%s", name);
                        }

                        uint32_t il_param_lsb = 0;
                        memcpy(&il_param_lsb, pMethodHeader+i+codeOffset+1, inst.param - 1);
                        _snwprintf(instruction_string, kInstructionStringLength - 1,
                            L"IL_%04x:  /* %02X   | (%02X)%06X       */ %s %s\r\n",
                            instruction_start_address,
                            il_byte,
                            il_metadata_token_first_byte, il_param_lsb,
                            inst.name.c_str(),
                            extra);

                    }
                    DebugPrintfW(instruction_string);

                    // move i to the next instruction
                    i += (int) inst.param;

                    break;
                }
            } // search over all instructions

            if (!instr_found)
            {
                char instruction_string[64] = { 0 };
                _snprintf(instruction_string, 63, "IL_%04x:  /* %02x   |                  */ %s\r\n", instruction_start_address, il_byte,"*** UNKNOWN ***");
                DebugPrintfA(instruction_string);
            }
        } // for all bytes in the method
    }


    virtual HRESULT STDMETHODCALLTYPE JITCompilationStarted(
        /* [in] */ FunctionID functionID,
        /* [in] */ BOOL fIsSafeToBlock)
    {
        HRESULT hr;

        const wchar_t *wszFunction = GetFunctionName(functionID);
        DebugPrintfW(L"JITCompilationStarted: %s", wszFunction);

#if 0
        // 1. get IMetaDataImport and method's token
        mdToken tkMethodDef;
        CComPtr<IMetaDataImport> pMetaDataImport = NULL;
        // TODO: Where do I release this?
        hr = m_pICorProfilerInfo2->GetTokenAndMetaDataFromFunction(
              functionID, IID_IMetaDataImport,
              (IUnknown** )&pMetaDataImport, &tkMethodDef);
        if ( FAILED(hr) )
        {
            DebugPrintfW( _T("**FAILURE: m_pCorProfilerInfo->GetTokenAndMetaDataFromFunction failed\r\n") );
            return hr;
        }
        if ( !pMetaDataImport )
            return E_FAIL;
#endif

        // 2. get functionID, classID, moduleID and MethodDef token
        ClassID classID;
        ModuleID moduleID;
        mdToken tkMethodDefTmp;
        hr = m_pICorProfilerInfo2->GetFunctionInfo( functionID, &classID, &moduleID, &tkMethodDefTmp );
        if ( FAILED(hr) )
        {
            DebugPrintfW( _T("**FAILURE: m_pCorProfilerInfo->GetFunctionInfo failed: functionID = %d\r\n"), functionID );
            // keep going // TODO: WTF
            // return hr;
        }



        CComPtr<IMetaDataImport> pMetaDataImport = NULL;
        hr = m_pICorProfilerInfo2->GetModuleMetaData(moduleID, ofRead, IID_IMetaDataImport,
     	    (IUnknown** )&pMetaDataImport);
 	    if (FAILED(hr))
        {
            DebugPrintfW( _T("**FAILURE: m_pICorProfilerInfo->GetModuleMetaData failed\r\n") );
            return hr;
        }

#if 0
        // 3. Get module base address
        LPCBYTE pModuleBaseLoadAddress = NULL;
        wchar_t wszModule[ _MAX_PATH ];
        ULONG cchModule = _DIMOF( wszModule );
        hr = m_pICorProfilerInfo2->GetModuleInfo( moduleID, &pModuleBaseLoadAddress, cchModule, &cchModule, wszModule, NULL );
        if ( FAILED(hr) )
        {
            DebugPrintfW( _T("**FAILURE: m_pCorProfilerInfo->GetModuleInfo failed: moduleID = %d\r\n"), moduleID );
            return hr;
        }

        // get detailed method info
        WCHAR wszMethod[ _MAX_PATH ] = L"_UNKNOWN_";
        DWORD cchMethod = _DIMOF( wszMethod );
        mdTypeDef tkTypeDef;
	    DWORD dwMethodAttr = 0;
        PCCOR_SIGNATURE sigBlob;
        ULONG cbSigBlob;
        ULONG codeRVA;
        DWORD implFlags;
        hr = pMetaDataImport->GetMethodProps(
            tkMethodDef, &tkTypeDef, wszMethod,
            cchMethod, &cchMethod, &dwMethodAttr, &sigBlob, &cbSigBlob, &codeRVA, &implFlags ); 
        if ( FAILED(hr) )
	    {
		    DebugPrintfW( _T("**FAILURE: pMetaDataImport->GetMethodProps failed\r\n") );
		    return hr;
	    }
	    if ( (tkTypeDef == 0) || IsNilToken(tkTypeDef) )
        {
		    DebugPrintfW( _T("**FAILURE: pMetaDataImport->GetMethodProps: IsNilToken(tkTypeDef)\r\n") );
            return E_FAIL;
        }
#else

#endif
        // Get the actual IL
        // mdMethodDef methodId;
        LPCBYTE pMethodHeader = NULL;
        ULONG cbMethodSizeIncludingHeader;
        hr = m_pICorProfilerInfo2->GetILFunctionBody(moduleID, tkMethodDefTmp, &pMethodHeader, &cbMethodSizeIncludingHeader);
        if ( FAILED(hr) )
	    {
		    DebugPrintfW( _T("**FAILURE: p_pICorProfilerInfo2->GetILFunctionBody failed\r\n") );
		    return hr;
	    }

        const IMAGE_COR_ILMETHOD* pMethod = reinterpret_cast<const IMAGE_COR_ILMETHOD*>(pMethodHeader);
 	    const IMAGE_COR_ILMETHOD_FAT* fat = &pMethod->Fat;
        size_t codeOffset;
        if ((fat->Flags & CorILMethod_FormatMask) == CorILMethod_FatFormat)
        {
            DebugPrintfA(" Method is fat of size %d.", (int)cbMethodSizeIncludingHeader);

            const size_t headerSizeInDwords = fat->Size;
            codeOffset = sizeof(DWORD) * headerSizeInDwords;
        }
        else
        {
            DebugPrintfA(" Method is tiny of size %d.", (int)cbMethodSizeIncludingHeader);
            const IMAGE_COR_ILMETHOD_TINY* tiny = &pMethod->Tiny;
            const size_t cbTinyHeaderSize = 1;
            codeOffset = cbTinyHeaderSize;
            // shift right 2 bits
            DWORD recompute =  ( ULONG ) ( ((unsigned) pMethod->Tiny.Flags_CodeSize) >> (CorILMethod_FormatShift - 1)/*0x2*/ ); 
            DebugPrintfA(" Recomputed tiny size (%02x) %d.", (int)pMethod->Tiny.Flags_CodeSize, (int)recompute);
        }

        const ULONG cbMethodSizeWithoutHeader = cbMethodSizeIncludingHeader - codeOffset;


        std::string all_bytes_as_string;
        for (ULONG i = 0; i < cbMethodSizeWithoutHeader; ++i)
        {
            unsigned char il_byte = pMethodHeader[i + codeOffset];
            char byte_as_string[8] = { 0 };
            _snprintf(byte_as_string, 7, "%02x ", il_byte);
            all_bytes_as_string += byte_as_string;
            if (i > 0 && ((i % 8) == 0))
            {
                DebugPrintfA(all_bytes_as_string.c_str());
                all_bytes_as_string = "";
            }
        }

        DebugPrintfA(all_bytes_as_string.c_str());

        std::vector<ULONG> bytes_to_nop;

        for (ULONG i = 0; i < cbMethodSizeWithoutHeader; ++i)
        {
            const unsigned char il_byte = pMethodHeader[i + codeOffset];
            const ULONG instruction_start_address = i;
            bool instr_found = false;
            for (size_t j = 0; j < m_instSet.size(); ++j)
            {
                const Instruction &inst = m_instSet[j];
                if (il_byte == inst.opcode2)
                {
                    instr_found = true;
                    if (inst.opcode_length > 1)
                        i += inst.opcode_length - 1;

                    const size_t kInstructionStringLength = 128;
                    wchar_t instruction_string[kInstructionStringLength] = { 0 };
                    if (inst.param == ParamKind::InlineNone)
                    {
                        _snwprintf(instruction_string, 63, L"IL_%04x:  /* %02x   |                  */ %s\r\n", instruction_start_address, il_byte, inst.name.c_str());
                    }
                    else
                    {
                        const uint8_t il_metadata_token_first_byte = pMethodHeader[i+codeOffset+inst.param];

                        wchar_t extra[128] = {0};
                        // TODO: check instruction inline argument type intead of hardcoding opcode
                        if (inst.opcode2 == 0x28 || inst.opcode2 == 0x6F)
                        {
                            uint32_t inline_argument_metadatatoken = 0;
                            memcpy(&inline_argument_metadatatoken, pMethodHeader+i+codeOffset+1, inst.param);

                            const wchar_t *name = GetFunctionNameFromMemberReference(moduleID, inline_argument_metadatatoken);
                            _snwprintf(extra, 127, L"%s", name);
                            if (name != NULL && std::wstring(name).find(L"System.Console.WriteLine") != std::wstring::npos)
                            {
                                for (int extra_bytes = 0; extra_bytes < inst.opcode_length + inst.param; ++extra_bytes)
                                {
                                    bytes_to_nop.push_back(instruction_start_address + codeOffset + extra_bytes);
                                }
                            }
                        }

                       /* if (inst.opcode2 == 0x72)
                        {
                            for (int extra_bytes = 0; extra_bytes < inst.opcode_length + inst.param; ++extra_bytes)
                            {
                                bytes_to_nop.push_back(instruction_start_address + codeOffset + extra_bytes);
                            }
                        }*/

                        uint32_t il_param_lsb = 0;
                        memcpy(&il_param_lsb, pMethodHeader+i+codeOffset+1, inst.param - 1);
                        _snwprintf(instruction_string, kInstructionStringLength - 1,
                            L"IL_%04x:  /* %02X   | (%02X)%06X       */ %s %s\r\n",
                            instruction_start_address,
                            il_byte,
                            il_metadata_token_first_byte, il_param_lsb,
                            inst.name.c_str(),
                            extra);

                    }
                    DebugPrintfW(instruction_string);

                    // move i to the next instruction
                    i += (int) inst.param;

                    break;
                }
            } // search over all instructions

            if (!instr_found)
            {
                char instruction_string[64] = { 0 };
                _snprintf(instruction_string, 63, "IL_%04x:  /* %02x   |                  */ %s\r\n", instruction_start_address, il_byte,"*** UNKNOWN ***");
                DebugPrintfA(instruction_string);
            }
        } // for all bytes in the method

#if 1
        if (bytes_to_nop.size() > 0 &&  ((fat->Flags & CorILMethod_FormatMask) != CorILMethod_FatFormat))
        {
 	        CComPtr<IMethodMalloc> pIMethodMalloc = NULL;
 	        IMAGE_COR_ILMETHOD* pNewMethod = NULL;
 	        hr = m_pICorProfilerInfo2->GetILFunctionBodyAllocator(moduleID, &pIMethodMalloc);
 	        if (FAILED(hr))
  	        {
                return hr;
            }

            const ULONG newMethodSizeIncludingHeader = cbMethodSizeIncludingHeader; //   - bytes_to_nop.size();

 	        pNewMethod = (IMAGE_COR_ILMETHOD*) pIMethodMalloc->Alloc(newMethodSizeIncludingHeader);
 	        if (pNewMethod == NULL)
  	        {
                return hr;
            }
            BYTE *newMethodAsBytes = reinterpret_cast<BYTE*>(pNewMethod);
            memset(newMethodAsBytes, 0, newMethodSizeIncludingHeader);
#if 1
            //const BYTE call[] = { 0x28, 0x11, 0x00, 0x00, 0x0A };
            const BYTE call[] = { 0x28, 0x11, 0x00, 0x00, 0x0A };
            // const BYTE call[] = { 0x28, 0x02, 0x00, 0x00, 0x06 };
            memcpy(pNewMethod, pMethod, cbMethodSizeIncludingHeader);
            for (size_t i = 0, j = 0; i < bytes_to_nop.size(); ++i)
            {
               const ULONG byte_offset = bytes_to_nop[i];
               newMethodAsBytes[byte_offset] = call[j++];
               DebugPrintfW(L"Overwriting byte IL_%04x", (int)byte_offset - codeOffset);
            }
#endif
            BYTE mask = (pMethod->Tiny.Flags_CodeSize & CorILMethod_TinyFormat);
            // pNewMethod->Tiny.Flags_CodeSize = CorILMethod_TinyFormat | (newMethodSizeIncludingHeader - 1);
            newMethodAsBytes[0] = (BYTE)CorILMethod_TinyFormat | (BYTE)(newMethodSizeIncludingHeader - 1) << 2;
            DebugPrintfA("newMethodAsBytes[0]: %02x", newMethodAsBytes[0]);
#if 0
            ULONG new_i = codeOffset;
            for (ULONG src_i = codeOffset; src_i < cbMethodSizeIncludingHeader; ++src_i)
            {
                bool skip = false;
                for (size_t j = 0; j < bytes_to_nop.size(); ++j)
                {
                    const ULONG byte_offset = bytes_to_nop[j];
                    if (src_i == byte_offset)
                    {
                        DebugPrintfW(L"Overwriting byte IL_%04x", (int)byte_offset - codeOffset);
                        skip = true;
                        break;
                    }
                }

                if (!skip)
                {
                    newMethodAsBytes[new_i++] = pMethodHeader[src_i];
                }
            }
#endif

            DebugPrintILFunction(moduleID, newMethodAsBytes, newMethodSizeIncludingHeader);

            hr = m_pICorProfilerInfo2->SetILFunctionBody(moduleID, tkMethodDefTmp, newMethodAsBytes);
 	        if (FAILED(hr))
  	        {
                return hr;
            }
        }
#endif

        return S_OK;
    }

private:
    ULONG m_ref_count;
    CComQIPtr<ICorProfilerInfo2> m_pICorProfilerInfo2;
    std::vector<Instruction> m_instSet;

    typedef std::map<mdMemberRef, std::wstring> MemberRefMap;
    typedef MemberRefMap::const_iterator MemberRefMapIterator;

    std::map<ModuleID, MemberRefMap> m_moduleMethodRefs;

    typedef std::map<FunctionID, std::wstring>::const_iterator FunctionIdMapIterator;
    std::map<FunctionID, std::wstring> m_functionIdToNameMap;
};


static UINT_PTR __stdcall FunctionMapper(FunctionID functionID, BOOL *pbHookFunction)
{
    if (g_pThisMyProfiler != NULL)
        g_pThisMyProfiler->MapFunction(functionID);

    // we must return the function ID passed as a parameter
    return (UINT_PTR)functionID;
}

// this function simply forwards the FunctionEnter call the global profiler object
void __stdcall FunctionEnterGlobal(FunctionID functionID, UINT_PTR clientData, COR_PRF_FRAME_INFO frameInfo, COR_PRF_FUNCTION_ARGUMENT_INFO *argInfo)
{
    if (g_pThisMyProfiler != NULL)
        g_pThisMyProfiler->FunctionEnter(functionID, clientData, frameInfo, argInfo);
}

HRESULT __stdcall MyProfilerFactory::QueryInterface(const IID& iid, void** ppv)
{
    DebugPrintfW(L"MyProfilerFactory::QueryInterface");

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
