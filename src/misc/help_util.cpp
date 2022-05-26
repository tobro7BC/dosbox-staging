#include "dosbox.h"

#include <map>
#include <string>

#include "help_util.h"
#include "string_utils.h"
#include "support.h"

namespace HelpUtil {

static std::map<const std::string, Detail> help_list = {};

void add_to_help_list(const std::string& cmd_name, const Detail& detail, bool replace_existing) {
    if (replace_existing || !contains(help_list, cmd_name)) {
        help_list[cmd_name] = detail;
    }
}

const std::map<const std::string, Detail>& get_help_list() {
    return help_list;
}

std::string get_short_help(const std::string& name) {
    const std::string short_key = "SHELL_CMD_" + name + "_HELP";
	if (std::string str = MSG_Get(short_key.c_str()); !starts_with("Message not Found!", str)) {
		return str;
	} 
	const std::string long_key = "SHELL_CMD_" + name + "_HELP_LONG";
	if (std::string str = MSG_Get(long_key.c_str()); !starts_with("Message not Found!", str)) {
		const auto pos = str.find('\n');
		return str.substr(0, pos != std::string::npos ? pos + 1 : pos);
	}
	return "No help available\n";
}

const char* category_heading(const Category category) {
    switch (category) {
    case Category::DOSBOX:
        return "Dosbox Commands";
    case Category::FILE:
        return "File/Directory Commands";
    case Category::BATCH:
        return "Batch File Commands";
    case Category::MISC:
        return "Miscellaneous Commands";
    default:
        return "Unknown Commands";
    }
}

}
