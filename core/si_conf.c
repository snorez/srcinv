/*
 * TODO
 * Copyright (C) 2019  zerons
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

static int si_conf_module(struct clib_json *n)
{
	struct si_module tmp_si_module;
	char tmpbuf[PATH_MAX];
	int err = 0;
	si_module_init(&tmp_si_module);

	struct clib_json_kv *cur;
	list_for_each_entry(cur, &n->kvs, sibling) {
		if (cur->val_type != CJVT_STRING) {
			err_msg("json format err");
			return -1;
		}

		char *bs_key = cur->key;
		char *bs_val = cur->value.value;

		if (!strcmp(bs_key, "name")) {
			tmp_si_module.name = bs_val;
		} else if (!strcmp(bs_key, "path")) {
			tmp_si_module.path = bs_val;
		} else if (!strcmp(bs_key, "comment")) {
			tmp_si_module.comment = bs_val;
		} else if (!strcmp(bs_key, "category")) {
			tmp_si_module.category =
					si_module_str_to_category(bs_val);
			if (tmp_si_module.category == -1) {
				err_msg("category not recognized: %s",
						bs_val);
				return -1;
			}
		} else if (!strcmp(bs_key, "autoload")) {
			tmp_si_module.autoload = atoi(bs_val);
		} else {
			err_msg("config format err");
			return -1;
		}
	}

	if (tmp_si_module.category != -1) {
		err = si_module_get_abs_path(tmpbuf, PATH_MAX,
					tmp_si_module.category,
					tmp_si_module.path);
		if (err) {
			err_msg("si_module_get_abs_path err");
			return -1;
		}
		tmp_si_module.path = tmpbuf;
	} else {
		err_msg("no category");
		return -1;
	}

	err = si_module_add(&tmp_si_module);
	if (err) {
		err_msg("si_module_add err");
		return -1;
	}
	return 0;
}

static int do_conf(char *path)
{
	int err = 0;
	struct list_head tmp_head;
	INIT_LIST_HEAD(&tmp_head);

	err = clib_json_load(path, &tmp_head);
	if (err) {
		err_msg("clib_json_load %s err", path);
		return -1;
	}

	struct clib_json *tmp;
	list_for_each_entry(tmp, &tmp_head, sibling) {
		err = si_conf_module(tmp);
		if (err) {
			err_msg("si_conf_module err");
			err = -1;
			goto free_out;
		}
	}

free_out:
	clib_json_cleanup(&tmp_head);
	return err;
}

int si_config(void)
{
	int err = 0;
	char confpath[PATH_MAX];

	si_module_cleanup();

	memset(confpath, 0, PATH_MAX);
	snprintf(confpath, PATH_MAX, "%s/module.json", DEF_CONFDIR);
	err = do_conf(confpath);
	if (err) {
		err_msg("do_conf err");
		return -1;
	}

	return 0;
}
