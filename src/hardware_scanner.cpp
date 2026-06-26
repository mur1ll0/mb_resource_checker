#include "hardware_scanner.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <setupapi.h>
#include <cfgmgr32.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "wbemuuid.lib")

// Raw SMBIOS / DMI structures
#pragma pack(push, 1)
struct RawSMBIOSData {
    BYTE UsedAddressID;
    BYTE MajorVersion;
    BYTE MinorVersion;
    BYTE DmiRevision;
    DWORD Length;
    BYTE SMBIOSTableData[1];
};

struct SMBIOSHeader {
    BYTE Type;
    BYTE Length;
    WORD Handle;
};

struct SMBIOS_MemoryDevice {
    SMBIOSHeader Header;
    WORD PhysicalArrayHandle;
    WORD ErrorInfoHandle;
    WORD TotalWidth;
    WORD DataWidth;
    WORD Size;
    BYTE FormFactor;
    BYTE DeviceSet;
    BYTE DeviceLocator; // String index
    BYTE BankLocator;   // String index
    BYTE MemoryType;
    WORD TypeDetail;
    WORD Speed;
    BYTE Manufacturer;  // String index
    BYTE SerialNumber;  // String index
    BYTE AssetTag;      // String index
    BYTE PartNumber;    // String index
};

struct SMBIOS_SystemSlots {
    SMBIOSHeader Header;
    BYTE SlotDesignation; // String index
    BYTE SlotType;        // Slot Type (e.g. 0xA5 for PCIe x16)
    BYTE SlotDataBusWidth;
    BYTE CurrentUsage;    // 0x03 = Available, 0x04 = In Use
    BYTE SlotLength;
    WORD SlotID;
    BYTE SlotCharacteristics1;
    BYTE SlotCharacteristics2;
    // SMBIOS 2.6+ fields
    WORD SegmentGroupNumber;
    BYTE BusNumber;
    BYTE DeviceFunctionNumber;
};
#pragma pack(pop)

// Helper to extract a string from SMBIOS string block
static std::string GetSMBIOSString(const SMBIOSHeader* header, BYTE index) {
    if (index == 0) return "Empty";
    const char* ptr = reinterpret_cast<const char*>(header) + header->Length;
    BYTE count = 1;
    while (count < index) {
        while (*ptr != 0) {
            ptr++;
        }
        ptr++; // Skip the null terminator
        if (*ptr == 0) {
            return "Empty"; // Double null means end of string block
        }
        count++;
    }
    return std::string(ptr);
}

// Convert SMBIOS memory type code to friendly string
static std::string GetMemoryTypeString(BYTE type) {
    switch (type) {
        case 0x01: return "Other";
        case 0x02: return "Unknown";
        case 0x03: return "DRAM";
        case 0x09: return "RAM";
        case 0x0F: return "SDRAM";
        case 0x12: return "DDR";
        case 0x13: return "DDR2";
        case 0x18: return "DDR3";
        case 0x1A: return "DDR4";
        case 0x1E: return "DDR5";
        default: return "DDRx";
    }
}

// Helper to query device description or friendly name from Config Manager
static bool GetDeviceDescription(DEVINST devInst, char* buffer, DWORD maxLen) {
    DWORD size = maxLen;
    // Try friendly name first (common for storage disks, network devices)
    if (CM_Get_DevNode_Registry_PropertyA(devInst, CM_DRP_FRIENDLYNAME, NULL, buffer, &size, 0) == CR_SUCCESS) {
        return true;
    }
    // Fallback to device description (common for GPUs, audio cards, bridges)
    size = maxLen;
    if (CM_Get_DevNode_Registry_PropertyA(devInst, CM_DRP_DEVICEDESC, NULL, buffer, &size, 0) == CR_SUCCESS) {
        return true;
    }
    return false;
}

