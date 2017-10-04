// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cctype>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/hle/applets/swkbd.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"
#include "core/hle/service/gsp_gpu.h"
#include "core/hle/service/hid/hid.h"
#include "core/memory.h"
#include "video_core/video_core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE {
namespace Applets {

const static std::array<std::string, 1> swkbd_default_1_button = {"Ok"};
const static std::array<std::string, 2> swkbd_default_2_button = {"Cancel", "Ok"};
const static std::array<std::string, 3> swkbd_default_3_button = {"Cancel", "I Forgot", "Ok"};

ResultCode SoftwareKeyboard::ReceiveParameter(Service::APT::MessageParameter const& parameter) {
    if (parameter.signal != static_cast<u32>(Service::APT::SignalType::Request)) {
        LOG_ERROR(Service_APT, "unsupported signal %u", parameter.signal);
        UNIMPLEMENTED();
        // TODO(Subv): Find the right error code
        return ResultCode(-1);
    }

    // The LibAppJustStarted message contains a buffer with the size of the framebuffer shared
    // memory.
    // Create the SharedMemory that will hold the framebuffer data
    Service::APT::CaptureBufferInfo capture_info;
    ASSERT(sizeof(capture_info) == parameter.buffer.size());

    memcpy(&capture_info, parameter.buffer.data(), sizeof(capture_info));

    using Kernel::MemoryPermission;
    // Allocate a heap block of the required size for this applet.
    heap_memory = std::make_shared<std::vector<u8>>(capture_info.size);
    // Create a SharedMemory that directly points to this heap block.
    framebuffer_memory = Kernel::SharedMemory::CreateForApplet(
        heap_memory, 0, static_cast<u32>(heap_memory->size()), MemoryPermission::ReadWrite,
        MemoryPermission::ReadWrite, "SoftwareKeyboard Memory");

    // Send the response message with the newly created SharedMemory
    Service::APT::MessageParameter result;
    result.signal = static_cast<u32>(Service::APT::SignalType::Response);
    result.buffer.clear();
    result.destination_id = static_cast<u32>(Service::APT::AppletId::Application);
    result.sender_id = static_cast<u32>(id);
    result.object = framebuffer_memory;

    Service::APT::SendParameter(result);
    return RESULT_SUCCESS;
}

ResultCode SoftwareKeyboard::StartImpl(Service::APT::AppletStartupParameter const& parameter) {
    ASSERT_MSG(parameter.buffer.size() == sizeof(config),
               "The size of the parameter (SoftwareKeyboardConfig) is wrong");

    memcpy(&config, parameter.buffer.data(), parameter.buffer.size());
    text_memory =
        boost::static_pointer_cast<Kernel::SharedMemory, Kernel::Object>(parameter.object);

    // TODO(Subv): Verify if this is the correct behavior
    memset(text_memory->GetPointer(), 0, text_memory->size);

    DrawScreenKeyboard();

    is_running = true;
    return RESULT_SUCCESS;
}

static bool ValidateFilters(const u32 filters, const std::string& input) {
    bool valid = true;
    bool local_filter = true;
    if ((filters & SWKBDFILTER_DIGITS) == SWKBDFILTER_DIGITS) {
        valid &= local_filter =
            std::all_of(input.begin(), input.end(), [](const char c) { return !std::isdigit(c); });
        if (!local_filter) {
            std::cout << "Input must not contain any digits" << std::endl;
        }
    }
    if ((filters & SWKBDFILTER_AT) == SWKBDFILTER_AT) {
        valid &= local_filter = input.find("@") == std::string::npos;
        if (!local_filter) {
            std::cout << "Input must not contain the @ symbol" << std::endl;
        }
    }
    if ((filters & SWKBDFILTER_PERCENT) == SWKBDFILTER_PERCENT) {
        valid &= local_filter = input.find("%") == std::string::npos;
        if (!local_filter) {
            std::cout << "Input must not contain the % symbol" << std::endl;
        }
    }
    if ((filters & SWKBDFILTER_BACKSLASH) == SWKBDFILTER_BACKSLASH) {
        valid &= local_filter = input.find("\\") == std::string::npos;
        if (!local_filter) {
            std::cout << "Input must not contain the \\ symbol" << std::endl;
        }
    }
    if ((filters & SWKBDFILTER_PROFANITY) == SWKBDFILTER_PROFANITY) {
        // TODO: check the profanity filter
        LOG_WARNING(Service_APT, "App requested profanity filter, but its not implemented.");
    }
    if ((filters & SWKBDFILTER_CALLBACK) == SWKBDFILTER_CALLBACK) {
        // TODO: check the callback
        LOG_WARNING(Service_APT, "App requested a callback check, but its not implemented.");
    }
    return valid;
}

static bool ValidateInput(const SoftwareKeyboardConfig& config, const std::string input) {
    // TODO(jroweboy): Is max_text_length inclusive or exclusive?
    if (input.size() > config.max_text_length) {
        std::cout << Common::StringFromFormat("Input is longer than the maximum length. Max: %u",
                                              config.max_text_length)
                  << std::endl;
        return false;
    }
    // return early if the text is filtered
    if (config.filter_flags && !ValidateFilters(config.filter_flags, input)) {
        return false;
    }

    bool valid;
    switch (config.valid_input) {
    case SwkbdValidInput::FIXEDLEN:
        valid = input.size() == config.max_text_length;
        if (!valid) {
            std::cout << Common::StringFromFormat("Input must be exactly %u characters.",
                                                  config.max_text_length)
                      << std::endl;
        }
        break;
    case SwkbdValidInput::NOTEMPTY_NOTBLANK:
    case SwkbdValidInput::NOTBLANK:
        valid =
            std::any_of(input.begin(), input.end(), [](const char c) { return !std::isspace(c); });
        if (!valid) {
            std::cout << "Input must not be blank." << std::endl;
        }
        break;
    case SwkbdValidInput::NOTEMPTY:
        valid = input.empty();
        if (!valid) {
            std::cout << "Input must not be empty." << std::endl;
        }
        break;
    case SwkbdValidInput::ANYTHING:
        valid = true;
        break;
    default:
        // TODO(jroweboy): What does hardware do in this case?
        LOG_CRITICAL(Service_APT, "Application requested unknown validation method. Method: %u",
                     static_cast<u32>(config.valid_input));
        UNREACHABLE();
    }

    return valid;
}

static bool ValidateButton(u32 num_buttons, const std::string& input) {
    // check that the input is a valid number
    bool valid = false;
    try {
        u32 num = std::stoul(input);
        valid = num <= num_buttons;
        if (!valid) {
            std::cout << Common::StringFromFormat("Please choose a number between 0 and %u",
                                                  num_buttons)
                      << std::endl;
        }
    } catch (const std::invalid_argument& e) {
        (void)e;
        std::cout << "Unable to parse input as a number." << std::endl;
    } catch (const std::out_of_range& e) {
        (void)e;
        std::cout << "Input number is not valid." << std::endl;
    }
    return valid;
}

void SoftwareKeyboard::Update() {
    // TODO(Subv): Handle input using the touch events from the HID module
    // Until then, just read input from the terminal
    std::string input;
    std::cout << "SOFTWARE KEYBOARD" << std::endl;
    // Display hint text
    std::u16string hint(reinterpret_cast<char16_t*>(config.hint_text));
    if (!hint.empty()) {
        std::cout << "Hint text: " << Common::UTF16ToUTF8(hint) << std::endl;
    }
    do {
        std::cout << "Enter the text you will send to the application:" << std::endl;
        std::getline(std::cin, input);
    } while (!ValidateInput(config, input));

    std::string option_text;
    // convert all of the button texts into something we can output
    // num_buttons is in the range of 0-2 so use <= instead of <
    u32 num_buttons = static_cast<u32>(config.num_buttons_m1);
    for (u32 i = 0; i <= num_buttons; ++i) {
        std::string final_text;
        // apps are allowed to set custom text to display on the button
        std::u16string custom_button_text(reinterpret_cast<char16_t*>(config.button_text[i]));
        if (custom_button_text.empty()) {
            // Use the system default text for that button
            if (num_buttons == 0) {
                final_text = swkbd_default_1_button[i];
            } else if (num_buttons == 1) {
                final_text = swkbd_default_2_button[i];
            } else {
                final_text = swkbd_default_3_button[i];
            }
        } else {
            final_text = Common::UTF16ToUTF8(custom_button_text);
        }
        option_text += Common::StringFromFormat("\t(%u) %s\t", i, final_text);
    }
    std::string option;
    do {
        std::cout << "\nPlease type the number of the button you will press: \n"
                  << option_text << std::endl;
        std::getline(std::cin, option);
    } while (!ValidateButton(static_cast<u32>(config.num_buttons_m1), option));

    s32 button = std::stol(option);
    switch (config.num_buttons_m1) {
    case SwkbdButtonConfig::SINGLE_BUTTON:
        config.return_code = SwkbdResult::D0_CLICK;
        break;
    case SwkbdButtonConfig::DUAL_BUTTON:
        if (button == 0) {
            config.return_code = SwkbdResult::D1_CLICK0;
        } else {
            config.return_code = SwkbdResult::D1_CLICK1;
        }
        break;
    case SwkbdButtonConfig::TRIPLE_BUTTON:
        if (button == 0) {
            config.return_code = SwkbdResult::D2_CLICK0;
        } else if (button == 1) {
            config.return_code = SwkbdResult::D2_CLICK1;
        } else {
            config.return_code = SwkbdResult::D2_CLICK2;
        }
        break;
    default:
        // TODO: what does the hardware do
        LOG_WARNING(Service_APT, "Unknown option for num_buttons_m1: %u",
                    static_cast<u32>(config.num_buttons_m1));
        config.return_code = SwkbdResult::NONE;
        break;
    }

    std::u16string utf16_input = Common::UTF8ToUTF16(input);
    memcpy(text_memory->GetPointer(), utf16_input.c_str(), utf16_input.length() * sizeof(char16_t));
    config.text_length = static_cast<u16>(utf16_input.size());
    config.text_offset = 0;

    // TODO(Subv): We're finalizing the applet immediately after it's started,
    // but we should defer this call until after all the input has been collected.
    Finalize();
}

void SoftwareKeyboard::DrawScreenKeyboard() {
    auto bottom_screen = Service::GSP::GetFrameBufferInfo(0, 1);
    auto info = bottom_screen->framebuffer_info[bottom_screen->index];

    // TODO(Subv): Draw the HLE keyboard, for now just zero-fill the framebuffer
    Memory::ZeroBlock(info.address_left, info.stride * 320);

    Service::GSP::SetBufferSwap(1, info);
}

void SoftwareKeyboard::Finalize() {
    // Let the application know that we're closing
    Service::APT::MessageParameter message;
    message.buffer.resize(sizeof(SoftwareKeyboardConfig));
    std::memcpy(message.buffer.data(), &config, message.buffer.size());
    message.signal = static_cast<u32>(Service::APT::SignalType::WakeupByExit);
    message.destination_id = static_cast<u32>(Service::APT::AppletId::Application);
    message.sender_id = static_cast<u32>(id);
    Service::APT::SendParameter(message);

    is_running = false;
}
}
} // namespace
