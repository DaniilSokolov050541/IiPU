#ifdef UNICODE
#define tcout wcout
#else
#define tcout cout
#endif 

#define _CRT_SECURE_NO_WARNINGS

#include <tchar.h>
#include <thread>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <Windows.h>
#include <SetupAPI.h>
#include <Dbt.h>
#include <winioctl.h>
#include <cfgmgr32.h>
#include <list>
#include <guiddef.h>
#include <string>

#pragma comment (lib, "SetupAPI.lib")

using namespace std;

typedef std::basic_string<TCHAR> tstring;

struct DeviceDesc {
	HDEVINFO devInfo;
	SP_DEVINFO_DATA devInfoData;
};

struct DeviceWrapper {
	tstring devicePath;
	HANDLE handle = nullptr;
	bool safely = false;
};

/// <summary>
/// [0] - GUID_DEVINTERFACE_DISK - 
///       This identifier is used to register devices such as hard drives, 
///       removable media such as flash drives and memory cards, and other storage devices.
/// [1] - GUID_DEVINTERFACE_MOUSE -
///       This identifier is used to register devices such as mice, trackballs,
///       touchpads and other cursor control devices on the screen.
/// [2] - GUID_DEVINTERFACE_IMAGE - 
///       This identifier is used to register devices such as digital cameras, 
///       scanners, printers and other devices related to image processing.
/// </summary>
const GUID GUID_DEVINTERFACE_USB_DEVICES[] = {
	{
		0x53F56307,
		0xB6BF,
		0x11D0,
		{
			0x94,
			0xF2,
			0x00,
			0xA0,
			0xC9,
			0x1E,
			0xFB,
			0x8B
		}},
	{
		0x378DE44C,
		0x56EF,
		0x11D1,
		{
			0xBC,
			0x8C,
			0x00,
			0xA0,
			0xC9,
			0x14,
			0x05,
			0xDD
		}},
	{
		0x6BDD1FC6,
		0x810F,
		0x11D0,
		{
			0xBE,
			0xC7,
			0x08,
			0x00,
			0x2B,
			0xE2,
			0x09,
			0x2F
		}}
};