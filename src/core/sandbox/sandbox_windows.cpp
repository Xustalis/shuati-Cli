#ifdef _WIN32
#include "shuati/sandbox.hpp"
#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include "shuati/utils/encoding.hpp"

namespace shuati {
namespace sandbox {

class WindowsSandbox : public ISandbox {
public:
    WindowsSandbox() = default;
    ~WindowsSandbox() override = default;

    SandboxResult execute(
        const std::string& executable_path,
        const std::vector<std::string>& args,
        const std::string& input_file,
        const std::string& output_file,
        const std::string& error_file,
        const SandboxLimits& limits
    ) override {
        SandboxResult result;
        result.status = SandboxResultStatus::InternalError;
        result.exit_code = -1;
        result.cpu_time_ms = 0;
        result.memory_mb = 0;

        // 1. Create Job Object
        HANDLE hJob = CreateJobObjectW(NULL, NULL);
        if (hJob == NULL) {
            result.internal_message = "Failed to create Job Object";
            return result;
        }

        // 2. Configure Limits
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
        jeli.BasicLimitInformation.LimitFlags = 
            JOB_OBJECT_LIMIT_PROCESS_MEMORY |      // Memory limit
            JOB_OBJECT_LIMIT_JOB_MEMORY |
            JOB_OBJECT_LIMIT_ACTIVE_PROCESS |      // active processes limit
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |   // Kill processes when job is closed (e.g. by us exiting)
            JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;

        jeli.ProcessMemoryLimit = (SIZE_T)limits.memory_mb * 1024 * 1024;
        jeli.JobMemoryLimit = (SIZE_T)limits.memory_mb * 1024 * 1024;
        jeli.BasicLimitInformation.ActiveProcessLimit = 1; // Prevent fork bombs

        if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
            CloseHandle(hJob);
            result.internal_message = "Failed to set Job Object memory and process limits";
            return result;
        }

        // 3. Prepare I/O Redirection
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE }; // Inheritable handles
        HANDLE hIn = INVALID_HANDLE_VALUE;
        HANDLE hOut = INVALID_HANDLE_VALUE;
        HANDLE hErr = INVALID_HANDLE_VALUE;

        if (!input_file.empty()) {
            std::wstring w = shuati::utils::utf8_to_wide(input_file);
            hIn = CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        }
        if (!output_file.empty()) {
            std::wstring w = shuati::utils::utf8_to_wide(output_file);
            hOut = CreateFileW(w.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        }
        if (!error_file.empty()) {
            std::wstring w = shuati::utils::utf8_to_wide(error_file);
            hErr = CreateFileW(w.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        }

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput = (hIn != INVALID_HANDLE_VALUE) ? hIn : GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = (hOut != INVALID_HANDLE_VALUE) ? hOut : GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = (hErr != INVALID_HANDLE_VALUE) ? hErr : GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi = {0};

        // 4. Build command line string safely
        std::string cmd = "\"" + executable_path + "\"";
        for (const auto& arg : args) {
            cmd +=(" \"" + arg + "\"");
        }

        std::wstring w_cmd = shuati::utils::utf8_to_wide(cmd);
        std::vector<wchar_t> cmd_buf(w_cmd.begin(), w_cmd.end());
        cmd_buf.push_back(L'\0');

        // 5. Create Process SUSPENDED
        BOOL cp_ret = CreateProcessW(
            NULL,               // Application name
            cmd_buf.data(),     // Command line
            NULL,               // Process security attributes
            NULL,               // Thread security attributes
            TRUE,               // Inherit handles
            CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB,  // Required for AssignProcessToJobObject
            NULL,               // Environment
            NULL,               // Current directory
            &si,                // Startup info
            &pi                 // Process info
        );

        if (!cp_ret) {
            if (hIn != INVALID_HANDLE_VALUE) CloseHandle(hIn);
            if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);
            if (hErr != INVALID_HANDLE_VALUE) CloseHandle(hErr);
            CloseHandle(hJob);
            result.internal_message = "CreateProcessFailed";
            return result;
        }

        // Close our ends of the handles (the child inherited them)
        if (hIn != INVALID_HANDLE_VALUE) CloseHandle(hIn);
        if (hOut != INVALID_HANDLE_VALUE) CloseHandle(hOut);
        if (hErr != INVALID_HANDLE_VALUE) CloseHandle(hErr);

        // 6. Assign Process to Job Object
        if (!AssignProcessToJobObject(hJob, pi.hProcess)) {
            TerminateProcess(pi.hProcess, ~0U);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hJob);
            result.internal_message = "AssignProcessToJobObject failed";
            return result;
        }

