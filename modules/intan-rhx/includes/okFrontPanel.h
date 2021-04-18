///------------------------------------------------------------------------
// okFrontPanel.h
//
// This is the header file for C/C++ compilation of the FrontPanel DLL
// import library.  This file must be included within any C/C++ source
// which references the FrontPanel DLL methods.
//
//------------------------------------------------------------------------
// Copyright (c) 2005-2020 Opal Kelly Incorporated
//------------------------------------------------------------------------

#ifndef __okFrontPanel_h__
#define __okFrontPanel_h__

// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the FRONTPANELDLL_EXPORTS
// symbol defined on the command line.  This symbol should not be defined on any project
// that uses this DLL.
#if defined(_WIN32)
	#ifdef FRONTPANELDLL_EXPORTS
		#define okDLLEXPORT __declspec(dllexport)
	#else
		#define okDLLEXPORT __declspec(dllimport)

		#ifdef _MSC_VER
			#pragma comment(lib, "okFrontPanel")

			// If the user code delay loads this DLL, we provide a way to
			// verify if loading it during run-time succeeded. Predefine
			// okNO_DELAY_LOAD_FRONTPANEL to completely disable this code.
			#ifndef okNO_DELAY_LOAD_FRONTPANEL
				#define okDELAY_LOAD_FRONTPANEL
			#endif

			#ifdef okDELAY_LOAD_FRONTPANEL
				#include <windows.h>
				#include <delayimp.h>

				// Exception filter used for handling exceptions thrown by MSVC
				// CRT when delay-loading DLLs.
				inline
				LONG WINAPI ok_DelayLoadDllExceptionFilter(PEXCEPTION_POINTERS pExcPointers)
				{
					switch (pExcPointers->ExceptionRecord->ExceptionCode) {
						case 0xc06d007e: // FACILITY_VISUALCPP ERROR_MOD_NOT_FOUND
						case 0xc06d007f: // FACILITY_VISUALCPP ERROR_PROC_NOT_FOUND
							return EXCEPTION_EXECUTE_HANDLER;
					}

					return EXCEPTION_CONTINUE_SEARCH;
				}
			#endif
		#endif
	#endif
	#define DLL_ENTRY   __stdcall
	typedef unsigned int  UINT32;
	typedef unsigned char UINT8;
#elif defined(__linux__) || defined(__APPLE__) || defined(__QNX__)
	#define okDLLEXPORT __attribute__ ((visibility("default")))
	#define DLL_ENTRY

	#include <stdint.h>
	typedef uint32_t UINT32;
	typedef uint8_t  UINT8;
#endif

#ifdef FRONTPANELDLL_EXPORTS
	#define OPALKELLY_NO_C_TYPES
	#define OPALKELLY_NO_CXX_TYPES
#endif

#if defined(_WIN32) && defined(_UNICODE)
	typedef wchar_t const * okFP_dll_pchar;
#elif defined(_WIN32)
	typedef const char * okFP_dll_pchar;
#else
	typedef const char * okFP_dll_pchar;
#endif

#ifdef __cplusplus
#include <climits>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

