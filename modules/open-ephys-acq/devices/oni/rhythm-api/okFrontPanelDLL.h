///------------------------------------------------------------------------
// okFrontPanelDLL.h
//
// This is the compatibility header file for FrontPanel library. Please use
// okFrontPanel.h in new code.
//
//------------------------------------------------------------------------
// Copyright (c) 2005-2020 Opal Kelly Incorporated
//------------------------------------------------------------------------

#ifndef __okFrontPanelDLL_h__
#define __okFrontPanelDLL_h__

#include "okFrontPanel.h"

extern "C"
{

// Define useless functions that exist only for backwards compatibility.
okDLLEXPORT Bool DLL_ENTRY okFrontPanelDLL_LoadLib(okFP_dll_pchar libname);
okDLLEXPORT void DLL_ENTRY okFrontPanelDLL_FreeLib(void);
okDLLEXPORT void DLL_ENTRY okFrontPanelDLL_GetVersion(char *date, char *time);

// Define compatibility synonyms for the functions without "DLL" in their name.
okDLLEXPORT int DLL_ENTRY okFrontPanelDLL_GetAPIVersionMajor();
okDLLEXPORT int DLL_ENTRY okFrontPanelDLL_GetAPIVersionMinor();
okDLLEXPORT int DLL_ENTRY okFrontPanelDLL_GetAPIVersionMicro();
okDLLEXPORT const char* DLL_ENTRY okFrontPanelDLL_GetAPIVersionString();
okDLLEXPORT Bool DLL_ENTRY okFrontPanelDLL_CheckAPIVersion(int major, int minor, int micro);

inline Bool okFrontPanelDLL_TryLoadLib() { return okFrontPanel_TryLoadLib(); }

}

#endif // __okFrontPanelDLL_h__
