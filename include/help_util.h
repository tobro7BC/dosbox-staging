#ifndef HELP_UTIL_H
#define HELP_UTIL_H

#include <map>
#include <string>

namespace HelpUtil {

enum class Filter { ALL, COMMON };

enum class Category { MISC, FILE, DOSBOX, BATCH };

enum class CmdType { SHELL, PROGRAM };

struct Detail {
	Filter filter{Filter::ALL};
	Category category{Category::MISC};
    CmdType type{CmdType::SHELL};
	std::string name{};
};

void add_to_help_list(const std::string &cmd_name, const Detail &detail,
                      bool replace_existing = false);
const std::map<const std::string, Detail> &get_help_list();
std::string get_short_help(const std::string &name);
const char *category_heading(const Category category);
}

#endif // HELP_UTIL_H