extern "C" {
#endif // __cplusplus

typedef struct okPLL22150Handle* okPLL22150_HANDLE;
typedef struct okPLL22393Handle* okPLL22393_HANDLE;
typedef struct okDeviceSensorsHandle* okDeviceSensors_HANDLE;
typedef struct okDeviceSettingNamesHandle* okDeviceSettingNames_HANDLE;
typedef struct okDeviceSettingsHandle* okDeviceSettings_HANDLE;
#ifndef okNO_LUA
typedef struct okBufferHandle* okBuffer_HANDLE;
typedef struct okScriptValueHandle* okScriptValue_HANDLE;
typedef struct okScriptValuesHandle* okScriptValues_HANDLE;
typedef struct okScriptEngineHandle* okScriptEngine_HANDLE;
typedef void (*okScriptEngineAsyncCallback)(void*, const char*, okScriptValues_HANDLE);
#endif /* okNO_LUA */
typedef struct okFrontPanelHandle * okFrontPanel_HANDLE;
#ifndef okNO_MANAGER
typedef struct okFrontPanelManagerHandle* okFrontPanelManager_HANDLE;
typedef struct okCFrontPanelManagerHandle* okCFrontPanelManager_HANDLE;
typedef struct okCFrontPanelDevicesHandle* okCFrontPanelDevices_HANDLE;
#endif
typedef int Bool;
#ifdef __cplusplus
	typedef bool okBool;
#else
	typedef char okBool;
#endif

#define MAX_SERIALNUMBER_LENGTH      10       // 10 characters + Does NOT include termination NULL.
#define MAX_DEVICEID_LENGTH          32       // 32 characters + Does NOT include termination NULL.
#define MAX_BOARDMODELSTRING_LENGTH  64       // 64 characters including termination NULL.

#ifndef TRUE
	#define TRUE    1
	#define FALSE   0
#endif

typedef enum {
	ok_ClkSrc22150_Ref=0,
	ok_ClkSrc22150_Div1ByN=1,
	ok_ClkSrc22150_Div1By2=2,
	ok_ClkSrc22150_Div1By3=3,
	ok_ClkSrc22150_Div2ByN=4,
	ok_ClkSrc22150_Div2By2=5,
	ok_ClkSrc22150_Div2By4=6
} ok_ClockSource_22150;

typedef enum {
	ok_ClkSrc22393_Ref=0,
	ok_ClkSrc22393_PLL0_0=2,
	ok_ClkSrc22393_PLL0_180=3,
	ok_ClkSrc22393_PLL1_0=4,
	ok_ClkSrc22393_PLL1_180=5,
	ok_ClkSrc22393_PLL2_0=6,
	ok_ClkSrc22393_PLL2_180=7
} ok_ClockSource_22393;

typedef enum {
	ok_DivSrc_Ref = 0,
	ok_DivSrc_VCO = 1
} ok_DividerSource;

typedef enum {
	ok_brdUnknown = 0,
	ok_brdXEM3001v1 = 1,
	ok_brdXEM3001v2 = 2,
	ok_brdXEM3010 = 3,
	ok_brdXEM3005 = 4,
	ok_brdXEM3001CL = 5,
	ok_brdXEM3020 = 6,
	ok_brdXEM3050 = 7,
	ok_brdXEM9002 = 8,
	ok_brdXEM3001RB = 9,
	ok_brdXEM5010 = 10,
	ok_brdXEM6110LX45 = 11,
	ok_brdXEM6110LX150 = 15,
	ok_brdXEM6001 = 12,
	ok_brdXEM6010LX45 = 13,
	ok_brdXEM6010LX150 = 14,
	ok_brdXEM6006LX9 = 16,
	ok_brdXEM6006LX16 = 17,
	ok_brdXEM6006LX25 = 18,
	ok_brdXEM5010LX110 = 19,
	ok_brdZEM4310=20,
	ok_brdXEM6310LX45=21,
	ok_brdXEM6310LX150=22,
	ok_brdXEM6110v2LX45=23,
	ok_brdXEM6110v2LX150=24,
	ok_brdXEM6002LX9=25,
	ok_brdXEM6310MTLX45T=26,
	ok_brdXEM6320LX130T=27,
	ok_brdXEM7350K70T=28,
	ok_brdXEM7350K160T=29,
	ok_brdXEM7350K410T=30,
	ok_brdXEM6310MTLX150T=31,
	ok_brdZEM5305A2 = 32,
	ok_brdZEM5305A7 = 33,
	ok_brdXEM7001A15 = 34,
	ok_brdXEM7001A35 = 35,
	ok_brdXEM7360K160T = 36,
	ok_brdXEM7360K410T = 37,
	ok_brdZEM5310A4 = 38,
	ok_brdZEM5310A7 = 39,
	ok_brdZEM5370A5 = 40,
	ok_brdXEM7010A50 = 41,
	ok_brdXEM7010A200 = 42,
	ok_brdXEM7310A75 = 43,
	ok_brdXEM7310A200 = 44,
	ok_brdXEM7320A75T = 45,
	ok_brdXEM7320A200T = 46,
	ok_brdXEM7305 = 47,
	ok_brdFPX1301 = 48,
	ok_brdXEM8350KU060 = 49,
	ok_brdXEM8350KU085 = 50,
	ok_brdXEM8350KU115 = 51,
	ok_brdXEM8350SECONDARY = 52,
	ok_brdXEM7310MTA75 = 53,
	ok_brdXEM7310MTA200 = 54,
} ok_BoardModel;

typedef enum {
	ok_NoError                    = 0,
	ok_Failed                     = -1,
	ok_Timeout                    = -2,
	ok_DoneNotHigh                = -3,
	ok_TransferError              = -4,
	ok_CommunicationError         = -5,
	ok_InvalidBitstream           = -6,
	ok_FileError                  = -7,
	ok_DeviceNotOpen              = -8,
	ok_InvalidEndpoint            = -9,
	ok_InvalidBlockSize           = -10,
	ok_I2CRestrictedAddress       = -11,
	ok_I2CBitError                = -12,
	ok_I2CNack                    = -13,
	ok_I2CUnknownStatus           = -14,
	ok_UnsupportedFeature         = -15,
	ok_FIFOUnderflow              = -16,
	ok_FIFOOverflow               = -17,
	ok_DataAlignmentError         = -18,
	ok_InvalidResetProfile        = -19,
	ok_InvalidParameter           = -20
} ok_ErrorCode;

#ifndef OPALKELLY_NO_C_TYPES

#define OK_MAX_DEVICEID_LENGTH              (33)     // 32-byte content + NULL termination
#define OK_MAX_SERIALNUMBER_LENGTH          (11)     // 10-byte content + NULL termination
#define OK_MAX_PRODUCT_NAME_LENGTH          (128)    // 127-byte content + NULL termination
#define OK_MAX_BOARD_MODEL_STRING_LENGTH    (128)

// ok_USBSpeed types
#define OK_USBSPEED_UNKNOWN                 (0)
#define OK_USBSPEED_FULL                    (1)
#define OK_USBSPEED_HIGH                    (2)
#define OK_USBSPEED_SUPER                   (3)

#ifndef OPALKELLY_NO_INTS_FOR_ENUMS
typedef int okEUSBSpeed;
#else
enum okEUSBSpeed {
	okUSBSPEED_UNKNOWN   = 0,
	okUSBSPEED_FULL      = 1,
	okUSBSPEED_HIGH      = 2,
	okUSBSPEED_SUPER     = 3
};
#endif

// ok_Interface types
#define OK_INTERFACE_UNKNOWN                (0)
#define OK_INTERFACE_USB2                   (1)
#define OK_INTERFACE_PCIE                   (2)
#define OK_INTERFACE_USB3                   (3)

#ifndef OPALKELLY_NO_INTS_FOR_ENUMS
typedef int okEDeviceInterface;
#else
enum okEDeviceInterface {
	okDEVICEINTERFACE_UNKNOWN = OK_INTERFACE_UNKNOWN,
	okDEVICEINTERFACE_USB2    = OK_INTERFACE_USB2,
	okDEVICEINTERFACE_PCIE    = OK_INTERFACE_PCIE,
	okDEVICEINTERFACE_USB3    = OK_INTERFACE_USB3
};
#endif

// ok_Product types
#define OK_PRODUCT_UNKNOWN                  (0)
#define OK_PRODUCT_XEM3001V1                (1)
#define OK_PRODUCT_XEM3001V2                (2)
#define OK_PRODUCT_XEM3010                  (3)
#define OK_PRODUCT_XEM3005                  (4)
#define OK_PRODUCT_XEM3001CL                (5)
#define OK_PRODUCT_XEM3020                  (6)
#define OK_PRODUCT_XEM3050                  (7)
#define OK_PRODUCT_XEM9002                  (8)
#define OK_PRODUCT_XEM3001RB                (9)
#define OK_PRODUCT_XEM5010                  (10)
#define OK_PRODUCT_XEM6110LX45              (11)
#define OK_PRODUCT_XEM6001                  (12)
#define OK_PRODUCT_XEM6010LX45              (13)
#define OK_PRODUCT_XEM6010LX150             (14)
#define OK_PRODUCT_XEM6110LX150             (15)
#define OK_PRODUCT_XEM6006LX9               (16)
#define OK_PRODUCT_XEM6006LX16              (17)
#define OK_PRODUCT_XEM6006LX25              (18)
#define OK_PRODUCT_XEM5010LX110             (19)
#define OK_PRODUCT_ZEM4310                  (20)
#define OK_PRODUCT_XEM6310LX45              (21)
#define OK_PRODUCT_XEM6310LX150             (22)
#define OK_PRODUCT_XEM6110V2LX45            (23)
#define OK_PRODUCT_XEM6110V2LX150           (24)
#define OK_PRODUCT_XEM6002LX9               (25)
#define OK_PRODUCT_XEM6310MTLX45T           (26)
#define OK_PRODUCT_XEM6320LX130T            (27)
#define OK_PRODUCT_XEM7350K70T              (28)
#define OK_PRODUCT_XEM7350K160T             (29)
#define OK_PRODUCT_XEM7350K410T             (30)
#define OK_PRODUCT_XEM6310MTLX150T          (31)
#define OK_PRODUCT_ZEM5305A2                (32)
#define OK_PRODUCT_ZEM5305A7                (33)
#define OK_PRODUCT_XEM7001A15               (34)
#define OK_PRODUCT_XEM7001A35               (35)
#define OK_PRODUCT_XEM7360K160T             (36)
#define OK_PRODUCT_XEM7360K410T             (37)
#define OK_PRODUCT_ZEM5310A4                (38)
#define OK_PRODUCT_ZEM5310A7                (39)
#define OK_PRODUCT_ZEM5370A5                (40)
#define OK_PRODUCT_XEM7010A50               (41)
#define OK_PRODUCT_XEM7010A200              (42)
#define OK_PRODUCT_XEM7310A75               (43)
#define OK_PRODUCT_XEM7310A200              (44)
#define OK_PRODUCT_XEM7320A75T              (45)
#define OK_PRODUCT_XEM7320A200T             (46)
#define OK_PRODUCT_XEM7305                  (47)
#define OK_PRODUCT_FPX1301                  (48)
#define OK_PRODUCT_XEM8350KU060             (49)
#define OK_PRODUCT_XEM8350KU085             (50)
#define OK_PRODUCT_XEM8350KU115             (51)
#define OK_PRODUCT_XEM8350SECONDARY         (52)
#define OK_PRODUCT_XEM7310MTA75             (53)
#define OK_PRODUCT_XEM7310MTA200            (54)

#define OK_PRODUCT_OEM_START                11000

#ifndef OPALKELLY_NO_INTS_FOR_ENUMS
typedef int okEProduct;
#else
enum okEProduct {
	okPRODUCT_UNKNOWN                = OK_PRODUCT_UNKNOWN,
	okPRODUCT_XEM3001V1              = OK_PRODUCT_XEM3001V1,
	okPRODUCT_XEM3001V2              = OK_PRODUCT_XEM3001V2,
	okPRODUCT_XEM3010                = OK_PRODUCT_XEM3010,
	okPRODUCT_XEM3005                = OK_PRODUCT_XEM3005,
	okPRODUCT_XEM3001CL              = OK_PRODUCT_XEM3001CL,
	okPRODUCT_XEM3020                = OK_PRODUCT_XEM3020,
	okPRODUCT_XEM3050                = OK_PRODUCT_XEM3050,
	okPRODUCT_XEM9002                = OK_PRODUCT_XEM9002,
	okPRODUCT_XEM3001RB              = OK_PRODUCT_XEM3001RB,
	okPRODUCT_XEM5010                = OK_PRODUCT_XEM5010,
	okPRODUCT_XEM6110LX45            = OK_PRODUCT_XEM6110LX45,
	okPRODUCT_XEM6001                = OK_PRODUCT_XEM6001,
	okPRODUCT_XEM6010LX45            = OK_PRODUCT_XEM6010LX45,
	okPRODUCT_XEM6010LX150           = OK_PRODUCT_XEM6010LX150,
	okPRODUCT_XEM6110LX150           = OK_PRODUCT_XEM6110LX150,
	okPRODUCT_XEM6006LX9             = OK_PRODUCT_XEM6006LX9,
	okPRODUCT_XEM6006LX16            = OK_PRODUCT_XEM6006LX16,
	okPRODUCT_XEM6006LX25            = OK_PRODUCT_XEM6006LX25,
	okPRODUCT_XEM5010LX110           = OK_PRODUCT_XEM5010LX110,
	okPRODUCT_ZEM4310                = OK_PRODUCT_ZEM4310,
	okPRODUCT_XEM6310LX45            = OK_PRODUCT_XEM6310LX45,
	okPRODUCT_XEM6310LX150           = OK_PRODUCT_XEM6310LX150,
	okPRODUCT_XEM6110V2LX45          = OK_PRODUCT_XEM6110V2LX45,
	okPRODUCT_XEM6110V2LX150         = OK_PRODUCT_XEM6110V2LX150,
	okPRODUCT_XEM6002LX9             = OK_PRODUCT_XEM6002LX9,
	okPRODUCT_XEM6310MTLX45T         = OK_PRODUCT_XEM6310MTLX45T,
	okPRODUCT_XEM6320LX130T          = OK_PRODUCT_XEM6320LX130T,
	okPRODUCT_XEM7350K70T            = OK_PRODUCT_XEM7350K70T,
	okPRODUCT_XEM7350K160T           = OK_PRODUCT_XEM7350K160T,
	okPRODUCT_XEM7350K410T           = OK_PRODUCT_XEM7350K410T,
	okPRODUCT_XEM6310MTLX150T        = OK_PRODUCT_XEM6310MTLX150T,
	okPRODUCT_ZEM5305A2              = OK_PRODUCT_ZEM5305A2,
	okPRODUCT_ZEM5305A7              = OK_PRODUCT_ZEM5305A7,
	okPRODUCT_XEM7001A15             = OK_PRODUCT_XEM7001A15,
	okPRODUCT_XEM7001A35             = OK_PRODUCT_XEM7001A35,
	okPRODUCT_XEM7360K160T           = OK_PRODUCT_XEM7360K160T,
	okPRODUCT_XEM7360K410T           = OK_PRODUCT_XEM7360K410T,
	okPRODUCT_ZEM5310A4              = OK_PRODUCT_ZEM5310A4,
	okPRODUCT_ZEM5310A7              = OK_PRODUCT_ZEM5310A7,
	okPRODUCT_ZEM5370A5              = OK_PRODUCT_ZEM5370A5,
	okPRODUCT_XEM7010A50             = OK_PRODUCT_XEM7010A50,
	okPRODUCT_XEM7010A200            = OK_PRODUCT_XEM7010A200,
	okPRODUCT_XEM7310A75             = OK_PRODUCT_XEM7310A75,
	okPRODUCT_XEM7310A200            = OK_PRODUCT_XEM7310A200,
	okPRODUCT_XEM7320A75T            = OK_PRODUCT_XEM7320A75T,
	okPRODUCT_XEM7320A200T           = OK_PRODUCT_XEM7320A200T,
	okPRODUCT_XEM7305                = OK_PRODUCT_XEM7305,
	okPRODUCT_FPX1301                = OK_PRODUCT_FPX1301,
	okPRODUCT_XEM8350KU060           = OK_PRODUCT_XEM8350KU060,
	okPRODUCT_XEM8350KU085           = OK_PRODUCT_XEM8350KU085,
	okPRODUCT_XEM8350KU115           = OK_PRODUCT_XEM8350KU115,
	okPRODUCT_XEM8350SECONDARY       = OK_PRODUCT_XEM8350SECONDARY,
	okPRODUCT_XEM7310MTA75           = OK_PRODUCT_XEM7310MTA75,
	okPRODUCT_XEM7310MTA200          = OK_PRODUCT_XEM7310MTA200,

	okPRODUCT_OEM_START              = OK_PRODUCT_OEM_START
};
#endif

#define OK_FPGACONFIGURATIONMETHOD_NVRAM    (0)
#define OK_FPGACONFIGURATIONMETHOD_JTAG     (1)
enum okEFPGAConfigurationMethod {
	ok_FPGAConfigurationMethod_NVRAM = OK_FPGACONFIGURATIONMETHOD_NVRAM,
	ok_FPGAConfigurationMethod_JTAG  = OK_FPGACONFIGURATIONMETHOD_JTAG
};

#define okFPGARESETPROFILE_MAGIC     0xBE097C3D

typedef enum {
	okFPGAVENDOR_UNKNOWN,
	okFPGAVENDOR_XILINX,
	okFPGAVENDOR_INTEL
} okEFPGAVendor;


typedef struct {
	UINT32   address;
	UINT32   data;
} okTRegisterEntry;

typedef struct {
	UINT32   address;
	UINT32   mask;
} okTTriggerEntry;


typedef struct {
	// Magic number indicating the profile is valid.  (4 byte = 0xBE097C3D)
	UINT32                     magic;

	// Location of the configuration file (Flash boot).  (4 bytes)
	UINT32                     configFileLocation;

	// Length of the configuration file.  (4 bytes)
	UINT32                     configFileLength;

	// Number of microseconds to wait after DONE goes high before
	// starting the reset profile.  (4 bytes)
	UINT32                     doneWaitUS;

	// Number of microseconds to wait after wires are updated
	// before deasserting logic RESET.  (4 bytes)
	UINT32                     resetWaitUS;

	// Number of microseconds to wait after RESET is deasserted
	// before loading registers.  (4 bytes)
	UINT32                     registerWaitUS;

	// Future expansion  (112 bytes)
	UINT32                     padBytes1[28];

	// Initial values of WireIns.  These are loaded prior to
	// deasserting logic RESET.  (32*4 = 128 bytes)
	UINT32                     wireInValues[32];

	// Number of valid Register Entries (4 bytes)
	UINT32                     registerEntryCount;

	// Initial register loads.  (256*8 = 2048 bytes)
	okTRegisterEntry           registerEntries[256];

	// Number of valid Trigger Entries (4 bytes)
	UINT32                     triggerEntryCount;

	// Initial trigger assertions.  These are performed last.
	// (32*8 = 256 bytes)
	okTTriggerEntry            triggerEntries[32];

	// Padding to a 4096-byte size for future expansion
	UINT8                      padBytes2[1520];
} okTFPGAResetProfile;


// Describes the layout of an available Flash memory on the device
typedef struct {
	UINT32             sectorCount;
	UINT32             sectorSize;
	UINT32             pageSize;
	UINT32             minUserSector;
	UINT32             maxUserSector;
} okTFlashLayout;


typedef struct {
	char            deviceID[OK_MAX_DEVICEID_LENGTH];
	char            serialNumber[OK_MAX_SERIALNUMBER_LENGTH];
	char            productName[OK_MAX_BOARD_MODEL_STRING_LENGTH];
	int             productID;
	okEDeviceInterface deviceInterface;
	okEUSBSpeed     usbSpeed;
	int             deviceMajorVersion;
	int             deviceMinorVersion;
	int             hostInterfaceMajorVersion;
	int             hostInterfaceMinorVersion;
	okBool          isPLL22150Supported;
	okBool          isPLL22393Supported;
	okBool          isFrontPanelEnabled;
	int             wireWidth;
	int             triggerWidth;
	int             pipeWidth;
	int             registerAddressWidth;
	int             registerDataWidth;

	okTFlashLayout  flashSystem;
	okTFlashLayout  flashFPGA;

	okBool          hasFMCEEPROM;
	okBool          hasResetProfiles;

	okEFPGAVendor   fpgaVendor;

	int             interfaceCount;
	int             interfaceIndex;

	okBool          configuresFromSystemFlash;
} okTDeviceInfo;


typedef struct {
	char            productName[OK_MAX_PRODUCT_NAME_LENGTH];
	okEProduct      productBaseID;
	int             productID;
	UINT32          usbVID;
	UINT32          usbPID;
	UINT32          pciDID;
} okTDeviceMatchInfo;


typedef enum {
	okDEVICESENSOR_INVALID,
	okDEVICESENSOR_BOOL,
	okDEVICESENSOR_INTEGER,
	okDEVICESENSOR_FLOAT,
	okDEVICESENSOR_VOLTAGE,
	okDEVICESENSOR_CURRENT,
	okDEVICESENSOR_TEMPERATURE,
	okDEVICESENSOR_FAN_RPM
} okEDeviceSensorType;

#define OK_MAX_DEVICE_SENSOR_NAME_LENGTH   (64)   // including NULL termination
#define OK_MAX_DEVICE_SENSOR_DESCRIPTION_LENGTH   (256)   // including NULL termination
typedef struct {
	int                    id;
	okEDeviceSensorType    type;
	char                   name[OK_MAX_DEVICE_SENSOR_NAME_LENGTH];
	char                   description[OK_MAX_DEVICE_SENSOR_DESCRIPTION_LENGTH];
	double                 min;
	double                 max;
	double                 step;
	double                 value;
} okTDeviceSensor;

#endif // ! OPALKELLY_NO_C_TYPES

typedef struct {
	int fd;
	void (*callback)(void*);
	void* param;
} okTCallbackInfo;

// Type of the progress callback used with some FrontPanel functions.
typedef void (*okTProgressCallback)(int maximum, int current, void *arg);

typedef enum okEFPGAConfigurationMethod ok_FPGAConfigurationMethod;

#ifndef OPALKELLY_NO_C_API

//
// Errors: several functions take okError** parameter which should be used to
// pass a pointer to an initially null okError*. If the provided pointer is
// non-null after the function return, it means that an error occurred and this
// pointer can be used to retrieve more details about it and must be freed to
// avoid memory leaks.
//

typedef struct okErrorPrivate okError;

okDLLEXPORT const char *DLL_ENTRY okError_GetMessage(const okError* err);
okDLLEXPORT void DLL_ENTRY okError_Free(okError* err);

//
// Version information
//

// Please note that these compile-time constants do not necessarily correspond
// to the version of the library actually used, see the functions below for
// this.
#define OK_API_VERSION_MAJOR 5
#define OK_API_VERSION_MINOR 2
#define OK_API_VERSION_MICRO 3

#define OK_API_VERSION_STRING "5.2.3"

// Check if the API version is at least equal to the specified one, use as
// #if OK_CHECK_API_VERSION(5, 0, 0)
//  ... v5-specific code ...
// #endif
#define OK_CHECK_API_VERSION(major, minor, micro) \
	(OK_API_VERSION_MAJOR > (major) || \
	(OK_API_VERSION_MAJOR == (major) && OK_API_VERSION_MINOR > (minor)) || \
	(OK_API_VERSION_MAJOR == (major) && OK_API_VERSION_MINOR == (minor) && OK_API_VERSION_MICRO >= (micro)))

// The functions return the version of the library actually used during
// run-time, which may be different from the version the application was
// compiled with.
okDLLEXPORT int DLL_ENTRY okFrontPanel_GetAPIVersionMajor();
okDLLEXPORT int DLL_ENTRY okFrontPanel_GetAPIVersionMinor();
okDLLEXPORT int DLL_ENTRY okFrontPanel_GetAPIVersionMicro();

okDLLEXPORT const char* DLL_ENTRY okFrontPanel_GetAPIVersionString();

// Return true if the library version is at least equal to the given one.
okDLLEXPORT Bool DLL_ENTRY okFrontPanel_CheckAPIVersion(int major, int minor, int micro);

// This function is special and doesn't really need to be called at all if the
// DLL is always available, as it will be always loaded anyhow. It is mostly
// useful when using delay-loading, i.e. using "/DELAYLOAD:okFrontPanel.dll"
// linker option as then it will return false if the DLL couldn't be loaded.
inline Bool okFrontPanel_TryLoadLib()
{
#ifdef okDELAY_LOAD_FRONTPANEL
	__try {
		return okFrontPanel_CheckAPIVersion(0, 0, 0);
	} __except (ok_DelayLoadDllExceptionFilter(GetExceptionInformation())) {
		return FALSE;
	}
#endif // okDELAY_LOAD_FRONTPANEL

	return TRUE;
}

//
// okPLL22393
//
okDLLEXPORT okPLL22393_HANDLE DLL_ENTRY okPLL22393_Construct();
okDLLEXPORT void DLL_ENTRY okPLL22393_Destruct(okPLL22393_HANDLE pll);
okDLLEXPORT Bool DLL_ENTRY okPLL22393_SetCrystalLoad(okPLL22393_HANDLE pll, double capload);
okDLLEXPORT void DLL_ENTRY okPLL22393_SetReference(okPLL22393_HANDLE pll, double freq);
okDLLEXPORT double DLL_ENTRY okPLL22393_GetReference(okPLL22393_HANDLE pll);
okDLLEXPORT Bool DLL_ENTRY okPLL22393_SetPLLParameters(okPLL22393_HANDLE pll, int n, int p, int q, Bool enable);
okDLLEXPORT Bool DLL_ENTRY okPLL22393_SetPLLLF(okPLL22393_HANDLE pll, int n, int lf);
okDLLEXPORT Bool DLL_ENTRY okPLL22393_SetOutputDivider(okPLL22393_HANDLE pll, int n, int div);
okDLLEXPORT Bool DLL_ENTRY okPLL22393_SetOutputSource(okPLL22393_HANDLE pll, int n, ok_ClockSource_22393 clksrc);
okDLLEXPORT void DLL_ENTRY okPLL22393_SetOutputEnable(okPLL22393_HANDLE pll, int n, Bool enable);
okDLLEXPORT int DLL_ENTRY okPLL22393_GetPLLP(okPLL22393_HANDLE pll, int n);
okDLLEXPORT int DLL_ENTRY okPLL22393_GetPLLQ(okPLL22393_HANDLE pll, int n);
okDLLEXPORT double DLL_ENTRY okPLL22393_GetPLLFrequency(okPLL22393_HANDLE pll, int n);
okDLLEXPORT int DLL_ENTRY okPLL22393_GetOutputDivider(okPLL22393_HANDLE pll, int n);
okDLLEXPORT ok_ClockSource_22393 DLL_ENTRY okPLL22393_GetOutputSource(okPLL22393_HANDLE pll, int n);
okDLLEXPORT double DLL_ENTRY okPLL22393_GetOutputFrequency(okPLL22393_HANDLE pll, int n);
okDLLEXPORT Bool DLL_ENTRY okPLL22393_IsOutputEnabled(okPLL22393_HANDLE pll, int n);
okDLLEXPORT Bool DLL_ENTRY okPLL22393_IsPLLEnabled(okPLL22393_HANDLE pll, int n);


//
// okPLL22150
//
okDLLEXPORT okPLL22150_HANDLE DLL_ENTRY okPLL22150_Construct();
okDLLEXPORT void DLL_ENTRY okPLL22150_Destruct(okPLL22150_HANDLE pll);
okDLLEXPORT void DLL_ENTRY okPLL22150_SetCrystalLoad(okPLL22150_HANDLE pll, double capload);
okDLLEXPORT void DLL_ENTRY okPLL22150_SetReference(okPLL22150_HANDLE pll, double freq, Bool extosc);
okDLLEXPORT double DLL_ENTRY okPLL22150_GetReference(okPLL22150_HANDLE pll);
okDLLEXPORT Bool DLL_ENTRY okPLL22150_SetVCOParameters(okPLL22150_HANDLE pll, int p, int q);
okDLLEXPORT int DLL_ENTRY okPLL22150_GetVCOP(okPLL22150_HANDLE pll);
okDLLEXPORT int DLL_ENTRY okPLL22150_GetVCOQ(okPLL22150_HANDLE pll);
okDLLEXPORT double DLL_ENTRY okPLL22150_GetVCOFrequency(okPLL22150_HANDLE pll);
okDLLEXPORT void DLL_ENTRY okPLL22150_SetDiv1(okPLL22150_HANDLE pll, ok_DividerSource divsrc, int n);
okDLLEXPORT void DLL_ENTRY okPLL22150_SetDiv2(okPLL22150_HANDLE pll, ok_DividerSource divsrc, int n);
okDLLEXPORT ok_DividerSource DLL_ENTRY okPLL22150_GetDiv1Source(okPLL22150_HANDLE pll);
okDLLEXPORT ok_DividerSource DLL_ENTRY okPLL22150_GetDiv2Source(okPLL22150_HANDLE pll);
okDLLEXPORT int DLL_ENTRY okPLL22150_GetDiv1Divider(okPLL22150_HANDLE pll);
okDLLEXPORT int DLL_ENTRY okPLL22150_GetDiv2Divider(okPLL22150_HANDLE pll);
okDLLEXPORT void DLL_ENTRY okPLL22150_SetOutputSource(okPLL22150_HANDLE pll, int output, ok_ClockSource_22150 clksrc);
okDLLEXPORT void DLL_ENTRY okPLL22150_SetOutputEnable(okPLL22150_HANDLE pll, int output, Bool enable);
okDLLEXPORT ok_ClockSource_22150 DLL_ENTRY okPLL22150_GetOutputSource(okPLL22150_HANDLE pll, int output);
okDLLEXPORT double DLL_ENTRY okPLL22150_GetOutputFrequency(okPLL22150_HANDLE pll, int output);
okDLLEXPORT Bool DLL_ENTRY okPLL22150_IsOutputEnabled(okPLL22150_HANDLE pll, int output);


//
// okDeviceSensors
//
okDLLEXPORT okDeviceSensors_HANDLE DLL_ENTRY okDeviceSensors_Construct();
okDLLEXPORT void DLL_ENTRY okDeviceSensors_Destruct(okDeviceSensors_HANDLE hnd);
okDLLEXPORT int DLL_ENTRY okDeviceSensors_GetSensorCount(okDeviceSensors_HANDLE hnd);
okDLLEXPORT okTDeviceSensor DLL_ENTRY okDeviceSensors_GetSensor(okDeviceSensors_HANDLE hnd, int n);

//
// okDeviceSettingNames
//
okDLLEXPORT okDeviceSettingNames_HANDLE DLL_ENTRY okDeviceSettingNames_Construct();
okDLLEXPORT void DLL_ENTRY okDeviceSettingNames_Destruct(okDeviceSettingNames_HANDLE hnd);
okDLLEXPORT int DLL_ENTRY okDeviceSettingNames_GetCount(okDeviceSettingNames_HANDLE hnd);
okDLLEXPORT const char* DLL_ENTRY okDeviceSettingNames_Get(okDeviceSettingNames_HANDLE hnd, int n);

//
// okDeviceSettings
//
okDLLEXPORT okDeviceSettings_HANDLE DLL_ENTRY okDeviceSettings_Construct();
okDLLEXPORT void DLL_ENTRY okDeviceSettings_Destruct(okDeviceSettings_HANDLE hnd);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okDeviceSettings_GetString(okDeviceSettings_HANDLE hnd, const char *key, int length, char *buf);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okDeviceSettings_SetString(okDeviceSettings_HANDLE hnd, const char *key, const char *buf);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okDeviceSettings_GetInt(okDeviceSettings_HANDLE hnd, const char *key, UINT32 *value);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okDeviceSettings_SetInt(okDeviceSettings_HANDLE hnd, const char *key, UINT32 value);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okDeviceSettings_Delete(okDeviceSettings_HANDLE hnd, const char *key);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okDeviceSettings_Save(okDeviceSettings_HANDLE hnd);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okDeviceSettings_List(okDeviceSettings_HANDLE hnd, okDeviceSettingNames_HANDLE names);

#ifndef okNO_LUA

//
// okBuffer
//
okDLLEXPORT okBuffer_HANDLE DLL_ENTRY okBuffer_Construct(unsigned size);
okDLLEXPORT okBuffer_HANDLE DLL_ENTRY okBuffer_FromData(void* ptr, unsigned size);
okDLLEXPORT okBuffer_HANDLE DLL_ENTRY okBuffer_Copy(okBuffer_HANDLE hnd);
okDLLEXPORT void DLL_ENTRY okBuffer_Destruct(okBuffer_HANDLE hnd);
okDLLEXPORT okBool DLL_ENTRY okBuffer_IsEmpty(okBuffer_HANDLE hnd);
okDLLEXPORT unsigned DLL_ENTRY okBuffer_GetSize(okBuffer_HANDLE hnd);
okDLLEXPORT unsigned char* DLL_ENTRY okBuffer_GetData(okBuffer_HANDLE hnd);

//
// okScriptValue
//
okDLLEXPORT okScriptValue_HANDLE DLL_ENTRY okScriptValue_Copy(okScriptValue_HANDLE h);
okDLLEXPORT okScriptValue_HANDLE DLL_ENTRY okScriptValue_NewString(const char* s);
okDLLEXPORT okScriptValue_HANDLE DLL_ENTRY okScriptValue_NewBool(okBool b);
okDLLEXPORT okScriptValue_HANDLE DLL_ENTRY okScriptValue_NewInt(int n);
okDLLEXPORT okScriptValue_HANDLE DLL_ENTRY okScriptValue_NewBuffer(okBuffer_HANDLE buf);
okDLLEXPORT okBool DLL_ENTRY okScriptValue_GetAsString(okScriptValue_HANDLE h, const char** ps);
okDLLEXPORT okBool DLL_ENTRY okScriptValue_GetAsBool(okScriptValue_HANDLE h, okBool *pb);
okDLLEXPORT okBool DLL_ENTRY okScriptValue_GetAsInt(okScriptValue_HANDLE h, int *pn);
okDLLEXPORT okBool DLL_ENTRY okScriptValue_GetAsBuffer(okScriptValue_HANDLE h, okBuffer_HANDLE *pbuf);
okDLLEXPORT void DLL_ENTRY okScriptValue_Destruct(okScriptValue_HANDLE h);

//
// okScriptValues
//
okDLLEXPORT okScriptValues_HANDLE DLL_ENTRY okScriptValues_Construct();
okDLLEXPORT okScriptValues_HANDLE DLL_ENTRY okScriptValues_Copy(okScriptValues_HANDLE h);
okDLLEXPORT void DLL_ENTRY okScriptValues_Destruct(okScriptValues_HANDLE hnd);
okDLLEXPORT void DLL_ENTRY okScriptValues_Clear(okScriptValues_HANDLE hnd);
okDLLEXPORT void DLL_ENTRY okScriptValues_Add(okScriptValues_HANDLE hnd, okScriptValue_HANDLE arg);
okDLLEXPORT int DLL_ENTRY okScriptValues_GetCount(okScriptValues_HANDLE hnd);
okDLLEXPORT okScriptValue_HANDLE DLL_ENTRY okScriptValues_Get(okScriptValues_HANDLE hnd, int n);

#endif /* okNO_LUA */

//
// okFrontPanel
//
okDLLEXPORT okFrontPanel_HANDLE DLL_ENTRY okFrontPanel_Construct();
#ifdef __ANDROID__
okDLLEXPORT okFrontPanel_HANDLE DLL_ENTRY okFrontPanel_ConstructFromFD(int fd);
#endif // __ANDROID__
okDLLEXPORT void DLL_ENTRY okFrontPanel_Destruct(okFrontPanel_HANDLE hnd);
okDLLEXPORT int DLL_ENTRY okFrontPanel_GetErrorString(int ec, char *buf, int length);
okDLLEXPORT const char* DLL_ENTRY okFrontPanel_GetLastErrorMessage(okFrontPanel_HANDLE hnd);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_AddCustomDevice(const okTDeviceMatchInfo* matchInfo, const okTDeviceInfo* devInfo);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_RemoveCustomDevice(int productID);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_WriteI2C(okFrontPanel_HANDLE hnd, const int addr, int length, unsigned char *data);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_ReadI2C(okFrontPanel_HANDLE hnd, const int addr, int length, unsigned char *data);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_FlashEraseSector(okFrontPanel_HANDLE hnd, UINT32 address);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_FlashWrite(okFrontPanel_HANDLE hnd, UINT32 address, UINT32 length, const UINT8 *buf);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_FlashRead(okFrontPanel_HANDLE hnd, UINT32 address, UINT32 length, UINT8 *buf);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_GetFPGAResetProfile(okFrontPanel_HANDLE hnd, ok_FPGAConfigurationMethod method, okTFPGAResetProfile *profile);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_GetFPGAResetProfileWithSize(okFrontPanel_HANDLE hnd, ok_FPGAConfigurationMethod method, okTFPGAResetProfile *profile, unsigned size);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_SetFPGAResetProfile(okFrontPanel_HANDLE hnd, ok_FPGAConfigurationMethod method, const okTFPGAResetProfile *profile);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_SetFPGAResetProfileWithSize(okFrontPanel_HANDLE hnd, ok_FPGAConfigurationMethod method, const okTFPGAResetProfile *profile, unsigned size);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_ReadRegister(okFrontPanel_HANDLE hnd, UINT32 addr, UINT32 *data);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_ReadRegisters(okFrontPanel_HANDLE hnd, unsigned num, okTRegisterEntry* regs);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_WriteRegister(okFrontPanel_HANDLE hnd, UINT32 addr, UINT32 data);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_WriteRegisters(okFrontPanel_HANDLE hnd, unsigned num, const okTRegisterEntry* regs);
okDLLEXPORT int DLL_ENTRY okFrontPanel_GetHostInterfaceWidth(okFrontPanel_HANDLE hnd);
okDLLEXPORT Bool DLL_ENTRY okFrontPanel_IsHighSpeed(okFrontPanel_HANDLE hnd);
okDLLEXPORT ok_BoardModel DLL_ENTRY okFrontPanel_GetBoardModel(okFrontPanel_HANDLE hnd);
okDLLEXPORT ok_BoardModel DLL_ENTRY okFrontPanel_FindUSBDeviceModel(unsigned usbVID, unsigned usbPID);
okDLLEXPORT void DLL_ENTRY okFrontPanel_GetBoardModelString(okFrontPanel_HANDLE hnd, ok_BoardModel m, char *buf);
okDLLEXPORT int DLL_ENTRY okFrontPanel_GetDeviceCount(okFrontPanel_HANDLE hnd);
okDLLEXPORT ok_BoardModel DLL_ENTRY okFrontPanel_GetDeviceListModel(okFrontPanel_HANDLE hnd, int num);
okDLLEXPORT void DLL_ENTRY okFrontPanel_GetDeviceListSerial(okFrontPanel_HANDLE hnd, int num, char *buf);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_OpenBySerial(okFrontPanel_HANDLE hnd, const char *serial);
okDLLEXPORT Bool DLL_ENTRY okFrontPanel_IsOpen(okFrontPanel_HANDLE hnd);
okDLLEXPORT Bool DLL_ENTRY okFrontPanel_IsRemote(okFrontPanel_HANDLE hnd);
okDLLEXPORT void DLL_ENTRY okFrontPanel_EnableAsynchronousTransfers(okFrontPanel_HANDLE hnd, Bool enable);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_SetBTPipePollingInterval(okFrontPanel_HANDLE hnd, int interval);
okDLLEXPORT void DLL_ENTRY okFrontPanel_SetTimeout(okFrontPanel_HANDLE hnd, int timeout);
okDLLEXPORT int DLL_ENTRY okFrontPanel_GetDeviceMajorVersion(okFrontPanel_HANDLE hnd);
okDLLEXPORT int DLL_ENTRY okFrontPanel_GetDeviceMinorVersion(okFrontPanel_HANDLE hnd);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_ResetFPGA(okFrontPanel_HANDLE hnd);
okDLLEXPORT void DLL_ENTRY okFrontPanel_Close(okFrontPanel_HANDLE hnd);
okDLLEXPORT void DLL_ENTRY okFrontPanel_GetSerialNumber(okFrontPanel_HANDLE hnd, char *buf);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_GetDeviceSensors(okFrontPanel_HANDLE hnd, okDeviceSensors_HANDLE settings);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_GetDeviceSettings(okFrontPanel_HANDLE hnd, okDeviceSettings_HANDLE settings);
#define okFrontPanel_GetDeviceInfo(hnd, info) okFrontPanel_GetDeviceInfoWithSize((hnd), (info), sizeof(okTDeviceInfo))
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_GetDeviceInfoWithSize(okFrontPanel_HANDLE hnd, okTDeviceInfo *info, unsigned size);
okDLLEXPORT void DLL_ENTRY okFrontPanel_GetDeviceID(okFrontPanel_HANDLE hnd, char *buf);
okDLLEXPORT void DLL_ENTRY okFrontPanel_SetDeviceID(okFrontPanel_HANDLE hnd, const char *strID);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_ConfigureFPGA(okFrontPanel_HANDLE hnd, const char *strFilename);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_ConfigureFPGAWithReset(okFrontPanel_HANDLE hnd, const char *strFilename, const okTFPGAResetProfile *reset);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_ConfigureFPGAFromMemory(okFrontPanel_HANDLE hnd, unsigned char *data, unsigned long length);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_ConfigureFPGAFromMemoryWithProgress(okFrontPanel_HANDLE hnd, unsigned char *data, unsigned long length, okTProgressCallback callback, void *arg);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_ConfigureFPGAFromMemoryWithReset(okFrontPanel_HANDLE hnd, unsigned char *data, unsigned long length, const okTFPGAResetProfile *reset);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_ConfigureFPGAFromFlash(okFrontPanel_HANDLE hnd, unsigned long configIndex);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_GetPLL22150Configuration(okFrontPanel_HANDLE hnd, okPLL22150_HANDLE pll);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_SetPLL22150Configuration(okFrontPanel_HANDLE hnd, okPLL22150_HANDLE pll);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_GetEepromPLL22150Configuration(okFrontPanel_HANDLE hnd, okPLL22150_HANDLE pll);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_SetEepromPLL22150Configuration(okFrontPanel_HANDLE hnd, okPLL22150_HANDLE pll);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_GetPLL22393Configuration(okFrontPanel_HANDLE hnd, okPLL22393_HANDLE pll);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_SetPLL22393Configuration(okFrontPanel_HANDLE hnd, okPLL22393_HANDLE pll);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_GetEepromPLL22393Configuration(okFrontPanel_HANDLE hnd, okPLL22393_HANDLE pll);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_SetEepromPLL22393Configuration(okFrontPanel_HANDLE hnd, okPLL22393_HANDLE pll);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_LoadDefaultPLLConfiguration(okFrontPanel_HANDLE hnd);
okDLLEXPORT Bool DLL_ENTRY okFrontPanel_IsFrontPanelEnabled(okFrontPanel_HANDLE hnd);
okDLLEXPORT Bool DLL_ENTRY okFrontPanel_IsFrontPanel3Supported(okFrontPanel_HANDLE hnd);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_UpdateWireIns(okFrontPanel_HANDLE hnd);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_GetWireInValue(okFrontPanel_HANDLE hnd, int epAddr, UINT32 *val);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_SetWireInValue(okFrontPanel_HANDLE hnd, int ep, unsigned long val, unsigned long mask);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_UpdateWireOuts(okFrontPanel_HANDLE hnd);
okDLLEXPORT unsigned long DLL_ENTRY okFrontPanel_GetWireOutValue(okFrontPanel_HANDLE hnd, int epAddr);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_ActivateTriggerIn(okFrontPanel_HANDLE hnd, int epAddr, int bit);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanel_UpdateTriggerOuts(okFrontPanel_HANDLE hnd);
okDLLEXPORT Bool DLL_ENTRY okFrontPanel_IsTriggered(okFrontPanel_HANDLE hnd, int epAddr, unsigned long mask);
okDLLEXPORT UINT32 DLL_ENTRY okFrontPanel_GetTriggerOutVector(okFrontPanel_HANDLE hnd, int epAddr);
okDLLEXPORT long DLL_ENTRY okFrontPanel_GetLastTransferLength(okFrontPanel_HANDLE hnd);
okDLLEXPORT long DLL_ENTRY okFrontPanel_WriteToPipeIn(okFrontPanel_HANDLE hnd, int epAddr, long length, unsigned char *data);
okDLLEXPORT long DLL_ENTRY okFrontPanel_ReadFromPipeOut(okFrontPanel_HANDLE hnd, int epAddr, long length, unsigned char *data);
okDLLEXPORT long DLL_ENTRY okFrontPanel_WriteToBlockPipeIn(okFrontPanel_HANDLE hnd, int epAddr, int blockSize, long length, unsigned char *data);
okDLLEXPORT long DLL_ENTRY okFrontPanel_ReadFromBlockPipeOut(okFrontPanel_HANDLE hnd, int epAddr, int blockSize, long length, unsigned char *data);

#ifndef okNO_LUA
//
// okScriptEngine
//
okDLLEXPORT okScriptEngine_HANDLE DLL_ENTRY okScriptEngine_ConstructLua(okFrontPanel_HANDLE fp);
okDLLEXPORT void DLL_ENTRY okScriptEngine_Destruct(okScriptEngine_HANDLE hnd);
okDLLEXPORT Bool DLL_ENTRY okScriptEngine_LoadScript(okScriptEngine_HANDLE hnd, const char* name, const char* code, okError** err);
okDLLEXPORT Bool DLL_ENTRY okScriptEngine_LoadFile(okScriptEngine_HANDLE hnd, const char* path, okError** err);
okDLLEXPORT void DLL_ENTRY okScriptEngine_PrependToScriptPath(okScriptEngine_HANDLE hnd, const char* dir);
okDLLEXPORT Bool DLL_ENTRY okScriptEngine_RunScriptFunction(okScriptEngine_HANDLE hnd, const char* name, okScriptValues_HANDLE* retval, okScriptValues_HANDLE args, okError** err);
okDLLEXPORT Bool DLL_ENTRY okScriptEngine_RunScriptFunctionAsync(okScriptEngine_HANDLE hnd, okScriptEngineAsyncCallback callback, void* data, const char* name, okScriptValues_HANDLE args, okError** err);
#endif /* okNO_LUA */

#ifndef okNO_MANAGER

typedef void (*okFrontPanelManager_OnDeviceCallback_t)(void* context, const char* serial);

//
// okFrontPanelManager
//
okDLLEXPORT okCFrontPanelManager_HANDLE DLL_ENTRY okFrontPanelManager_Construct(okFrontPanelManager_HANDLE self, const char* realm);
okDLLEXPORT okCFrontPanelManager_HANDLE DLL_ENTRY okFrontPanelManager_ConstructWithCallbacks(okFrontPanelManager_HANDLE self, const char* realm, okFrontPanelManager_OnDeviceCallback_t onAdded, okFrontPanelManager_OnDeviceCallback_t onRemoved);
okDLLEXPORT void DLL_ENTRY okFrontPanelManager_Destruct(okCFrontPanelManager_HANDLE hnd);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanelManager_StartMonitoring(okCFrontPanelManager_HANDLE hnd);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanelManager_StartMonitoringWithCBInfo(okCFrontPanelManager_HANDLE hnd, okTCallbackInfo* cbInfo);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanelManager_StopMonitoring(okCFrontPanelManager_HANDLE hnd);
okDLLEXPORT int DLL_ENTRY okFrontPanelManager_EnterMonitorLoop(okCFrontPanelManager_HANDLE hnd, const okTCallbackInfo* cbInfo);
okDLLEXPORT int DLL_ENTRY okFrontPanelManager_EnterMonitorLoopWithTimeout(okCFrontPanelManager_HANDLE hnd, const okTCallbackInfo* cbInfo, int millisecondsTimeout);
okDLLEXPORT void DLL_ENTRY okFrontPanelManager_ExitMonitorLoop(okCFrontPanelManager_HANDLE hnd, int exitCode);
okDLLEXPORT okFrontPanel_HANDLE DLL_ENTRY okFrontPanelManager_Open(okCFrontPanelManager_HANDLE hnd, const char *serial);
okDLLEXPORT ok_ErrorCode DLL_ENTRY okFrontPanelManager_EmulateTestDeviceConnection(const char *serial, Bool connect);

//
// FrontPanelDevices
//
okDLLEXPORT okCFrontPanelDevices_HANDLE DLL_ENTRY okFrontPanelDevices_Construct(const char* realm, okError** err);
okDLLEXPORT void DLL_ENTRY okFrontPanelDevices_Destruct(okCFrontPanelDevices_HANDLE hnd);
okDLLEXPORT int DLL_ENTRY okFrontPanelDevices_GetCount(okCFrontPanelDevices_HANDLE hnd);
okDLLEXPORT void DLL_ENTRY okFrontPanelDevices_GetSerial(okCFrontPanelDevices_HANDLE hnd, int num, char* buf);
okDLLEXPORT okFrontPanel_HANDLE DLL_ENTRY okFrontPanelDevices_Open(okCFrontPanelDevices_HANDLE hnd, const char* serial);
#endif /* okNO_MANAGER */

#endif // OPALKELLY_NO_C_API

#ifdef __cplusplus
} // extern "C"

