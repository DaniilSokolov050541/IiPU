#include "USBDevices.h"

list<DeviceWrapper> USBDevices;

void DetectNewDevices();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT OnDeviceChange(HWND, WPARAM, LPARAM);
void OnDeviceInterfaceUpdate(HWND, PDEV_BROADCAST_DEVICEINTERFACE, WPARAM);
void GetDeviceDesc(TCHAR*, DeviceDesc*);
tstring GetDeviceRegistryProperty(HDEVINFO, SP_DEVINFO_DATA, DWORD);
void OnDeviceCreate(TCHAR*, HWND);
void OnDeviceHandleUpdate(HWND, PDEV_BROADCAST_HANDLE, WPARAM);
void EjectDeviceSafely(TCHAR);
DEVINST GetDrivesDevInstByDeviceNumber(DWORD, UINT);

int main() {
	_tsetlocale(LC_ALL, _T("Russian"));
	thread listener(DetectNewDevices);
	bool exit = false;
	while (!exit) {
		TCHAR driveLetter;
		if (_tscanf_s(_T("%c"), &driveLetter) == 0) {
			cout << "Error reading Drive Letter" << endl;
			continue;
		}
		if (driveLetter == 'q') {
			exit = true;
			continue;
		}

		EjectDeviceSafely(toupper(driveLetter));
	}

	return 0;
}