// Helper to query device class name from Config Manager
static std::string GetDeviceClass(DEVINST devInst) {
    char classBuf[128] = {0};
    DWORD size = sizeof(classBuf);
    if (CM_Get_DevNode_Registry_PropertyA(devInst, CM_DRP_CLASS, NULL, classBuf, &size, 0) == CR_SUCCESS) {
        return std::string(classBuf);
    }
    return "";
}

// Struct to store SMBIOS Slot information for SetupAPI correlation
struct TempSmbiosSlot {
    std::string designation;
    BYTE slotType;
    BYTE busNum;
    BYTE devFunc;
    bool occupied;
    bool hasBusInfo;
    bool matched;
    std::string deviceName;
    std::string details;
    SlotType deducedType;
};

HardwareScanner::HardwareScanner() 
    : m_coInitialized(false), m_wmiInitialized(false), m_pLoc(NULL), m_pSvc(NULL) {
}

HardwareScanner::~HardwareScanner() {
    shutdownWMI();
}

bool HardwareScanner::initializeWMI() {
    if (m_wmiInitialized) return true;

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        if (hr == RPC_E_CHANGED_MODE) {
            m_coInitialized = false; // Initialized elsewhere (e.g. Qt)
        } else {
            return false;
        }
    } else {
        m_coInitialized = true;
    }

    hr = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL
    );

    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        shutdownWMI();
        return false;
    }

    hr = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&m_pLoc
    );

    if (FAILED(hr)) {
        shutdownWMI();
        return false;
    }

    hr = m_pLoc->ConnectServer(
        SysAllocString(L"ROOT\\CIMV2"),
        NULL, NULL, 0, 0, 0, 0,
        &m_pSvc
    );

    if (FAILED(hr)) {
        shutdownWMI();
        return false;
    }

    hr = CoSetProxyBlanket(
        m_pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );

    if (FAILED(hr)) {
        shutdownWMI();
        return false;
    }

    m_wmiInitialized = true;
    return true;
}

void HardwareScanner::shutdownWMI() {
    if (m_pSvc) {
        m_pSvc->Release();
        m_pSvc = NULL;
    }
    if (m_pLoc) {
        m_pLoc->Release();
        m_pLoc = NULL;
    }
    if (m_coInitialized) {
        CoUninitialize();
        m_coInitialized = false;
    }
    m_wmiInitialized = false;
}

bool HardwareScanner::queryWMI(const std::wstring& query, 
                              const std::vector<std::wstring>& properties, 
                              std::vector<std::vector<std::wstring> >& results) {
    if (!initializeWMI()) return false;

    IEnumWbemClassObject* pEnumerator = NULL;
    HRESULT hr = m_pSvc->ExecQuery(
        SysAllocString(L"WQL"),
        SysAllocString(query.c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );

    if (FAILED(hr)) return false;

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    while (pEnumerator) {
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn || FAILED(hr)) {
            break;
        }

        std::vector<std::wstring> row;
        for (size_t i = 0; i < properties.size(); ++i) {
            VARIANT vtProp;
            VariantInit(&vtProp);

            hr = pclsObj->Get(properties[i].c_str(), 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr)) {
                std::wstringstream wss;
                if (vtProp.vt == VT_BSTR && vtProp.bstrVal != NULL) {
                    wss << vtProp.bstrVal;
                } else if (vtProp.vt == VT_I4) {
                    wss << vtProp.lVal;
                } else if (vtProp.vt == VT_UI4) {
                    wss << vtProp.ulVal;
                } else if (vtProp.vt == VT_BOOL) {
                    wss << (vtProp.boolVal ? L"True" : L"False");
                }
                row.push_back(wss.str());
            } else {
                row.push_back(L"");
            }
            VariantClear(&vtProp);
        }
        results.push_back(row);
        pclsObj->Release();
    }

    if (pEnumerator) pEnumerator->Release();
    return true;
}