typedef std::vector<okTRegisterEntry> okTRegisterEntries;

#if !defined(OPALKELLY_NO_CXX_TYPES)

namespace OpalKelly
{

class FrontPanelManager;
class FrontPanelDevices;
class ProgressCallback;

} // namespace OpalKelly

namespace OpalKellyLegacy
{

//------------------------------------------------------------------------
// okCPLL22150 C++ wrapper class
//------------------------------------------------------------------------
class okCPLL22150
{
public:
	okPLL22150_HANDLE h;
	enum ClockSource {
			ClkSrc_Ref=0,
			ClkSrc_Div1ByN=1,
			ClkSrc_Div1By2=2,
			ClkSrc_Div1By3=3,
			ClkSrc_Div2ByN=4,
			ClkSrc_Div2By2=5,
			ClkSrc_Div2By4=6 };

	enum DividerSource {
			DivSrc_Ref = 0,
			DivSrc_VCO = 1 };
public:
	okCPLL22150();
	~okCPLL22150();
	void SetCrystalLoad(double capload);
	void SetReference(double freq, bool extosc);
	double GetReference();
	bool SetVCOParameters(int p, int q);
	int GetVCOP();
	int GetVCOQ();
	double GetVCOFrequency();
	void SetDiv1(DividerSource divsrc, int n);
	void SetDiv2(DividerSource divsrc, int n);
	DividerSource GetDiv1Source();
	DividerSource GetDiv2Source();
	int GetDiv1Divider();
	int GetDiv2Divider();
	void SetOutputSource(int output, ClockSource clksrc);
	void SetOutputEnable(int output, bool enable);
	ClockSource GetOutputSource(int output);
	double GetOutputFrequency(int output);
	bool IsOutputEnabled(int output);
};

//------------------------------------------------------------------------
// okCPLL22150 C++ wrapper class
//------------------------------------------------------------------------
class okCPLL22393
{
public:
	okPLL22393_HANDLE h;
	enum ClockSource {
			ClkSrc_Ref=0,
			ClkSrc_PLL0_0=2,
			ClkSrc_PLL0_180=3,
			ClkSrc_PLL1_0=4,
			ClkSrc_PLL1_180=5,
			ClkSrc_PLL2_0=6,
			ClkSrc_PLL2_180=7 };
private:
	bool to_bool(Bool x);
	Bool from_bool(bool x);
public:
	okCPLL22393();
	~okCPLL22393();
	bool SetCrystalLoad(double capload);
	void SetReference(double freq);
	double GetReference();
	bool SetPLLParameters(int n, int p, int q, bool enable=true);
	bool SetPLLLF(int n, int lf);
	bool SetOutputDivider(int n, int div);
	bool SetOutputSource(int n, ClockSource clksrc);
	void SetOutputEnable(int n, bool enable);
	int GetPLLP(int n);
	int GetPLLQ(int n);
	double GetPLLFrequency(int n);
	int GetOutputDivider(int n);
	ClockSource GetOutputSource(int n);
	double GetOutputFrequency(int n);
	bool IsOutputEnabled(int n);
	bool IsPLLEnabled(int n);
};

//------------------------------------------------------------------------
// okCFrontPanel C++ wrapper class
//------------------------------------------------------------------------

class okCDeviceSensors;
class okCDeviceSettings;

class okCFrontPanel
{
public:
	okFrontPanel_HANDLE h;
	enum BoardModel {
		brdUnknown=0,
		brdXEM3001v1=1,
		brdXEM3001v2=2,
		brdXEM3010=3,
		brdXEM3005=4,
		brdXEM3001CL=5,
		brdXEM3020=6,
		brdXEM3050=7,
		brdXEM9002=8,
		brdXEM3001RB=9,
		brdXEM5010=10,
		brdXEM6110LX45=11,
		brdXEM6001=12,
		brdXEM6010LX45=13,
		brdXEM6010LX150=14,
		brdXEM6110LX150=15,
		brdXEM6006LX9=16,
		brdXEM6006LX16=17,
		brdXEM6006LX25=18,
		brdXEM5010LX110=19,
		brdZEM4310=20,
		brdXEM6310LX45=21,
		brdXEM6310LX150=22,
		brdXEM6110v2LX45=23,
		brdXEM6110v2LX150=24,
		brdXEM6002LX9=25,
		brdXEM6310MTLX45T=26,
		brdXEM6320LX130T=27,
		brdXEM7350K70T=28,
		brdXEM7350K160T=29,
		brdXEM7350K410T=30,
		brdXEM6310MTLX150T=31,
		brdZEM5305A2=32,
		brdZEM5305A7=33,
		brdXEM7001A15=34,
		brdXEM7001A35=35,
		brdXEM7360K160T=36,
		brdXEM7360K410T=37,
		brdZEM5310A4=38,
		brdZEM5310A7=39,
		brdZEM5370A5=40,
		brdXEM7010A50=41,
		brdXEM7010A200=42,
		brdXEM7310A75=43,
		brdXEM7310A200=44,
		brdXEM7320A75T=45,
		brdXEM7320A200T=46,
		brdXEM7305=47,
		brdFPX1301=48,
		brdXEM8350KU060=49,
		brdXEM8350KU085=50,
		brdXEM8350KU115=51,
		brdXEM8350SECONDARY=52,
		brdXEM7310MTA75=53,
		brdXEM7310MTA200=54,
	};
	enum ErrorCode {
		NoError                    = 0,
		Failed                     = -1,
		Timeout                    = -2,
		DoneNotHigh                = -3,
		TransferError              = -4,
		CommunicationError         = -5,
		InvalidBitstream           = -6,
		FileError                  = -7,
		DeviceNotOpen              = -8,
		InvalidEndpoint            = -9,
		InvalidBlockSize           = -10,
		I2CRestrictedAddress       = -11,
		I2CBitError                = -12,
		I2CNack                    = -13,
		I2CUnknownStatus           = -14,
		UnsupportedFeature         = -15,
		FIFOUnderflow              = -16,
		FIFOOverflow               = -17,
		DataAlignmentError         = -18,
		InvalidResetProfile        = -19,
		InvalidParameter           = -20
	};
private:
	bool to_bool(Bool x);
	Bool from_bool(bool x);

