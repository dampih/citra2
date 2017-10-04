// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/soc/soc_u.h"

namespace Service {
namespace SOC {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010044, nullptr, "InitializeSockets"},
    {0x000200C2, nullptr, "Socket"},
    {0x00030082, nullptr, "Listen"},
    {0x00040082, nullptr, "Accept"},
    {0x00050084, nullptr, "Bind"},
    {0x00060084, nullptr, "Connect"},
    {0x00070104, nullptr, "recvfrom_other"},
    {0x00080102, nullptr, "RecvFrom"},
    {0x00090106, nullptr, "sendto_other"},
    {0x000A0106, nullptr, "SendTo"},
    {0x000B0042, nullptr, "Close"},
    {0x000C0082, nullptr, "Shutdown"},
    {0x000D0082, nullptr, "GetHostByName"},
    {0x000E00C2, nullptr, "GetHostByAddr"},
    {0x000F0106, nullptr, "GetAddrInfo"},
    {0x00100102, nullptr, "GetNameInfo"},
    {0x00110102, nullptr, "GetSockOpt"},
    {0x00120104, nullptr, "SetSockOpt"},
    {0x001300C2, nullptr, "Fcntl"},
    {0x00140084, nullptr, "Poll"},
    {0x00150042, nullptr, "SockAtMark"},
    {0x00160000, nullptr, "GetHostId"},
    {0x00170082, nullptr, "GetSockName"},
    {0x00180082, nullptr, "GetPeerName"},
    {0x00190000, nullptr, "ShutdownSockets"},
    {0x001A00C0, nullptr, "GetNetworkOpt"},
    {0x001B0040, nullptr, "ICMPSocket"},
    {0x001C0104, nullptr, "ICMPPing"},
    {0x001D0040, nullptr, "ICMPCancel"},
    {0x001E0040, nullptr, "ICMPClose"},
    {0x001F0040, nullptr, "GetResolverInfo"},
    {0x00210002, nullptr, "CloseSockets"},
    {0x00230040, nullptr, "AddGlobalSocket"},
};

SOC_U::SOC_U() {
    Register(FunctionTable);
}

SOC_U::~SOC_U() {}

} // namespace SOC
} // namespace Service
