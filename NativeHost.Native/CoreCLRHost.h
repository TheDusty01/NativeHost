// Standard headers
//#include <stdio.h>
#include <stdint.h>
//#include <stdlib.h>
//#include <string.h>
//#include <assert.h>
//#include <iostream>
#include <filesystem>

// CoreCLR headers
#include <nethost.h>
#include <coreclr_delegates.h>
#include <hostfxr.h>

// Set windows def if platform is windows
#ifdef _WIN32
#define WINDOWS 1
#endif

#ifdef _WIN64
#ifndef WINDOWS
#define WINDOWS 1
#endif
#endif

// Define macros that are platform specific
#ifdef WINDOWS
#include <Windows.h>

#define STR(s) L ## s
#define CH(c) L ## c

#else
#include <dlfcn.h>
#include <limits.h>

#define STR(s) s
#define CH(c) c
#define MAX_PATH PATH_MAX

#endif

enum class CoreCLRResult : uint32_t
{
    Success = 0,
    ErrorHostfxrGetPath = 1,
    ErrorHostfxrLoad = 2,

    ErrorHostfxrGetInitFn = 3,
    ErrorHostfxrGetDelegateFn = 4,
    ErrorHostfxrGetCloseFn = 5,

    ErrorHostfxrInitCall = 10,
    ErrorHostfxrGetDelegateCall = 11,

    ErrorGetMethodFunctionPointer = 20
};

class CoreCLRHost
{
// Fields
private:
    static hostfxr_initialize_for_runtime_config_fn s_InitFptr;
    static hostfxr_get_runtime_delegate_fn s_GetDelegateFptr;
    static hostfxr_close_fn s_CloseFptr;

    static load_assembly_and_get_function_pointer_fn s_LoadAssemblyAndGetFunctionPointerFptr;

    static std::filesystem::path s_AssemblyPath;
public:
    static constexpr int32_t ClrSuccessRc = 0;

    // The last result code of the CLR functions
    static int32_t s_LastClrRc;

// Methods
private:
    // Helper
    static void* LoadAssembly(const char_t* path);
    static void* GetExport(void* h, const char* name);

    static CoreCLRResult LoadHostFxr();
    static CoreCLRResult SetupDotNetLoadAssemblyFunction(const char_t* assembly);

public:
    static CoreCLRResult Create(std::filesystem::path runtimeConfigPath, std::filesystem::path assemblyPath);
    static CoreCLRResult GetMethodFunctionPointer(const char_t* dotnetType, const char_t* methodName, void** methodFnPtr);

    template <class TFunctionPointer>
    static CoreCLRResult GetMethodFunctionPointer(const char_t* dotnetType, const char_t* methodName, TFunctionPointer* methodFnPtr)
    {
        return GetMethodFunctionPointer(dotnetType, methodName, (void**)methodFnPtr);
    }

    // Types
    struct NativeString
    {
        const char* RawString = nullptr;
    };

};

// 
// Implementation
//

hostfxr_initialize_for_runtime_config_fn CoreCLRHost::s_InitFptr = nullptr;
hostfxr_get_runtime_delegate_fn CoreCLRHost::s_GetDelegateFptr = nullptr;
hostfxr_close_fn CoreCLRHost::s_CloseFptr = nullptr;

load_assembly_and_get_function_pointer_fn CoreCLRHost::s_LoadAssemblyAndGetFunctionPointerFptr = nullptr;

std::filesystem::path CoreCLRHost::s_AssemblyPath;

int32_t CoreCLRHost::s_LastClrRc = CoreCLRHost::ClrSuccessRc;

void* CoreCLRHost::LoadAssembly(const char_t* path)
{
#if WINDOWS
    void* h = (void*)::LoadLibrary(path);
#else
    void* h = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
#endif
    return h;
}

void* CoreCLRHost::GetExport(void* h, const char* name)
{
#if WINDOWS
    void* f = ::GetProcAddress((HMODULE)h, name);
#else
    void* f = dlsym(h, name);
#endif
    return f;
}

CoreCLRResult CoreCLRHost::LoadHostFxr()
{
    // Pre-allocate a large buffer for the path to hostfxr
    char_t buffer[MAX_PATH];
    size_t buffer_size = sizeof(buffer) / sizeof(char_t);
    s_LastClrRc = get_hostfxr_path(buffer, &buffer_size, nullptr);
    if (s_LastClrRc != CoreCLRHost::ClrSuccessRc)
        return CoreCLRResult::ErrorHostfxrGetPath;

    // Load hostfxr and get desired exports
    void* lib = CoreCLRHost::LoadAssembly(buffer);
    if (lib == nullptr)
        return CoreCLRResult::ErrorHostfxrLoad;



    s_InitFptr = (hostfxr_initialize_for_runtime_config_fn)CoreCLRHost::GetExport(lib, "hostfxr_initialize_for_runtime_config");
    if (s_InitFptr == nullptr)
        return CoreCLRResult::ErrorHostfxrGetInitFn;

    s_GetDelegateFptr = (hostfxr_get_runtime_delegate_fn)CoreCLRHost::GetExport(lib, "hostfxr_get_runtime_delegate");
    if (s_GetDelegateFptr == nullptr)
        return CoreCLRResult::ErrorHostfxrGetDelegateFn;

    s_CloseFptr = (hostfxr_close_fn)CoreCLRHost::GetExport(lib, "hostfxr_close");
    if (s_CloseFptr == nullptr)
        return CoreCLRResult::ErrorHostfxrGetCloseFn;

    return CoreCLRResult::Success;
}

