#include "SystemInfo.h"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cstdio>

#ifdef _WIN32
    #include <lmcons.h>
    #include <objbase.h>
    #include <comdef.h>
    #include <sysinfoapi.h>
    #include <winioctl.h>
    #pragma comment(lib, "ole32.lib")
#endif

SystemInfo::SystemInfo() {
}

SystemInfo::~SystemInfo() {
}

// 宽字符转多字节字符
std::string SystemInfo::wideCharToMultiByte(const wchar_t* wideStr) const {
    if (!wideStr) return "";
    
#ifdef _WIN32
    int size = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
    if (size == 0) return "";
    
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, &result[0], size, nullptr, nullptr);
    return result;
#else
    return "";
#endif
}

// 获取当前登录用户名
std::string SystemInfo::getCurrentUser() const {
#ifdef _WIN32
    wchar_t username[UNLEN + 1];
    DWORD usernameLen = UNLEN + 1;
    
    if (GetUserNameW(username, &usernameLen)) {
        return wideCharToMultiByte(username);
    }
#endif
    return "unknown";
}

// 获取计算机名
std::string SystemInfo::getComputerName() const {
#ifdef _WIN32
    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD computerNameLen = MAX_COMPUTERNAME_LENGTH + 1;
    
    if (GetComputerNameW(computerName, &computerNameLen)) {
        return wideCharToMultiByte(computerName);
    }
#endif
    return "unknown";
}

// 获取系统版本信息（使用新版API替代已弃用的GetVersionExW）
std::string SystemInfo::getOSVersion() const {
#ifdef _WIN32
    // 使用RtlGetVersion或VerifyVersionInfo API
    typedef NTSTATUS (WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (fxPtr) {
            RTL_OSVERSIONINFOW rovi = { 0 };
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            
            if (fxPtr(&rovi) == 0) {
                std::ostringstream oss;
                oss << "Windows " << rovi.dwMajorVersion << "." << rovi.dwMinorVersion 
                    << " Build " << rovi.dwBuildNumber;
                
                // 判断是工作站还是服务器
                OSVERSIONINFOEXW osvi = { 0 };
                osvi.dwOSVersionInfoSize = sizeof(osvi);
                osvi.dwMajorVersion = rovi.dwMajorVersion;
                osvi.dwMinorVersion = rovi.dwMinorVersion;
                osvi.dwBuildNumber = rovi.dwBuildNumber;
                
                DWORDLONG conditionMask = 0;
                VER_SET_CONDITION(conditionMask, VER_PRODUCT_TYPE, VER_EQUAL);
                
                osvi.wProductType = VER_NT_WORKSTATION;
                if (VerifyVersionInfoW(&osvi, VER_PRODUCT_TYPE, conditionMask)) {
                    oss << " (Workstation)";
                } else {
                    oss << " (Server)";
                }
                
                return oss.str();
            }
        }
    }
#endif
    return "unknown";
}

// 获取系统位数
int SystemInfo::getSystemBits() const {
#ifdef _WIN32
    SYSTEM_INFO systemInfo;
    GetNativeSystemInfo(&systemInfo);
    
    switch (systemInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
        case PROCESSOR_ARCHITECTURE_IA64:
            return 64;
        case PROCESSOR_ARCHITECTURE_INTEL:
            return 32;
        default:
            return 32;
    }
#endif
    return 0;
}

// 格式化MAC地址
std::string SystemInfo::formatMacAddress(const unsigned char* mac, size_t length) const {
    std::ostringstream oss;
    for (size_t i = 0; i < length; i++) {
        if (i > 0) oss << ":";
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mac[i]);
    }
    return oss.str();
}

