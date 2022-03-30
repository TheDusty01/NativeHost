# NativeHost
A small header-only crossplatform library which makes it easier to host the CoreCLR runtime inside of an native app.
For more information on this topic check out the [.NET Documentation](https://docs.microsoft.com/en-us/dotnet/core/tutorials/netcore-hosting) and [this article]( https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md) on the [dotnet/runtime repo](https://github.com/dotnet/runtime) about the API design.

The goal of this repo is to also show off the common use cases with the CLR host and should act as a small showcase of the current possibilities with it (since the documentation on this is pretty narrow at the moment).

## Setup
Just install the latest release from the [Releases tab](https://github.com/TheDusty01/NativeHost/releases).
### Dependencies
* ``coreclr_delegates.h``
* ``hostfxr.h``
* ``nethost.h``
* ``nethost.lib`` or ``nethost.dll`` or ``nethost.so``

Make sure to include the needed headers and libraries from [NativeHost.Native/include](NativeHost.Native/include) and [NativeHost.Native/lib](NativeHost.Native/lib).

You can also get the headers from the dotnet runtime repo:
* [``coreclr_delegates.h``](https://github.com/dotnet/runtime/blob/main/src/native/corehost/coreclr_delegates.h)
* [``hostfxr.h``](https://github.com/dotnet/runtime/blob/main/src/native/corehost/hostfxr.h)
* [``nethost.h``](https://github.com/dotnet/runtime/blob/main/src/native/corehost/nethost/nethost.h)
* ``nethost.lib``, ``nethost.dll``, ``nethost.so`` - Can be found in your local dotnet installation

## Usage
Don't forget to checkout the example project which contains various samples like calling a managed method with parameters like strings, calling an unmanaged method back from the managed context using function pointers and much more (check [Samples](https://github.com/TheDusty01/NativeHost#samples)).

### Creating a CLR host
To create a CLR host you just need the path to the runtimeconfig (more info on this below) and to the managed assembly (DLL).
```c++
std::filesystem::path runtimeConfigPath = "Path" / "To" / "The" / "AssemblyName.runtimeconfig.json";
std::filesystem::path assemblyPath = "Path" / "To" / "The" / "AssemblyName.dll";

CoreCLRResult rc = CoreCLRHost::Create(runtimeConfigPath, assemblyPath);
if (rc != CoreCLRResult::Success)
{
    std::cout << "RC Create: " << (uint32_t)rc << std::endl;
    return;
}
```

### Calling a method
The following code calls the method ``public static void Main()`` inside of the ``NativeHost.ManagedLib.ManagedApi`` class (in the ``NativeHost.ManagedLib.dll`` assembly).
```c++
const char_t* = STR("NativeHost.ManagedLib.ManagedApi, NativeHost.ManagedLib"); // Namespace.Class, AssemblyName
const char_t* methodName = STR("Main");

using ClrMainFn = void (CORECLR_DELEGATE_CALLTYPE*)();

// Create a function pointer with the specified signature
ClrMainFn method = nullptr;
CoreCLRResult rc = CoreCLRHost::GetMethodFunctionPointer<ClrMainFn>(dotnetType, methodName, &method);
if (rc != CoreCLRResult::Success)
{
    std::cout << "RC GetMethodFunctionPointer: " << (uint32_t)rc << std::endl;
    return;
}

// Call the method
method();
```

### Creating a managed (.NET) library
Make sure to mark your methods with the ``UnmanagedCallersOnly`` attribute so they can be called from your host.
```cs
namespace NativeHost.ManagedLib
{
    public class ManagedApi
    {
        [UnmanagedCallersOnly]
        public static void Main()
        {
            Console.WriteLine($"C# Main");
        }
    }
}
```

### Creating a runtimeconfig
Before running your own CLR host, make sure to create a runtimeconfig in the same directory as your managed DLL. The naming scheme for this file is pretty straight forward, you just call it like your DLL but instead of ``.dll``, you append ``.runtimeconfig.json`` at the end.\
In the example from before you would call the file ``NativeHost.ManagedLib.runtimeconfig.json``.

With the following contents you configure the runtime to use .NET 6 as the target framework:
```json
{
  "runtimeOptions": {
    "tfm": "net6.0",
    "rollForward": "LatestMinor",
    "framework": {
      "name": "Microsoft.NETCore.App",
      "version": "6.0.0"
    }
  }
}
```

For more information about the options, check out the .NET Documentation on this topic: https://docs.microsoft.com/en-us/dotnet/core/runtime-config/threading.

### Samples
More samples like calling a managed method with parameters like strings, calling an unmanaged method back from the managed context using function pointers and much more can be found in the unamanged C++ project ([NativeHost.Native/NativeHost.Native.cpp](NativeHost.Native/NativeHost.Native.cpp)) and the managed C# library ([NativeHost.ManagedLib/ManagedApi.cs](NativeHost.ManagedLib/ManagedApi.cs)).

## License
NativeHost is licensed under The Unlicense, see [LICENSE.txt](/LICENSE.txt) for more information.