void DetectNewDevices() {
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = _T("Window Class");
	if (RegisterClass(&wc) == 0) {
		cout << GetLastError() << endl;
		exit(EXIT_FAILURE);
	}

	HWND hWnd = CreateWindowEx(0,
		wc.lpszClassName,
		_T(""),
		WS_ICONIC,
		0,
		0,
		CW_USEDEFAULT,
		0,
		nullptr,
		nullptr,
		GetModuleHandle(nullptr),
		nullptr
	);
	if (hWnd == nullptr) {
		cout << GetLastError() << endl;
		exit(EXIT_FAILURE);
	}

	for (GUID i : GUID_DEVINTERFACE_USB_DEVICES) {
		DEV_BROADCAST_DEVICEINTERFACE filter;
		ZeroMemory(&filter, sizeof(filter));
		filter.dbcc_size = sizeof(filter);
		filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
		filter.dbcc_classguid = i;
		if (RegisterDeviceNotification(
			hWnd,
			&filter,
			DEVICE_NOTIFY_WINDOW_HANDLE
		) == nullptr)
		{
			cout << GetLastError() << endl;
			exit(EXIT_FAILURE);
		}
	}

	HDEVINFO devInfo = SetupDiGetClassDevs(
		(LPGUID)&GUID_DEVINTERFACE_USB_DEVICES,
		nullptr,
		nullptr,
		DIGCF_PRESENT |
		DIGCF_DEVICEINTERFACE
	);

	if (devInfo == INVALID_HANDLE_VALUE) {
		cout << GetLastError() << endl;
		exit(EXIT_FAILURE);
	}

	SP_DEVICE_INTERFACE_DATA devInterfaceData;
	ZeroMemory(&devInterfaceData, sizeof(SP_DEVICE_INTERFACE_DATA));
	devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	DWORD error;
	int deviceIndex = 0;

	while (SetupDiEnumDeviceInterfaces(
		devInfo,
		nullptr,
		(LPGUID)&GUID_DEVINTERFACE_USB_DEVICES,
		deviceIndex,
		&devInterfaceData
	)) {
		deviceIndex++;

		DWORD bufSize = 0;
		PSP_DEVICE_INTERFACE_DETAIL_DATA devInterfaceDetailData = nullptr;

		while (!SetupDiGetDeviceInterfaceDetail(
			devInfo,
			&devInterfaceData,
			devInterfaceDetailData,
			bufSize,
			&bufSize,
			nullptr
		))
		{
			error = GetLastError();
			if (error == ERROR_INSUFFICIENT_BUFFER) {
				if (devInterfaceDetailData != nullptr) {
					free(devInterfaceDetailData);
				}

				devInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(bufSize);
				devInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
			}
			else {
				cout << error << endl;
				exit(EXIT_FAILURE);
			}
		}
		if (devInterfaceDetailData == nullptr) {
			cout << GetLastError() << endl;
			exit(EXIT_FAILURE);
		}

		OnDeviceCreate(devInterfaceDetailData->DevicePath, hWnd);
		free(devInterfaceDetailData);
	}

	SetupDiDestroyDeviceInfoList(devInfo);
	cout << "Listening for new USB devices..." << endl;

	MSG msg;
	while (GetMessage(
		&msg,
		nullptr,
		0,
		0
	)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

LRESULT CALLBACK WndProc(
	HWND hWnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam
) {
	if (message == WM_DEVICECHANGE) {
		return OnDeviceChange(hWnd, wParam, lParam);
	}
	else {
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
}

LRESULT OnDeviceChange(HWND hWnd, WPARAM wParam, LPARAM lParam) {
	if (wParam == DBT_DEVICEARRIVAL ||
		wParam == DBT_DEVICEREMOVECOMPLETE ||
		wParam == DBT_DEVICEQUERYREMOVE ||
		wParam == DBT_DEVICEQUERYREMOVEFAILED
		) {
		PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
		switch (pHdr->dbch_devicetype) {
		case DBT_DEVTYP_DEVICEINTERFACE:
			OnDeviceInterfaceUpdate(hWnd, (PDEV_BROADCAST_DEVICEINTERFACE)pHdr, wParam);
			break;
		case DBT_DEVTYP_HANDLE:
			OnDeviceHandleUpdate(hWnd, (PDEV_BROADCAST_HANDLE)pHdr, wParam);
			break;
		}

		if (pHdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
		{
			PDEV_BROADCAST_VOLUME info = (PDEV_BROADCAST_VOLUME)pHdr;
			if (info->dbcv_unitmask)
			{
				cout << "Logical drives on this device: ";
				for (int i = 0; i < 31; i++)
					if (info->dbcv_unitmask & (DWORD)pow(2, i))
						cout << (char)('A' + i) << ' ';
				cout << endl << endl;
			}
		}
	}

	return 0;
}

void OnDeviceInterfaceUpdate(HWND hWnd, PDEV_BROADCAST_DEVICEINTERFACE pDevInf, WPARAM wParam) {
	DeviceDesc devDesc = {};
	GetDeviceDesc(pDevInf->dbcc_name, &devDesc);

	tstring name = GetDeviceRegistryProperty(
		devDesc.devInfo,
		devDesc.devInfoData,
		SPDRP_FRIENDLYNAME
	);
	if (name == _T("Unknown")) {
		name = GetDeviceRegistryProperty(
			devDesc.devInfo,
			devDesc.devInfoData,
			SPDRP_DEVICEDESC
		);
	}

	if (DBT_DEVICEARRIVAL == wParam) {
		OnDeviceCreate(pDevInf->dbcc_name, hWnd);
		tcout << "Connected device: " << name << " (ID: "
			<< GetDeviceRegistryProperty(
				devDesc.devInfo,
				devDesc.devInfoData,
				SPDRP_HARDWAREID
			) << ")" << endl;				
	}
	else {
		for (auto device = USBDevices.begin(); device != USBDevices.end(); device++) {
			if (device->devicePath == _tcsupr(pDevInf->dbcc_name)) {
				if (device->safely) {
					tcout << "Safe ejected: " << name << endl;
					USBDevices.erase(device);
					return;
				}
				break;
			}
		}

		tcout << "Ejected: " << name << endl;
	}

	SetupDiDestroyDeviceInfoList(devDesc.devInfo);
}

void GetDeviceDesc(TCHAR* devicePath, DeviceDesc* devDesc) {
	DWORD error;
	HDEVINFO devInfo = SetupDiCreateDeviceInfoList(nullptr, nullptr);
	if (devInfo == INVALID_HANDLE_VALUE) {
		cout << GetLastError() << endl;
		exit(EXIT_FAILURE);
	}

	SP_DEVICE_INTERFACE_DATA devInterfaceData;
	ZeroMemory(&devInterfaceData, sizeof(SP_DEVICE_INTERFACE_DATA));
	devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	if (!SetupDiOpenDeviceInterface(
		devInfo,
		devicePath,
		0,
		&devInterfaceData
	)) {
		cout << GetLastError() << endl;
		exit(EXIT_FAILURE);
	}

	SP_DEVINFO_DATA devInfoData;
	ZeroMemory(&devInfoData, sizeof(SP_DEVINFO_DATA));
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	if (!SetupDiGetDeviceInterfaceDetail(
		devInfo,
		&devInterfaceData,
		nullptr,
		0,
		nullptr,
		&devInfoData
	)) {
		error = GetLastError();
		if (error != ERROR_INSUFFICIENT_BUFFER) {
			cout << GetLastError() << endl;
			exit(EXIT_FAILURE);
		}
	}

	devDesc->devInfo = devInfo;
	devDesc->devInfoData = devInfoData;
}

tstring GetDeviceRegistryProperty(HDEVINFO devInfo, SP_DEVINFO_DATA devInfoData, DWORD devPropertyCode) {
	DWORD error;
	DWORD bufSize = 0;
	PBYTE buffer = nullptr;

	while (!SetupDiGetDeviceRegistryProperty(
		devInfo,
		&devInfoData,
		devPropertyCode,
		nullptr,
		buffer,
		bufSize,
		&bufSize
	))
	{
		error = GetLastError();
		if (error == ERROR_INVALID_DATA)
		{
			delete[] buffer;
			return _T("Unknown");
		}
		else if (error == ERROR_INSUFFICIENT_BUFFER) {
			delete[] buffer;
			buffer = new BYTE[bufSize];
		}
		else {
			cout << error << endl;
			exit(EXIT_FAILURE);
		}
	}

	if (buffer == nullptr) {
		return _T("Unknown");
	}

	tstring result((TCHAR*)buffer);
	delete[] buffer;

	return result;
}

void OnDeviceCreate(TCHAR* devicePath, HWND hWnd) {
	for (auto device = USBDevices.begin(); device != USBDevices.end(); device++) {
		if (device->devicePath == _tcsupr(devicePath)) { // Check if device already exists
			return;
		}
	}

	HANDLE hDevice = CreateFile(
		devicePath,
		GENERIC_READ | GENERIC_WRITE,
		0,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr
	);
	if (hDevice == INVALID_HANDLE_VALUE) {
		return;
	}

	DEV_BROADCAST_HANDLE filter;
	ZeroMemory(&filter, sizeof(filter));
	memset(&filter, 0, sizeof(filter));
	filter.dbch_size = sizeof(filter);
	filter.dbch_devicetype = DBT_DEVTYP_HANDLE;
	filter.dbch_handle = hDevice;
	if (RegisterDeviceNotification(
		hWnd,
		&filter,
		DEVICE_NOTIFY_WINDOW_HANDLE
	) == nullptr) {
		return;
	}

	DeviceWrapper wrapper;
	wrapper.devicePath = _tcsupr(devicePath);
	wrapper.handle = hDevice;
	wrapper.safely = false;
	USBDevices.push_back(wrapper);
}

void OnDeviceHandleUpdate(HWND hWnd, PDEV_BROADCAST_HANDLE pDevHnd, WPARAM wParam) {
	if (wParam != DBT_DEVICEQUERYREMOVE && wParam != DBT_DEVICEQUERYREMOVEFAILED) {
		return;
	}

	for (auto device = USBDevices.begin(); device != USBDevices.end(); device++) {
		if (device->handle == pDevHnd->dbch_handle) {
			DeviceDesc devDesc{};
			GetDeviceDesc((TCHAR*)device->devicePath.c_str(), &devDesc);

			tstring name = GetDeviceRegistryProperty(
				devDesc.devInfo,
				devDesc.devInfoData,
				SPDRP_FRIENDLYNAME
			);
			if (name == _T("unknown")) {
				name = GetDeviceRegistryProperty(
					devDesc.devInfo,
					devDesc.devInfoData,
					SPDRP_DEVICEDESC
				);
			}

			if (wParam == DBT_DEVICEQUERYREMOVEFAILED) {
				if (!device->safely) {
					return;
				}

				tcout << "Safe eject failed " << name << endl;
				tstring strDevicePath = device->devicePath.c_str();
				TCHAR devicePath[sizeof(strDevicePath)];
				int count = sizeof(strDevicePath);
				for (int i = 0; i < count; i++)
				{
					devicePath[i] = toupper(strDevicePath[i]);
				}
				USBDevices.erase(device);
				OnDeviceCreate(devicePath, hWnd);
			}
			else {
				if (device->safely) {
					return;
				}

				tcout << "Try safe eject: " << name << endl;
				CloseHandle(device->handle);
				device->safely = true;
			}

			SetupDiDestroyDeviceInfoList(devDesc.devInfo);

			return;
		}
	}
}



void EjectDeviceSafely(TCHAR drive) {
	if (drive < 'A' || drive > 'Z') {
		return;
	}

	char volumeAccessPath[] = R"(\\.\A:)";
	volumeAccessPath[4] = drive;
	HANDLE hDevice = CreateFileA(
		volumeAccessPath,
		0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr
	);

	if (hDevice == INVALID_HANDLE_VALUE) {
		cout << "Not found drive " << drive << endl << endl;
		return;
	}

	STORAGE_DEVICE_NUMBER sdn = { 0 };
	DWORD returnedBytes = 0;
	if (!DeviceIoControl(hDevice,
		IOCTL_STORAGE_GET_DEVICE_NUMBER,
		nullptr,
		0,
		&sdn,
		sizeof(sdn),
		&returnedBytes,
		nullptr
	)) {
		return;
	}

	CloseHandle(hDevice);
	DWORD deviceNumber = sdn.DeviceNumber;
	if (deviceNumber == -1) {
		return;
	}

	TCHAR drivePath[] = _T("A:\\");
	drivePath[0] = drive;
	DEVINST devInst = GetDrivesDevInstByDeviceNumber(deviceNumber, GetDriveType(
		drivePath));
	if (!devInst) {
		return;
	}

	DEVINST devInstParent;
	if (CM_Get_Parent(&devInstParent, devInst, 0) != CR_SUCCESS)
	{
		return;
	}

	PNP_VETO_TYPE VetoType = PNP_VetoTypeUnknown;
	TCHAR VetoName[MAX_PATH];
	VetoName[0] = 0;

	CM_Request_Device_Eject(
		devInstParent,
		&VetoType,
		VetoName,
		MAX_PATH,
		0
	);
}

DEVINST GetDrivesDevInstByDeviceNumber(DWORD deviceNumber, UINT driveType) {
	if (driveType != DRIVE_REMOVABLE) {
		return 0;
	}

	HDEVINFO devInfo = SetupDiGetClassDevs(
		&GUID_DEVINTERFACE_USB_DEVICES[0],
		nullptr,
		nullptr,
		DIGCF_PRESENT |
		DIGCF_DEVICEINTERFACE
	);

	if (devInfo == INVALID_HANDLE_VALUE) {
		return 0;
	}

	SP_DEVINFO_DATA devInfoData;
	ZeroMemory(&devInfoData, sizeof(SP_DEVINFO_DATA));
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	SP_DEVICE_INTERFACE_DATA devInterfaceData;
	ZeroMemory(&devInterfaceData, sizeof(SP_DEVICE_INTERFACE_DATA));
	devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	DWORD error;
	int deviceIndex = 0;

	while (SetupDiEnumDeviceInterfaces(
		devInfo,
		nullptr,
		&GUID_DEVINTERFACE_USB_DEVICES[0],
		deviceIndex,
		&devInterfaceData
	)) {
		deviceIndex++;

		DWORD bufSize = 0;
		PSP_DEVICE_INTERFACE_DETAIL_DATA devInterfaceDetailData = nullptr;

		while (!SetupDiGetDeviceInterfaceDetail(
			devInfo,
			&devInterfaceData,
			devInterfaceDetailData,
			bufSize,
			&bufSize,
			&devInfoData
		))
		{
			error = GetLastError();
			if (error == ERROR_INSUFFICIENT_BUFFER) {
				if (devInterfaceDetailData != nullptr) {
					free(devInterfaceDetailData);
				}

				devInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(bufSize);
				devInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
			}
			else {
				return 0;
			}
		}

		if (devInterfaceDetailData == nullptr) {
			return 0;
		}

		HANDLE hDevice = CreateFile(
			devInterfaceDetailData->DevicePath,
			0,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_EXISTING,
			0,
			nullptr
		);
		if (hDevice == INVALID_HANDLE_VALUE) {
			return 0;
		}

		free(devInterfaceDetailData);
		STORAGE_DEVICE_NUMBER sdn = { 0 };
		DWORD returnedBytes = 0;
		if (!DeviceIoControl(hDevice,
			IOCTL_STORAGE_GET_DEVICE_NUMBER,
			nullptr,
			0,
			&sdn,
			sizeof(sdn),
			&returnedBytes,
			nullptr
		)) {
			return 0;
		}

		CloseHandle(hDevice);
		if (deviceNumber == sdn.DeviceNumber) {
			SetupDiDestroyDeviceInfoList(devInfo);
			return devInfoData.DevInst;
		}
	}

	SetupDiDestroyDeviceInfoList(devInfo);
	return 0;
}
