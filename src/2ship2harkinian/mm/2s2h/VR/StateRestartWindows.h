#pragma once
#ifdef _WIN32
#include <windows.h>
#include <filesystem>
#include <cwchar>
#include <stdexcept>
#include <string>
#include <vector>

namespace mmvr::state_restart {
// Kept separate from the updater: this helper never downloads, installs, or
// replaces files. It inherits the exact selected OpenXR runtime/environment.
inline constexpr wchar_t WindowsHelper[] = LR"MMVR(
$ErrorActionPreference='Stop'
$parentId=__PARENT__
$readyName=__READY__
$commitName=__COMMIT__
$executable=__EXECUTABLE__
$directory=__DIRECTORY__
$parent=$null; $ready=$null; $commit=$null
try {
    $ready=[Threading.EventWaitHandle]::OpenExisting($readyName)
    $commit=[Threading.EventWaitHandle]::OpenExisting($commitName)
    $parent=[Diagnostics.Process]::GetProcessById($parentId)
    $null=$parent.Handle # Retain the original process handle; never wait on a reused PID.
    $null=$ready.Set()
    if (-not $commit.WaitOne(7000)) { exit 2 }
    if (-not $parent.WaitForExit(60000)) { throw 'Game cleanup did not finish. Restart the game manually.' }
    $launch=New-Object Diagnostics.ProcessStartInfo
    $launch.FileName=$executable
    $launch.WorkingDirectory=$directory
    $launch.UseShellExecute=$false
    $process=[Diagnostics.Process]::Start($launch)
    if ($null -eq $process) { throw 'Game restart did not create a process.' }
    $process.Dispose()
} catch {
    try {
        $log=Join-Path $directory 'logs'
        $null=[IO.Directory]::CreateDirectory($log)
        [IO.File]::AppendAllText((Join-Path $log 'state-restart.log'),([DateTime]::UtcNow.ToString('o')+' '+$_.Exception.Message+[Environment]::NewLine))
    } catch {}
    exit 1
} finally {
    if ($null -ne $parent) { $parent.Dispose() }
    if ($null -ne $ready) { $ready.Dispose() }
    if ($null -ne $commit) { $commit.Dispose() }
}
)MMVR";

inline std::wstring PowerShellLiteral(const std::wstring& value) {
    std::wstring result = L"'";
    for (wchar_t c : value) { result += c; if (c == L'\'') result += c; }
    return result + L"'";
}
inline void Substitute(std::wstring& script, const wchar_t* name, const std::wstring& value) {
    const auto at = script.find(name);
    if (at == std::wstring::npos) throw std::runtime_error("Restart helper parameter is missing.");
    script.replace(at, std::wcslen(name), value);
}
inline std::wstring EncodedCommand(const std::wstring& script) {
    constexpr wchar_t alphabet[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::wstring result;
    const auto* bytes = reinterpret_cast<const unsigned char*>(script.data());
    const size_t size = script.size() * sizeof(wchar_t);
    for (size_t i = 0; i < size; i += 3) {
        const unsigned bits = (unsigned(bytes[i]) << 16) | (i+1 < size ? unsigned(bytes[i+1]) << 8 : 0) |
                              (i+2 < size ? unsigned(bytes[i+2]) : 0);
        result += alphabet[(bits >> 18) & 63]; result += alphabet[(bits >> 12) & 63];
        result += i+1 < size ? alphabet[(bits >> 6) & 63] : L'=';
        result += i+2 < size ? alphabet[bits & 63] : L'=';
    }
    return result;
}
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value) CloseHandle(value); }
};
inline bool StartWindowsHelper(std::string& error) {
    std::vector<wchar_t> executable(32768);
    const auto count = GetModuleFileNameW(nullptr, executable.data(), DWORD(executable.size()));
    if (!count || count >= executable.size()) { error = "Cannot locate the running executable."; return false; }
    wchar_t system[MAX_PATH]{};
    if (!GetSystemDirectoryW(system, MAX_PATH)) { error = "Cannot locate Windows PowerShell."; return false; }
    // Unique per request; default process security limits event access to this user.
    const auto token = std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    const auto readyName = L"Local\\MMVR-StateReady-" + token;
    const auto commitName = L"Local\\MMVR-StateCommit-" + token;
    Handle ready{CreateEventW(nullptr, TRUE, FALSE, readyName.c_str())};
    Handle commit{CreateEventW(nullptr, TRUE, FALSE, commitName.c_str())};
    if (!ready.value || !commit.value) { error = "Cannot create the restart handshake."; return false; }
    const auto root = std::filesystem::current_path();
    std::wstring script = WindowsHelper;
    Substitute(script, L"__PARENT__", std::to_wstring(GetCurrentProcessId()));
    Substitute(script, L"__READY__", PowerShellLiteral(readyName));
    Substitute(script, L"__COMMIT__", PowerShellLiteral(commitName));
    Substitute(script, L"__EXECUTABLE__", PowerShellLiteral(std::wstring(executable.data(), count)));
    Substitute(script, L"__DIRECTORY__", PowerShellLiteral(root.wstring()));
    const auto shell = std::wstring(system) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    std::wstring command = L"\"" + shell + L"\" -NoLogo -NoProfile -NonInteractive -EncodedCommand " + EncodedCommand(script);
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(shell.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        root.c_str(), &startup, &process)) {
        error = "Cannot start restart helper: " + std::to_string(GetLastError()); return false;
    }
    CloseHandle(process.hThread);
    Handle helper{process.hProcess};
    HANDLE wait[] = {ready.value, helper.value};
    if (WaitForMultipleObjects(2, wait, FALSE, 5000) != WAIT_OBJECT_0) {
        error = "Restart helper did not become ready. Please restart the game manually."; return false;
    }
    // A timed-out caller never grants this event, so an orphan helper exits
    // without unexpectedly launching when the user eventually quits the game.
    if (!SetEvent(commit.value)) { error = "Cannot confirm the restart."; return false; }
    return true;
}
}
#endif
