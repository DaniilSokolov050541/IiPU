#include <Windows.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <ntddscsi.h>

using namespace std;

bool CallDeviceIoControl(HANDLE driveHandle, STORAGE_PROPERTY_ID propertyId, LPVOID lpOutBuffer, DWORD outBufferSize);
string GetHardDiskBySeekPenalty(HANDLE driveHandle);
string GetHardDiskByTrim(HANDLE driveHandle);
string GetBusTypeName(STORAGE_BUS_TYPE busTypeValue);
void PrintDriveMemory(HANDLE driveHandle, int driveNum);
void PrintSupportedModeList(HANDLE driveHandle);

/// <summary>
/// Standarts:
/// https://standards.incits.org/apps/group_public/projects.php?end_in_year=&page=1
/// Hard Disk IM2S3138E-128GM-B
/// https://www.runonpc.com/im2s3138e-128gm-b-driver-downloads-update/
/// Hard Disk ST2000LM003 HN-M201RAD
/// https://www.runonpc.com/st2000lm003-hn-m201rad-driver-downloads-update/
/// </summary>
int main()
{
	string driveName = R"(\\.\PhysicalDrive)";

	for (int driveNum = 0;; driveNum++)
	{
		HANDLE driveHandle = CreateFileA(
			(driveName + to_string(driveNum)).c_str(), 
			GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 
			nullptr, 
			OPEN_EXISTING, 
			0, 
			nullptr
		);
		if (driveHandle == INVALID_HANDLE_VALUE) {
			break;
		}	

		STORAGE_DESCRIPTOR_HEADER  descHeader;
		if (!CallDeviceIoControl(
			driveHandle, 
			StorageDeviceProperty, 
			&descHeader, 
			sizeof(STORAGE_DESCRIPTOR_HEADER))
			) {
			cout << "Failed to get device descriptor for physical drive " << driveNum 
				<< ". Error code: " << GetLastError() << endl;
			continue;
		}

		DWORD outBufferSize = descHeader.Size;
		char* outBuffer = new char[outBufferSize];
		if (!CallDeviceIoControl(driveHandle, StorageDeviceProperty, outBuffer, outBufferSize)) {
			cout << "Failed to get device descriptor for physical drive " << driveNum 
				<< ". Error code: " << GetLastError() << endl;
			delete[] outBuffer;
			outBuffer = nullptr;
			continue;
		}
		
		STORAGE_DEVICE_DESCRIPTOR* deviceDescriptor = (STORAGE_DEVICE_DESCRIPTOR*)outBuffer;
		deviceDescriptor->Size = outBufferSize;

		string vendor = outBuffer + deviceDescriptor->VendorIdOffset;
		STORAGE_BUS_TYPE busTypeValue = deviceDescriptor->BusType;
		bool isHardDisk = busTypeValue == BusTypeAta || busTypeValue == BusTypeSata;

		cout << endl << "Physical drive " << driveNum << endl	
			<< "\tHard disk (preliminarily) : " 
				<< "by seek penalty - " << (isHardDisk ? GetHardDiskBySeekPenalty(driveHandle) : "No")
				<< ", by trim - " << (isHardDisk ? GetHardDiskByTrim(driveHandle) : "No") << endl
			<< "\tModel" << setw(23) << " : " << outBuffer + deviceDescriptor->ProductIdOffset << endl
			<< "\tVendor" << setw(22) << " : " << (vendor.size() <= 1 ? "Unknown" : vendor) << endl
			<< "\tSerial number" << setw(15) << " : " << outBuffer + deviceDescriptor->SerialNumberOffset << endl
			<< "\tFirmware version" << setw(12) << " : " << outBuffer + deviceDescriptor->ProductRevisionOffset << endl;
		PrintDriveMemory(driveHandle, driveNum);
		cout << "\tInterface type" << setw(14) << " : " << GetBusTypeName(busTypeValue) << endl;

		if (isHardDisk) {
			PrintSupportedModeList(driveHandle);
		}

		cout << endl;

		delete[] outBuffer;
		outBuffer = nullptr;
		CloseHandle(driveHandle);
	}

	cin.get();

	return 0;
}

bool CallDeviceIoControl(HANDLE driveHandle, STORAGE_PROPERTY_ID propertyId, LPVOID lpOutBuffer, DWORD outBufferSize) {
	STORAGE_PROPERTY_QUERY query;
	query.PropertyId = propertyId;
	query.QueryType = PropertyStandardQuery;

	return DeviceIoControl(
		driveHandle,
		IOCTL_STORAGE_QUERY_PROPERTY,
		&query,
		sizeof(query),
		lpOutBuffer,
		outBufferSize,
		nullptr,
		nullptr
	);
}

string GetHardDiskBySeekPenalty(HANDLE driveHandle) {
	DEVICE_SEEK_PENALTY_DESCRIPTOR deviseSeekPenaltyDescriptor{};
	if (!CallDeviceIoControl(
		driveHandle, 
		StorageDeviceSeekPenaltyProperty, 
		&deviseSeekPenaltyDescriptor, 
		sizeof(deviseSeekPenaltyDescriptor))
		) {
		cout << "Failed to get hard disk value by seek penalty " << ". Error code: " << GetLastError() << endl;
		return "";
	}

	return deviseSeekPenaltyDescriptor.IncursSeekPenalty ? "HDD" : "SSD";
}