	explicit okCFrontPanel(okFrontPanel_HANDLE hnd);
	friend class OpalKelly::FrontPanelManager;
	friend class OpalKelly::FrontPanelDevices;

public:
	okCFrontPanel();
#ifdef __ANDROID__
	explicit okCFrontPanel(int fd);
#endif // __ANDROID__
	~okCFrontPanel();
	void Swap(okCFrontPanel& fp);
	static std::string GetErrorString(int errorCode);
	const char* GetLastErrorMessage() const;
	static ErrorCode AddCustomDevice(const okTDeviceMatchInfo& matchInfo, const okTDeviceInfo* devInfo = NULL);
	static ErrorCode RemoveCustomDevice(int productID);
	int GetHostInterfaceWidth();
	BoardModel GetBoardModel();
	static BoardModel FindUSBDeviceModel(unsigned usbVID, unsigned usbPID);
	static std::string GetBoardModelString(BoardModel m);
	int GetDeviceCount();
	ErrorCode GetFPGAResetProfile(okEFPGAConfigurationMethod method, okTFPGAResetProfile *profile);
	ErrorCode SetFPGAResetProfile(okEFPGAConfigurationMethod method, const okTFPGAResetProfile *profile);
	ErrorCode FlashEraseSector(UINT32 address);
	ErrorCode FlashWrite(UINT32 address, UINT32 length, const UINT8 *buf);
	ErrorCode FlashRead(UINT32 address, UINT32 length, UINT8 *buf);
	ErrorCode ReadRegister(UINT32 addr, UINT32 *data);
	ErrorCode ReadRegisters(okTRegisterEntries& regs);
	ErrorCode WriteRegister(UINT32 addr, UINT32 data);
	ErrorCode WriteRegisters(const okTRegisterEntries& regs);
	BoardModel GetDeviceListModel(int num);
	std::string GetDeviceListSerial(int num);
	void EnableAsynchronousTransfers(bool enable);
	ErrorCode OpenBySerial(std::string str = "");
	bool IsOpen();
	bool IsRemote();
	ErrorCode GetDeviceInfo(okTDeviceInfo *info);
	int GetDeviceMajorVersion();
	int GetDeviceMinorVersion();
	std::string GetSerialNumber();
	ErrorCode GetDeviceSettings(okCDeviceSettings& settings);
	ErrorCode GetDeviceSensors(okCDeviceSensors& sensors);
	std::string GetDeviceID();
	void SetDeviceID(const std::string& str);
	ErrorCode SetBTPipePollingInterval(int interval);
	void SetTimeout(int timeout);
	ErrorCode ResetFPGA();
	void Close();
	ErrorCode ConfigureFPGA(const std::string& strFilename);
	ErrorCode ConfigureFPGAWithReset(const std::string& strFilename, const okTFPGAResetProfile *reset);
	ErrorCode ConfigureFPGAFromMemory(unsigned char *data, const unsigned long length, OpalKelly::ProgressCallback* callback = NULL);
	ErrorCode ConfigureFPGAFromMemoryWithReset(unsigned char *data, const unsigned long length, const okTFPGAResetProfile *reset);
	ErrorCode ConfigureFPGAFromFlash(unsigned long configIndex);
	ErrorCode WriteI2C(const int addr, int length, unsigned char *data);
	ErrorCode ReadI2C(const int addr, int length, unsigned char *data);
	ErrorCode GetPLL22150Configuration(okCPLL22150& pll);
	ErrorCode SetPLL22150Configuration(const okCPLL22150& pll);
	ErrorCode GetEepromPLL22150Configuration(okCPLL22150& pll);
	ErrorCode SetEepromPLL22150Configuration(const okCPLL22150& pll);
	ErrorCode GetPLL22393Configuration(okCPLL22393& pll);
	ErrorCode SetPLL22393Configuration(const okCPLL22393& pll);
	ErrorCode GetEepromPLL22393Configuration(okCPLL22393& pll);
	ErrorCode SetEepromPLL22393Configuration(const okCPLL22393& pll);
	ErrorCode LoadDefaultPLLConfiguration();
	bool IsHighSpeed();
	bool IsFrontPanelEnabled();
	bool IsFrontPanel3Supported();
	ErrorCode UpdateWireIns();
	ErrorCode GetWireInValue(int epAddr, UINT32 *val);
	ErrorCode SetWireInValue(int ep, UINT32 val, UINT32 mask = 0xffffffff);
	ErrorCode UpdateWireOuts();
	unsigned long GetWireOutValue(int epAddr);
	ErrorCode ActivateTriggerIn(int epAddr, int bit);
	ErrorCode UpdateTriggerOuts();
	bool IsTriggered(int epAddr, UINT32 mask);
	UINT32 GetTriggerOutVector(int epAddr) const;
	long GetLastTransferLength();
	long WriteToPipeIn(int epAddr, long length, unsigned char *data);
	long ReadFromPipeOut(int epAddr, long length, unsigned char *data);
	long WriteToBlockPipeIn(int epAddr, int blockSize, long length, unsigned char *data);
	long ReadFromBlockPipeOut(int epAddr, int blockSize, long length, unsigned char *data);
};

class okCDeviceSettingNames
{
public:
	okCDeviceSettingNames() : h(okDeviceSettingNames_Construct()) {}

