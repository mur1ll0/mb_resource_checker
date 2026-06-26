#ifndef HARDWARE_SCANNER_H
#define HARDWARE_SCANNER_H

#include "types.h"
#include <windows.h>
#include <wbemidl.h>
#include <vector>
#include <string>

class HardwareScanner {
public:
    HardwareScanner();
    ~HardwareScanner();

    // Performs the sweep across SMBIOS, SetupAPI, and WMI
    std::vector<SlotInfo> scanHardware();

    // Query WMI to get CPU name
    std::string scanCPU();

    // Helper to query WMI and return matching property fields
    // Using standard C++98 types for maximum compatibility
    bool queryWMI(const std::wstring& query, 
                  const std::vector<std::wstring>& properties, 
                  std::vector<std::vector<std::wstring> >& results);

private:
    // WMI Connection helpers
    bool initializeWMI();
    void shutdownWMI();

    // Core scanning routines
    std::vector<SlotInfo> scanRAMViaSMBIOS();
    std::vector<SlotInfo> scanPCIeViaSetupAPI();
    std::vector<SlotInfo> scanStorageViaWMI();

    // Helper to extract SMBIOS table via GetSystemFirmwareTable
    std::vector<BYTE> getRawSMBIOSData();

    bool m_coInitialized;
    bool m_wmiInitialized;
    IWbemLocator* m_pLoc;
    IWbemServices* m_pSvc;
};

#endif // HARDWARE_SCANNER_H
