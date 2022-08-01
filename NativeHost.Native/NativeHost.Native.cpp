#include <iostream>

#include "CoreCLRHost.h"

void CreateHost(std::filesystem::path runtimeConfigPath, std::filesystem::path assemblyPath, const char_t* dotnetType);

void CallMain(const char_t* dotnetType);
void CallPrint(const char_t* dotnetType);
void CallPrintAndReturn(const char_t* dotnetType);
void CallDoStuff(const char_t* dotnetType);
void CallDoStuffStruct(const char_t* dotnetType);

extern "C" __declspec(dllexport) void SomeExportedFunction()
{
    std::cout << "C++ SomeExportedFunction" << std::endl;
}

int main(int argc, char* argv[])
{
    char nativeHostExe[MAX_PATH];
    auto size = ::GetFullPathNameA(argv[0], sizeof(nativeHostExe) / sizeof(char), nativeHostExe, nullptr);
    
    std::filesystem::path host(nativeHostExe);
    std::filesystem::path hostPath = host.parent_path();

    std::cout << "Host path is " << hostPath << '\n';

    std::filesystem::path configPath = hostPath / "net6.0" / "NativeHost.ManagedLib.runtimeconfig.json";
    std::filesystem::path assemblyPath = hostPath / "net6.0" / "NativeHost.ManagedLib.dll";

    CreateHost(configPath, assemblyPath, STR("NativeHost.ManagedLib.ManagedApi, NativeHost.ManagedLib"));
}

void CreateHost(std::filesystem::path runtimeConfigPath, std::filesystem::path assemblyPath, const char_t* dotnetType)
{
    CoreCLRResult rc = CoreCLRHost::Create(runtimeConfigPath, assemblyPath);
    if (rc != CoreCLRResult::Success)
    {
        std::cout << "RC Create: " << (uint32_t)rc << std::endl;
        return;
    }

    CallMain(dotnetType);
    CallPrint(dotnetType);
    CallPrintAndReturn(dotnetType);
    CallDoStuff(dotnetType);
    CallDoStuffStruct(dotnetType);
}

void CallMain(const char_t* dotnetType)
{
    // Load managed assembly and get function pointer to a managed method
    const char_t* methodName = STR("Main");

    using ClrMainFn = void (CORECLR_DELEGATE_CALLTYPE*)(void*);

    ClrMainFn method = nullptr;
    CoreCLRResult rc = CoreCLRHost::GetMethodFunctionPointer<ClrMainFn>(dotnetType, methodName, &method);
    if (rc != CoreCLRResult::Success)
    {
        std::cout << "RC GetMethodFunctionPointer: " << (uint32_t)rc << std::endl;
        return;
    }

#if WINDOWS
    void* mainProgramHandle = GetModuleHandleW(NULL);
#else
    void* mainProgramHandle = dlopen(nullptr);
#endif

    // Call the method
    method(mainProgramHandle);
}

void CallPrint(const char_t* dotnetType)
{
    using ClrPrintFn = void (CORECLR_DELEGATE_CALLTYPE*)(CoreCLRHost::NativeString);
    ClrPrintFn printMethod = nullptr;
    CoreCLRResult rc = CoreCLRHost::GetMethodFunctionPointer<ClrPrintFn>(dotnetType, STR("Print"), &printMethod);
    if (rc != CoreCLRResult::Success)
    {
        std::cout << "RC GetMethodFunctionPointer: " << (uint32_t)rc << std::endl;
        return;
    }

    auto msg = CoreCLRHost::NativeString{ "Test" };
    printMethod(msg);
}

void CallPrintAndReturn(const char_t* dotnetType)
{
    using ClrPrintAndReturnFn = CoreCLRHost::NativeString(CORECLR_DELEGATE_CALLTYPE*)(CoreCLRHost::NativeString);
    ClrPrintAndReturnFn printMethod = nullptr;
    CoreCLRResult rc = CoreCLRHost::GetMethodFunctionPointer<ClrPrintAndReturnFn>(dotnetType, STR("PrintAndReturn"), &printMethod);
    if (rc != CoreCLRResult::Success)
    {
        std::cout << "RC GetMethodFunctionPointer: " << (uint32_t)rc << std::endl;
        return;
    }

    auto msg = CoreCLRHost::NativeString{ "Test" };
    auto returnValue = printMethod(msg);
    std::cout << "Returned from C#: " << returnValue.RawString << std::endl;
}

using CsharpPrimitiveCallbackFn = void(*)(bool);
void CsharpPrimitiveCallback(bool test)
{
    std::cout << "Hello from C++" << std::endl;
    std::cout << "Number from C#: " << test << std::endl;
}

void CallDoStuff(const char_t* dotnetType)
{
    const char_t* methodName = STR("DoStuff");

    using ClrDoStuffFn = void (CORECLR_DELEGATE_CALLTYPE*)(CsharpPrimitiveCallbackFn);

    ClrDoStuffFn method = nullptr;
    CoreCLRResult rc = CoreCLRHost::GetMethodFunctionPointer<ClrDoStuffFn>(dotnetType, methodName, &method);
    if (rc != CoreCLRResult::Success)
    {
        std::cout << "RC GetMethodFunctionPointer: " << (uint32_t)rc << std::endl;
        return;
    }

    // Call the method
    method(&CsharpPrimitiveCallback);
}

using CsharpStructCallbackFn = void(*)(CoreCLRHost::NativeString);
void CsharpStructCallback(CoreCLRHost::NativeString data)
{
    std::cout << "Hello from C++" << std::endl;
    std::cout << "Response from C#: " << data.RawString << std::endl;
}

void CallDoStuffStruct(const char_t* dotnetType)
{
    const char_t* methodName = STR("DoStuffStruct");

    using ClrDoStuffFn = void (CORECLR_DELEGATE_CALLTYPE*)(CsharpStructCallbackFn);

    ClrDoStuffFn method = nullptr;
    CoreCLRResult rc = CoreCLRHost::GetMethodFunctionPointer<ClrDoStuffFn>(dotnetType, methodName, &method);
    if (rc != CoreCLRResult::Success)
    {
        std::cout << "RC GetMethodFunctionPointer: " << (uint32_t)rc << std::endl;
        return;
    }

    // Call the method
    method(&CsharpStructCallback);
}
