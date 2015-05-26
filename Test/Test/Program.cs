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


            //const string SAMPLE_PROFILER_GUID = "{9E2B38F2-7355-4C61-A54F-434B7AC266C0}";
            //const string MY_PROFILER_GUID = SAMPLE_PROFILER_GUID;

            const string MY_PROFILER_GUID = "{8062FF69-AB86-4183-AC28-9B83DE4AA6C1}";

            // set the COR_PROFILER env var. This indicates to the CLR which COM object to
            // instantiate for profiling.
            if (psi.EnvironmentVariables.ContainsKey("COR_PROFILER"))
                psi.EnvironmentVariables["COR_PROFILER"] = MY_PROFILER_GUID;
            else
                psi.EnvironmentVariables.Add("COR_PROFILER", MY_PROFILER_GUID);
        }

        static void Main(string[] args)
        {
            if (args.Length == 1 && args[0] == "--bootstrap")
            {
                Console.WriteLine("Boostrapping hello world ...");

                var psi = new ProcessStartInfo(Process.GetCurrentProcess().Modules[0].FileName);

                SetEnvironmentVariables(psi);

                psi.UseShellExecute = false;
                Process p = Process.Start(psi);
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
