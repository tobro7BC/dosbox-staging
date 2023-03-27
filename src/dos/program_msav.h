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

#ifndef DOSBOX_PROGRAM_MSAV_H
#define DOSBOX_PROGRAM_MSAV_H

#include "programs.h"

class MSAV final : public Program {
public:
	MSAV()
	{
		AddMessages();
		help_detail = {HELP_Filter::All,
		               HELP_Category::File,
		               HELP_CmdType::Program,
		               "MSAV"};
	}
	void Run();

private:
	static void AddMessages();

        uint16_t max_columns = 0;

        struct Summary {
                size_t directories  = 0;
                size_t files        = 0;
                uint64_t total_size = 0;
                size_t infected     = 0;
                size_t skipped      = 0;
        };

        bool ScanDrive(const char drive_letter,
                       const std::vector<std::string> &path_list,
                       Summary &summary);
        bool ScanPath(const std::string &path, Summary &summary);
        bool ScanFile(const std::string &file_name,
                      const uint32_t file_size,
                      Summary &summary);

        std::vector<std::string> GetObjectList();

        void GetDirContents(const std::string &path,
                            std::vector<DOS_DTA::Result> &dir_contents);

        bool IsCancelRequest();
        void ClearLine(const size_t num_characters);

        bool has_option_scan_only     = false;
        bool has_option_scan_clean    = false;
        bool has_option_skip_fdd      = false;
        bool has_option_skip_fdd_net  = false;
        bool has_option_no_file_names = false;
        bool has_option_report        = false;
};

#endif