        // 7. Resume the Process
        ResumeThread(pi.hThread);

        // 8. Wait for Process with Time Limit
        auto wait_res = WaitForSingleObject(pi.hProcess, (DWORD)limits.cpu_time_ms);

        if (wait_res == WAIT_TIMEOUT) {
            result.status = SandboxResultStatus::TimeLimitExceeded;
            TerminateProcess(pi.hProcess, ~0U); // Force kill
        } else {
            // Process terminated (either normally, via crash, or via job object MLE)
            DWORD exit_code = 0;
            GetExitCodeProcess(pi.hProcess, &exit_code);
            result.exit_code = (int)exit_code;

            // Check if it was killed by Job Object due to memory limit
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {0};
            if (QueryInformationJobObject(hJob, JobObjectExtendedLimitInformation, &info, sizeof(info), NULL)) {
                SIZE_T limit_bytes = (SIZE_T)limits.memory_mb * 1024 * 1024;
                // Allow a larger 10% or 5MB leniency since std::vector reallocations can fail with bad_alloc
                // well before the exact byte limit is hit (e.g., trying to allocate 10MB when only 8MB is free).
                SIZE_T threshold = limit_bytes - (limit_bytes / 10); // 90%
                if (threshold < limit_bytes - 5 * 1024 * 1024) {
                    threshold = limit_bytes - 5 * 1024 * 1024;
                }

                if (info.PeakProcessMemoryUsed >= threshold || info.PeakJobMemoryUsed >= threshold) {
                    // If it crashed or hit the threshold limit closely, it is MLE
                    if (exit_code != 0 || info.PeakProcessMemoryUsed >= limit_bytes) {
                        result.status = SandboxResultStatus::MemoryLimitExceeded;
                    }
                }
            }
            
            // Check for Runtime Error
            if (result.status != SandboxResultStatus::MemoryLimitExceeded) {
                 if (exit_code != 0) {
                     result.status = SandboxResultStatus::RuntimeError;
                 } else {
                     result.status = SandboxResultStatus::OK;
                 }
            }

            // Retrieve resource usage
            FILETIME creationTime, exitTime, kernelTime, userTime;
            if (GetProcessTimes(pi.hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
                ULARGE_INTEGER k, u;
                k.HighPart = kernelTime.dwHighDateTime;
                k.LowPart = kernelTime.dwLowDateTime;
                u.HighPart = userTime.dwHighDateTime;
                u.LowPart = userTime.dwLowDateTime;
                // Time is in 100-nanosecond intervals. Multiply by 1e-4 to get ms.
                result.cpu_time_ms = (k.QuadPart + u.QuadPart) / 10000;
            }

            if (QueryInformationJobObject(hJob, JobObjectExtendedLimitInformation, &info, sizeof(info), NULL)) {
                result.memory_mb = (info.PeakProcessMemoryUsed) / (1024 * 1024);
            }
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        CloseHandle(hJob);

        return result;
    }
};

std::unique_ptr<ISandbox> create_sandbox() {
    return std::make_unique<WindowsSandbox>();
}

} // namespace sandbox
} // namespace shuati
#endif // _WIN32