	int GetCount() const { return okDeviceSettingNames_GetCount(h); }
	const char* Get(int n) const { return okDeviceSettingNames_Get(h, n); }

	~okCDeviceSettingNames() { okDeviceSettingNames_Destruct(h); }

private:
	friend class okCDeviceSettings;

	okDeviceSettingNames_HANDLE h;
};

class okCDeviceSettings
{
public:
	okCDeviceSettings()
	{
		h = okDeviceSettings_Construct();
	}

	~okCDeviceSettings()
	{
		okDeviceSettings_Destruct(h);
	}

	okCFrontPanel::ErrorCode GetString(const std::string& key, std::string* value)
	{
		char buf[256];
		ok_ErrorCode rc = okDeviceSettings_GetString(h, key.c_str(), sizeof(buf), buf);
		if (rc == ok_NoError) {
			if (!value)
				rc = ok_InvalidParameter;
			else
				value->assign(buf);
		}

		return static_cast<okCFrontPanel::ErrorCode>(rc);
	}

	okCFrontPanel::ErrorCode GetInt(const std::string& key, UINT32* value)
	{
		return static_cast<okCFrontPanel::ErrorCode>(
					okDeviceSettings_GetInt(h, key.c_str(), value)
				);
	}

	okCFrontPanel::ErrorCode SetString(const std::string& key, const std::string& value)
	{
		return static_cast<okCFrontPanel::ErrorCode>(
					okDeviceSettings_SetString(h, key.c_str(), value.c_str())
				);
	}

