#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <string>
#include <vector>
#include <map>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <iphlpapi.h>
    #pragma comment(lib, "iphlpapi.lib")
    #pragma comment(lib, "ws2_32.lib")
#endif

// 网卡信息结构
struct NetworkAdapterInfo {
    std::string adapterName;      // 网卡名称
    std::string macAddress;       // MAC地址
    std::vector<std::string> ipAddresses;  // IP地址列表
    std::string serviceName;      // 服务名
};

// 辅助函数：转义JSON字符串中的特殊字符
std::string escapeJsonString(const std::string& str);

// 辅助函数：将网卡信息转换为JSON字符串
std::string adapterInfoToJson(const NetworkAdapterInfo& adapter);

// 系统信息类
class SystemInfo {
public:
    SystemInfo();
    ~SystemInfo();

    // 获取当前登录用户名
    std::string getCurrentUser() const;
    
    // 获取计算机名
    std::string getComputerName() const;
    
    // 获取系统版本信息
    std::string getOSVersion() const;
    
    // 获取系统位数(32或64)
    int getSystemBits() const;
    
    // 获取硬盘序列号
    std::string getHardDiskSerialNumber() const;
    
    // 获取系统唯一GUID
    std::string getSystemGUID() const;
    
    // 获取所有网卡信息
    std::vector<NetworkAdapterInfo> getNetworkAdapters() const;
    
    // 将所有网卡信息保存为JSON文件
    bool saveNetworkAdaptersToJson(const std::string& filePath) const;
    
    // 获取完整系统信息报告
    std::string getFullSystemInfo() const;

private:
    // 辅助函数：宽字符转多字节字符
    std::string wideCharToMultiByte(const wchar_t* wideStr) const;
    
    // 辅助函数：格式化MAC地址
    std::string formatMacAddress(const unsigned char* mac, size_t length) const;
    
    // 辅助函数：从卷信息获取序列号（备用方法）
    std::string getHardDiskSerialFromVolume() const;
};

#endif // SYSTEM_INFO_H
