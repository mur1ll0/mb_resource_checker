#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <vector>

// Enum representing the physical slot types
enum SlotType {
    SLOT_PCIE,
    SLOT_M2,
    SLOT_SATA,
    SLOT_RAM
};

// Structure holding the low-level data parsed from SMBIOS, SetupAPI, or WMI
struct SlotInfo {
    std::string name;             // Friendly display name
    SlotType type;                // Type of the slot
    bool occupied;                // True if a device is connected/populated
    std::string deviceName;       // Name of the connected hardware (if occupied)
    std::string physicalLocation; // Physical locator on the motherboard (e.g. DIMM_A1, PCIEX16)
    std::string details;          // Additional technical detail (speed, size, channel, bus width)

    SlotInfo() : type(SLOT_PCIE), occupied(false) {}
};

#endif // TYPES_H