std::vector<BYTE> HardwareScanner::getRawSMBIOSData() {
    std::vector<BYTE> buffer;
    
    // Dynamically resolve GetSystemFirmwareTable to avoid link errors on older XP 32-bit SP2
    typedef UINT (WINAPI *PFN_GetSystemFirmwareTable)(DWORD, DWORD, PVOID, DWORD);
    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
    if (!hKernel) return buffer;

    PFN_GetSystemFirmwareTable pGetSystemFirmwareTable = 
        (PFN_GetSystemFirmwareTable)GetProcAddress(hKernel, "GetSystemFirmwareTable");

    if (pGetSystemFirmwareTable) {
        // RSMB signature is 0x52534D42 ('RSMB')
        DWORD size = pGetSystemFirmwareTable('RSMB', 0, NULL, 0);
        if (size > 0) {
            buffer.resize(size);
            pGetSystemFirmwareTable('RSMB', 0, &buffer[0], size);
        }
    }
    return buffer;
}

std::vector<SlotInfo> HardwareScanner::scanRAMViaSMBIOS() {
    std::vector<SlotInfo> slots;
    std::vector<BYTE> smbiosBuf = getRawSMBIOSData();

    if (smbiosBuf.empty()) {
        // Fallback to WMI for systems where GetSystemFirmwareTable is unavailable
        std::vector<std::wstring> props;
        props.push_back(L"DeviceLocator");
        props.push_back(L"Capacity");
        props.push_back(L"Speed");
        props.push_back(L"Manufacturer");

        std::vector<std::vector<std::wstring> > wmiResults;
        if (queryWMI(L"SELECT DeviceLocator, Capacity, Speed, Manufacturer FROM Win32_PhysicalMemory", props, wmiResults)) {
            for (size_t i = 0; i < wmiResults.size(); ++i) {
                SlotInfo slot;
                std::wstring wLocator = wmiResults[i][0];
                std::wstring wCapacity = wmiResults[i][1];
                std::wstring wSpeed = wmiResults[i][2];
                std::wstring wManuf = wmiResults[i][3];

                slot.type = SLOT_RAM;
                slot.physicalLocation = std::string(wLocator.begin(), wLocator.end());
                slot.name = slot.physicalLocation;
                
                unsigned long long capacityBytes = 0;
                std::wstringstream wss(wCapacity);
                wss >> capacityBytes;
                
                if (capacityBytes > 0) {
                    slot.occupied = true;
                    double capacityGB = capacityBytes / (1024.0 * 1024.0 * 1024.0);
                    std::stringstream ss;
                    ss << "DIMM " << capacityGB << " GB " 
                       << std::string(wSpeed.begin(), wSpeed.end()) << "MHz";
                    slot.deviceName = ss.str();
                    slot.details = std::string(wManuf.begin(), wManuf.end());
                } else {
                    slot.occupied = false;
                    slot.deviceName = "Empty Slot";
                }
                slots.push_back(slot);
            }
        }
        
        // If we still have 0 slots, simulate a standard layout
        if (slots.empty()) {
            for (int i = 1; i <= 4; ++i) {
                SlotInfo slot;
                slot.type = SLOT_RAM;
                std::stringstream ss;
                ss << "DIMM_" << i;
                slot.physicalLocation = ss.str();
                slot.name = slot.physicalLocation;
                slot.occupied = false;
                slot.deviceName = "Empty Slot";
                slots.push_back(slot);
            }
        }
        return slots;
    }

    // Direct SMBIOS parsing
    const RawSMBIOSData* smbiosData = reinterpret_cast<const RawSMBIOSData*>(&smbiosBuf[0]);
    const BYTE* ptr = smbiosData->SMBIOSTableData;
    const BYTE* endPtr = ptr + smbiosData->Length;

    int slotIndex = 1;
    while (ptr < endPtr) {
        const SMBIOSHeader* header = reinterpret_cast<const SMBIOSHeader*>(ptr);
        if (header->Length < sizeof(SMBIOSHeader)) break;

        if (header->Type == 17) { // Memory Device
            const SMBIOS_MemoryDevice* mem = reinterpret_cast<const SMBIOS_MemoryDevice*>(ptr);
            SlotInfo slot;
            slot.type = SLOT_RAM;
            
            std::string locator = GetSMBIOSString(header, mem->DeviceLocator);
            if (locator == "Empty" || locator.empty()) {
                std::stringstream ss;
                ss << "DIMM_" << slotIndex++;
                locator = ss.str();
            }
            slot.physicalLocation = locator;
            slot.name = locator;

            // Determine if slot is occupied
            WORD size = mem->Size;
            if (size == 0 || size == 0xFFFF) {
                slot.occupied = false;
                slot.deviceName = "Empty Slot";
            } else {
                slot.occupied = true;
                double sizeGB = 0;
                if (size == 0x7FFF && header->Length >= 0x20) {
                    // SMBIOS 2.7+ Extended Size
                    DWORD extSize = *reinterpret_cast<const DWORD*>(ptr + 0x1C);
                    sizeGB = extSize / 1024.0;
                } else {
                    if (size & 0x8000) {
                        sizeGB = (size & 0x7FFF) / (1024.0 * 1024.0); // KB to GB
                    } else {
                        sizeGB = size / 1024.0; // MB to GB
                    }
                }
                
                std::string memType = GetMemoryTypeString(mem->MemoryType);
                std::stringstream ss;
                ss << memType << " " << sizeGB << " GB";
                slot.deviceName = ss.str();

                std::stringstream detailsSS;
                detailsSS << mem->Speed << " MHz | Part: " << GetSMBIOSString(header, mem->PartNumber);
                slot.details = detailsSS.str();
            }
            slots.push_back(slot);
        }

        // Advance to next structure
        const BYTE* strPtr = ptr + header->Length;
        while (strPtr < endPtr && !(*strPtr == 0 && *(strPtr + 1) == 0)) {
            strPtr++;
        }
        strPtr += 2; // Skip double null
        ptr = strPtr;
    }

    return slots;
}

