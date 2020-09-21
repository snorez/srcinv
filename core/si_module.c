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

static struct slist_head si_module_head[SI_PLUGIN_CATEGORY_MAX] = {
	[SI_PLUGIN_CATEGORY_CORE] = SLIST_HEAD_INIT(si_module_head[SI_PLUGIN_CATEGORY_CORE]),
	[SI_PLUGIN_CATEGORY_COLLECT] = SLIST_HEAD_INIT(si_module_head[SI_PLUGIN_CATEGORY_COLLECT]),
	[SI_PLUGIN_CATEGORY_ANALYSIS] = SLIST_HEAD_INIT(si_module_head[SI_PLUGIN_CATEGORY_ANALYSIS]),
	[SI_PLUGIN_CATEGORY_HACKING] = SLIST_HEAD_INIT(si_module_head[SI_PLUGIN_CATEGORY_HACKING]),
};

static char *si_module_category_strings[] = {
	[SI_PLUGIN_CATEGORY_CORE] = "core",
	[SI_PLUGIN_CATEGORY_COLLECT] = "collect",
	[SI_PLUGIN_CATEGORY_ANALYSIS] = "analysis",
	[SI_PLUGIN_CATEGORY_HACKING] = "hacking",
};

void si_module_init(struct si_module *p)
{
	INIT_SLIST_HEAD(&p->sibling);
	p->name = NULL;
	p->path = NULL;
	p->comment = NULL;
	p->category = -1;
	/* p->autoload is set by si_conf.c */
	p->loaded = 0;
}

int8_t si_module_str_to_category(char *string)
{
	if (unlikely(!string)) {
		err_msg("arg check err");
		return -1;
	}

	int i = SI_PLUGIN_CATEGORY_MIN;
	for (; i < SI_PLUGIN_CATEGORY_MAX; i++) {
		if (!si_module_category_strings[i])
			continue;
		if (!strcmp(string, si_module_category_strings[i])) {
			return i;
		}
	}

	return -1;
}

int si_module_get_abs_path(char *buf, size_t len, int category, char *path)
{
	char *category_str = si_module_category_strings[category];

	if (*path == '.') {
		err_msg("module path can not be relative path");
		return -1;
	} else if (*path == '/') {
		snprintf(buf, len, "%s", path);
		return 0;
	} else {
		if (category == SI_PLUGIN_CATEGORY_CORE)
			snprintf(buf, len, "%s/%s", DEF_PLUGINDIR, path);
		else
			snprintf(buf, len, "%s/%s/%s", DEF_PLUGINDIR,
					category_str, path);
		return 0;
	}
}

struct slist_head *si_module_get_head(int category)
{
	if ((category <= SI_PLUGIN_CATEGORY_MIN) ||
			(category >= SI_PLUGIN_CATEGORY_MAX)) {
		err_msg("category not recognized");
		return NULL;
	}

	return &si_module_head[category];
}

struct si_module *si_module_find_by_name(char *name, struct slist_head *head)
{
	struct si_module *tmp;
	slist_for_each_entry(tmp, head, sibling) {
		if (!strcmp(tmp->name, name))
			return tmp;
	}

	return NULL;
}

static struct si_module *si_module_dup(struct si_module *old)
{
	size_t len;
	struct si_module *_new;
	_new = malloc(sizeof(*_new));
	if (!_new) {
		err_msg("malloc err");
		return NULL;
	}
	memset(_new, 0, sizeof(*_new));
	memcpy(_new, old, sizeof(*_new));

	len = strlen(old->name) + 1;
	_new->name = malloc(len);
	if (!_new->name) {
		err_msg("malloc err");
		goto out0;
	}
	memcpy(_new->name, old->name, len);

	len = strlen(old->path) + 1;
	_new->path = malloc(len);
	if (!_new->path) {
		err_msg("malloc err");
		goto out0;
	}
	memcpy(_new->path, old->path, len);

	len = strlen(old->comment) + 1;
	_new->comment = malloc(len);
	if (!_new->comment) {
		err_msg("malloc err");
		goto out0;
	}
	memcpy(_new->comment, old->comment, len);

	return _new;

out0:
	if (_new->comment)
		free(_new->comment);
	if (_new->path)
		free(_new->path);
	if (_new->name)
		free(_new->name);
	free(_new);
	return NULL;
}

int si_module_add(struct si_module *p)
{
	struct slist_head *head = si_module_get_head(p->category);
	if (!head) {
		err_msg("category err");
		return -1;
	}

	if (si_module_find_by_name(p->name, head)) {
		err_msg("category: %s already has name %s",
				si_module_category_strings[p->category], p->name);
		return -1;
	}

	struct si_module *_new = si_module_dup(p);
	if (!_new) {
		err_msg("si_module_dup err");
		return -1;
	}

	INIT_SLIST_HEAD(&_new->sibling);
	slist_add_tail(&_new->sibling, head);
	return 0;
}

static int si_module_act(struct si_module *sm, int action)
{
	char *argv[1];
	argv[0] = sm->path;
	int err;

	/* check if the module exists */
	if (!path_exists(sm->path))
		return 0;

	if ((!action) && (!sm->loaded)) {
		/* load */
		err = clib_module_load(1, argv);
		if (err) {
			err_msg("clib_module_load %s err", sm->name);
			return -1;
		}
		sm->loaded = 1;
	} else if (action && (sm->loaded)){
		/* unload */
		err = clib_module_unload(1, argv);
		if (err) {
			err_msg("clib_module_unload %s err", sm->name);
			return -1;
		}
		sm->loaded = 0;
	}
	return 0;
}

int si_module_load_all(struct slist_head *head)
{
	struct si_module *tmp;
	slist_for_each_entry(tmp, head, sibling) {
		if (si_module_act(tmp, 0))
			return -1;
	}
	return 0;
}

int si_module_unload_all(struct slist_head *head)
{
	struct si_module *tmp;
	slist_for_each_entry(tmp, head, sibling) {
		if (si_module_act(tmp, 1))
			return -1;
	}
	return 0;
}

int si_module_setup(void)
{
	int err = 0;
	err = si_module_load_all(&si_module_head[SI_PLUGIN_CATEGORY_CORE]);
	if (err) {
		err_msg("si_module_load_all err");
		return -1;
	}

	return 0;
}

static void si_module_cleanup_head(struct slist_head *head)
{
	struct si_module *tmp, *next;
	slist_for_each_entry_safe(tmp, next, head, sibling) {
		slist_del(&tmp->sibling, head);
		free(tmp->name);
		free(tmp->path);
		free(tmp->comment);
		free(tmp);
	}
}

void si_module_cleanup(void)
{
	struct slist_head *h = si_module_get_head(SI_PLUGIN_CATEGORY_CORE);
	si_module_unload_all(h);
	si_module_cleanup_head(h);

	/* now, cleanup the collect/analysis/hacking modules, just list_head */
	int i = SI_PLUGIN_CATEGORY_COLLECT;
	for (; i <= SI_PLUGIN_CATEGORY_HACKING; i++) {
		h = si_module_get_head(i);
		si_module_cleanup_head(h);
	}
}
