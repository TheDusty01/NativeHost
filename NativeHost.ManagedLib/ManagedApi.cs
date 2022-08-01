using System;
using System.Reflection;
using System.Runtime.InteropServices;
using NativeHost.ManagedLib;

namespace NativeHost.ManagedLib
{

    public unsafe class ManagedApi
    {
        // Function pointer types:
        //delegate* managed<void> func
        //delegate* unmanaged<void> func
        //delegate* unmanaged[Cdecl]<void> func
        //delegate* unmanaged[Stdcall]<void> func
        //delegate* unmanaged[Fastcall]<void> func
        //delegate* unmanaged[Thiscall]<void> func

        public const string InternalDllImport = "__Internal";
        public static IntPtr MainProgramHandle = IntPtr.Zero;

        [DllImport(InternalDllImport/*, EntryPoint = "Test"*/)]
        internal static extern void SomeExportedFunction();

        [UnmanagedCallersOnly]
        public static void Main(IntPtr mainProgramHandle)
        {
            Console.WriteLine($"C# Main");

            MainProgramHandle = mainProgramHandle;

            NativeLibrary.SetDllImportResolver(Assembly.GetExecutingAssembly(), (string libraryName, Assembly assembly, DllImportSearchPath? searchPath) =>
            {
                if (libraryName == InternalDllImport)
                    return MainProgramHandle;

                NativeLibrary.TryLoad(libraryName, assembly, searchPath, out IntPtr handle);
                return handle;
            });

            SomeExportedFunction();
        }

        [UnmanagedCallersOnly]
        public static void Print(NativeString message)
        {
            string msg = message.ToString();
            Console.WriteLine($"C# Print: {msg}");
        }

        [UnmanagedCallersOnly]
        public static void DoStuff(delegate* unmanaged<bool, void> func)
        {
            Console.WriteLine($"C# DoStuff");
            func(true);
        }

        [UnmanagedCallersOnly]
        public static void DoStuffStruct(delegate* unmanaged<NativeString, void> func)
        {
            Console.WriteLine($"C# DoStuffStruct");

            var ns = NativeString.FromString("xyz");
            func(ns);
            ns.Dispose();
        }

        [UnmanagedCallersOnly]
        public static NativeString JustReturn()
        {
            Console.WriteLine($"C# Print without message");

            return NativeString.FromString($"C# replied!");
        }

        [UnmanagedCallersOnly]
        public static NativeString PrintAndReturn(NativeString message)
        {
            string msg = message.ToString();
            Console.WriteLine($"C# Print: {msg}");

            return NativeString.FromString($"C# reply: {msg}");
        }

    }

}