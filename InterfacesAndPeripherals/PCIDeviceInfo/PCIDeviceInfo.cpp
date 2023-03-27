#ifdef UNICODE
#define tcout wcout
#define tcin wcin
#else
#define tcout cout
#define tcin cin
#endif 

#include <tchar.h>
#include <Windows.h>
#include <SetupAPI.h>
#include <iostream>
#include <stdlib.h>

#pragma comment(lib, "SetupAPI.lib")

using namespace std;

typedef std::basic_string<TCHAR> tstring;

struct devicePropertyInfo { 
	bool success = false; 
	DWORD errorCode = 0;
	tstring deviceProperty;
};

devicePropertyInfo GetDeviceRegistryProperty(HDEVINFO hDevInfo, SP_DEVINFO_DATA *devInfoData, DWORD propertyCode);

int main() {
	_tsetlocale(LC_ALL, _T("Russian")); 

	SP_DEVINFO_DATA devInfoData;
	devicePropertyInfo result;
	size_t propertyIdIndex = 0;
	size_t notPos = -1;

	HDEVINFO hDevInfo = SetupDiGetClassDevs(nullptr, _T("PCI"), nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
	if (hDevInfo == INVALID_HANDLE_VALUE) {
		tcout << _T("Failed to get device information list. Error code: ") << GetLastError() << endl;
		return 1;
	}

	ZeroMemory(&devInfoData, sizeof(SP_DEVINFO_DATA));
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	DWORD propertyCode[] = { SPDRP_DEVICEDESC, SPDRP_FRIENDLYNAME, SPDRP_MFG };
	tstring propertyName[] = {
		_T("Description"), _T("   : "), 
		//_T("Friendly name"), _T(" : "),
		_T("Manufacturer"), _T("  : ")
	};
	DWORD propertyCount = size(propertyCode);

	for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
		tcout << _T("Device ") << i << endl;
		
		result = GetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_HARDWAREID);
		if (!result.success) {
			tcout << _T("Failed to get device HARDWARE ID for device ") << i << _T(". Error code: ") << result.errorCode << endl;
			continue;
		}
		
		propertyIdIndex = result.deviceProperty.find(_T("DEV_"));
		if (propertyIdIndex != notPos) {
			tcout << _T("\tDevice ID     : ") << result.deviceProperty.substr(propertyIdIndex + 4, 4).c_str() << endl;
		}

		propertyIdIndex = result.deviceProperty.find(_T("VEN_"));
		if (propertyIdIndex != notPos) {
			tcout << _T("\tVendor ID     : ") << result.deviceProperty.substr(propertyIdIndex + 4, 4).c_str() << endl;
		}

		for (DWORD j = 0; j < propertyCount; j++) {
			result = GetDeviceRegistryProperty(hDevInfo, &devInfoData, propertyCode[j]);
			if (!result.success) {
				tcout << _T("\t!!!Failed to get device ") << propertyName[j * 2].c_str() << _T(" for device ") << i << _T(".Error code : ") << result.errorCode << endl;
			}
			else {
				tcout << _T("\t") << propertyName[j * 2].c_str() << propertyName[j * 2 + 1].c_str() << result.deviceProperty << endl;
			}
		}
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);

	tcin.get();

	return 0;
}

devicePropertyInfo GetDeviceRegistryProperty(HDEVINFO hDevInfo, SP_DEVINFO_DATA *devInfoData, DWORD propertyCode) {
	DWORD requiredSize = 0;

	if (!SetupDiGetDeviceRegistryProperty(hDevInfo, devInfoData, propertyCode, nullptr, nullptr, 0, &requiredSize)) {
		DWORD error = GetLastError();
		if (error != ERROR_INSUFFICIENT_BUFFER) {
			return devicePropertyInfo{ false, error, {} };
		}
	}

	TCHAR* buffer = new TCHAR[requiredSize];
	if (!SetupDiGetDeviceRegistryProperty(hDevInfo, devInfoData, propertyCode, nullptr, (PBYTE)buffer, requiredSize, nullptr)) {
		delete[] buffer;
		buffer = nullptr;
		return devicePropertyInfo{ false, GetLastError(), {} };
	}

	tstring devProperty(buffer);
	delete[] buffer;
	buffer = nullptr;

	return devicePropertyInfo{ true, 0, devProperty };
}
