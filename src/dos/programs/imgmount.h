// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_IMGMOUNT_H
#define DOSBOX_PROGRAM_IMGMOUNT_H

#include "dos/programs.h"

class IMGMOUNT final : public Program {
public:
	IMGMOUNT()
	{
		help_detail = {HELP_Filter::Common,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "IMGMOUNT"};
	}
	void ListImgMounts();
	void Run() override;
};

#endif // DOSBOX_PROGRAM_IMGMOUNT_H