std::vector<SlotInfo> HardwareScanner::scanPCIeViaSetupAPI() {
    std::vector<SlotInfo> slots;
    std::vector<TempSmbiosSlot> smbiosSlots;

    // Phase 1: Try parsing SMBIOS Type 9 (System Slots) to get motherboard designations & bus paths
    std::vector<BYTE> smbiosBuf = getRawSMBIOSData();
    if (!smbiosBuf.empty()) {
        const RawSMBIOSData* smbiosData = reinterpret_cast<const RawSMBIOSData*>(&smbiosBuf[0]);
        const BYTE* ptr = smbiosData->SMBIOSTableData;
        const BYTE* endPtr = ptr + smbiosData->Length;

        while (ptr < endPtr) {
            const SMBIOSHeader* header = reinterpret_cast<const SMBIOSHeader*>(ptr);
            if (header->Length < sizeof(SMBIOSHeader)) break;

            if (header->Type == 9) { // System Slots
                const SMBIOS_SystemSlots* s = reinterpret_cast<const SMBIOS_SystemSlots*>(ptr);
                TempSmbiosSlot tSlot;
                tSlot.designation = GetSMBIOSString(header, s->SlotDesignation);
                tSlot.slotType = s->SlotType;
                tSlot.occupied = (s->CurrentUsage == 0x04);
                tSlot.hasBusInfo = false;

                // Segment, Bus, Dev/Func are defined in SMBIOS 2.6+
                if (header->Length >= 17) {
                    tSlot.busNum = s->BusNumber;
                    tSlot.devFunc = s->DeviceFunctionNumber;
                    tSlot.hasBusInfo = true;
                }
                smbiosSlots.push_back(tSlot);
            }

            const BYTE* strPtr = ptr + header->Length;
            while (strPtr < endPtr && !(*strPtr == 0 && *(strPtr + 1) == 0)) {
                strPtr++;
            }
            strPtr += 2;
            ptr = strPtr;
        }
    }

    // Phase 2: Sweep PCI bus via SetupAPI and query present devices (non-bridges)
    HDEVINFO hDevInfo = SetupDiGetClassDevsA(
        NULL,
        "PCI",
        NULL,
        DIGCF_ALLCLASSES | DIGCF_PRESENT
    );

    if (hDevInfo != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        
        DWORD index = 0;
        while (SetupDiEnumDeviceInfo(hDevInfo, index++, &devInfoData)) {
            // Check if device is a PCI-to-PCI Bridge (we skip bridges, we want devices plugged in)
            char hwIdBuf[512] = {0};
            bool isBridge = false;
            if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_HARDWAREID, NULL, (BYTE*)hwIdBuf, sizeof(hwIdBuf), NULL)) {
                char* p = hwIdBuf;
                while (*p) {
                    std::string id(p);
                    if (id.find("CC_0604") != std::string::npos) {
                        isBridge = true;
                        break;
                    }
                    p += id.length() + 1;
                }
            }

            if (isBridge) continue; // Skip bridge controllers themselves

            // Get parent of this PCI device (which is the PCIe Root Port bridge)
            DEVINST parentInst = 0;
            if (CM_Get_Parent(&parentInst, devInfoData.DevInst, 0) != CR_SUCCESS) {
                continue;
            }

            // Query parent's Bus Number and Device Address
            DWORD busNumber = 0;
            DWORD size = sizeof(busNumber);
            if (CM_Get_DevNode_Registry_PropertyA(parentInst, CM_DRP_BUSNUMBER, NULL, &busNumber, &size, 0) != CR_SUCCESS) {
                continue;
            }

            DWORD address = 0;
            size = sizeof(address);
            if (CM_Get_DevNode_Registry_PropertyA(parentInst, CM_DRP_ADDRESS, NULL, &address, &size, 0) != CR_SUCCESS) {
                continue;
            }

            BYTE devNum = (BYTE)((address >> 16) & 0xFF);
            BYTE funcNum = (BYTE)(address & 0xFF);
            BYTE devFuncByte = (devNum << 3) | (funcNum & 0x07);

            // Fetch device details
            char devDescBuf[256] = {0};
            std::string deviceName = "Unknown Device";
            if (GetDeviceDescription(devInfoData.DevInst, devDescBuf, sizeof(devDescBuf))) {
                deviceName = devDescBuf;
            }

            // Determine if the device is a storage controller (like NVMe) or has child disks
            std::string className = GetDeviceClass(devInfoData.DevInst);
            SlotType deducedType = SLOT_PCIE;
            
            if (className == "Display" || deviceName.find("NVIDIA") != std::string::npos || deviceName.find("Radeon") != std::string::npos || deviceName.find("GeForce") != std::string::npos || deviceName.find("AMD") != std::string::npos) {
                deducedType = SLOT_PCIE;
            } else if (className == "SCSIAdapter" || deviceName.find("NVM Express") != std::string::npos || deviceName.find("NVMe") != std::string::npos) {
                deducedType = SLOT_M2;
                // If it's an NVMe controller, try to get the friendly name of the child disk drive
                DEVINST childInst = 0;
                if (CM_Get_Child(&childInst, devInfoData.DevInst, 0) == CR_SUCCESS) {
                    char childDescBuf[256] = {0};
                    if (GetDeviceDescription(childInst, childDescBuf, sizeof(childDescBuf))) {
                        std::string childDesc = childDescBuf;
                        if (!childDesc.empty() && childDesc.find("Controller") == std::string::npos) {
                            deviceName = childDesc; // Use the actual disk friendly name
                        }
                    }
                }
            } else if (className == "Net" && (deviceName.find("Wireless") != std::string::npos || deviceName.find("Wi-Fi") != std::string::npos || deviceName.find("WLAN") != std::string::npos)) {
                deducedType = SLOT_M2; // M.2 Wi-Fi slot
            }

            // Correlate with SMBIOS Slots
            bool correlated = false;
            for (size_t i = 0; i < smbiosSlots.size(); ++i) {
                if (smbiosSlots[i].hasBusInfo && smbiosSlots[i].busNum == (BYTE)busNumber && smbiosSlots[i].devFunc == devFuncByte) {
                    smbiosSlots[i].occupied = true;
                    smbiosSlots[i].deviceName = deviceName;
                    smbiosSlots[i].deducedType = deducedType;
                    smbiosSlots[i].matched = true;
                    
                    std::stringstream detailsSS;
                    detailsSS << "Bus " << busNumber << " | Dev " << (int)devNum << " | Func " << (int)funcNum;
                    smbiosSlots[i].details = detailsSS.str();
                    
                    correlated = true;
                    break;
                }
            }

            // If this is a major expansion device (like a GPU or NVMe) but could not be correlated with SMBIOS
            // (e.g. incomplete BIOS), add it as a dynamic custom slot so it's not lost!
            if (!correlated && (deducedType == SLOT_M2 || className == "Display")) {
                SlotInfo dSlot;
                std::stringstream ss;
                if (deducedType == SLOT_M2) {
                    ss << "M.2 Slot [Bus " << busNumber << "]";
                } else {
                    ss << "PCIe Slot [Bus " << busNumber << "]";
                }
                dSlot.name = ss.str();
                dSlot.physicalLocation = ss.str();
                dSlot.type = deducedType;
                dSlot.occupied = true;
                dSlot.deviceName = deviceName;

                std::stringstream detailsSS;
                detailsSS << "Bus " << busNumber << " | Dev " << (int)devNum << " | Func " << (int)funcNum;
                dSlot.details = detailsSS.str();

                slots.push_back(dSlot);
            }
        }
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }

    // Now copy all SMBIOS slots (occupied and empty) to the output slots list
    for (size_t i = 0; i < smbiosSlots.size(); ++i) {
        SlotInfo slot;
        slot.name = smbiosSlots[i].designation;
        slot.physicalLocation = smbiosSlots[i].designation;
        slot.type = smbiosSlots[i].deducedType;
        slot.occupied = smbiosSlots[i].occupied;
        slot.deviceName = smbiosSlots[i].occupied ? smbiosSlots[i].deviceName : "Empty Slot";
        slot.details = smbiosSlots[i].details;
        
        if (slot.details.empty() && smbiosSlots[i].hasBusInfo) {
            std::stringstream detailsSS;
            detailsSS << "Bus " << (int)smbiosSlots[i].busNum << " | Dev " << (int)(smbiosSlots[i].devFunc >> 3) << " | Func " << (int)(smbiosSlots[i].devFunc & 0x07);
            slot.details = detailsSS.str();
        }

        slots.push_back(slot);
    }

    // If everything failed, populate fallback testing slots
    if (slots.empty()) {
        SlotInfo pcieSlot;
        pcieSlot.name = "PCIEX16_1";
        pcieSlot.physicalLocation = "PCIEX16_1";
        pcieSlot.type = SLOT_PCIE;
        pcieSlot.occupied = false;
        pcieSlot.deviceName = "Empty Slot";
        slots.push_back(pcieSlot);

        SlotInfo m2Slot;
        m2Slot.name = "M2_1";
        m2Slot.physicalLocation = "M2_1";
        m2Slot.type = SLOT_M2;
        m2Slot.occupied = false;
        m2Slot.deviceName = "Empty Slot";
        slots.push_back(m2Slot);
    }

    return slots;
}