// 辅助函数：转义JSON字符串中的特殊字符
std::string escapeJsonString(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '\"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

// 辅助函数：将网卡信息转换为JSON字符串
std::string adapterInfoToJson(const NetworkAdapterInfo& adapter) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "    \"adapterName\": \"" << escapeJsonString(adapter.adapterName) << "\",\n";
    oss << "    \"macAddress\": \"" << escapeJsonString(adapter.macAddress) << "\",\n";
    oss << "    \"serviceName\": \"" << escapeJsonString(adapter.serviceName) << "\",\n";
    oss << "    \"ipAddresses\": [";
    
    for (size_t i = 0; i < adapter.ipAddresses.size(); i++) {
        if (i > 0) oss << ", ";
        oss << "\"" << escapeJsonString(adapter.ipAddresses[i]) << "\"";
    }
    
    oss << "]\n";
    oss << "}";
    
    return oss.str();
}

// 获取所有网卡信息（独立初始化Winsock）
std::vector<NetworkAdapterInfo> SystemInfo::getNetworkAdapters() const {
    std::vector<NetworkAdapterInfo> adapters;
    
#ifdef _WIN32
    // 仅在需要时初始化Winsock
    WSADATA wsaData;
    bool wsInitialized = false;
    
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        wsInitialized = true;
    }
    
    ULONG bufferSize = 15000;
    PIP_ADAPTER_INFO pAdapterInfo = (PIP_ADAPTER_INFO)malloc(bufferSize);
    
    if (!pAdapterInfo) {
        if (wsInitialized) WSACleanup();
        return adapters;
    }
    
    ULONG result = GetAdaptersInfo(pAdapterInfo, &bufferSize);
    
    // 如果缓冲区不够，重新分配
    if (result == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (PIP_ADAPTER_INFO)malloc(bufferSize);
        if (!pAdapterInfo) {
            if (wsInitialized) WSACleanup();
            return adapters;
        }
        result = GetAdaptersInfo(pAdapterInfo, &bufferSize);
    }
    
    if (result == NO_ERROR) {
        PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
        
        while (pAdapter) {
            NetworkAdapterInfo adapter;
            
            // 网卡名称
            adapter.adapterName = pAdapter->Description;
            
            // MAC地址
            adapter.macAddress = formatMacAddress(pAdapter->Address, pAdapter->AddressLength);
            
            // 服务名
            adapter.serviceName = pAdapter->AdapterName;
            
            // IP地址列表
            IP_ADDR_STRING* pIpAddr = &(pAdapter->IpAddressList);
            while (pIpAddr) {
                std::string ip = pIpAddr->IpAddress.String;
                // 只添加非空IP
                if (!ip.empty() && ip != "0.0.0.0") {
                    adapter.ipAddresses.push_back(ip);
                }
                pIpAddr = pIpAddr->Next;
            }
            
            adapters.push_back(adapter);
            pAdapter = pAdapter->Next;
        }
    }
    
    if (pAdapterInfo) {
        free(pAdapterInfo);
    }
    
    // 清理Winsock
    if (wsInitialized) {
        WSACleanup();
    }
#endif
    
    return adapters;
}

// 保存网卡信息到JSON文件
bool SystemInfo::saveNetworkAdaptersToJson(const std::string& filePath) const {
    std::vector<NetworkAdapterInfo> adapters = getNetworkAdapters();
    
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "{\n";
    file << "    \"networkAdapters\": [\n";
    
    for (size_t i = 0; i < adapters.size(); i++) {
        file << "        " << adapterInfoToJson(adapters[i]);
        if (i < adapters.size() - 1) {
            file << ",";
        }
        file << "\n";
    }
    
    file << "    ]\n";
    file << "}\n";
    
    file.close();
    return true;
}