	okCFrontPanel::ErrorCode SetInt(const std::string& key, UINT32 value)
	{
		return static_cast<okCFrontPanel::ErrorCode>(
					okDeviceSettings_SetInt(h, key.c_str(), value)
				);
	}

	okCFrontPanel::ErrorCode List(okCDeviceSettingNames& names)
	{
		return static_cast<okCFrontPanel::ErrorCode>(okDeviceSettings_List(h, names.h));
	}

	okCFrontPanel::ErrorCode Delete(const std::string& key)
	{
		return static_cast<okCFrontPanel::ErrorCode>(
					okDeviceSettings_Delete(h, key.c_str())
				);
	}

	okCFrontPanel::ErrorCode Save()
	{
		return static_cast<okCFrontPanel::ErrorCode>(okDeviceSettings_Save(h));
	}

private:
	okDeviceSettings_HANDLE h;

	friend class okCFrontPanel;
};

class okCDeviceSensors
{
public:
	okCDeviceSensors()
	{
		h = okDeviceSensors_Construct();
	}

	~okCDeviceSensors()
	{
		okDeviceSensors_Destruct(h);
	}

	int GetSensorCount() const
	{
		return okDeviceSensors_GetSensorCount(h);
	}

	okTDeviceSensor GetSensor(int n) const
	{
		return okDeviceSensors_GetSensor(h, n);
	}

private:
	okDeviceSensors_HANDLE h;

	friend class OpalKellyLegacy::okCFrontPanel;
};

} // namespace OpalKellyLegacy

// Make the legacy classes defined above available under simpler names.
namespace OpalKelly
{

typedef OpalKellyLegacy::okCPLL22150 PLL22150;
typedef OpalKellyLegacy::okCPLL22393 PLL22393;
typedef OpalKellyLegacy::okCFrontPanel FrontPanel;
typedef OpalKellyLegacy::okCDeviceSettingNames DeviceSettingNames;
typedef OpalKellyLegacy::okCDeviceSettings DeviceSettings;
typedef OpalKellyLegacy::okCDeviceSensors DeviceSensors;

// These functions deal with run-time version information, unlike compile-time
// OK_API_VERSION_XXX constants.
inline int GetAPIVersionMajor() { return okFrontPanel_GetAPIVersionMajor(); }
inline int GetAPIVersionMinor() { return okFrontPanel_GetAPIVersionMinor(); }
inline int GetAPIVersionMicro() { return okFrontPanel_GetAPIVersionMicro(); }

inline const char* GetAPIVersionString() { return okFrontPanel_GetAPIVersionString(); }

inline bool CheckAPIVersion(int major, int minor, int micro)
{
	return okFrontPanel_CheckAPIVersion(major, minor, micro) == TRUE;
}

namespace Impl
{
	class Error : public std::runtime_error
	{
	public:
		explicit Error(okError* error) :
			std::runtime_error(okError_GetMessage(error)) {
			okError_Free(error);
		}
	};
} // namespace Impl

#ifndef okNO_LUA

class Buffer
{
public:
	Buffer();
	explicit Buffer(size_t size);
	Buffer(void* ptr, size_t size);

	Buffer(const Buffer& buf);
	Buffer& operator=(const Buffer& buf);

	bool IsEmpty() const;
	size_t GetSize() const;
	unsigned char* GetData() const;

	unsigned char& GetAt(size_t n);
	unsigned char operator[](size_t n) const;
	unsigned char& operator[](size_t n);

	~Buffer();

private:
	friend class ScriptValue;

	// Check that the given size_t value fits into uint (throws if it doesn't).
	static unsigned SizeCast(size_t n);

	explicit Buffer(okBuffer_HANDLE hnd);

	okBuffer_HANDLE h;
};


class ScriptValue
{
public:
	ScriptValue();
	ScriptValue(int n);
	ScriptValue(bool b);
	ScriptValue(const char* s);
	ScriptValue(const std::string& s);
	ScriptValue(Buffer buf);

	ScriptValue(const ScriptValue& value);
	ScriptValue& operator=(const ScriptValue& value);

	bool GetAsInt(int* pn) const;
	bool GetAsBool(bool* pb) const;
	bool GetAsString(std::string* ps) const;
	bool GetAsBuffer(Buffer* pbuf) const;

	bool IsNumber() const { return DoIsOfType<int>(); }
	int GetNumber() const { return DoGetOfType<int>(); }
	bool IsBool() const { return DoIsOfType<bool>(); }
	bool GetBool() const { return DoGetOfType<bool>(); }
	bool IsString() const { return DoIsOfType<std::string>(); }
	std::string GetString() const { return DoGetOfType<std::string>(); }
	bool IsBuffer() const { return DoIsOfType<Buffer>(); }
	Buffer GetBuffer() const { return DoGetOfType<Buffer>(); }

	~ScriptValue();

private:
	bool DoGetAsType(int* val) const { return GetAsInt(val); }
	bool DoGetAsType(bool* val) const { return GetAsBool(val); }
	bool DoGetAsType(std::string* val) const { return GetAsString(val); }
	bool DoGetAsType(Buffer* val) const { return GetAsBuffer(val); }

	template <typename T>
	bool DoIsOfType() const
	{
		T val;
		return DoGetAsType(&val);
	}

	template <typename T>
	T DoGetOfType() const
	{
		T val;
		if ( !DoGetAsType(&val) )
			throw std::runtime_error("Value is not of the requested type");

		return val;
	}

	friend class ScriptValues;

	explicit ScriptValue(okScriptValue_HANDLE hnd);

	okScriptValue_HANDLE h;
};


class ScriptValues
{
public:
	ScriptValues();

	void Clear();
	void Add(const ScriptValue& value);
	void Set(const ScriptValue& value);

	int GetCount() const;
	ScriptValue Get(int n = 0) const;

	ScriptValues(const ScriptValues& value);
	ScriptValues& operator=(const ScriptValues& value);

	~ScriptValues();

private:
	friend class ScriptEngine;

	explicit ScriptValues(okScriptValues_HANDLE hnd);

	okScriptValues_HANDLE h;
};


class ScriptEngine
{
public:
	ScriptEngine();

	void ConstructLua(OpalKellyLegacy::okCFrontPanel& fp);

	bool IsValid() const;

	void LoadScript(const std::string& name, const std::string& code);
	void LoadFile(const std::string& path);
	void PrependToScriptPath(const std::string& dir);

	ScriptValues RunScriptFunction(
			const std::string& name,
			const ScriptValues& args = ScriptValues()
		);

	class AsyncResultHandler
	{
	public:
		virtual void OnResult(const std::string& error, ScriptValues retvals) = 0;

		virtual ~AsyncResultHandler() {}
	};

	void RunScriptFunctionAsync(
			AsyncResultHandler& handler,
			const std::string& name,
			const ScriptValues& args = ScriptValues()
		);

	~ScriptEngine();

private:
	okScriptEngine_HANDLE GetHandle() {
		if (!h)
			throw std::runtime_error("Can't use uninitialized script engine");

		return h;
	}

	static void AsyncResultCallback(void* handler, const char* error, okScriptValues_HANDLE retvals) {
		return static_cast<AsyncResultHandler*>(handler)->OnResult(error, ScriptValues(retvals));
	}


	ScriptEngine(const ScriptEngine&);
	ScriptEngine& operator=(const ScriptEngine&);

	okScriptEngine_HANDLE h;
};

#endif /* okNO_LUA */

#ifndef okNO_MANAGER

#define okREALM_LOCAL "local"
#define okREALM_TEST "test"

class FrontPanelManager
{
public:
	struct CallbackInfo : okTCallbackInfo
	{
		CallbackInfo()
		{
			fd = -1;
			callback = NULL;
			param = NULL;
		}

		bool IsUsed() const { return fd != -1; }
	};

	explicit FrontPanelManager(const std::string& realm = std::string());
	virtual ~FrontPanelManager();

	void StartMonitoring(CallbackInfo* cbInfo = NULL);
	void StopMonitoring();
	int EnterMonitorLoop(const CallbackInfo* cbInfo = NULL, int millisecondsTimeout = 0);
	void ExitMonitorLoop(int exitCode = 0);

	virtual void OnDeviceAdded(const char *serial) = 0;
	virtual void OnDeviceRemoved(const char *serial) = 0;

	OpalKellyLegacy::okCFrontPanel* Open(const char *serial);

private:
	static void CallOnDeviceAdded(void* self, const char *serial)
		{ static_cast<FrontPanelManager*>(self)->OnDeviceAdded(serial); }
	static void CallOnDeviceRemoved(void* self, const char *serial)
		{ static_cast<FrontPanelManager*>(self)->OnDeviceRemoved(serial); }

	okCFrontPanelManager_HANDLE h;
};

inline FrontPanel::ErrorCode
FrontPanelEmulateTestDeviceConnection(const std::string& serial, bool connect);

// Use std::unique_ptr<> whenever it is available.
//
// Predefine OPALKELLY_NO_STD_UNIQUE_PTR to disable this and use deprecated
// std::auto_ptr<> even when the newer class could be used.
//
// Predefine OPALKELLY_UNIQUE_PTR_TEMPLATE to use the given template directly.
#ifndef OPALKELLY_UNIQUE_PTR_TEMPLATE
	#if !defined(OPALKELLY_NO_STD_UNIQUE_PTR) && \
		(__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1800))
		template <typename T>
		using auto_ptr = std::unique_ptr<T>;
	#else // C++03
		using std::auto_ptr;
	#endif // C++03/11

	typedef auto_ptr<OpalKellyLegacy::okCFrontPanel> FrontPanelPtr;
#else
	typedef OPALKELLY_UNIQUE_PTR_TEMPLATE<OpalKellyLegacy::okCFrontPanel> FrontPanelPtr;
#endif

class FrontPanelDevices
{
public:
	explicit FrontPanelDevices(const std::string& realm = std::string());

	int GetCount() const;
	std::string GetSerial(int num) const;
	FrontPanelPtr Open(const std::string& serial = std::string()) const;

	~FrontPanelDevices();

	okCFrontPanelDevices_HANDLE h;
};

#endif /* okNO_MANAGER */

class ProgressCallback
{
public:
	ProgressCallback() { }
	virtual ~ProgressCallback() { }

	virtual void OnProgress(int maximum, int current) = 0;

private:
	static void CCallback(int maximum, int current, void* self)
	{
		static_cast<ProgressCallback*>(self)->OnProgress(maximum, current);
	}

	friend class OpalKellyLegacy::okCFrontPanel;
};

} // namespace OpalKelly