std::vector<SlotInfo> HardwareScanner::scanStorageViaWMI() {
    std::vector<SlotInfo> slots;
    
    // Query WMI Disk Drives to enrich SATA and M.2 storage visualization
    std::vector<std::wstring> props;
    props.push_back(L"Model");
    props.push_back(L"InterfaceType");
    props.push_back(L"PNPDeviceID");

    std::vector<std::vector<std::wstring> > results;
    int sataOccupiedCount = 0;
    
    if (queryWMI(L"SELECT Model, InterfaceType, PNPDeviceID FROM Win32_DiskDrive", props, results)) {
        for (size_t i = 0; i < results.size(); ++i) {
            std::string model = std::string(results[i][0].begin(), results[i][0].end());
            std::string interfaceType = std::string(results[i][1].begin(), results[i][1].end());
            std::string pnpId = std::string(results[i][2].begin(), results[i][2].end());

            std::transform(pnpId.begin(), pnpId.end(), pnpId.begin(), ::tolower);
            std::transform(interfaceType.begin(), interfaceType.end(), interfaceType.begin(), ::tolower);

            // Exclude USB drives
            if (interfaceType.find("usb") != std::string::npos || pnpId.find("usb") != std::string::npos) {
                continue;
            }

            // Identify NVMe vs SATA
            if (pnpId.find("nvme") != std::string::npos || model.find("NVMe") != std::string::npos) {
                // If it is NVMe, it is mapped to SLOT_M2.
                // Usually this is already handled by SetupAPI PCIe root ports, 
                // but we will keep track of storage names here to correlate if needed.
                continue;
            }

            // If it's not USB and not NVMe, treat it as a SATA drive
            sataOccupiedCount++;
            SlotInfo slot;
            std::stringstream ss;
            ss << "SATA6G_" << sataOccupiedCount;
            slot.name = ss.str();
            slot.physicalLocation = ss.str();
            slot.type = SLOT_SATA;
            slot.occupied = true;
            slot.deviceName = model;
            slot.details = "SATA III 6.0 Gb/s";
            slots.push_back(slot);
        }
    }

    // Add remaining unoccupied SATA ports (assume a standard motherboard with 4 SATA ports)
    int maxSataPorts = 4;
    if (sataOccupiedCount < maxSataPorts) {
        for (int i = sataOccupiedCount + 1; i <= maxSataPorts; ++i) {
            SlotInfo slot;
            std::stringstream ss;
            ss << "SATA6G_" << i;
            slot.name = ss.str();
            slot.physicalLocation = ss.str();
            slot.type = SLOT_SATA;
            slot.occupied = false;
            slot.deviceName = "Empty Port";
            slot.details = "SATA III 6.0 Gb/s";
            slots.push_back(slot);
        }
    }

    return slots;
}

