# BYOVD-KillProcess

通过 BYOVD (Bring Your Own Vulnerable Driver) 技术终止进程的工具

## 📋 项目简介

本项目演示了如何利用有漏洞的驱动程序来终止进程。BYOVD (Bring Your Own Vulnerable Driver) 是一种攻击技术，攻击者利用合法但存在漏洞的驱动程序来实现恶意操作，绕过安全软件的检测。

### ⚠️ 重要声明

**本项目仅用于安全研究和教育目的。请勿用于非法用途。使用者需自行承担使用风险，作者不对任何滥用行为负责。**

## 🎯 功能特性

- ✅ **通过驱动终止进程** - 利用驱动程序的 IOCTL 接口终止指定进程
- ✅ **自动设备检测** - 自动尝试多个常见的设备名称
- ✅ **详细调试信息** - 提供详细的执行过程和错误信息
- ✅ **进程验证** - 自动验证进程是否成功终止
- ✅ **多种输入格式** - 支持命令行参数和选项

## 🏗️ 项目结构

```
BYOVD-KillProcess/
├── Project3/
│   ├── FileName.c          # 主程序源代码
│   ├── Project3.vcxproj    # Visual Studio 项目文件
│   ├── Project3.rc         # 资源文件
│   └── resource.h          # 资源头文件
├── Project3.sln            # Visual Studio 解决方案
└── README.md               # 项目说明文档
```

## 🔧 技术原理

### BYOVD (Bring Your Own Vulnerable Driver)

BYOVD 是一种攻击技术，攻击者利用合法但存在漏洞的驱动程序来实现恶意操作。这种技术的优势在于：

1. **绕过检测** - 使用合法的驱动程序，减少被安全软件检测的风险
2. **权限提升** - 驱动程序运行在内核模式，具有更高的权限
3. **隐蔽性强** - 利用系统已有的驱动，不引入新的恶意文件

### 工作流程

```
1. 打开设备对象 (CreateFileW)
   ↓
2. 准备 IOCTL 请求
   - 填充进程ID
   - 设置输入缓冲区 (1036 字节)
   ↓
3. 发送 DeviceIoControl 请求
   - IOCTL 代码: 0x22201C
   - 驱动程序在内核模式执行终止操作
   ↓
4. 验证进程是否被终止
```

### 关键代码结构

#### 输入缓冲区结构

```c
typedef struct _TERMINATE_PROCESS_INPUT {
    DWORD ProcessId;           // 进程ID (4字节)
    BYTE  Reserved[1032];      // 保留字段 (1032字节)
} TERMINATE_PROCESS_INPUT;
```

#### IOCTL 代码

```c
#define IOCTL_TERMINATE_PROCESS  0x22201C
```

#### 设备名称

程序会自动尝试以下设备名称：

- `\\.\Warsaw_PM` (主要设备名称)
- `\DosDevices\Warsaw_PM`
- `\\.\YourDriverDevice`
- 其他常见设备名称

## 🚀 编译环境

- **开发工具**: Visual Studio 2019/2022
- **平台工具集**: v143
- **支持平台**: Win32, x64, ARM64
- **Windows SDK**: 10.0
- **依赖库**: psapi.lib (用于获取进程信息)

### 编译步骤

1. 使用 Visual Studio 打开 `Project3.sln`
2. 选择目标平台（推荐 x64 Release）
3. 生成解决方案 (Build Solution)

### 命令行编译

```bash
# 使用 MSVC 编译器
cl.exe FileName.c /Fe:Project3.exe /link psapi.lib

# 使用 GCC (MinGW)
gcc FileName.c -o Project3.exe -lpsapi
```

## 📖 使用方法

### 基本用法

```bash
# 终止指定进程ID的进程
Project3.exe <进程ID>

# 示例：终止进程ID为 1234 的进程
Project3.exe 1234
```

### 命令行选项

