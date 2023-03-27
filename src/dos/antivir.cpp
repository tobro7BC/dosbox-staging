/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "antivir.h"

#include "callback.h"
#include "checks.h"
#include "control.h"
#include "dosbox.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"

#include <chrono>
#include <string>

CHECK_NARROWING();


// Common implementation

class ClamAV_Base {
public:
        ClamAV_Base() = default;
        virtual ~ClamAV_Base() = default;

        bool PrepareConnection();

        std::string GetEngineVersion() const;
        std::string GetDatabaseVersion() const;

        VirusCheckResult ScanFile(const uint16_t handle,
                                  const std::string &file_name,
                                  std::string &virus_name);

protected:
        std::chrono::steady_clock::time_point GetTimeBegin() const;
        bool IsTimeout(const std::chrono::steady_clock::time_point time_begin,
                       const uint16_t timeout_ms) const;

        virtual uint8_t GetNumProtocols() const = 0;

        virtual bool Open(const uint8_t num_protocol) = 0;
        virtual void Close() = 0;

        virtual bool Send(const std::string &msg) = 0;
        virtual bool Send(const uint8_t* buffer, const size_t size) = 0;
        virtual std::string Receive(const uint16_t timeout_ms) = 0;

private:
        ClamAV_Base(ClamAV_Base &other)            = delete;
        ClamAV_Base& operator=(ClamAV_Base &other) = delete;

        bool is_initialized   = false;
        uint8_t used_protocol = 0;

        std::string engine_version   = {};
        std::string database_version = {};

        bool SendCommand(const std::string &cmd);
        std::string GetResponse();

        bool PingDaemon();
        void RetrieveVersion();
};

std::chrono::steady_clock::time_point ClamAV_Base::GetTimeBegin() const
{
        return std::chrono::steady_clock::now();
}

bool ClamAV_Base::IsTimeout(const std::chrono::steady_clock::time_point time_begin,
                       const uint16_t timeout_ms) const
{
        const auto time_now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(time_now - time_begin).count() >= timeout_ms;
}

bool ClamAV_Base::PrepareConnection()
{
        if (is_initialized) {
                // XXX reconnect if needed
                return true;
        }

        engine_version   = {};
        database_version = {};

        const auto num_protocols = GetNumProtocols();

        for (uint8_t idx = 0; idx < num_protocols; ++idx) {
                if (Open(idx) && SendCommand("IDSESSION") && PingDaemon()) {
                        // XXX store address
                        used_protocol  = idx;
                        is_initialized = true;
                        break;
                }
        }

        if (is_initialized) {
                // XXX what if this fails?
                RetrieveVersion();
        }

        return is_initialized;
}

std::string ClamAV_Base::GetEngineVersion() const
{
        return engine_version;
}

std::string ClamAV_Base::GetDatabaseVersion() const
{
        return database_version;
}

VirusCheckResult ClamAV_Base::ScanFile(const uint16_t handle,
                                       const std::string &file_name,
                                       std::string &virus_name)
{
        virus_name.clear();

        // Buffer layout: 4 bytes (length of data chunk) + data chunk
        uint8_t buffer[UINT16_MAX + 4];
        buffer[0] = 0;
        buffer[1] = 0;

        uint16_t amount = UINT16_MAX;
        if (!DOS_ReadFile(handle, &buffer[4], &amount)) {
                return VirusCheckResult::ReadError;
        }

        if (!amount) {
                // Empty file
                return VirusCheckResult::Clean;
        }

        SendCommand("INSTREAM"); // XXX check for transmission errors, everywhere

        bool read_error = false;
        while (amount && !read_error) {
                buffer[2] = static_cast<uint8_t>(amount / 0x100);
                buffer[3] = static_cast<uint8_t>(amount % 0x100);
                Send(&buffer[0], amount + 4);

                amount = UINT16_MAX;
                read_error = !DOS_ReadFile(handle, &buffer[4], &amount);
        }

        buffer[2] = 0;
        buffer[3] = 0;
        Send(&buffer[0], 4);

        const auto response = GetResponse();

        if (read_error) {
                return VirusCheckResult::ReadError;
        }

        // Parse ClamAV response

        const auto pos_name   = response.rfind("stream: ");
        const auto pos_status = response.rfind(' ');

        if (pos_name == std::string::npos || pos_status == std::string::npos) {
                // XXX trace parser error
                return VirusCheckResult::ScannerError;
        }

        if (response.substr(pos_status + 1) == "OK") {
                return VirusCheckResult::Clean;
        }

        if (response.substr(pos_status + 1) == "FOUND") {
                if (pos_name >= pos_status) {
                        // XXX trace parser error
                        return VirusCheckResult::ScannerError; 
                }

                // Extract virus name from ClamAV response

                const auto pos_start = pos_name + 8;
                const auto pos_end   = pos_status - pos_name - 8;

                virus_name = response.substr(pos_start, pos_end);
                LOG_WARNING("ANTIVIR: File '%s' infected with '%s'",
                            file_name.c_str(), virus_name.c_str());

                // Strip prefix from file name

                if (virus_name.length() > 4 &&
                    starts_with("Win.", virus_name.c_str())) {
                        // TODO: C++20 offers starts_with as std::string method
                        virus_name = virus_name.substr(4);
                }

                // Strip suffix from file name

                for (size_t pos = virus_name.length() - 1; pos > 1; --pos) {
                        if (virus_name[pos] >= '0' && virus_name[pos] <= '9') {
                                continue;
                        }
                        if (virus_name[pos] != '-') {
                                break;
                        }

                        virus_name = virus_name.substr(0, pos);
                        break;
                }

                return VirusCheckResult::Infected;
        }

        LOG_WARNING("ANTIVIR: Scanning file '%s' resulted with unsupported response '%s'",
                    file_name.c_str(), response.c_str());
        return VirusCheckResult::ScannerError;
}

