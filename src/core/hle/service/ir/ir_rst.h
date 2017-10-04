// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/service.h"

namespace Service {
namespace IR {

union PadState {
    u32_le hex{};

    BitField<14, 1, u32_le> zl;
    BitField<15, 1, u32_le> zr;

    BitField<24, 1, u32_le> c_stick_right;
    BitField<25, 1, u32_le> c_stick_left;
    BitField<26, 1, u32_le> c_stick_up;
    BitField<27, 1, u32_le> c_stick_down;
};

class IR_RST_Interface : public Service::Interface {
public:
    IR_RST_Interface();

    std::string GetPortName() const override {
        return "ir:rst";
    }
};

void InitRST();
void ShutdownRST();

/// Reload input devices. Used when input configuration changed
void ReloadInputDevicesRST();

} // namespace IR
} // namespace Service
