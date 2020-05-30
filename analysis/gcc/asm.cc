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

static __thread size_t obj_done, obj_cnt;
static __thread struct sibuf *cur_sibuf;
static __thread void *cur_ctx;
static __thread char *cur_srcfile;

static void getbase(void)
{
	CLIB_DBG_FUNC_ENTER();

	/*
	 * TODO:
	 * get symbols defined in the file and create a sinode for it
	 */
	struct sinode *sn_new = NULL, *sn_tmp = NULL, *loc_file = NULL;
	enum sinode_type type;
	int err = 0;
	elf_file *ef;
	struct list_head syms_head;
	INIT_LIST_HEAD(&syms_head);

	loc_file = analysis__sinode_search(TYPE_FILE, SEARCH_BY_SPEC,
						(void *)cur_srcfile);
	if (loc_file) {
		si_log1_emer("Same srcfile, not supposed\n");
		goto out;
	}
	loc_file = analysis__sinode_new(TYPE_FILE, cur_srcfile,
					strlen(cur_srcfile)+1,
					NULL, 0);
	BUG_ON(analysis__sinode_insert(loc_file, SINODE_INSERT_BH_NONE));

	ef = elf_parse_data(cur_ctx);
	if (!ef) {
		si_log1_emer("elf_parse_data err\n");
		goto out;
	}

	err = elf_get_syms(ef, &syms_head);
	if (err == -1) {
		si_log1_emer("elf_get_syms err\n");
		goto ef_out;
	}

	struct _elf_sym_full *tmp;
	list_for_each_entry(tmp, &syms_head, sibling) {
		get_sym_detail(ef, tmp);

		if ((tmp->type != STT_OBJECT) && (tmp->type != STT_FUNC))
			continue;

		if ((tmp->type == STT_FUNC) && (tmp->bind == STB_LOCAL))
			type = TYPE_FUNC_STATIC;
		else if ((tmp->type == STT_FUNC) && (tmp->bind == STB_GLOBAL))
			type = TYPE_FUNC_GLOBAL;
		else if ((tmp->type == STT_OBJECT) && (tmp->bind == STB_LOCAL))
			type = TYPE_VAR_STATIC;
		else if ((tmp->type == STT_OBJECT) && (tmp->bind == STB_GLOBAL))
			type = TYPE_VAR_GLOBAL;
		else
			continue;

		if ((type == TYPE_FUNC_GLOBAL) || (type == TYPE_VAR_GLOBAL)) {
			long args[3];
			args[0] = (long)cur_sibuf->rf;
			args[1] = (long)get_builtin_resfile();
			args[2] = (long)tmp->name;
			sn_tmp = analysis__sinode_search(type, SEARCH_BY_SPEC,
							 (void *)args);
		} else if ((type == TYPE_FUNC_STATIC) ||
				(type == TYPE_VAR_STATIC)) {
			long args[4];
			args[0] = (long)loc_file;
			args[1] = 0;
			args[2] = 0;
			args[3] = (long)tmp->name;
			sn_tmp = analysis__sinode_search(type, SEARCH_BY_SPEC,
							 (void *)args);
		}

		/* TODO: STB_WEAK not handled */
		if (sn_tmp)
			continue;

		sn_new = analysis__sinode_new(type, tmp->name,
					      strlen(tmp->name) + 1,
					      NULL, 0);
		BUG_ON(!sn_new);
		sn_new->buf = cur_sibuf;
		sn_new->loc_file = loc_file;

		if ((type == TYPE_VAR_GLOBAL) || (type == TYPE_VAR_STATIC)) {
			struct var_node *vn;
			vn = var_node_new(tmp->load_addr);
			vn->name = sn_new->name;
			vn->size = tmp->size;
			sn_new->data = (char *)vn;
			sn_new->datalen = sizeof(*vn);
		} else if ((type == TYPE_FUNC_GLOBAL) ||
				(type == TYPE_FUNC_STATIC)) {
			struct func_node *fn = NULL;
			fn = func_node_new((void *)tmp->load_addr);
			fn->name = sn_new->name;
			fn->size = tmp->size;
			sn_new->data = (char *)fn;
			sn_new->datalen = sizeof(*fn);
		}

		BUG_ON(analysis__sinode_insert(sn_new, SINODE_INSERT_BH_NONE));
	}

	elf_drop_syms(&syms_head);
ef_out:
	elf_cleanup(ef);
out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void getdetail(void)
{
	return;
}

static void getstep4(void)
{
	return;
}

static void getindcfg1(void)
{
	return;
}

static void getindcfg2(void)
{
	return;
}

static __thread long show_progress_arg[0x10];
static int show_progress_timeout = 3;
static void this_show_progress(int signo, siginfo_t *si, void *arg, int last)
{
	long *args = (long *)arg;
	char *parsing_file = (char *)args[1];
	long cur = *(long *)args[2];
	long goal = *(long *)args[3];
	pthread_t id = (pthread_t)args[4];

	long percent = (cur) * 100 / (goal);
	mt_print1(id, "0x%lx %s (%ld%%)\n", id, parsing_file, percent);
}

static int gcc_ver_major = __GNUC__;
static int gcc_ver_minor = __GNUC_MINOR__;
static int callback(struct sibuf *buf, int parse_mode)
{
	CLIB_DBG_FUNC_ENTER();

	struct file_content *fc = (struct file_content *)buf->load_addr;
	if ((fc->gcc_ver_major != gcc_ver_major) ||
		(fc->gcc_ver_minor > gcc_ver_minor)) {
		si_log1_emer("gcc version not match, need %d.%d\n",
				fc->gcc_ver_major, fc->gcc_ver_minor);
		CLIB_DBG_FUNC_EXIT();
		return -1;
	}

	cur_sibuf = buf;
	cur_ctx = fc_pldptr(fc);
	cur_srcfile = fc->path;
	obj_done = 0;
	obj_cnt = 1;

	char *sp_path = fc->path + fc->srcroot_len;

	switch (parse_mode) {
	case MODE_ADJUST:
	{
		show_progress_arg[0] = (long)"ADJUST";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_done;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, this_show_progress,
				show_progress_arg, 0, 1);

		sleep(1);
		obj_done = 1;

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETBASE:
	{
		show_progress_arg[0] = (long)"GETBASE";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_done;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, this_show_progress,
				show_progress_arg, 0, 1);

		getbase();
		obj_done = 1;

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETDETAIL:
	{
		show_progress_arg[0] = (long)"GETDETAIL";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_done;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, this_show_progress,
				show_progress_arg, 0, 1);

		getdetail();
		obj_done = 1;

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETSTEP4:
	{
		show_progress_arg[0] = (long)"GETSTEP4";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_done;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, this_show_progress,
				show_progress_arg, 0, 1);

		getstep4();
		obj_done = 1;

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETINDCFG1:
	{
		show_progress_arg[0] = (long)"GETINDCFG1";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_done;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, this_show_progress,
				show_progress_arg, 0, 1);

		getindcfg1();
		obj_done = 1;

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETINDCFG2:
	{
		show_progress_arg[0] = (long)"GETINDCFG2";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_done;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, this_show_progress,
				show_progress_arg, 0, 1);

		getindcfg2();
		obj_done = 1;

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	default:
	{
		BUG();
	}
	}

	CLIB_DBG_FUNC_EXIT();
	return 0;
}