CoreCLRResult CoreCLRHost::SetupDotNetLoadAssemblyFunction(const char_t* assembly)
{
    // Load .NET Core
    hostfxr_handle cxt = nullptr;
    s_LastClrRc = CoreCLRHost::s_InitFptr(assembly, nullptr, &cxt);
    if (s_LastClrRc != CoreCLRHost::ClrSuccessRc || cxt == nullptr)
    {
        CoreCLRHost::s_CloseFptr(cxt);
        return CoreCLRResult::ErrorHostfxrInitCall;
    }

    // Get the load assembly function pointer
    s_LastClrRc = s_GetDelegateFptr(
        cxt,
        hdt_load_assembly_and_get_function_pointer,
        (void**)&s_LoadAssemblyAndGetFunctionPointerFptr
    );

    if (s_LastClrRc != CoreCLRHost::ClrSuccessRc || s_LoadAssemblyAndGetFunctionPointerFptr == nullptr)
    {
        CoreCLRHost::s_CloseFptr(cxt);
        return CoreCLRResult::ErrorHostfxrGetDelegateCall;
    }

    // Release hostfxr handle
    CoreCLRHost::s_CloseFptr(cxt);

    return CoreCLRResult::Success;
}

CoreCLRResult CoreCLRHost::Create(std::filesystem::path runtimeConfigPath, std::filesystem::path assemblyPath)
{
    // STEP 1: Load HostFxr and get exported hosting functions
    CoreCLRResult rc = LoadHostFxr();
    if (rc != CoreCLRResult::Success)
        return rc;

    // STEP 2: Initialize and start the .NET Core runtime
    rc = SetupDotNetLoadAssemblyFunction(runtimeConfigPath.c_str());
    if (rc != CoreCLRResult::Success)
        return rc;

    s_AssemblyPath = assemblyPath;

    return CoreCLRResult::Success;
}

CoreCLRResult CoreCLRHost::GetMethodFunctionPointer(const char_t* dotnetType, const char_t* methodName, void** methodFnPtr)
{
    s_LastClrRc = CoreCLRHost::s_LoadAssemblyAndGetFunctionPointerFptr(
        s_AssemblyPath.c_str(),
        dotnetType,
        methodName,
        UNMANAGEDCALLERSONLY_METHOD,
        nullptr,
        methodFnPtr
    );

    if (s_LastClrRc != CoreCLRHost::ClrSuccessRc || methodFnPtr == nullptr)
    {
        return CoreCLRResult::ErrorGetMethodFunctionPointer;
    }

    return CoreCLRResult::Success;
}

// Non static version - currently unsupported
// 
//class CoreCLRHost
//{
//// Fields
//private:
//    static hostfxr_initialize_for_runtime_config_fn s_InitFptr;
//    static hostfxr_get_runtime_delegate_fn s_GetDelegateFptr;
//    static hostfxr_close_fn s_CloseFptr;
//
//    load_assembly_and_get_function_pointer_fn s_LoadAssemblyAndGetFunctionPointerFptr = nullptr;
//
//    std::filesystem::path s_AssemblyPath;
//
//    int32_t s_LastClrRc = 0;
//public:
//
//// Methods
//private:public:
//    // Helper
//    static void* LoadAssembly(const char_t* path);
//    static void* GetExport(void* h, const char* name);
//
//    static int32_t LoadHostFxr();
//    int32_t SetupDotNetLoadAssemblyFunction(const char_t* assembly);
//
//public:
//    int32_t Create(std::filesystem::path runtimeConfigPath, std::filesystem::path assemblyPath);
//    int32_t GetMethodFunctionPointer(const char_t* dotnetType, const char_t* methodName, void** methodFnPtr);
//    
//    template <class TFunctionPointer>
//    int32_t GetMethodFunctionPointer(const char_t* dotnetType, const char_t* methodName, TFunctionPointer* methodFnPtr)
//    {
//        return GetMethodFunctionPointer(dotnetType, methodName, (void**)methodFnPtr);
//    }
//
//    void Destroy();
//
//};