string GetHardDiskByTrim(HANDLE driveHandle) {
	DEVICE_TRIM_DESCRIPTOR deviseTrimDescription = { 0 };
	if (!CallDeviceIoControl(
		driveHandle, 
		StorageDeviceTrimProperty, 
		&deviseTrimDescription, 
		sizeof(deviseTrimDescription))
		) {
		cout << "Failed to get hard disk value by trim " << ". Error code: " << GetLastError() << endl;
		return {};
	}

	return deviseTrimDescription.TrimEnabled ? "SSD" : "HDD";
}

string GetBusTypeName(STORAGE_BUS_TYPE busTypeValue) {
	switch (busTypeValue)
	{
		case BusTypeUnknown:
			return "Unknown";
		case BusTypeScsi:
			return "Scsi";
		case BusTypeAtapi:
			return "Atapi";
		case BusTypeAta:
			return "Ata";
		case BusType1394:
			return "1394";
		case BusTypeSsa:
			return "Ssa";
		case BusTypeFibre:
			return "Fibre";
		case BusTypeUsb:
			return "Usb";
		case BusTypeRAID:
			return "RAID";
		case BusTypeiScsi:
			return "Scsi";
		case BusTypeSas:
			return "Sas";
		case BusTypeSata:
			return "Sata";
		case BusTypeSd:
			return "Sd";
		case BusTypeMmc:
			return "Mmc";
		case BusTypeVirtual:
			return "Virtual";
		case BusTypeFileBackedVirtual:
			return "FileBackedVirtual";
		case BusTypeSpaces:
			return "Spaces";
		case BusTypeNvme:
			return "Nvme";
		case BusTypeSCM:
			return "SCM";
		case BusTypeUfs:
			return "Ufs";
		case BusTypeMax:
			return "Max";
		default:
			return "NewBusType";
	}
}

void PrintDriveMemory(HANDLE driveHandle, int driveNum)
{
	DISK_GEOMETRY_EX diskGeometry;
	if (!DeviceIoControl(
		driveHandle,
		IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
		nullptr,
		0,
		&diskGeometry,
		sizeof(diskGeometry),
		nullptr,
		nullptr
	)) {
		cout << "Failed to get memory value " << ". Error code: " << GetLastError() << endl;
		return;
	}
	cout << "\tMemory" << setw(22) << " : "
		<< round((diskGeometry.DiskSize.QuadPart / 1024.0 / 1024.0 / 1024.0) * 100) / 100 << " GB" << endl;
	
	DWORD logicalDrivesMask = GetLogicalDrives();
	for (int i = 0; i < 26; i++) {
		if ((logicalDrivesMask >> i) & 1) {
			char* buffer = new char[BUFSIZ];
			char symbol = char(65 + i);
			string pathHandle = "\\\\.\\", pathName = ":\\";
			HANDLE logicalDriveHandle = CreateFileA(
				(pathHandle + symbol + ":").c_str(), 
				0, 
				FILE_SHARE_READ | FILE_SHARE_WRITE, 
				nullptr,
				OPEN_EXISTING, 
				0, 
				nullptr
			);

			if (logicalDriveHandle == INVALID_HANDLE_VALUE) {
				break;
			}

			if (!DeviceIoControl(
				logicalDriveHandle,
				IOCTL_STORAGE_GET_DEVICE_NUMBER,
				nullptr,
				sizeof(STORAGE_DEVICE_NUMBER),
				buffer,
				BUFSIZ,
				nullptr,
				nullptr
			)) {
				cout << "Failed to get memory value" << ". Error code: " << GetLastError() << endl;
				delete[] buffer; 
				buffer = nullptr;
				break;
			}

			STORAGE_DEVICE_NUMBER* deviceNumber = (STORAGE_DEVICE_NUMBER*)buffer;
			if (driveNum == deviceNumber->DeviceNumber) {
				ULARGE_INTEGER totalBytes = { 0 },
					freeBytes = { 0 };
				if (!GetDiskFreeSpaceExA(
					(symbol + pathName).c_str(),
					nullptr,
					&totalBytes,
					&freeBytes
				)) {
					break;
				}

				double totalMemory = round((totalBytes.QuadPart / 1024.0 / 1024.0 / 1024.0) * 100) / 100;
				double freeMemory = round((freeBytes.QuadPart / 1024.0 / 1024.0 / 1024.0) * 100) / 100;
				cout << setw(32) << "Logical disk " << symbol << " : " << endl
					<< setw(44) << "Used  : " << totalMemory - freeMemory << " GB" << endl
					<< setw(44) << "Free  : " << freeMemory << " GB" << endl
					<< setw(44) << "Total : " << totalMemory << " GB" << endl;
			}

			delete[] buffer;
			buffer = nullptr;
			CloseHandle(logicalDriveHandle);
		}
	}
}