namespace OpalKellyLegacy
{

//------------------------------------------------------------------------
// okCPLL22150 C++ wrapper class
//------------------------------------------------------------------------
inline okCPLL22150::okCPLL22150()
	{ h=okPLL22150_Construct(); }
inline okCPLL22150::~okCPLL22150()
	{ okPLL22150_Destruct(h); }
inline void okCPLL22150::SetCrystalLoad(double capload)
	{ okPLL22150_SetCrystalLoad(h, capload); }
inline void okCPLL22150::SetReference(double freq, bool extosc)
	{ okPLL22150_SetReference(h, freq, extosc); }
inline double okCPLL22150::GetReference()
	{ return(okPLL22150_GetReference(h)); }
inline bool okCPLL22150::SetVCOParameters(int p, int q)
	{ return okPLL22150_SetVCOParameters(h,p,q) == TRUE; }
inline int okCPLL22150::GetVCOP()
	{ return(okPLL22150_GetVCOP(h)); }
inline int okCPLL22150::GetVCOQ()
	{ return(okPLL22150_GetVCOQ(h)); }
inline double okCPLL22150::GetVCOFrequency()
	{ return(okPLL22150_GetVCOFrequency(h)); }
inline void okCPLL22150::SetDiv1(DividerSource divsrc, int n)
	{ okPLL22150_SetDiv1(h, (ok_DividerSource)divsrc, n); }
inline void okCPLL22150::SetDiv2(DividerSource divsrc, int n)
	{ okPLL22150_SetDiv2(h, (ok_DividerSource)divsrc, n); }
inline okCPLL22150::DividerSource okCPLL22150::GetDiv1Source()
	{ return((DividerSource) okPLL22150_GetDiv1Source(h)); }
inline okCPLL22150::DividerSource okCPLL22150::GetDiv2Source()
	{ return((DividerSource) okPLL22150_GetDiv2Source(h)); }
inline int okCPLL22150::GetDiv1Divider()
	{ return(okPLL22150_GetDiv1Divider(h)); }
inline int okCPLL22150::GetDiv2Divider()
	{ return(okPLL22150_GetDiv2Divider(h)); }
inline void okCPLL22150::SetOutputSource(int output, okCPLL22150::ClockSource clksrc)
	{ okPLL22150_SetOutputSource(h, output, (ok_ClockSource_22150)clksrc); }
inline void okCPLL22150::SetOutputEnable(int output, bool enable)
	{ okPLL22150_SetOutputEnable(h, output, enable); }
inline okCPLL22150::ClockSource okCPLL22150::GetOutputSource(int output)
	{ return( (ClockSource)okPLL22150_GetOutputSource(h, output)); }
inline double okCPLL22150::GetOutputFrequency(int output)
	{ return(okPLL22150_GetOutputFrequency(h, output)); }
inline bool okCPLL22150::IsOutputEnabled(int output)
	{ return okPLL22150_IsOutputEnabled(h, output) == TRUE; }

//------------------------------------------------------------------------
// okCPLL22393 C++ wrapper class
//------------------------------------------------------------------------
inline okCPLL22393::okCPLL22393()
	{ h=okPLL22393_Construct(); }
inline okCPLL22393::~okCPLL22393()
	{ okPLL22393_Destruct(h); }
inline bool okCPLL22393::SetCrystalLoad(double capload)
	{ return okPLL22393_SetCrystalLoad(h, capload); }
inline void okCPLL22393::SetReference(double freq)
	{ okPLL22393_SetReference(h, freq); }
inline double okCPLL22393::GetReference()
	{ return(okPLL22393_GetReference(h)); }
inline bool okCPLL22393::SetPLLParameters(int n, int p, int q, bool enable)
	{ return okPLL22393_SetPLLParameters(h, n, p, q, enable) == TRUE; }
inline bool okCPLL22393::SetPLLLF(int n, int lf)
	{ return okPLL22393_SetPLLLF(h, n, lf) == TRUE; }
inline bool okCPLL22393::SetOutputDivider(int n, int div)
	{ return okPLL22393_SetOutputDivider(h, n, div) == TRUE; }
inline bool okCPLL22393::SetOutputSource(int n, okCPLL22393::ClockSource clksrc)
	{ return okPLL22393_SetOutputSource(h, n, (ok_ClockSource_22393)clksrc) == TRUE; }
inline void okCPLL22393::SetOutputEnable(int n, bool enable)
	{ okPLL22393_SetOutputEnable(h, n, enable); }
inline int okCPLL22393::GetPLLP(int n)
	{ return(okPLL22393_GetPLLP(h, n)); }
inline int okCPLL22393::GetPLLQ(int n)
	{ return(okPLL22393_GetPLLQ(h, n)); }
inline double okCPLL22393::GetPLLFrequency(int n)
	{ return(okPLL22393_GetPLLFrequency(h, n)); }
inline int okCPLL22393::GetOutputDivider(int n)
	{ return(okPLL22393_GetOutputDivider(h, n)); }
inline okCPLL22393::ClockSource okCPLL22393::GetOutputSource(int n)
	{ return((ClockSource) okPLL22393_GetOutputSource(h, n)); }
inline double okCPLL22393::GetOutputFrequency(int n)
	{ return(okPLL22393_GetOutputFrequency(h, n)); }
inline bool okCPLL22393::IsOutputEnabled(int n)
	{ return okPLL22393_IsOutputEnabled(h, n) == TRUE; }
inline bool okCPLL22393::IsPLLEnabled(int n)
	{ return okPLL22393_IsPLLEnabled(h, n) == TRUE; }

//------------------------------------------------------------------------
// okCFrontPanel C++ wrapper class
//------------------------------------------------------------------------
inline okCFrontPanel::okCFrontPanel(okFrontPanel_HANDLE hnd)
	{ h=hnd; }
inline okCFrontPanel::okCFrontPanel()
	{ h=okFrontPanel_Construct(); }
#ifdef __ANDROID__
inline okCFrontPanel::okCFrontPanel(int fd)
	{ h=okFrontPanel_ConstructFromFD(fd); }
#endif // __ANDROID__
inline okCFrontPanel::~okCFrontPanel()
	{ okFrontPanel_Destruct(h); }
inline void okCFrontPanel::Swap(okCFrontPanel& fp)
	{ okFrontPanel_HANDLE hOrig = h; h = fp.h; fp.h = hOrig; }
inline std::string okCFrontPanel::GetErrorString(int ec)
	{
		int len = okFrontPanel_GetErrorString(ec, NULL, 0);
		std::string s;
		if (len > 0) {
			s.resize(len);
			okFrontPanel_GetErrorString(ec, &s[0], len + 1);
		}
		return s;
	}
inline const char* okCFrontPanel::GetLastErrorMessage() const
	{ return okFrontPanel_GetLastErrorMessage(h); }
inline okCFrontPanel::ErrorCode okCFrontPanel::AddCustomDevice(const okTDeviceMatchInfo& matchInfo, const okTDeviceInfo* devInfo)
	{ return (okCFrontPanel::ErrorCode)okFrontPanel_AddCustomDevice(&matchInfo, devInfo); }
inline okCFrontPanel::ErrorCode okCFrontPanel::RemoveCustomDevice(int productID)
	{ return (okCFrontPanel::ErrorCode)okFrontPanel_RemoveCustomDevice(productID); }
inline int okCFrontPanel::GetHostInterfaceWidth()
	{ return(okFrontPanel_GetHostInterfaceWidth(h)); }
inline bool okCFrontPanel::IsHighSpeed()
	{ return okFrontPanel_IsHighSpeed(h) == TRUE; }
inline okCFrontPanel::BoardModel okCFrontPanel::GetBoardModel()
	{ return((okCFrontPanel::BoardModel)okFrontPanel_GetBoardModel(h)); }
inline okCFrontPanel::BoardModel okCFrontPanel::FindUSBDeviceModel(unsigned usbVID, unsigned usbPID)
	{ return((okCFrontPanel::BoardModel)okFrontPanel_FindUSBDeviceModel(usbVID, usbPID)); }
inline std::string okCFrontPanel::GetBoardModelString(okCFrontPanel::BoardModel m)
	{
		char str[MAX_BOARDMODELSTRING_LENGTH];
		okFrontPanel_GetBoardModelString(NULL, (ok_BoardModel)m, str);
		return(std::string(str));
	}
inline int okCFrontPanel::GetDeviceCount()
	{ return(okFrontPanel_GetDeviceCount(h)); }
inline okCFrontPanel::BoardModel okCFrontPanel::GetDeviceListModel(int num)
	{ return((okCFrontPanel::BoardModel)okFrontPanel_GetDeviceListModel(h, num)); }
inline std::string okCFrontPanel::GetDeviceListSerial(int num)
	{
		char str[MAX_SERIALNUMBER_LENGTH+1];
		okFrontPanel_GetDeviceListSerial(h, num, str);
		str[MAX_SERIALNUMBER_LENGTH] = '\0';
		return(std::string(str));
	}
inline okCFrontPanel::ErrorCode okCFrontPanel::GetDeviceInfo(okTDeviceInfo *info)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_GetDeviceInfoWithSize(h, info, sizeof(*info))); }
inline void okCFrontPanel::EnableAsynchronousTransfers(bool enable)
	{ okFrontPanel_EnableAsynchronousTransfers(h, enable); }
inline okCFrontPanel::ErrorCode okCFrontPanel::OpenBySerial(std::string str)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_OpenBySerial(h, str.c_str())); }
inline bool okCFrontPanel::IsOpen()
	{ return okFrontPanel_IsOpen(h) == TRUE; }
inline bool okCFrontPanel::IsRemote()
	{ return okFrontPanel_IsRemote(h) == TRUE; }
inline int okCFrontPanel::GetDeviceMajorVersion()
	{ return(okFrontPanel_GetDeviceMajorVersion(h)); }
inline int okCFrontPanel::GetDeviceMinorVersion()
	{ return(okFrontPanel_GetDeviceMinorVersion(h)); }
inline std::string okCFrontPanel::GetSerialNumber()
	{
		char str[MAX_SERIALNUMBER_LENGTH+1];
		okFrontPanel_GetSerialNumber(h, str);
		return(std::string(str));
	}
inline okCFrontPanel::ErrorCode okCFrontPanel::GetDeviceSettings(okCDeviceSettings& settings)
	{
		return (ErrorCode)okFrontPanel_GetDeviceSettings(h, settings.h);
	}
inline okCFrontPanel::ErrorCode okCFrontPanel::GetDeviceSensors(okCDeviceSensors& sensors)
	{
		return (ErrorCode)okFrontPanel_GetDeviceSensors(h, sensors.h);
	}
inline std::string okCFrontPanel::GetDeviceID()
	{
		char str[MAX_DEVICEID_LENGTH+1];
		okFrontPanel_GetDeviceID(h, str);
		return(std::string(str));
	}
inline void okCFrontPanel::SetDeviceID(const std::string& str)
	{ okFrontPanel_SetDeviceID(h, str.c_str()); }
inline okCFrontPanel::ErrorCode okCFrontPanel::SetBTPipePollingInterval(int interval)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_SetBTPipePollingInterval(h, interval)); }
inline void okCFrontPanel::SetTimeout(int timeout)
	{ okFrontPanel_SetTimeout(h, timeout); }
inline okCFrontPanel::ErrorCode okCFrontPanel::ResetFPGA()
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_ResetFPGA(h)); }
inline void okCFrontPanel::Close()
	{ okFrontPanel_Close(h); }
inline okCFrontPanel::ErrorCode okCFrontPanel::ConfigureFPGA(const std::string& strFilename)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_ConfigureFPGA(h, strFilename.c_str())); }
inline okCFrontPanel::ErrorCode okCFrontPanel::ConfigureFPGAWithReset(const std::string& strFilename, const okTFPGAResetProfile *reset)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_ConfigureFPGAWithReset(h, strFilename.c_str(), reset)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::ConfigureFPGAFromMemory(
		unsigned char *data,
		const unsigned long length,
		OpalKelly::ProgressCallback* callback
	)
	{
		ok_ErrorCode ec;
		if (callback) {
			ec = okFrontPanel_ConfigureFPGAFromMemoryWithProgress(
						h,
						data,
						length,
						&OpalKelly::ProgressCallback::CCallback,
						callback
					);
		} else {
			ec = okFrontPanel_ConfigureFPGAFromMemory(h, data, length);
		}
		return (okCFrontPanel::ErrorCode)ec;
	}
inline okCFrontPanel::ErrorCode okCFrontPanel::ConfigureFPGAFromMemoryWithReset(unsigned char *data, const unsigned long length, const okTFPGAResetProfile* reset)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_ConfigureFPGAFromMemoryWithReset(h, data, length, reset)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::ConfigureFPGAFromFlash(unsigned long configIndex)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_ConfigureFPGAFromFlash(h, configIndex)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::GetFPGAResetProfile(okEFPGAConfigurationMethod method, okTFPGAResetProfile *profile)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_GetFPGAResetProfileWithSize(h, method, profile, sizeof(*profile))); }
inline okCFrontPanel::ErrorCode okCFrontPanel::ReadRegister(UINT32 addr, UINT32 *data)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_ReadRegister(h, addr, data)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::ReadRegisters(std::vector<okTRegisterEntry>& regs)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_ReadRegisters(h, (unsigned int)regs.size(), regs.empty() ? NULL : &regs[0])); }
inline okCFrontPanel::ErrorCode okCFrontPanel::SetFPGAResetProfile(okEFPGAConfigurationMethod method, const okTFPGAResetProfile *profile)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_SetFPGAResetProfileWithSize(h, method, profile, sizeof(*profile))); }
inline okCFrontPanel::ErrorCode okCFrontPanel::FlashEraseSector(UINT32 address)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_FlashEraseSector(h, address)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::FlashWrite(UINT32 address, UINT32 length, const UINT8 *buf)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_FlashWrite(h, address, length, buf)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::FlashRead(UINT32 address, UINT32 length, UINT8 *buf)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_FlashRead(h, address, length, buf)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::WriteRegister(UINT32 addr, UINT32 data)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_WriteRegister(h, addr, data)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::WriteRegisters(const std::vector<okTRegisterEntry>& regs)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_WriteRegisters(h, (unsigned int)regs.size(), regs.empty() ? NULL : &regs[0])); }
inline okCFrontPanel::ErrorCode okCFrontPanel::GetWireInValue(int epAddr, UINT32 *val)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_GetWireInValue(h, epAddr, val)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::WriteI2C(const int addr, int length, unsigned char *data)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_WriteI2C(h, addr, length, data)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::ReadI2C(const int addr, int length, unsigned char *data)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_ReadI2C(h, addr, length, data)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::GetPLL22150Configuration(okCPLL22150& pll)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_GetPLL22150Configuration(h, pll.h)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::SetPLL22150Configuration(const okCPLL22150& pll)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_SetPLL22150Configuration(h, pll.h)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::GetEepromPLL22150Configuration(okCPLL22150& pll)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_GetEepromPLL22150Configuration(h, pll.h)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::SetEepromPLL22150Configuration(const okCPLL22150& pll)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_SetEepromPLL22150Configuration(h, pll.h)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::GetPLL22393Configuration(okCPLL22393& pll)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_GetPLL22393Configuration(h, pll.h)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::SetPLL22393Configuration(const okCPLL22393& pll)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_SetPLL22393Configuration(h, pll.h)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::GetEepromPLL22393Configuration(okCPLL22393& pll)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_GetEepromPLL22393Configuration(h, pll.h)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::SetEepromPLL22393Configuration(const okCPLL22393& pll)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_SetEepromPLL22393Configuration(h, pll.h)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::LoadDefaultPLLConfiguration()
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_LoadDefaultPLLConfiguration(h)); }
inline bool okCFrontPanel::IsFrontPanelEnabled()
	{ return okFrontPanel_IsFrontPanelEnabled(h) == TRUE; }
