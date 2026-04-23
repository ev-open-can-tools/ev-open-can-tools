#pragma once

#include "can_frame_types.h"
#include "shared_types.h"

#if defined(BYPASS_TLSSC_REQUIREMENT) && !defined(ESP32_DASHBOARD)
inline constexpr bool kBypassTlsscRequirementDefaultEnabled = true;
inline constexpr bool kBypassTlsscRequirementBuildEnabled = true;
#else
inline constexpr bool kBypassTlsscRequirementDefaultEnabled = false;
inline constexpr bool kBypassTlsscRequirementBuildEnabled = false;
#endif

#if defined(ISA_SPEED_CHIME_SUPPRESS) && !defined(ESP32_DASHBOARD)
inline constexpr bool kIsaSpeedChimeSuppressDefaultEnabled = true;
inline constexpr bool kIsaSpeedChimeSuppressBuildEnabled = true;
#else
inline constexpr bool kIsaSpeedChimeSuppressDefaultEnabled = false;
inline constexpr bool kIsaSpeedChimeSuppressBuildEnabled = false;
#endif

#if defined(EMERGENCY_VEHICLE_DETECTION) && !defined(ESP32_DASHBOARD)
inline constexpr bool kEmergencyVehicleDetectionDefaultEnabled = true;
inline constexpr bool kEmergencyVehicleDetectionBuildEnabled = true;
#else
inline constexpr bool kEmergencyVehicleDetectionDefaultEnabled = false;
inline constexpr bool kEmergencyVehicleDetectionBuildEnabled = false;
#endif

#if defined(ENHANCED_AUTOPILOT) && !defined(ESP32_DASHBOARD)
inline constexpr bool kEnhancedAutopilotDefaultEnabled = true;
inline constexpr bool kEnhancedAutopilotBuildEnabled = true;
#else
inline constexpr bool kEnhancedAutopilotDefaultEnabled = false;
inline constexpr bool kEnhancedAutopilotBuildEnabled = false;
#endif

#if defined(NAG_KILLER) && !defined(ESP32_DASHBOARD)
inline constexpr bool kNagKillerDefaultEnabled = true;
inline constexpr bool kNagKillerBuildEnabled = true;
#else
inline constexpr bool kNagKillerDefaultEnabled = false;
inline constexpr bool kNagKillerBuildEnabled = false;
#endif

inline Shared<bool> bypassTlsscRequirementRuntime{kBypassTlsscRequirementDefaultEnabled};
inline Shared<bool> isaSpeedChimeSuppressRuntime{kIsaSpeedChimeSuppressDefaultEnabled};
inline Shared<bool> emergencyVehicleDetectionRuntime{kEmergencyVehicleDetectionDefaultEnabled};
inline Shared<bool> enhancedAutopilotRuntime{kEnhancedAutopilotDefaultEnabled};
inline Shared<bool> nagKillerRuntime{kNagKillerDefaultEnabled};

inline uint8_t readMuxID(const CanFrame &frame)
{
    return frame.data[0] & 0x07;
}

inline bool isADSelectedInUI(const CanFrame &frame)
{
    if (bypassTlsscRequirementRuntime)
        return true;
    return (frame.data[4] >> 6) & 0x01;
}

inline uint8_t readGTWAutopilot(const CanFrame &frame)
{
    return static_cast<uint8_t>((frame.data[5] >> 2) & 0x07);
}

inline const char *describeGTWAutopilot(uint8_t value)
{
    switch (value)
    {
    case 0:
        return "NONE";
    case 1:
        return "HIGHWAY";
    case 2:
        return "ENHANCED";
    case 3:
        return "SELF_DRIVING";
    case 4:
        return "BASIC";
    default:
        return "UNKNOWN";
    }
}

inline void setSpeedProfileV12V13(CanFrame &frame, int profile)
{
    frame.data[6] &= ~0x06;
    frame.data[6] |= (profile << 1);
}

inline uint8_t computeVehicleChecksum(const CanFrame &frame, uint8_t checksumByteIndex = 7)
{
    if (checksumByteIndex >= frame.dlc)
        return 0;

    uint16_t sum = static_cast<uint16_t>(frame.id & 0xFF) +
                   static_cast<uint16_t>((frame.id >> 8) & 0xFF);
    for (uint8_t i = 0; i < frame.dlc; ++i)
    {
        if (i == checksumByteIndex)
            continue;
        sum += frame.data[i];
    }
    return static_cast<uint8_t>(sum & 0xFF);
}

inline void setBit(CanFrame &frame, int bit, bool value)
{
    if (bit < 0 || bit >= 64)
        return; // bounds guard: CanFrame.data is 8 bytes
    int byteIndex = bit / 8;
    int bitIndex = bit % 8;
    uint8_t mask = static_cast<uint8_t>(1U << bitIndex);
    if (value)
    {
        frame.data[byteIndex] |= mask;
    }
    else
    {
        frame.data[byteIndex] &= static_cast<uint8_t>(~mask);
    }
}