void PrintSupportedModeList(HANDLE driveHandle) {
	DWORD  identifyDeviceDataSize = 512;
	DWORD bufferSize = sizeof(ATA_PASS_THROUGH_EX) + identifyDeviceDataSize;
	BYTE* buffer = new BYTE[bufferSize];
	PATA_PASS_THROUGH_EX pte((PATA_PASS_THROUGH_EX)(buffer));
	pte->AtaFlags = ATA_FLAGS_DATA_IN | ATA_FLAGS_DRDY_REQUIRED;
	pte->Length = sizeof(ATA_PASS_THROUGH_EX);
	pte->DataBufferOffset = sizeof(ATA_PASS_THROUGH_EX);
	pte->DataTransferLength = identifyDeviceDataSize;
	pte->TimeOutValue = 2;
	pte->CurrentTaskFile[6] = ID_CMD;

	if (!DeviceIoControl(driveHandle,
		IOCTL_ATA_PASS_THROUGH, 
		&buffer[0],
		bufferSize,
		&buffer[0], 
		bufferSize,
		nullptr,
		nullptr
	)) {
		cout << "Failed to get supported mode list" << ". Error code: " << GetLastError() << endl;
		delete[] buffer;
		buffer = nullptr;
		return;
	}

	WORD* data = (WORD*)(buffer + sizeof(ATA_PASS_THROUGH_EX));

	cout << "\tSupported mode list" << setw(8) << " :" << endl;

	cout << setw(62) << "Multiword DMA. Versions : ";
	WORD multiWordDMASupport = data[63];
	for (int i = 0; i <= 2; i++) {
		if (multiWordDMASupport & (1 << i)) {
			cout << i << ' ';
		}
	}

	cout << endl;
	
	cout << setw(62) << "PIO.           Versions : ";
	WORD pioSupport = data[64];
	for (int i = 0; i <= 7; i++) {
		if (pioSupport & (1 << i)) {
			cout << i << ' ';
		}
	}

	cout << endl;

	cout << setw(62) << "Ultra DMA.     Versions : ";
	WORD ultraDMASupport = data[88];
	for (int i = 0; i <= 6; i++) {
		if (ultraDMASupport & (1 << i)) {
			cout << i << ' ';
		}
	}

	cout << endl;

	cout << setw(62) << "ATA.           Versions : " << endl;
	string ataVersion[] = { "Reserved", "ATA-1", "ATA-2", "ATA-3", "ATA/ATAPI-4", "ATA/ATAPI-5", "ATA/ATAPI-6", "ATA/ATAPI-7",
		    "ATA8-ACS", "Version 9", "Version 10", "Version 11", "Version 12", "Version 13", "Version 14", "Version 15" };
	for (int i = 1; i <= 15; i++) {
		if (data[80] & (1 << i)) {
			cout << setw(73) << ataVersion[i] << endl;
		}
	}

	WORD interfaceId = data[222];	
	
	if ((data[80] >> 8) == 0)
	{
		if (data[93] == 0)
			cout << setw(53) << "Disk is old SATA" << endl;
		else
			cout << setw(57) << "Disk is PATA" << endl;
	}
	else if ((interfaceId >> 12) == 0)
	{
		cout << setw(57) << "Disk is PATA" << endl;
		string pataVersion[] = { "Version 0 (ATA8_APT)", "Version 1 (ATA/ATAPI-7)", "Version 2", "Version 3", "Version 4",
			"Version 5", "Version 6", "Version 7", "Version 8", "Version 9", "Version 10", "Version 11" };
		interfaceId &= 0x7FF;
		WORD bitIdx = 0;
		while (interfaceId > 0)
		{
			if ((interfaceId & 0x01) != 0) {
				cout << setw(73) << pataVersion[bitIdx] << endl;
			}

			interfaceId >>= 1;
			bitIdx++;
		}
	}
	else if ((interfaceId >> 12) == 1)
	{
		cout << setw(57) << "Disk is SATA" << endl;
		string sataVersion[] = { "ATA8-AST", "SATA 1.0a", "SATA II: Extensions", "SATA Rev 2.5", "SATA Rev 2.6", 
			"Version 5 (SATA 3.0)", "Version 6 (SATA 3.1)", "Version 7 (SATA 3.2)", "Version 8 (SATA 3.3)", 
			"Version 9 (SATA 3.4)", "Version 10 (SATA 3.5)", "Version 11" };
		interfaceId &= 0x7FF;
		WORD bitIdx = 0;
		while (interfaceId > 0)
		{
			if ((interfaceId & 0x01) != 0) {
				cout << setw(73) << sataVersion[bitIdx] << endl;
			}
				
			interfaceId >>= 1;
			bitIdx++;
		}
	}
	else
	{
		cout << setw(46) << "ATA version are unknown" << endl;
	}

	cout << "\tHard disk" << setw(19) << " : " << (data[338] == 0 ? "HDD" : "SSD") << endl;

	delete[] buffer;
	buffer = nullptr;
	data = nullptr;
}

