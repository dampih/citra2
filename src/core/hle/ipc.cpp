// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/ipc.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"

namespace IPC {

ResultCode TranslateCommandBuffer(Kernel::SharedPtr<Kernel::Thread> src_thread,
                                  Kernel::SharedPtr<Kernel::Thread> dst_thread, VAddr src_address,
                                  VAddr dst_address) {
    IPC::Header header;
    // TODO(Subv): Replace by Memory::Read32 when possible.
    Memory::ReadBlock(*src_thread->owner_process, src_address, &header.raw, sizeof(header.raw));

    size_t untranslated_size = 1u + header.normal_params_size;
    size_t command_size = untranslated_size + header.translate_params_size;

    // Note: The real kernel does not check that the command length fits into the IPC buffer area.
    ASSERT(command_size <= IPC::COMMAND_BUFFER_LENGTH);

    std::vector<u32> cmd_buf(command_size);
    Memory::ReadBlock(*src_thread->owner_process, src_address, cmd_buf.data(),
                      command_size * sizeof(u32));

    size_t i = untranslated_size;
    while (i < command_size) {
        u32 descriptor = cmd_buf[i];
        i += 1;

        switch (IPC::GetDescriptorType(descriptor)) {
        case IPC::DescriptorType::CopyHandle:
        case IPC::DescriptorType::MoveHandle: {
            u32 num_handles = IPC::HandleNumberFromDesc(descriptor);
            // Note: The real kernel does not check that the number of handles fits into the command
            // buffer before writing them, only after finishing.
            if (i + num_handles > command_size) {
                return ResultCode(ErrCodes::CommandTooLarge, ErrorModule::OS,
                                  ErrorSummary::InvalidState, ErrorLevel::Status);
            }

            for (u32 j = 0; j < num_handles; ++j) {
                Kernel::Handle handle = cmd_buf[i];
                Kernel::SharedPtr<Kernel::Object> object = nullptr;
                // Perform pseudo-handle detection here because by the time this function is called,
                // the current thread and process are no longer the ones which created this IPC
                // request, but the ones that are handling it.
                if (handle == Kernel::CurrentThread) {
                    object = src_thread;
                } else if (handle == Kernel::CurrentProcess) {
                    object = src_thread->owner_process;
                } else if (handle != 0) {
                    object = Kernel::g_handle_table.GetGeneric(handle);
                    if (descriptor == IPC::DescriptorType::MoveHandle) {
                        Kernel::g_handle_table.Close(handle);
                    }
                }

                if (object == nullptr) {
                    // Note: The real kernel sets invalid translated handles to 0 in the target
                    // command buffer.
                    cmd_buf[i++] = 0;
                    continue;
                }

                Kernel::Handle translated_handle = 0;
                auto result = Kernel::g_handle_table.Create(std::move(object));
                if (result.Succeeded()) {
                    translated_handle = result.Unwrap();
                }
                cmd_buf[i++] = translated_handle;
            }
            break;
        }
        case IPC::DescriptorType::CallingPid: {
            cmd_buf[i++] = src_thread->owner_process->process_id;
            break;
        }
        default:
            UNIMPLEMENTED_MSG("Unsupported handle translation: 0x%08X", descriptor);
        }
    }

    Memory::WriteBlock(*dst_thread->owner_process, dst_address, cmd_buf.data(),
                       command_size * sizeof(u32));

    return RESULT_SUCCESS;
}
} // namespace IPC