std::vector<SlotInfo> HardwareScanner::scanHardware() {
    std::vector<SlotInfo> allSlots;

    // Scan memory slots
    std::vector<SlotInfo> ramSlots = scanRAMViaSMBIOS();
    allSlots.insert(allSlots.end(), ramSlots.begin(), ramSlots.end());

    // Scan PCIe and M.2 Root Ports
    std::vector<SlotInfo> pcieSlots = scanPCIeViaSetupAPI();
    allSlots.insert(allSlots.end(), pcieSlots.begin(), pcieSlots.end());

    // Scan Storage drives (SATA)
    std::vector<SlotInfo> sataSlots = scanStorageViaWMI();
    allSlots.insert(allSlots.end(), sataSlots.begin(), sataSlots.end());

    return allSlots;
}

std::string HardwareScanner::scanCPU() {
    std::vector<std::wstring> props;
    props.push_back(L"Name");

    std::vector<std::vector<std::wstring> > results;
    if (queryWMI(L"SELECT Name FROM Win32_Processor", props, results) && !results.empty()) {
        std::string name(results[0][0].begin(), results[0][0].end());
        // Trim whitespace
        name.erase(name.find_last_not_of(" \t\r\n") + 1);
        name.erase(0, name.find_first_not_of(" \t\r\n"));
        return name;
    }
    return "Generic CPU";
}