```bash
# 指定设备名称
Project3.exe <进程ID> -d <设备名称>

# 详细模式（默认）
Project3.exe <进程ID> -v

# 安静模式（减少输出）
Project3.exe <进程ID> -q

# 显示帮助信息
Project3.exe -h
```

### 使用示例

```bash
# 基本用法
Project3.exe 1234

# 指定设备名称
Project3.exe 1234 -d \\.\Warsaw_PM

# 安静模式
Project3.exe 1234 --quiet

# 详细模式（显示详细信息）
Project3.exe 1234 --verbose
```

## ⚙️ 配置说明

### 设备名称配置

如果默认设备名称不匹配，可以通过以下方式修改：

1. **命令行指定** - 使用 `-d` 参数
2. **修改代码** - 在 `COMMON_DEVICE_NAMES` 数组中添加或修改设备名称

```c
static const wchar_t* COMMON_DEVICE_NAMES[] = {
    L"\\\\.\\Warsaw_PM",        // 你的设备名称
    L"\\\\.\\YourDevice",       // 添加更多设备名称
    NULL
};
```

### IOCTL 代码配置

如果驱动程序的 IOCTL 代码不同，需要修改：

```c
#define IOCTL_TERMINATE_PROCESS  0x22201C  // 修改为你的 IOCTL 代码
```

## ⚠️ 注意事项

### 使用限制

1. **需要管理员权限** - 必须管理员权限运行
2. **需要驱动支持** - 目标系统必须加载了相应的驱动程序
3. **进程保护** - 无法终止受保护的进程（如系统关键进程）
4. **仅用于研究** - 本工具仅用于安全研究和学习目的

### 常见错误

#### ERROR_FILE_NOT_FOUND (2)
- **原因**: 设备不存在
- **解决**: 检查设备名称是否正确，确保驱动程序已加载

#### ERROR_ACCESS_DENIED (5)
- **原因**: 访问被拒绝
- **解决**: 以管理员权限运行程序

#### ERROR_INVALID_PARAMETER (87)
- **原因**: 无效的参数
- **解决**: 检查输入缓冲区格式和大小

## 🔍 安全研究应用

### 合法用途

- ✅ 安全研究和教育
- ✅ 漏洞分析和验证
- ✅ 安全软件测试
- ✅ 恶意软件分析

### 防御措施

1. **驱动签名验证** - 启用驱动签名强制验证
2. **驱动加载监控** - 监控异常的驱动加载行为
3. **进程保护** - 使用进程保护机制（PPL）
4. **行为分析** - 分析异常的进程终止行为

## 📚 相关资源

### 参考资料

- [BYOVD - MITRE ATT&CK](https://attack.mitre.org/techniques/T1068/)
- [DeviceIoControl - Microsoft Docs](https://docs.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-deviceiocontrol)
- [Windows Driver Development](https://docs.microsoft.com/en-us/windows-hardware/drivers/)

### 相关技术

- **BYOVD** - Bring Your Own Vulnerable Driver
- **IOCTL** - Input/Output Control
- **Kernel Mode** - 内核模式操作
- **Process Termination** - 进程终止技术

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

本项目仅用于教育和研究目的。请遵守相关法律法规，不得用于非法用途。

---

## 📸 项目截图

以下是项目的演示截图：

![image-20251128110411072](https://raw.githubusercontent.com/250wuyifan/WaiLian250/main/img/image-20251128110411072.png)

![image-20251128110429741](https://raw.githubusercontent.com/250wuyifan/WaiLian250/main/img/image-20251128110429741.png)

![a763ecee629dbd0b617fba27834cc1ca](https://raw.githubusercontent.com/250wuyifan/WaiLian250/main/img/a763ecee629dbd0b617fba27834cc1ca.png)

![5ff9647bf511434a07e8380d3d80adc1](https://raw.githubusercontent.com/250wuyifan/WaiLian250/main/img/5ff9647bf511434a07e8380d3d80adc1.png)

