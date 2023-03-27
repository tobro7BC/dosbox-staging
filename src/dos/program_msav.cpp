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

#include "program_msav.h"

#include "../ints/int10.h"
#include "ansi_code_markup.h"
#include "antivir.h"
#include "callback.h"
#include "checks.h"
#include "dos_inc.h"
#include "program_more_output.h"
#include "shell.h"

CHECK_NARROWING();


void MSAV::Run()
{
        // Handle command line
        if (HelpRequested()) {
                MoreOutputStrings output(*this);
                output.AddString(MSG_Get("PROGRAM_MSAV_HELP_LONG"));
                output.Display();
                return;
        }

        constexpr bool remove_if_found = true;

        // Originally this option uses MSAV.TXT file from MSAV directory
        // as text user interface definition - not supported, our MSAV.EXE
        // always resides in Z: drive; ignore this for compatibility
        cmd->FindExist("/n", remove_if_found);

        // Originally this enforces the console (command line) interface;
        // since this is the only supported one, ignore this for compatibility
        cmd->FindExist("/p", remove_if_found);

        // TODO: the '/video' option is not supported, as we have no GUI yet

        has_option_scan_only     = cmd->FindExist("/s", remove_if_found);
        has_option_scan_clean    = cmd->FindExist("/c", remove_if_found); // XXX
        has_option_skip_fdd      = cmd->FindExist("/a", remove_if_found); // XXX
        has_option_skip_fdd_net  = cmd->FindExist("/l", remove_if_found); // XXX
        has_option_no_file_names = cmd->FindExist("/f", remove_if_found);
        has_option_report        = cmd->FindExist("/r", remove_if_found); // XXX

        if ((has_option_scan_only && has_option_scan_clean) ||
            (has_option_skip_fdd && has_option_skip_fdd_net)) {

                // Illegal switch combination
                // XXX original MSAV allows these - TODO check switch priorities
                WriteOut(MSG_Get("SHELL_SYNTAX_ERROR"));
                return;
        }

        std::string tmp_str = {};
        if (cmd->FindStringBegin("/", tmp_str)) {
                tmp_str = std::string("/") + tmp_str;
                WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), tmp_str.c_str());
                return;
        }

        // Check screen width

        constexpr uint16_t min_columns = 40;
        max_columns = std::max(min_columns, INT10_GetTextColumns());

        // Display header

        WriteOut("\n\n%s\n\n", MSG_Get("PROGRAM_MSAV_TITLE_MAIN"));

        std::string engine_version   = {};
        std::string database_version = {};
        if (!ANTIVIR_GetVersion(engine_version, database_version)) {
                // XXX improve for ClamAV without database
                WriteOut(MSG_Get("PROGRAM_MSAV_ERROR_ENGINE_NOT_AVAILABLE"),
                         ANTIVIR_GetConfiguredEngineName().c_str());
                WriteOut("\n\n\n");
                ANTIVIR_EndSession();
                return;
        }

        WriteOut("    %s %s\n    %s %s\n\n\n",
                 MSG_Get("PROGRAM_MSAV_ENGINE"),
                 engine_version.c_str(),
                 MSG_Get("PROGRAM_MSAV_DATABASE"),
                 database_version.c_str());

        // TODO: scan memory

        // Retrieve paths from the command line

        auto object_list = GetObjectList();
        if (object_list.empty()) {
                ANTIVIR_EndSession();
                return;
        }

        // Scan the drives

        std::sort(object_list.begin(), object_list.end());

        Summary summary = {};
        std::vector<std::string> drive_object_list = {};

        assert(!object_list[0].empty());
        auto drive_letter = object_list[0][0];

        for (const auto &entry : object_list) {
                assert(!entry.empty());

                // If object belongs to the same drive - add it to the list

                if (drive_letter == entry[0]) {
                        drive_object_list.push_back(entry);
                        continue;
                }

                // Else scan the previous drive, it's list is complete

                if (!ScanDrive(drive_letter, drive_object_list, summary)) {
                        ANTIVIR_EndSession();
                        return;
                }

                // Begin the list for the new drive

                drive_object_list.clear();
                drive_object_list.push_back(entry);
        }

        // Scan the last drive mentionned in arguments

        if (!ScanDrive(drive_letter, drive_object_list, summary)) {
                ANTIVIR_EndSession();
                return;
        }

        ANTIVIR_EndSession();

        // Print out the summary

        WriteOut(MSG_Get("PROGRAM_MSAV_TITLE_SUMMARY"));
        WriteOut("\n\n");

        WriteOut("    %s %d\n",
                 MSG_Get("PROGRAM_MSAV_SUMMARY_DIRECTORIES"),
                 summary.directories);
        WriteOut("    %s %d (%s)\n",
                 MSG_Get("PROGRAM_MSAV_SUMMARY_FILES"),
                 summary.files,
                 format_size(summary.total_size).c_str());
        if (summary.infected) {
                WriteOut("    %s %s%d%s\n",
                         MSG_Get("PROGRAM_MSAV_SUMMARY_INFECTED"),
                         convert_ansi_markup("[color=red]").c_str(),
                         summary.infected,
                         convert_ansi_markup("[reset]").c_str());
        } else {
                WriteOut("    %s 0\n", MSG_Get("PROGRAM_MSAV_SUMMARY_INFECTED"));
        }
        WriteOut("    %s %d\n",
                 MSG_Get("PROGRAM_MSAV_SUMMARY_SKIPPED"),
                 summary.skipped);

        WriteOut("\n");
}

