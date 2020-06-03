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

static int __do_conf(char *buf, struct list_head *head)
{
	if (!list_empty(head))
		buf_struct_list_cleanup(head);
	if (!list_empty(head)) {
		err_msg("list_head still not empty");
		return -1;
	}

	int err = get_dict_key_value(head, buf, ":");
	if (err) {
		err_msg("get_dict_key_value err");
		return -1;
	}

	return 0;
}

static int do_conf_check(char *buf)
{
	char *pos = buf;
	int ch;

	while (*pos) {
		ch = *pos;
		if (ch != '{') {
			err_msg("the first char must be {");
			return -1;
		}

		pos = get_matched_pair(pos);
		if (!pos) {
			err_msg("doesn't find the close }");
			return -1;
		}

		pos++;
		while (isspace(*pos))
			pos++;
	}

	return 0;
}

static int si_conf_module(struct list_head *orig_head)
{
	struct si_module tmp_si_module;
	char tmpbuf[PATH_MAX];
	int err = 0;
	si_module_init(&tmp_si_module);

	list_comm *cur, *next;
	list_for_each_entry_safe(cur, next, orig_head, list_head) {
		buf_struct *bs_key = (buf_struct *)cur->data;
		buf_struct *bs_val = (buf_struct *)next->data;

		if (!strcmp(bs_key->buf, "name")) {
			tmp_si_module.name = bs_val->buf;
		} else if (!strcmp(bs_key->buf, "path")) {
			tmp_si_module.path = bs_val->buf;
		} else if (!strcmp(bs_key->buf, "comment")) {
			tmp_si_module.comment = bs_val->buf;
		} else if (!strcmp(bs_key->buf, "category")) {
			tmp_si_module.category =
					si_module_str_to_category(bs_val->buf);
			if (tmp_si_module.category == -1) {
				err_msg("category not recognized: %s",
						bs_val->buf);
				return -1;
			}
		} else if (!strcmp(bs_key->buf, "type")) {
			err = si_module_str_to_type(&tmp_si_module.type,
							bs_val->buf);
			if (err) {
				err_msg("si_module_str_to_type err");
				return -1;
			}
		} else if (!strcmp(bs_key->buf, "autoload")) {
			tmp_si_module.autoload = atoi(bs_val->buf);
		} else {
			err_msg("config format err");
			return -1;
		}

		next = list_next_entry(next, list_head);
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

static void del_conf_comment(char *confbuf)
{
	char *pos_s = confbuf, *pos_e;
	while (*pos_s) {
		pos_e = strchr(pos_s, '\n');
		if (*pos_s == '#') {
			if (!pos_e) {
				*pos_s = '\0';
				break;
			} else {
				memmove(pos_s, pos_e+1, strlen(pos_e+1)+1);
				continue;
			}
		}
		if (!pos_e)
			break;
		pos_s = pos_e + 1;
	}
}

#define	FL_CONF_PLUGIN	0
#define	FL_CONF_XXX	1
static int do_conf(char *path, int flag)
{
	int err = 0;
	struct list_head tmp_head;
	INIT_LIST_HEAD(&tmp_head);

	char *confbuf = clib_loadfile(path, NULL);
	if (!confbuf) {
		err_msg("clib_loadfile err");
		return -1;
	}

	del_conf_comment(confbuf);

	if (do_conf_check(confbuf)) {
		err_msg("do_conf_check err");
		err = -1;
		goto free_out;
	}

	char *pos = confbuf;
	while (*pos) {
		char *pos_e = get_matched_pair(pos);
		size_t len = pos_e - pos + 1;
		char tmpbuf[len+1];
		memcpy(tmpbuf, pos, len);
		tmpbuf[len] = 0;

		err = __do_conf(tmpbuf, &tmp_head);
		if (err) {
			err_msg("__do_conf err");
			err = -1;
			goto free_out;
		}

		switch (flag) {
		case FL_CONF_PLUGIN:
			err = si_conf_module(&tmp_head);
			break;
		default:
			break;
		}

		if (err) {
			err_msg("si_conf_* err");
			err = -1;
			goto free_out;
		}

		pos = pos_e+1;
		while (isspace(*pos))
			pos++;
	}

free_out:
	if (!list_empty(&tmp_head))
		buf_struct_list_cleanup(&tmp_head);
	free(confbuf);
	return err;
}

int si_config(void)
{
	int err = 0;
	char confpath[PATH_MAX];

	si_module_cleanup();

	memset(confpath, 0, PATH_MAX);
	snprintf(confpath, PATH_MAX, "%s/module.conf", DEF_CONFDIR);
	err = do_conf(confpath, FL_CONF_PLUGIN);
	if (err) {
		err_msg("do_conf err");
		return -1;
	}

	return 0;
}