//// =========================================================================================================
//// ================================================== OOP ================================================== 
//// =========================================================================================================
//hostfxr_initialize_for_runtime_config_fn CoreCLRHost::s_InitFptr = nullptr;
//hostfxr_get_runtime_delegate_fn CoreCLRHost::s_GetDelegateFptr = nullptr;
//hostfxr_close_fn CoreCLRHost::s_CloseFptr = nullptr;
//
////load_assembly_and_get_function_pointer_fn CoreCLRHost::s_LoadAssemblyAndGetFunctionPointerFptr = nullptr;
////
////std::filesystem::path CoreCLRHost::s_AssemblyPath;
////
////int32_t CoreCLRHost::s_LastClrRc = CLR_SUCCESS;
//
//void* CoreCLRHost::LoadAssembly(const char_t* path)
//{
//#if WINDOWS
//    void* h = (void*)::LoadLibrary(path);
//#else
//    void* h = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
//#endif
//    return h;
//}
//
//void* CoreCLRHost::GetExport(void* h, const char* name)
//{
//#if WINDOWS
//    void* f = ::GetProcAddress((HMODULE)h, name);
//#else
//    void* f = dlsym(h, name);
//#endif
//    return f;
//}
//
//int32_t CoreCLRHost::LoadHostFxr()
//{
//    // Pre-allocate a large buffer for the path to hostfxr
//    char_t buffer[MAX_PATH];
//    size_t buffer_size = sizeof(buffer) / sizeof(char_t);
//    int rc = get_hostfxr_path(buffer, &buffer_size, nullptr);
//    if (rc != CLR_SUCCESS)
//        return CLR_ERROR_HOSTFXR_GET_PATH;
//
//    // Load hostfxr and get desired exports
//    void* lib = CoreCLRHost::LoadAssembly(buffer);
//    if (lib == nullptr)
//        return CLR_ERROR_HOSTFXR_LOAD;
//
//
//    s_InitFptr = (hostfxr_initialize_for_runtime_config_fn)CoreCLRHost::GetExport(lib, "hostfxr_initialize_for_runtime_config");
//    if (s_InitFptr == nullptr)
//        return CLR_ERROR_HOSTFXR_GET_INIT_FN;
//
//    s_GetDelegateFptr = (hostfxr_get_runtime_delegate_fn)CoreCLRHost::GetExport(lib, "hostfxr_get_runtime_delegate");
//    if (s_GetDelegateFptr == nullptr)
//        return CLR_ERROR_HOSTFXR_GET_DELEGATE_FN;
//
//    s_CloseFptr = (hostfxr_close_fn)CoreCLRHost::GetExport(lib, "hostfxr_close");
//    if (s_CloseFptr == nullptr)
//        return CLR_ERROR_HOSTFXR_GET_CLOSE_FN;
//
//    return CLR_SUCCESS;
//}
//
//#include <iostream>
//int32_t CoreCLRHost::SetupDotNetLoadAssemblyFunction(const char_t* assembly)
//{
//    // Load .NET Core
//    hostfxr_handle cxt = nullptr;
//    s_LastClrRc = CoreCLRHost::s_InitFptr(assembly, nullptr, &cxt);
//    if (s_LastClrRc != CLR_SUCCESS && s_LastClrRc != 1)
//    {
//        std::cout << "s_InitFptr: " << s_InitFptr << std::endl;
//        std::cout << "s_LastClrRc: " << s_LastClrRc << std::endl;
//
//        CoreCLRHost::s_CloseFptr(cxt);
//        return CLR_ERROR_HOSTFXR_INIT_CALL;
//    }
//
//    // Get the load assembly function pointer
//    s_LastClrRc = s_GetDelegateFptr(
//        cxt,
//        hdt_load_assembly_and_get_function_pointer,
//        (void**)&s_LoadAssemblyAndGetFunctionPointerFptr
//    );
//
//    if (s_LastClrRc != CLR_SUCCESS || s_LoadAssemblyAndGetFunctionPointerFptr == nullptr)
//    {
//        return CLR_ERROR_HOSTFXR_GET_DELEGATE_CALL;
//        CoreCLRHost::s_CloseFptr(cxt);
//    }
//
//    // Release hostfxr handle
//    CoreCLRHost::s_CloseFptr(cxt);
//
//    return CLR_SUCCESS;
//}
//
//int32_t CoreCLRHost::Create(std::filesystem::path runtimeConfigPath, std::filesystem::path assemblyPath)
//{
//    // STEP 2: Initialize and start the .NET Core runtime
//    int32_t rc = SetupDotNetLoadAssemblyFunction(runtimeConfigPath.c_str());
//    if (rc != CLR_SUCCESS)
//        return rc;
//
//    s_AssemblyPath = assemblyPath;
//
//    return CLR_SUCCESS;
//}
//
//int32_t CoreCLRHost::GetMethodFunctionPointer(const char_t* dotnetType, const char_t* methodName, void** methodFnPtr)
//{
//    int rc = CoreCLRHost::s_LoadAssemblyAndGetFunctionPointerFptr(
//        s_AssemblyPath.c_str(),
//        dotnetType,
//        methodName,
//        UNMANAGEDCALLERSONLY_METHOD,
//        nullptr,
//        methodFnPtr
//    );
//
//    if (rc != CLR_SUCCESS || methodFnPtr == nullptr)
//    {
//        std::cout << "GetMethodFunctionPointer rc internal: " << rc << std::endl;
//        return CLR_ERROR_GET_METHOD_FUNCTION_POINTER;
//    }
//
//    return CLR_SUCCESS;
//}
//
//void CoreCLRHost::Destroy()
//{
//}
