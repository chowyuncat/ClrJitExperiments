using System;
using System.Diagnostics;

namespace Test
{
    class Program
    {

        static void SetEnvironmentVariables(ProcessStartInfo psi)
        {
            // set the Cor_Enable_Profiling env var. This indicates to the CLR whether or
            // not we are using the profiler at all.  1 = yes, 0 = no.
            if (psi.EnvironmentVariables.ContainsKey("COR_ENABLE_PROFILING"))
                psi.EnvironmentVariables["COR_ENABLE_PROFILING"] = "1";
            else
                psi.EnvironmentVariables.Add(
                    "COR_ENABLE_PROFILING", "1");

            // CLR V4 does will not load a CLR V2 profiler by default
            if (psi.EnvironmentVariables.ContainsKey("COMPLUS_ProfAPI_ProfilerCompatibilitySetting"))
                psi.EnvironmentVariables["COMPLUS_ProfAPI_ProfilerCompatibilitySetting"] = "EnableV2Profiler";
            else
                psi.EnvironmentVariables.Add(
                   "COMPLUS_ProfAPI_ProfilerCompatibilitySetting", "EnableV2Profiler");

            const string MY_PROFILER_GUID = "{8062FF69-AB86-4183-AC28-9B83DE4AA6C1}";

            // set the COR_PROFILER env var. This indicates to the CLR which COM object to
            // instantiate for profiling.
            if (psi.EnvironmentVariables.ContainsKey("COR_PROFILER"))
                psi.EnvironmentVariables["COR_PROFILER"] = MY_PROFILER_GUID;
            else
                psi.EnvironmentVariables.Add("COR_PROFILER", MY_PROFILER_GUID);
        }

        static void BootstrapSelf()
        {
            Console.WriteLine("Boostrapping hello world ...");

            string exeName = Process.GetCurrentProcess().Modules[0].FileName;

            var psi = new ProcessStartInfo(exeName);

            SetEnvironmentVariables(psi);

            psi.UseShellExecute = false;
            Process p = Process.Start(psi);
        }

        static void Main(string[] args)
        {
            if (args.Length == 1 && args[0] == "--bootstrap")
            {
                BootstrapSelf();
            }
            else
            {
                var p = Process.GetCurrentProcess();
                System.Diagnostics.Debug.WriteLine("PID: [{0}] Test Hello World!", p.Id);
                Console.WriteLine("PID: [{0}] Test Hello World!", p.Id);
            }
        }
    }
}
