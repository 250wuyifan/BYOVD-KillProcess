/*
 * 用户态程序 - 完整版本
 *
 * 功能:
 * 1. 通过设备对象发送 I/O 控制请求终止进程
 * 2. 自动检测设备名称（尝试多个常见名称）
 * 3. 详细的调试信息输出
 * 4. 支持多种输入格式
 *
 * 编译:
 *   cl.exe terminate_process_client_complete.c /Fe:terminate_process_client_complete.exe
 *   或: gcc terminate_process_client_complete.c -o terminate_process_client_complete.exe
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psapi.h>  // 添加这个头文件

 // 添加链接库
#pragma comment(lib, "psapi.lib")

 // IOCTL 代码定义
#define IOCTL_TERMINATE_PROCESS  0x22201C  // 根据驱动分析得出的代码

// 常见的设备名称（根据驱动分析得出）
static const wchar_t* COMMON_DEVICE_NAMES[] = {
    L"\\\\.\\Warsaw_PM",             // ✅ 实际设备名称（从驱动分析得出）
    L"\\DosDevices\\Warsaw_PM",      // 完整路径格式
    L"\\\\.\\YourDriverDevice",      // 备用名称
    L"\\\\.\\RootkitDevice",
    L"\\\\.\\SecurityDriver",
    L"\\\\.\\ProcessMonitor",
    L"\\\\.\\KernelDriver",
    NULL
};

// 输入缓冲区结构（1036 字节，根据驱动分析）
typedef struct _TERMINATE_PROCESS_INPUT {
    DWORD ProcessId;           // 进程ID (4字节)
    BYTE  Reserved[1032];     // 保留字段 (1032字节)
} TERMINATE_PROCESS_INPUT, * PTERMINATE_PROCESS_INPUT;

// 输出缓冲区结构
typedef struct _TERMINATE_PROCESS_OUTPUT {
    NTSTATUS Status;           // 返回状态
    DWORD    Reserved[256];    // 额外空间
} TERMINATE_PROCESS_OUTPUT, * PTERMINATE_PROCESS_OUTPUT;

// 颜色输出（如果支持）
#ifdef _WIN32
#define COLOR_RESET   ""
#define COLOR_RED     ""
#define COLOR_GREEN   ""
#define COLOR_YELLOW  ""
#define COLOR_BLUE    ""
#else
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#endif

// 打印带颜色的消息
void PrintSuccess(const char* msg) {
    printf(COLOR_GREEN "[+] %s" COLOR_RESET "\n", msg);
}

void PrintError(const char* msg) {
    printf(COLOR_RED "[-] %s" COLOR_RESET "\n", msg);
}

void PrintInfo(const char* msg) {
    printf(COLOR_BLUE "[*] %s" COLOR_RESET "\n", msg);
}

void PrintWarning(const char* msg) {
    printf(COLOR_YELLOW "[!] %s" COLOR_RESET "\n", msg);
}

// 打印详细的错误信息
void PrintDetailedError(const char* function, DWORD error) {
    PrintError("");
    printf("    函数: %s\n", function);
    printf("    错误代码: 0x%08X (%d)\n", error, error);

    switch (error) {
    case ERROR_FILE_NOT_FOUND:
        printf("    原因: 设备不存在\n");
        printf("    解决: 检查设备名称是否正确，确保驱动程序已加载\n");
        break;
    case ERROR_ACCESS_DENIED:
        printf("    原因: 访问被拒绝\n");
        printf("    解决: 请以管理员权限运行此程序\n");
        break;
    case ERROR_INVALID_HANDLE:
        printf("    原因: 无效的句柄\n");
        break;
    case ERROR_INVALID_PARAMETER:
        printf("    原因: 无效的参数\n");
        printf("    解决: 检查输入缓冲区格式和大小\n");
        break;
    case ERROR_NOT_ENOUGH_MEMORY:
        printf("    原因: 内存不足\n");
        break;
    default:
        printf("    原因: 未知错误\n");
        break;
    }
}

// 检查进程是否存在
BOOL IsProcessRunning(DWORD processId) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
    if (hProcess == NULL) {
        return FALSE;
    }
    CloseHandle(hProcess);
    return TRUE;
}

// 获取进程名称
BOOL GetProcessName(DWORD processId, char* buffer, DWORD bufferSize) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess == NULL) {
        return FALSE;
    }

    char processPath[MAX_PATH];
    if (GetModuleFileNameExA(hProcess, NULL, processPath, MAX_PATH) > 0) {
        // 提取文件名
        char* fileName = strrchr(processPath, '\\');
        if (fileName) {
            fileName++;
            strncpy_s(buffer, bufferSize, fileName, _TRUNCATE);
        }
        else {
            strncpy_s(buffer, bufferSize, processPath, _TRUNCATE);
        }
        CloseHandle(hProcess);
        return TRUE;
    }

    CloseHandle(hProcess);
    return FALSE;
}

// 打开设备对象
HANDLE OpenDevice(const wchar_t* deviceName, BOOL verbose) {
    HANDLE hDevice;

    if (verbose) {
        PrintInfo("正在打开设备...");
        printf("    设备名称: %ls\n", deviceName);
    }

    hDevice = CreateFileW(
        deviceName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        if (verbose) {
            PrintDetailedError("CreateFileW", GetLastError());
        }
        return INVALID_HANDLE_VALUE;
    }

    if (verbose) {
        PrintSuccess("设备打开成功");
        printf("    句柄: 0x%p\n", hDevice);
    }

    return hDevice;
}

// 尝试打开多个可能的设备名称
HANDLE TryOpenDevice(const wchar_t* preferredName, BOOL verbose) {
    HANDLE hDevice;

    // 首先尝试用户指定的设备名称
    if (preferredName && wcslen(preferredName) > 0) {
        hDevice = OpenDevice(preferredName, verbose);
        if (hDevice != INVALID_HANDLE_VALUE) {
            return hDevice;
        }
    }

    // 尝试常见的设备名称
    if (verbose) {
        PrintInfo("尝试常见的设备名称...");
    }

    for (int i = 0; COMMON_DEVICE_NAMES[i] != NULL; i++) {
        if (verbose) {
            printf("    尝试: %ls\n", COMMON_DEVICE_NAMES[i]);
        }

        hDevice = OpenDevice(COMMON_DEVICE_NAMES[i], FALSE);
        if (hDevice != INVALID_HANDLE_VALUE) {
            if (verbose) {
                PrintSuccess("找到设备");
                printf("    设备名称: %ls\n", COMMON_DEVICE_NAMES[i]);
            }
            return hDevice;
        }
    }

    return INVALID_HANDLE_VALUE;
}

// 发送终止进程请求
BOOL TerminateProcessViaDriver(HANDLE hDevice, DWORD processId, BOOL verbose) {
    BOOL success = FALSE;
    DWORD bytesReturned = 0;
    TERMINATE_PROCESS_INPUT input = { 0 };
    TERMINATE_PROCESS_OUTPUT output = { 0 };

    // 填充输入缓冲区
    input.ProcessId = processId;
    // 其他字段保持为0

    if (verbose) {
        PrintInfo("准备发送终止进程请求");
        printf("    进程ID: %d (0x%08X)\n", processId, processId);
        printf("    IOCTL 代码: 0x%08X\n", IOCTL_TERMINATE_PROCESS);
        printf("    输入缓冲区大小: %zu 字节\n", sizeof(input));
        printf("    输出缓冲区大小: %zu 字节\n", sizeof(output));

        // 显示进程信息
        if (IsProcessRunning(processId)) {
            char processName[MAX_PATH] = { 0 };
            if (GetProcessName(processId, processName, MAX_PATH)) {
                printf("    进程名称: %s\n", processName);
            }
            PrintWarning("目标进程当前正在运行");
        }
        else {
            PrintWarning("目标进程可能不存在或已终止");
        }
    }

    // 发送 DeviceIoControl 请求
    success = DeviceIoControl(
        hDevice,
        IOCTL_TERMINATE_PROCESS,
        &input,
        sizeof(input),
        &output,
        sizeof(output),
        &bytesReturned,
        NULL
    );

    if (!success) {
        if (verbose) {
            PrintDetailedError("DeviceIoControl", GetLastError());
        }
        return FALSE;
    }

    if (verbose) {
        PrintSuccess("请求发送成功");
        printf("    返回字节数: %d\n", bytesReturned);
    }

    // 检查返回状态
    if (bytesReturned >= sizeof(NTSTATUS)) {
        NTSTATUS status = output.Status;

        if (verbose) {
            printf("    返回状态 (NTSTATUS): 0x%08X\n", status);
        }

        if (status == 0) {
            PrintSuccess("进程终止成功 (STATUS_SUCCESS)");
            return TRUE;
        }
        else {
            if (verbose) {
                PrintWarning("进程终止可能失败");
                printf("    常见 NTSTATUS 值:\n");
                printf("      0x00000000 - STATUS_SUCCESS\n");
                printf("      0xC0000001 - STATUS_UNSUCCESSFUL\n");
                printf("      0xC000000D - STATUS_INVALID_PARAMETER\n");
                printf("      0xC000000F - STATUS_NO_SUCH_FILE\n");
                printf("      0xC0000135 - STATUS_DLL_NOT_FOUND\n");
            }
            return FALSE;
        }
    }

    return TRUE;
}

// 打印使用说明
void PrintUsage(const char* programName) {
    printf("\n");
    printf("========================================\n");
    printf("  进程终止工具 (通过驱动程序)\n");
    printf("========================================\n");
    printf("\n");
    printf("用法: %s <进程ID> [选项]\n", programName);
    printf("\n");
    printf("参数:\n");
    printf("  进程ID          - 要终止的进程ID (必需)\n");
    printf("\n");
    printf("选项:\n");
    printf("  -d, --device <名称>  - 指定设备名称\n");
    printf("  -q, --quiet         - 安静模式（减少输出）\n");
    printf("  -v, --verbose       - 详细模式（默认）\n");
    printf("  -h, --help          - 显示此帮助信息\n");
    printf("\n");
    printf("示例:\n");
    printf("  %s 1234\n", programName);
    printf("  %s 1234 -d \\\\.\\MyDriver\n", programName);
    printf("  %s 1234 --quiet\n", programName);
    printf("\n");
    printf("注意:\n");
    printf("  - 需要管理员权限运行\n");
    printf("  - 设备名称如果未指定，将尝试常见的名称\n");
    printf("  - 请确保驱动程序已加载\n");
    printf("\n");
}

// 主函数
int main(int argc, char* argv[]) {
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    DWORD processId = 0;
    const wchar_t* deviceName = NULL;
    BOOL verbose = TRUE;
    BOOL quiet = FALSE;

    // 解析命令行参数
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            PrintUsage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            quiet = TRUE;
            verbose = FALSE;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = TRUE;
            quiet = FALSE;
        }
        else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
            if (i + 1 < argc) {
                // 将 ANSI 字符串转换为宽字符串
                int len = MultiByteToWideChar(CP_ACP, 0, argv[++i], -1, NULL, 0);
                wchar_t* wDeviceName = (wchar_t*)malloc(len * sizeof(wchar_t));
                if (wDeviceName) {
                    MultiByteToWideChar(CP_ACP, 0, argv[i], -1, wDeviceName, len);
                    deviceName = wDeviceName;
                }
            }
            else {
                PrintError("设备名称参数缺少值");
                return 1;
            }
        }
        else if (processId == 0) {
            // 第一个非选项参数作为进程ID
            processId = (DWORD)atoi(argv[i]);
            if (processId == 0) {
                PrintError("无效的进程ID");
                printf("    输入: %s\n", argv[i]);
                return 1;
            }
        }
        else {
            PrintWarning("未知参数，忽略");
            printf("    参数: %s\n", argv[i]);
        }
    }

    if (processId == 0) {
        PrintError("未指定进程ID");
        PrintUsage(argv[0]);
        return 1;
    }

    if (!quiet) {
        printf("\n");
        printf("========================================\n");
        printf("  进程终止工具 (通过驱动程序)\n");
        printf("========================================\n");
        printf("\n");

        PrintInfo("配置信息");
        printf("    目标进程ID: %d (0x%08X)\n", processId, processId);
        if (deviceName) {
            printf("    指定设备: %ls\n", deviceName);
        }
        else {
            printf("    设备名称: 自动检测\n");
        }
        printf("    详细模式: %s\n", verbose ? "是" : "否");
        printf("\n");
    }

    // 打开设备
    hDevice = TryOpenDevice(deviceName, verbose);
    if (hDevice == INVALID_HANDLE_VALUE) {
        PrintError("无法打开设备");
        if (!quiet) {
            printf("\n");
            printf("可能的解决方案:\n");
            printf("  1. 检查设备名称是否正确\n");
            printf("  2. 确保驱动程序已加载\n");
            printf("  3. 使用 -d 参数指定正确的设备名称\n");
            printf("  4. 以管理员权限运行\n");
        }

        if (deviceName) {
            free((void*)deviceName);
        }
        return 1;
    }

    // 发送终止进程请求
    if (!quiet) {
        printf("\n");
    }

    BOOL success = TerminateProcessViaDriver(hDevice, processId, verbose);

    // 验证进程是否被终止
    if (success && verbose) {
        Sleep(500);  // 等待一下
        if (!IsProcessRunning(processId)) {
            PrintSuccess("确认: 目标进程已终止");
        }
        else {
            PrintWarning("警告: 目标进程可能仍在运行");
        }
    }

    // 清理
    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
        if (verbose) {
            PrintInfo("设备句柄已关闭");
        }
    }

    if (deviceName) {
        free((void*)deviceName);
    }

    if (!quiet) {
        printf("\n");
    }

    return success ? 0 : 1;
}

