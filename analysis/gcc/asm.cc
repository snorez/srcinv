/*
 * TODO
 *
 * Copyright (C) 2020 zerons
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "si_gcc.h"
#include "../analysis.h"

static int callback(struct sibuf *, int);
static struct lang_ops ops;

CLIB_MODULE_NAME(asm);
CLIB_MODULE_NEEDED0();

CLIB_MODULE_INIT()
{
	ops.callback = callback;
	ops.type.binary = SI_TYPE_SRC;
	ops.type.kernel = SI_TYPE_BOTH;
	ops.type.os_type = SI_TYPE_OS_LINUX;
	ops.type.type_more = SI_TYPE_MORE_GCC_ASM;
	register_lang_ops(&ops);
	return 0;
}

CLIB_MODULE_EXIT()
{
	unregister_lang_ops(&ops);
	return;
}

static int gcc_ver_major = __GNUC__;
static int gcc_ver_minor = __GNUC_MINOR__;
static int callback(struct sibuf *buf, int parse_mode)
{
	CLIB_DBG_FUNC_ENTER();

	struct file_content *fc = (struct file_content *)buf->load_addr;
	if ((fc->gcc_ver_major != gcc_ver_major) ||
		(fc->gcc_ver_minor != gcc_ver_minor)) {
		err_dbg(0, "gcc version not match, need %d.%d",
				fc->gcc_ver_major, fc->gcc_ver_minor);
		CLIB_DBG_FUNC_EXIT();
		return -1;
	}

	/* TODO: find export symbols in the objfile */

	CLIB_DBG_FUNC_EXIT();
	return 0;
}