bool MSAV::IsCancelRequest()
{
        // XXX also add to TREE.COM, print out ^C by DOS_IsCancelRequest

        constexpr uint8_t code_ctrl_c = 0x03;

        const auto code = DOS_IsCancelRequest();
        if (code == code_ctrl_c) {
                WriteOut("^C");
        }

        return code;
}

void MSAV::ClearLine(const size_t num_characters)
{
        WriteOut("\033[M");
        for (size_t idx = 0; idx < num_characters; ++idx) {
                WriteOut("\033[D");
        }
}

std::vector<std::string> MSAV::GetObjectList()
{
        std::vector<std::string> params;
        cmd->FillVector(params);

        // If parameter list is empty, use current directory

        if (params.empty()) {
                char buffer[DOS_PATHLENGTH];
                const auto current_drive = DOS_GetDefaultDrive();
                if (!DOS_GetCurrentDir(current_drive + 1, buffer)) {
                        WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
                        return {};
                }
                std::string tmp_str = {};
                tmp_str += static_cast<char>('A' + current_drive);

                return { tmp_str + ":\\" + buffer + "\\" };
        }

        // If not empty - check if objects exist
        // TODO: add support for wildcards

        std::vector<std::string> object_list;
        for (const auto &entry : params) {
                char buffer[DOS_PATHLENGTH];
                if (!DOS_Canonicalize(entry.c_str(), buffer)) {
                        // XXX
                        LOG_ERR("XXX no path '%s'", entry.c_str());
                        continue;
                }

                object_list.push_back(buffer);
        }

        return object_list;
}

void MSAV::GetDirContents(const std::string &path,
                          std::vector<DOS_DTA::Result> &dir_contents)
{
        dir_contents.clear();

        const RealPt save_dta = dos.dta();
        dos.dta(dos.tables.tempdta);

        const auto pattern = path + "*.*";
        FatAttributeFlags flags;
        flags.system        = true;
        flags.hidden        = true;
        flags.directory     = true;
        bool has_next_entry = DOS_FindFirst(pattern.c_str(), flags);

        while (has_next_entry) {
                DOS_DTA::Result result = {};

                const DOS_DTA dta(dos.dta());
                dta.GetResult(result);
                assert(!result.name.empty());

                has_next_entry = DOS_FindNext();
                if (result.IsDummyDirectory()) {
                        continue;
                }

                dir_contents.emplace_back(result);
        }

        dos.dta(save_dta);
}

bool MSAV::ScanDrive(const char drive_letter,
                     const std::vector<std::string> &path_list,
                     Summary &summary)
{
        assert(!object_list.empty());

        // TODO: scan boot sectors

        WriteOut(MSG_Get("PROGRAM_MSAV_TITLE_DRIVE"), drive_letter);
        WriteOut("\n\n");

        for (const auto &path : path_list) {
                // XXX handle individual files too
                if (!ScanPath(path, summary)) {
                        return false;
                }
        }

        return true;
}

bool MSAV::ScanPath(const std::string &path, Summary &summary)
{
        ++summary.directories;

        // Search for files to scan

        std::vector<DOS_DTA::Result> dir_contents;
        GetDirContents(path, dir_contents);
        DOS_Sort(dir_contents, ResultSorting::ByName);

        // Scan the files

        for (const auto &entry : dir_contents) {
                if (IsCancelRequest()) {
                        return false;
                }

                if (entry.IsDirectory()) {
                        if (!ScanPath(path + entry.name + "\\", summary)) {
                                return false;
                        }
                } else {
                        if (!ScanFile(path + entry.name, entry.size, summary)) {
                                return false;
                        }
                }
        }

        return true;
}

