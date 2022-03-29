using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace NativeHost.ManagedLib
{

    [StructLayout(LayoutKind.Sequential)]
    public struct NativeString : IDisposable
    {
        public IntPtr rawString;

        public override string ToString()
        {
            //return RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            //    ? Marshal.PtrToStringUni(rawString)
            //    : Marshal.PtrToStringUTF8(rawString);

            return Marshal.PtrToStringUTF8(rawString);
        }

        public static NativeString FromString(string text)
        {
            return new NativeString
            {
                rawString = Marshal.StringToCoTaskMemUTF8(text)
                //rawString = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
                //? Marshal.StringToHGlobalUni(text)
                //: Marshal.StringToCoTaskMemUTF8(text)
            };
        }

        public void Dispose()
        {
            Marshal.FreeHGlobal(rawString);
        }
    }

}