// 获取硬盘序列号（真实物理序列号）
std::string SystemInfo::getHardDiskSerialNumber() const {
#ifdef _WIN32
    // 方法1：使用WMI查询（需要COM初始化）
    // 方法2：使用DeviceIoControl直接访问硬盘
    // 这里使用方法2，更轻量
    
    HANDLE hDevice = CreateFileW(
        L"\\\\.\\PhysicalDrive0",  // 第一个物理硬盘
        0,                          // 不需要访问权限
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,                       // 默认安全属性
        OPEN_EXISTING,              // 打开已存在的设备
        0,                          // 无特殊属性
        NULL                        // 无模板文件
    );
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        // 如果无法访问PhysicalDrive0，尝试其他方法
        return getHardDiskSerialFromVolume();
    }
    
    // 使用ATA命令获取硬盘信息
    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    
    BYTE buffer[1024] = {};
    DWORD bytesReturned = 0;
    
    if (DeviceIoControl(
        hDevice,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &query,
        sizeof(query),
        buffer,
        sizeof(buffer),
        &bytesReturned,
        NULL)) {
        
        STORAGE_DEVICE_DESCRIPTOR* deviceDescriptor = (STORAGE_DEVICE_DESCRIPTOR*)buffer;
        
        if (deviceDescriptor->SerialNumberOffset != 0) {
            char* serialNumber = (char*)(buffer + deviceDescriptor->SerialNumberOffset);
            std::string result(serialNumber);
            
            // 去除首尾空格
            size_t start = result.find_first_not_of(" \t\n\r");
            size_t end = result.find_last_not_of(" \t\n\r");
            
            if (start != std::string::npos && end != std::string::npos) {
                result = result.substr(start, end - start + 1);
            }
            
            CloseHandle(hDevice);
            
            if (!result.empty()) {
                return result;
            }
        }
    }
    
    CloseHandle(hDevice);
    
    // 如果上述方法失败，回退到卷序列号
    return getHardDiskSerialFromVolume();
#endif
    return "unknown";
}

// 备用方法：从卷信息获取序列号
std::string SystemInfo::getHardDiskSerialFromVolume() const {
#ifdef _WIN32
    wchar_t volumeName[MAX_PATH] = {0};
    wchar_t fileSystemName[MAX_PATH] = {0};
    DWORD serialNumber = 0;
    DWORD maxComponentLength = 0;
    DWORD fileSystemFlags = 0;
    
    if (GetVolumeInformationW(
        L"C:\\",
        volumeName,
        MAX_PATH,
        &serialNumber,
        &maxComponentLength,
        &fileSystemFlags,
        fileSystemName,
        MAX_PATH)) {
        
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << serialNumber;
        return oss.str();
    }
#endif
    return "unknown";
}

// 获取系统唯一GUID
std::string SystemInfo::getSystemGUID() const {
#ifdef _WIN32
    GUID guid;
    HRESULT hr = CoCreateGuid(&guid);
    
    if (SUCCEEDED(hr)) {
        char guidString[39];
        sprintf_s(guidString, sizeof(guidString),
            "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        
        return std::string(guidString);
    }
#endif
    return "unknown";
}

// 获取完整系统信息报告
std::string SystemInfo::getFullSystemInfo() const {
    std::ostringstream oss;
    
    oss << "=== 系统信息报告 ===" << std::endl;
    oss << "当前用户: " << getCurrentUser() << std::endl;
    oss << "计算机名: " << getComputerName() << std::endl;
    oss << "系统版本: " << getOSVersion() << std::endl;
    oss << "系统位数: " << getSystemBits() << "位" << std::endl;
    oss << "硬盘序列号: " << getHardDiskSerialNumber() << std::endl;
    oss << "系统GUID: " << getSystemGUID() << std::endl;
    oss << std::endl;
    
    oss << "=== 网卡信息 ===" << std::endl;
    std::vector<NetworkAdapterInfo> adapters = getNetworkAdapters();
    
    for (size_t i = 0; i < adapters.size(); i++) {
        oss << "网卡 " << (i + 1) << ":" << std::endl;
        oss << "  名称: " << adapters[i].adapterName << std::endl;
        oss << "  MAC地址: " << adapters[i].macAddress << std::endl;
        oss << "  服务名: " << adapters[i].serviceName << std::endl;
        oss << "  IP地址: ";
        
        for (size_t j = 0; j < adapters[i].ipAddresses.size(); j++) {
            if (j > 0) oss << ", ";
            oss << adapters[i].ipAddresses[j];
        }
        oss << std::endl;
        oss << std::endl;
    }
    
    return oss.str();
}
