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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_ANTIVIR_H
#define DOSBOX_ANTIVIR_H

#include "dosbox.h"

#include <string>

std::string ANTIVIR_GetConfiguredEngineName();

enum class VirusCheckResult {
        Clean,
        Infected,
        ReadError,
        FileTooLarge,
        ConnectionLost,
        ScannerError,
};

void ANTIVIR_EndSession();

bool ANTIVIR_GetVersion(std::string &engine_version,
                        std::string &database_version);

VirusCheckResult ANTIVIR_ScanFile(const uint16_t handle,
                                  const std::string &file_name,
                                  std::string &virus_name);
// TODO: try to add scanning boot sectors and memory

// Internal antivirus configuration and initialization
void ANTIVIR_AddConfigSection(const config_ptr_t &conf);


// close the header
#endif