inline bool okCFrontPanel::IsFrontPanel3Supported()
	{ return okFrontPanel_IsFrontPanel3Supported(h) == TRUE; }
//	void UnregisterAll();
//	void AddEventHandler(EventHandler *handler);
inline okCFrontPanel::ErrorCode okCFrontPanel::UpdateWireIns()
	{ return (okCFrontPanel::ErrorCode)okFrontPanel_UpdateWireIns(h); }
inline okCFrontPanel::ErrorCode okCFrontPanel::SetWireInValue(int ep, UINT32 val, UINT32 mask)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_SetWireInValue(h, ep, val, mask)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::UpdateWireOuts()
	{ return (okCFrontPanel::ErrorCode)okFrontPanel_UpdateWireOuts(h); }
inline unsigned long okCFrontPanel::GetWireOutValue(int epAddr)
	{ return(okFrontPanel_GetWireOutValue(h, epAddr)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::ActivateTriggerIn(int epAddr, int bit)
	{ return((okCFrontPanel::ErrorCode) okFrontPanel_ActivateTriggerIn(h, epAddr, bit)); }
inline okCFrontPanel::ErrorCode okCFrontPanel::UpdateTriggerOuts()
	{ return (okCFrontPanel::ErrorCode)okFrontPanel_UpdateTriggerOuts(h); }
inline bool okCFrontPanel::IsTriggered(int epAddr, UINT32 mask)
	{ return okFrontPanel_IsTriggered(h, epAddr, mask) == TRUE; }
inline UINT32 okCFrontPanel::GetTriggerOutVector(int epAddr) const
	{ return okFrontPanel_GetTriggerOutVector(h, epAddr); }
inline long okCFrontPanel::GetLastTransferLength()
	{ return(okFrontPanel_GetLastTransferLength(h)); }
inline long okCFrontPanel::WriteToPipeIn(int epAddr, long length, unsigned char *data)
	{ return(okFrontPanel_WriteToPipeIn(h, epAddr, length, data)); }
inline long okCFrontPanel::ReadFromPipeOut(int epAddr, long length, unsigned char *data)
	{ return(okFrontPanel_ReadFromPipeOut(h, epAddr, length, data)); }
inline long okCFrontPanel::WriteToBlockPipeIn(int epAddr, int blockSize, long length, unsigned char *data)
	{ return(okFrontPanel_WriteToBlockPipeIn(h, epAddr, blockSize, length, data)); }
inline long okCFrontPanel::ReadFromBlockPipeOut(int epAddr, int blockSize, long length, unsigned char *data)
	{ return(okFrontPanel_ReadFromBlockPipeOut(h, epAddr, blockSize, length, data)); }

} // namespace OpalKellyLegacy

namespace OpalKelly
{

#ifndef okNO_LUA

inline unsigned Buffer::SizeCast(size_t n)
	{
		if (n >= UINT_MAX)
			throw std::out_of_range("Buffer size is too big.");

		return static_cast<unsigned>(n);
	}
inline Buffer::Buffer() : h(NULL)
	{ }
inline Buffer::Buffer(okBuffer_HANDLE hnd) : h(hnd)
	{ }
inline Buffer::Buffer(size_t size) : h(okBuffer_Construct(SizeCast(size)))
	{ }
inline Buffer::Buffer(void* ptr, size_t size) : h(okBuffer_FromData(ptr, SizeCast(size)))
	{ }
inline Buffer::Buffer(const Buffer& buf) : h(okBuffer_Copy(buf.h))
	{ }
inline Buffer& Buffer::operator=(const Buffer& buf)
	{ okBuffer_Destruct(h); h = okBuffer_Copy(buf.h); return *this; }
inline bool Buffer::IsEmpty() const
	{ return okBuffer_IsEmpty(h); }
inline size_t Buffer::GetSize() const
	{ return okBuffer_GetSize(h); }
inline unsigned char* Buffer::GetData() const
	{ return okBuffer_GetData(h); }
inline unsigned char& Buffer::GetAt(size_t n)
	{
		if (n >= GetSize())
			throw std::out_of_range("Buffer index out of range.");

		return GetData()[n];
	}
inline unsigned char Buffer::operator[](size_t n) const
	{ return const_cast<Buffer*>(this)->GetAt(n); }
inline unsigned char& Buffer::operator[](size_t n)
	{ return GetAt(n); }
inline Buffer::~Buffer()
	{ okBuffer_Destruct(h); }

inline ScriptValue::ScriptValue() : h(NULL)
	{ }
inline ScriptValue::ScriptValue(okScriptValue_HANDLE hnd) : h(hnd)
	{ }
inline ScriptValue::ScriptValue(int n) : h(okScriptValue_NewInt(n))
	{ }
inline ScriptValue::ScriptValue(bool b) : h(okScriptValue_NewBool(b))
	{ }
inline ScriptValue::ScriptValue(const char* s) : h(okScriptValue_NewString(s))
	{ }
inline ScriptValue::ScriptValue(const std::string& s) : h(okScriptValue_NewString(s.c_str()))
	{ }
inline ScriptValue::ScriptValue(Buffer buf) : h(okScriptValue_NewBuffer(buf.h))
	{ }
inline ScriptValue::ScriptValue(const ScriptValue& value) : h(okScriptValue_Copy(value.h))
	{ }
inline ScriptValue& ScriptValue::operator=(const ScriptValue& value)
	{ okScriptValue_Destruct(h); h = okScriptValue_Copy(value.h); return *this; }
inline bool ScriptValue::GetAsInt(int *pn) const
	{ return okScriptValue_GetAsInt(h, pn); }
inline bool ScriptValue::GetAsBool(bool* pb) const
	{ return okScriptValue_GetAsBool(h, pb); }
inline bool ScriptValue::GetAsString(std::string* ps) const
	{
		const char* pc;
		if (!okScriptValue_GetAsString(h, &pc))
			return false;

		*ps = pc;
		return true;
	}
inline bool ScriptValue::GetAsBuffer(Buffer* pbuf) const
	{
		okBuffer_HANDLE hbuf;
		if (!okScriptValue_GetAsBuffer(h, &hbuf))
			return false;

		*pbuf = Buffer(hbuf);
		return true;
	}
inline ScriptValue::~ScriptValue()
	{ okScriptValue_Destruct(h); }

inline ScriptValues::ScriptValues() : h(okScriptValues_Construct())
	{ }
inline ScriptValues::ScriptValues(okScriptValues_HANDLE hnd) : h(hnd)
	{ }
inline void ScriptValues::Clear()
	{ okScriptValues_Clear(h); }
inline void ScriptValues::Add(const ScriptValue& value)
	{ okScriptValues_Add(h, value.h); }
inline void ScriptValues::Set(const ScriptValue& value)
	{ Clear(); Add(value); }
inline int ScriptValues::GetCount() const
	{ return okScriptValues_GetCount(h); }
inline ScriptValue ScriptValues::Get(int n) const
	{
		okScriptValue_HANDLE hnd = okScriptValues_Get(h, n);
		if (!hnd)
			throw std::runtime_error("Script value index out of range.");

		return ScriptValue(hnd);
	}
inline ScriptValues::ScriptValues(const ScriptValues& values) : h(okScriptValues_Copy(values.h))
	{ }
inline ScriptValues& ScriptValues::operator=(const ScriptValues& values)
	{ okScriptValues_Destruct(h); h = okScriptValues_Copy(values.h); return *this; }
inline ScriptValues::~ScriptValues()
	{ okScriptValues_Destruct(h); }

inline ScriptEngine::ScriptEngine()
	: h(NULL)
	{ }
inline void ScriptEngine::ConstructLua(OpalKellyLegacy::okCFrontPanel& fp)
	{
		if (h)
			okScriptEngine_Destruct(h);
		h = okScriptEngine_ConstructLua(fp.h);
		if (!h)
			throw std::runtime_error("Failed to create Lua scripting engine.");
	}
inline bool ScriptEngine::IsValid() const
	{ return h != NULL; }
inline void ScriptEngine::LoadScript(const std::string& name, const std::string& code)
	{
		okError* error;
		if (!okScriptEngine_LoadScript(GetHandle(), name.c_str(), code.c_str(), &error))
			throw Impl::Error(error);
	}
inline void ScriptEngine::LoadFile(const std::string& path)
	{
		okError* error;
		if (!okScriptEngine_LoadFile(GetHandle(), path.c_str(), &error))
			throw Impl::Error(error);
	}
inline void ScriptEngine::PrependToScriptPath(const std::string& dir)
	{
		okScriptEngine_PrependToScriptPath(GetHandle(), dir.c_str());
	}
inline ScriptValues ScriptEngine::RunScriptFunction(const std::string& name, const ScriptValues& args)
	{
		okScriptValues_HANDLE retvals;
		okError* error;
		if (!okScriptEngine_RunScriptFunction(GetHandle(), name.c_str(), &retvals, args.h, &error))
			throw Impl::Error(error);

		return ScriptValues(retvals);
	}
inline void ScriptEngine::RunScriptFunctionAsync(
		AsyncResultHandler& handler,
		const std::string& name,
		const ScriptValues& args
	)
	{
		okError* error;
		if (!okScriptEngine_RunScriptFunctionAsync(GetHandle(), AsyncResultCallback, &handler, name.c_str(), args.h, &error))
			throw Impl::Error(error);
	}
inline ScriptEngine::~ScriptEngine()
	{ okScriptEngine_Destruct(h); }

#endif /* okNO_LUA */

#ifndef okNO_MANAGER

inline FrontPanelManager::FrontPanelManager(const std::string& realm)
	{
		h = okFrontPanelManager_ConstructWithCallbacks(
				reinterpret_cast<okFrontPanelManager_HANDLE>(this),
				realm.c_str(),
				FrontPanelManager::CallOnDeviceAdded,
				FrontPanelManager::CallOnDeviceRemoved
			);
	}
inline FrontPanelManager::~FrontPanelManager()
	{ okFrontPanelManager_Destruct(h); }

inline void FrontPanelManager::StartMonitoring(CallbackInfo* cbInfo)
{
	if (okFrontPanelManager_StartMonitoringWithCBInfo(h, cbInfo) != ok_NoError)
		throw std::runtime_error("Failed to start monitoring devices connection.");
}

inline void FrontPanelManager::StopMonitoring()
{
	if (okFrontPanelManager_StopMonitoring(h) != ok_NoError)
		throw std::runtime_error("Failed to stop monitoring devices connection.");
}

inline int FrontPanelManager::EnterMonitorLoop(const CallbackInfo* cbInfo, int millisecondsTimeout)
{
	return okFrontPanelManager_EnterMonitorLoopWithTimeout(h, cbInfo, millisecondsTimeout);
}

inline void FrontPanelManager::ExitMonitorLoop(int exitCode)
{
	okFrontPanelManager_ExitMonitorLoop(h, exitCode);
}

inline OpalKellyLegacy::okCFrontPanel* FrontPanelManager::Open(const char *serial)
{
	okFrontPanel_HANDLE hFP = okFrontPanelManager_Open(h, serial);
	return hFP ? new OpalKellyLegacy::okCFrontPanel(hFP) : NULL;
}

inline FrontPanel::ErrorCode FrontPanelEmulateTestDeviceConnection(const std::string& serial, bool connect)
{
	return((FrontPanel::ErrorCode) okFrontPanelManager_EmulateTestDeviceConnection(serial.c_str(), connect));
}

inline FrontPanelDevices::FrontPanelDevices(const std::string& realm)
	{
		okError* error;
		h = okFrontPanelDevices_Construct(realm.c_str(), &error);
		if (!h)
			throw Impl::Error(error);
	}
inline FrontPanelDevices::~FrontPanelDevices()
	{ okFrontPanelDevices_Destruct(h); }

inline int FrontPanelDevices::GetCount() const
	{ return okFrontPanelDevices_GetCount(h); }
inline std::string FrontPanelDevices::GetSerial(int num) const
	{
		char str[MAX_SERIALNUMBER_LENGTH+1];
		okFrontPanelDevices_GetSerial(h, num, str);
		return str;
	}

inline FrontPanelPtr FrontPanelDevices::Open(const std::string& serial) const
	{
		okFrontPanel_HANDLE hFP = okFrontPanelDevices_Open(h, serial.c_str());
		FrontPanelPtr p;
		if (hFP)
			p.reset(new OpalKellyLegacy::okCFrontPanel(hFP));
		return p;
	}

#endif /* okNO_MANAGER */

} // namespace OpalKelly

// For compatibility with the existing user code, all classes in this namespace
// are currently made globally accessible, this will change in the future API
// versions when their counterparts in OpalKelly namespace are provided.
using namespace OpalKellyLegacy;

#endif // !defined(OPALKELLY_NO_CXX_TYPES)

#endif // __cplusplus

#endif // __okFrontPanel_h__
