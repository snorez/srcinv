/*
 * this is for parsing the collected tree nodes, thus we MUST NOT use any function
 * that manipulate pointer in tree_node structure before we adjust the pointer
 * TODO
 * Copyright (C) 2018  zerons
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
#include "./analysis.h"

C_SYM int parse_resfile(char *path, int built_in, int step, int autoy);
static char parse_cmdname[] = "parse";
static void parse_usage(void)
{
	fprintf(stdout, "\t(resfile) (kernel) (builtin) (step) (auto_Y)\n"
			"\tGet information of resfile, steps are:\n"
			"\t\t0 Get all information\n"
			"\t\t1 Get information adjusted\n"
			"\t\t2 Get base information\n"
			"\t\t3 Get detail information\n"
			"\t\t4 Prepare for step5\n"
			"\t\t5 Get indirect call information\n"
			"\t\t6 Check if all GIMPLE_CALL are set\n");
}
static long parse_cb(int argc, char *argv[])
{
	int err;
	char respath[PATH_MAX];

	if ((argc != 5) && (argc != 6)) {
		parse_usage();
		err_dbg(0, "argc invalid");
		return -1;
	}

	if (si_current_resfile(respath, PATH_MAX, argv[1])) {
		err_dbg(0, "si_current_resfile err");
		return -1;
	}

	int kernel = atoi(argv[2]);
	int builtin = atoi(argv[3]);
	int step = atoi(argv[4]);
	int autoy = 0;
	if (argc == 6)
		autoy = atoi(argv[5]);

	err = parse_resfile(respath, builtin, step, autoy);
	if (err) {
		err_dbg(0, "parse_resfile err");
		return -1;
	}

	si->is_kernel = kernel;

	return 0;
}

C_SYM int parse_sibuf_bypath(char *srcpath, int step, int force);
static char one_sibuf_cmdname[] = "one_sibuf";
static void one_sibuf_usage(void)
{
	fprintf(stdout, "\t(srcpath) (step) (force)\n");
}
static long one_sibuf_cb(int argc, char *argv[])
{
	if (argc != 4) {
		one_sibuf_usage();
		err_dbg(0, "argc invalid");
		return -1;
	}

	char *path = argv[1];
	int step = atoi(argv[2]);
	int force = atoi(argv[3]);
	return parse_sibuf_bypath(path, step, !!force);
}

static char getoffs_cmdname[] = "getoffs";
static void getoffs_usage(void)
{
	fprintf(stdout, "\t(resfile) (filecnt)\n"
			"\tCount filecnt files and calculate the offset\n");
}
static long getoffs_cb(int argc, char *argv[])
{
	long err;
	char respath[PATH_MAX];

	if (argc != 3) {
		getoffs_usage();
		err_dbg(0, "argc invalid");
		return -1;
	}

	if (si_current_resfile(respath, PATH_MAX, argv[1])) {
		err_dbg(0, "si_current_resfile err");
		return -1;
	}

	unsigned long filecnt = atoll(argv[2]);
	unsigned long offs;
	err = analysis__resfile_get_offset(respath, filecnt, &offs);
	if (err) {
		err_dbg(0, "analysis__resfile_get_offset err");
		return -1;
	}

	fprintf(stdout, "%ld files take size %ld\n", filecnt, offs);
	return 0;
}

static char cmdline_cmdname[] = "cmdline";
static void cmdline_usage(void)
{
	fprintf(stdout, "\t(resfile) (filepath)\n"
			"\tShow the command line used to compile the file\n");
}
static long cmdline_cb(int argc, char *argv[])
{
	char respath[PATH_MAX];

	if (argc != 3) {
		cmdline_usage();
		err_dbg(0, "arg invalid");
		return -1;
	}

	if (argv[2][0] != '/') {
		err_dbg(0, "filepath not an absolute path");
		return -1;
	}

	if (si_current_resfile(respath, PATH_MAX, argv[1])) {
		err_dbg(0, "si_current_resfile err");
		return -1;
	}

	struct file_content *fc;
	int idx;
	fc = analysis__resfile_get_fc(respath, argv[2], &idx);
	if (!fc) {
		err_dbg(0, "analysis__resfile_get_fc err");
		return -1;
	}

	fprintf(stdout, "the target file idx: %d, command line is\n"
			"\t%s\n", idx, (char *)fc_cmdptr(fc));
	return 0;
}

SI_MOD_SUBENV_INIT()
{
	int err;
	INIT_LIST_HEAD(&analysis_lang_ops_head);

	err = clib_cmd_ac_add(parse_cmdname, parse_cb, parse_usage);
	if (err) {
		err_dbg(0, "clib_cmd_ac_add err");
		return -1;
	}

	err = clib_cmd_ac_add(getoffs_cmdname, getoffs_cb, getoffs_usage);
	if (err) {
		err_dbg(0, "clib_cmd_ac_add err");
		goto err0;
	}

	err = clib_cmd_ac_add(cmdline_cmdname, cmdline_cb, cmdline_usage);
	if (err) {
		err_dbg(0, "clib_cmd_ac_add err");
		goto err1;
	}

	err = clib_cmd_ac_add(one_sibuf_cmdname, one_sibuf_cb, one_sibuf_usage);
	if (err) {
		err_dbg(0, "clib_cmd_ac_add err");
		goto err2;
	}

	/*
	 * load analysis modules first
	 */
	struct list_head *head;
	head = si_module_get_head(SI_PLUGIN_CATEGORY_ANALYSIS);
	if (!head) {
		err_dbg(0, "si_module_get_head err");
		goto errl;
	}

	err = si_module_load_all(head);
	if (err) {
		err_dbg(0, "si_module_load_all err");
		goto errl;
	}

	return 0;

errl:
	clib_cmd_ac_del(one_sibuf_cmdname);
err2:
	clib_cmd_ac_del(cmdline_cmdname);
err1:
	clib_cmd_ac_del(getoffs_cmdname);
err0:
	clib_cmd_ac_del(parse_cmdname);
	return -1;
}

SI_MOD_SUBENV_DEINIT()
{
	struct list_head *head;
	head = si_module_get_head(SI_PLUGIN_CATEGORY_ANALYSIS);
	si_module_unload_all(head);
}

SI_MOD_SUBENV_SETUP(analysis);
