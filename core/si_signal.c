/*
 * setup signal handlers for srcinv
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

#include "si_core.h"

static int do_sibuf_eh(int signo, siginfo_t *sinfo, void *arg)
{
	if (unlikely(signo != SIGSEGV))
		return EH_STATUS_NOT_HANDLED;

	struct sibuf *b;
	b = find_target_sibuf((void *)sinfo->si_addr);
	if (!b)
		return EH_STATUS_NOT_HANDLED;

	int held = analysis__sibuf_hold(b);
	if (!held)
		analysis__sibuf_drop(b);
	return EH_STATUS_DONE;
}

static int setup_sibuf_eh(void)
{
	struct eh_list *new_eh;
	new_eh = eh_list_new(do_sibuf_eh, SIGSEGV, 1, 0);
	enable_silent_mode();
	set_eh(new_eh);
	return 0;
}

C_SYM int src_buf_fix(void *fault_addr);
static int do_src_eh(int signo, siginfo_t *sinfo, void *arg)
{
	if (unlikely(signo != SIGSEGV))
		return EH_STATUS_NOT_HANDLED;

	if (!is_write_fault(arg))
		return EH_STATUS_NOT_HANDLED;

	if (!src_buf_fix((void *)sinfo->si_addr))
		return EH_STATUS_DONE;
	else
		return EH_STATUS_DEF;
}

static int setup_src_eh(void)
{
	struct eh_list *new_eh;
	new_eh = eh_list_new(do_src_eh, SIGSEGV, 1, 0);
	enable_silent_mode();
	set_eh(new_eh);
	return 0;
}

int si_signal_setup(void)
{
	int err = 0;

	err = setup_sibuf_eh();
	if (err) {
		err_msg("setup_sibuf_eh err");
		return -1;
	}

	err = setup_src_eh();
	if (err) {
		err_msg("setup_src_eh err");
		return -1;
	}

	return 0;
}