bool ClamAV_Base::SendCommand(const std::string &cmd)
{
        // LOG_WARNING("XXX sending command '%s'", cmd.c_str());

        return Send('z' + cmd + '\0');
}

std::string ClamAV_Base::GetResponse() // XXX more time when scanning files
{
        constexpr uint16_t timeout_ms_first = 500;
        constexpr uint16_t timeout_ms_next  = 300;

        auto strip_first_result = [](std::string &result) {
                // XXX make it more strict - strip initial number
                const auto pos = result.find(": ");
                if (pos == std::string::npos) {
                        return;
                }

                result = result.substr(pos + 2);
        };

        auto result = Receive(timeout_ms_first);
        if (result.empty()) {
                return result;
        }

        strip_first_result(result);
        if (!result.empty()) {
                return result;
        }

        return Receive(timeout_ms_next);
}

bool ClamAV_Base::PingDaemon()
{
        return SendCommand("PING") && (GetResponse() == "PONG");
}

void ClamAV_Base::RetrieveVersion()
{
        SendCommand("VERSION");

        engine_version   = GetResponse();
        database_version = {};

        // Split engine version from database version

        auto pos = engine_version.find("/");
        if (pos != std::string::npos) {
                database_version = engine_version.substr(pos + 1);
                engine_version   = engine_version.substr(0, pos);
        }

        // Make database version more readable

        pos = database_version.find("/");
        if (pos != std::string::npos) {
                // XXX parse date, output it only using DOS locale
                database_version = database_version.substr(0, pos) +
                                   " (" +
                                   database_version.substr(pos + 1) +
                                   ")";
        }
}

// Constants

static constexpr size_t   InBufSize = 2048;

static const char *       EngineAddr = "127.0.0.1";    
static constexpr uint16_t EnginePort = 3310;

#if !(defined(_WIN32))

// POSIX-specific implementation

static const char *       EngineSocket = "/run/clamav/clamd.ctl";

#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

class ClamAV final : public ClamAV_Base {
public:
        ClamAV();
        ~ClamAV();

protected:
        uint8_t GetNumProtocols() const override;

        bool Open(const uint8_t num_protocol) override;
        void Close() override;

        bool Send(const std::string &msg) override;
        bool Send(const uint8_t* buffer, const size_t size) override;
        std::string Receive(const uint16_t timeout_ms) override;

private:
        bool Open_TcpIp();
        bool Open_Unix();

        static constexpr int Invalid = -1;

        int socket_desc = Invalid;
};

ClamAV::ClamAV() : ClamAV_Base()
{
        static bool is_sigpipe_ignored = false;
        if (!is_sigpipe_ignored) {
                // We need to ignore SIGPIPE signal; this way emulator won't be
                // killed if ClamAV terminates the connection
                signal(SIGPIPE, SIG_IGN);            
        }
}

ClamAV::~ClamAV()
{
        Close();
}

uint8_t ClamAV::GetNumProtocols() const
{
        return 2;
}

bool ClamAV::Open(const uint8_t num_protocol)
{
        Close();

        switch (num_protocol) {
        case 0: return Open_Unix();
        case 1: return Open_TcpIp();
        default: assert(false);
        }

        return false;
}

bool ClamAV::Open_TcpIp()
{
        // Create socket

        socket_desc = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_desc == Invalid)
        {
                assert(false);
                return false;
        }

        // Connect socket

        struct sockaddr_in server;

        server.sin_family      = AF_INET;
        server.sin_addr.s_addr = inet_addr(EngineAddr);
        server.sin_port        = htons(EnginePort);

        auto server_cast = reinterpret_cast<struct sockaddr *>(&server);
        if (connect(socket_desc, server_cast, sizeof(server)) < 0)
        {
                Close(); // unable to connect
                return false;
        }

        return true;
}

