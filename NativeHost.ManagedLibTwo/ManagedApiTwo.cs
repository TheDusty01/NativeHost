using System.Runtime.InteropServices;

namespace NativeHost.ManagedLibTwo
{
    public class ManagedApiTwo
    {

        [UnmanagedCallersOnly]
        public static void Main()
        {
            Console.WriteLine($"C# Main");
        }
    }
}