bool MSAV::ScanFile(const std::string &file_name,
                    const uint32_t file_size,
                    Summary &summary)
{
        ++summary.files;
        summary.total_size += file_size;

        const auto short_name_str = shorten_path(file_name, max_columns - 5);
        if (!has_option_no_file_names) {
                WriteOut("    %s", short_name_str.c_str());
        }

        // Open the file

        uint16_t handle = 0;
        if (!DOS_OpenFile(file_name.c_str(), 0, &handle)) {
                ++summary.skipped;
                WriteOut("\n    "); 
                WriteOut(MSG_Get("PROGRAM_MSAV_OPEN_ERROR"));
                WriteOut("\n\n");
                return true;
        }

        // Perform scanning

        std::string virus_name = {};
        const auto result = ANTIVIR_ScanFile(handle, file_name, virus_name);
        DOS_CloseFile(handle);

        // Check scanning result

        if (result == VirusCheckResult::Clean) {
                if (!has_option_no_file_names) {
                        // Clear line, move cursor back
                        ClearLine(short_name_str.length() + 4);
                }
                return true; 
        }

        // Scanning error or infected file

        if (result == VirusCheckResult::Infected) {
                ++summary.infected;
        } else {
                ++summary.skipped;
        }

        if (has_option_no_file_names) {
                WriteOut("    %s\n    ", short_name_str.c_str());
        } else {
                WriteOut("\n    ");  
        }

        bool should_continue = false;

        switch (result) {
        case VirusCheckResult::Infected:
                WriteOut(MSG_Get("PROGRAM_MSAV_FILE_INFECTED"),
                         virus_name.c_str());
                should_continue = true;
                break;
        case VirusCheckResult::ReadError:
                WriteOut(MSG_Get("PROGRAM_MSAV_READ_ERROR"));
                should_continue = true;
                break;
        case VirusCheckResult::FileTooLarge:
                WriteOut(MSG_Get("PROGRAM_MSAV_FILE_TOO_LARGE"));
                should_continue = true;
                break;
        case VirusCheckResult::ConnectionLost:
                WriteOut(MSG_Get("PROGRAM_MSAV_CONECTION_LOST"));
                break;
        case VirusCheckResult::ScannerError:
                WriteOut(MSG_Get("PROGRAM_MSAV_SCANNER_ERROR"));
                break;
        default:
                assert(false);
                break;
        }

        WriteOut("\n\n");
        return should_continue;
}

void MSAV::AddMessages()
{
        MSG_Add("PROGRAM_MSAV_HELP_LONG",
                "Scan system for viruses.\n"
                "\n"
                "Usage:\n"
                "  [color=green]msav[reset] [color=cyan][PATH] [...][reset] [/s | /c] [/f] [/r]\n"
                "  [color=green]msav[reset] [/s | /c] [/a | /l] [/f] [/r]\n"
                "\n"
                "Where:\n"
                "  [color=cyan]PATH[reset] is the name of the file or directory to scan, multiple allowed.\n"
                "  /s        XXX\n"
                "  /c        XXX\n"
                "  /a        XXX\n"
                "  /l        XXX\n"
                "  /f        do not display scanned file names.\n"
                "  /r        XXX\n"
                "\n"
                "Notes:\n"
                "  XXX\n"
                "\n"
                "Examples:\n"
                "  XXX\n");

        MSG_Add("PROGRAM_MSAV_TITLE_MAIN",    "[color=white]Anti-Virus Scanner[reset]");
        MSG_Add("PROGRAM_MSAV_TITLE_DRIVE",   "[color=white]Scanning drive %c:[reset]");
        MSG_Add("PROGRAM_MSAV_TITLE_SUMMARY", "[color=white]Summary[reset]");

        MSG_Add("PROGRAM_MSAV_ENGINE",   "Detection engine     :");
        MSG_Add("PROGRAM_MSAV_DATABASE", "Database revision    :");
        MSG_Add("PROGRAM_MSAV_ERROR_ENGINE_NOT_AVAILABLE", "%s engine not available.");

        MSG_Add("PROGRAM_MSAV_FILE_INFECTED",  "- detected [color=red]%s[reset]");
        MSG_Add("PROGRAM_MSAV_OPEN_ERROR",     "- error opening file");
        MSG_Add("PROGRAM_MSAV_READ_ERROR",     "- error reading file");
        MSG_Add("PROGRAM_MSAV_FILE_TOO_LARGE", "- file too large to scan");
        MSG_Add("PROGRAM_MSAV_CONECTION_LOST", "- lost connection to scanning engine");
        MSG_Add("PROGRAM_MSAV_SCANNER_ERROR",  "- error scanning the file");

        MSG_Add("PROGRAM_MSAV_SUMMARY_DIRECTORIES", "Directories    :");
        MSG_Add("PROGRAM_MSAV_SUMMARY_FILES",       "Files          :");
        MSG_Add("PROGRAM_MSAV_SUMMARY_INFECTED",    "- infected     :");
        MSG_Add("PROGRAM_MSAV_SUMMARY_SKIPPED",     "- skipped      :");

        // XXX recommended actions after a virus was detected
}