bool ClamAV::Open_Unix()
{
        // Create socket

        socket_desc = socket(AF_UNIX, SOCK_STREAM, 0);
        if (socket_desc == Invalid)
        {
                assert(false);
                return false;
        }

        // Connect socket

        struct sockaddr_un server;

        server.sun_family = AF_UNIX;
        strncpy(&server.sun_path[0], EngineSocket, sizeof(server.sun_path));

        auto server_cast = reinterpret_cast<struct sockaddr *>(&server);
        if (connect(socket_desc, server_cast, sizeof(server)) < 0)
        {
                Close(); // unable to connect
                return false;
        }

        return true;
}

void ClamAV::Close()
{
        if (socket_desc != Invalid)
        {
                close(socket_desc);
                socket_desc = Invalid;
        }
}

bool ClamAV::Send(const std::string &msg) // XXX obsolete
{
        if (socket_desc == Invalid) {
                return false;
        }

        const auto result = send(socket_desc, msg.c_str(), msg.length(), 0); // XXX timeout, MSG_DONTWAIT flag
        // XXX handle case if less has been sent
        return (result >= 0);
}

bool ClamAV::Send(const uint8_t* buffer, const size_t size)
{
        assert(buffer);

        if (socket_desc == Invalid) {
                return false;
        }

        const auto result = send(socket_desc, buffer, size, 0); // XXX timeout, MSG_DONTWAIT flag
        // XXX handle case if less has been sent
        return (result >= 0);       
}

std::string ClamAV::Receive(const uint16_t timeout_ms)
{
        if (socket_desc == Invalid) {
                return std::string();
        }

        char buf[InBufSize + 1];

        const auto time_begin = GetTimeBegin();
        while (!IsTimeout(time_begin, timeout_ms)) {
                const auto result = recv(socket_desc, buf, sizeof(buf), MSG_DONTWAIT);
                assert(result <= InBufSize);

                if (result <= 0)
                {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                CALLBACK_Idle();
                                continue;
                        }

                        if (result <= 0)
                        {
                                break;
                        }
                }

                buf[result] = 0; // XXX if buf[result] does not end with 0, append next part
                // LOG_WARNING("XXX received '%s'", buf);

                return buf;
        }

        return std::string();
}

#else

// Windows-specific implementation

#error "Windows not supported yet"

#endif

// ***************************************************************************
// External interface
// ***************************************************************************

static ClamAV clamav;

std::string ANTIVIR_GetConfiguredEngineName()
{
        return "ClamAV";
}

void ANTIVIR_EndSession()
{
        // XXX
}

bool ANTIVIR_GetVersion(std::string &engine_version,
                        std::string &database_version)
{
        if (!clamav.PrepareConnection()) {
                return false;
        }

        engine_version   = clamav.GetEngineVersion();
        database_version = clamav.GetDatabaseVersion();

        return !engine_version.empty() && !database_version.empty();
}

VirusCheckResult ANTIVIR_ScanFile(const uint16_t handle,
                                  const std::string &file_name,
                                  std::string &virus_name)
{
        return clamav.ScanFile(handle, file_name, virus_name);
}

// ***************************************************************************
// Initialization and configuration
// ***************************************************************************

// TODO:
// - implement VSAFE.COM and its protection mechanisms
// - scan floppy bootsectors
// - scan hard disk MBR and partition boot area
// - also handle boot area of ElTorito CD-ROMs
// - scan memory (just pass used blocks to ClamAV)
// - use video overlay for VSAFE prompts

static void antivir_read_config(Section* sec)
{
        // XXX
}

static void antivir_init_dosbox_settings(Section_prop& secprop)
{
        constexpr auto on_start = Property::Changeable::OnlyAtStart;

        // WARNING: Never allow to change these settings by guest code, we don't
        // want any potential DOSBox-aware malware to be able to disrupt virus
        // detection or change antivirus settings to became less restrictive
        // than configured by the user!

        auto prop_str = secprop.Add_string("clamav_socket", on_start, "");
        assert(prop_str);
        prop_str->Set_help(
                "Socket of the ClamAV daemon, as configured in the 'clamd.conf' configuration\n"
                "file. On Windows host it has to be a TCP socket (e.g. 127.0.0.1:3310), on\n"
                "many other systems it can also be a local socket (e.g. /run/clamav/clamd.ctl).\n"
                "If empty (default), tries to find the daemon by probing default sockets.");
}

void ANTIVIR_AddConfigSection(const config_ptr_t &conf)
{
        assert(conf);

        constexpr auto changeable_at_runtime = false;
        Section_prop* sec = conf->AddSection_prop("antivirus",
                                                  &antivir_read_config,
                                                  changeable_at_runtime);
        assert(sec);
        antivir_init_dosbox_settings(*sec);
}
