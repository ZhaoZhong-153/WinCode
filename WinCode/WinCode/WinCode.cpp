// WinCode.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "SystemInfo.h"
#include <iostream>
#include <fstream>

int main() {
	// 初始化COM库（用于GUID生成）
#ifdef _WIN32
	CoInitialize(NULL);
#endif

	SystemInfo sysInfo;

	// 显示完整系统信息
	std::cout << sysInfo.getFullSystemInfo() << std::endl;

	// 保存网卡信息到JSON文件
	std::string jsonPath = "network_adapters.json";
	if (sysInfo.saveNetworkAdaptersToJson(jsonPath)) {
		std::cout << "网卡信息已保存到: " << jsonPath << std::endl;

		// 读取并显示JSON文件内容
		std::ifstream file(jsonPath);
		if (file.is_open()) {
			std::cout << "\nJSON文件内容:" << std::endl;
			std::cout << "========================" << std::endl;
			std::string line;
			while (std::getline(file, line)) {
				std::cout << line << std::endl;
			}
			file.close();
		}
	}
	else {
		std::cerr << "保存网卡信息失败!" << std::endl;
	}

#ifdef _WIN32
	CoUninitialize();
#endif
	system("pause");
	return 0;
}

