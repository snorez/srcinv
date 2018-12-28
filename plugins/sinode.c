/*
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
#include "../si_core.h"

CLIB_PLUGIN_NAME(sinode);
CLIB_PLUGIN_NEEDED1(src);
CLIB_PLUGIN_INIT()
{
	return 0;
}

CLIB_PLUGIN_EXIT()
{
	return;
}

struct sinode *sinode_new(enum sinode_type type,
			char *name, size_t namelen,
			char *data, size_t datalen)
{
	struct sinode *ret = NULL;
	size_t len = sizeof(*ret);
	ret = (struct sinode *)src__src_buf_get(len);
	if (!ret) {
		err_dbg(0, err_fmt("src__src_buf_get err"));
		return ret;
	}
	memset(ret, 0, len);

	/* arrange id for sinode */
	sinode_id(type, &ret->node_id.id);
	if (name) {
		ret->name = (char *)src__src_buf_get(namelen);
		BUG_ON(!ret->name);
		memcpy(ret->name, name, namelen);
		ret->namelen = namelen;
	}
	if (data) {
		ret->data = (char *)src__src_buf_get(datalen);
		BUG_ON(!ret->data);
		memcpy(ret->data, data, datalen);
		ret->datalen = datalen;
	}
#if 0
	if ((type == TYPE_TYPE_NAME) || (type == TYPE_FUNC_GLOBAL) ||
		(type == TYPE_VAR_GLOBAL))
		INIT_LIST_HEAD(&ret->same_name_head);
	else if ((type == TYPE_TYPE_LOC) || (type == TYPE_FUNC_STATIC) ||
		(type == TYPE_VAR_STATIC))
		INIT_LIST_HEAD(&ret->same_loc_line_head);
#endif

	INIT_LIST_HEAD(&ret->attributes);

	return ret;
}

static __maybe_unused void sinode_remove(struct sinode *n)
{
	return;
}

static int sinode_insert_by_id(struct sinode *node)
{
	enum sinode_type type = sinode_get_id_type(node);
	write_lock(&si->lock);
	struct rb_root *root = &si->sinodes[type][RB_NODE_BY_ID];
	struct rb_node **newtmp = &(root->rb_node), *parent = NULL;
	int err = 0;

	while (*newtmp) {
		struct sinode *data = container_of(*newtmp, struct sinode,
							SINODE_BY_ID_NODE);
		parent = *newtmp;
		if (sinode_get_id_whole(data) > sinode_get_id_whole(node))
			newtmp = &((*newtmp)->rb_left);
		else if (sinode_get_id_whole(data) < sinode_get_id_whole(node))
			newtmp = &((*newtmp)->rb_right);
		else {
			err_dbg(0, err_fmt("insert same id"));
			err = -1;
			break;
		}
	}

	if (!err) {
		rb_link_node(&node->SINODE_BY_ID_NODE, parent, newtmp);
		rb_insert_color(&node->SINODE_BY_ID_NODE, root);
	}

	write_unlock(&si->lock);
	return err;
}

static int sinode_insert_file(struct sinode *sn)
{
	BUG_ON(!sn->name);

	write_lock(&si->lock);
	struct rb_root *root = &si->sinodes[TYPE_FILE][RB_NODE_BY_SPEC];
	struct rb_node **node = &(root->rb_node), *parent = NULL;

	while (*node) {
		struct sinode *data;
		data = container_of(*node, struct sinode, SINODE_BY_SPEC_NODE);

		int res = strcmp(data->name, sn->name);
		parent = *node;
		if (res > 0) {
			node = &((*node)->rb_left);
		} else if (res < 0) {
			node = &((*node)->rb_right);
		} else {
			err_dbg(0, err_fmt("insert same file name"));
			write_unlock(&si->lock);
			return -1;
		}
	}

	rb_link_node(&sn->SINODE_BY_SPEC_NODE, parent, node);
	rb_insert_color(&sn->SINODE_BY_SPEC_NODE, root);

	write_unlock(&si->lock);
	return 0;
}

static int _sinode_insert_name_loc(enum sinode_type type, struct sinode *sn)
{
	BUG_ON(!sn->name);
	BUG_ON(!sn->loc_file);

	write_lock(&si->lock);
	struct rb_root *root = &si->sinodes[type][RB_NODE_BY_SPEC];
	struct rb_node **node = &(root->rb_node), *parent = NULL;

	while (*node) {
		struct sinode *data;
		data = container_of(*node, struct sinode, SINODE_BY_SPEC_NODE);

		int res;
		res = strcmp(data->name, sn->name);
		parent = *node;
		if (res > 0) {
			node = &((*node)->rb_left);
			continue;
		}

		if (res < 0) {
			node = &((*node)->rb_right);
			continue;
		}

		if (data->loc_file > sn->loc_file) {
			node = &((*node)->rb_left);
			continue;
		}

		if (data->loc_file < sn->loc_file) {
			node = &((*node)->rb_right);
			continue;
		}

		if (data->loc_line > sn->loc_line) {
			node = &((*node)->rb_left);
			continue;
		}

		if (data->loc_line < sn->loc_line) {
			node = &((*node)->rb_right);
			continue;
		}

		if (data->loc_col > sn->loc_col) {
			node = &((*node)->rb_left);
			continue;
		}

		if (data->loc_col < sn->loc_col) {
			node = &((*node)->rb_right);
			continue;
		}

		err_dbg(0, err_fmt("insert same name locfile"));
		write_unlock(&si->lock);
		return -1;
	}

	rb_link_node(&sn->SINODE_BY_SPEC_NODE, parent, node);
	rb_insert_color(&sn->SINODE_BY_SPEC_NODE, root);

	write_unlock(&si->lock);
	return 0;
}

static void sinode_insert_do_replace(struct sinode *oldn, struct sinode *newn)
{
	char *odata = oldn->data;
	struct file_obj *oobj = oldn->obj;
	size_t olddlen = oldn->datalen;
	struct sibuf *oldbuf = oldn->buf;
	struct sinode *old_loc_file = oldn->loc_file;
	int old_loc_line = oldn->loc_line;
	int old_loc_col = oldn->loc_col;

	oldn->data = newn->data;
	oldn->obj = newn->obj;
	oldn->datalen = newn->datalen;
	oldn->buf = newn->buf;
	oldn->loc_file = newn->loc_file;
	oldn->loc_line = newn->loc_line;
	oldn->loc_col = newn->loc_col;

	newn->data = odata;
	newn->obj = oobj;
	newn->datalen = olddlen;
	newn->buf = oldbuf;
	newn->loc_file = old_loc_file;
	newn->loc_line = old_loc_line;
	newn->loc_col = old_loc_col;
}

static int _sinode_insert_name_resfile(enum sinode_type type, struct sinode *sn,
					int behavior)
{
	BUG_ON(!sn->name);

	write_lock(&si->lock);
	struct rb_root *root = &si->sinodes[type][RB_NODE_BY_SPEC];
	struct rb_node **node = &(root->rb_node), *parent = NULL;

	while (*node) {
		struct sinode *data;
		data = container_of(*node, struct sinode, SINODE_BY_SPEC_NODE);

		int res;
		res = strcmp(data->name, sn->name);
		parent = *node;
		if (res > 0) {
			node = &((*node)->rb_left);
			continue;
		}

		if (res < 0) {
			node = &((*node)->rb_right);
			continue;
		}

		if (data->buf->rf > sn->buf->rf) {
			node = &((*node)->rb_left);
			continue;
		}

		if (data->buf->rf < sn->buf->rf) {
			node = &((*node)->rb_right);
			continue;
		}

		switch (behavior) {
		default:
		{
			err_dbg(0, err_fmt("insert same name resfile"));
			write_unlock(&si->lock);
			return -1;
		}
		case SINODE_INSERT_BH_REPLACE:
		{
			BUG_ON(sn->datalen != data->datalen);

			write_lock(&data->lock);
			sinode_insert_do_replace(data, sn);
			write_unlock(&data->lock);

			write_unlock(&si->lock);
			return 0;
		}
		}
	}

	rb_link_node(&sn->SINODE_BY_SPEC_NODE, parent, node);
	rb_insert_color(&sn->SINODE_BY_SPEC_NODE, root);

	write_unlock(&si->lock);
	return 0;
}

static int sinode_insert_type(struct sinode *sn)
{
	return _sinode_insert_name_loc(TYPE_TYPE, sn);
}

static int sinode_insert_func_global(struct sinode *sn, int behavior)
{
	return _sinode_insert_name_resfile(TYPE_FUNC_GLOBAL, sn, behavior);
}

static int sinode_insert_func_static(struct sinode *sn)
{
	return _sinode_insert_name_loc(TYPE_FUNC_STATIC, sn);
}

static int sinode_insert_var_global(struct sinode *sn, int behavior)
{
	return _sinode_insert_name_resfile(TYPE_VAR_GLOBAL, sn, behavior);
}

static int sinode_insert_var_static(struct sinode *sn)
{
	return _sinode_insert_name_loc(TYPE_VAR_STATIC, sn);
}

int sinode_insert(struct sinode *node, int behavior)
{
	int err = 0;
	enum sinode_type type = sinode_get_id_type(node);

	/* every sinode has an id */
	if (behavior != SINODE_INSERT_BH_REPLACE) {
		err = sinode_insert_by_id(node);
		if (err) {
			err_dbg(0, err_fmt("sinode_insert_by_id err"));
			return -1;
		}
	}

	/* only ID for TYPE_CODEP */
	if (type == TYPE_CODEP)
		return 0;

	switch (type) {
	case TYPE_FILE:
	{
		err = sinode_insert_file(node);
		break;
	}
	case TYPE_TYPE:
	{
		err = sinode_insert_type(node);
		break;
	}
	case TYPE_FUNC_GLOBAL:
	{
		err = sinode_insert_func_global(node, behavior);
		break;
	}
	case TYPE_FUNC_STATIC:
	{
		err = sinode_insert_func_static(node);
		break;
	}
	case TYPE_VAR_GLOBAL:
	{
		err = sinode_insert_var_global(node, behavior);
		break;
	}
	case TYPE_VAR_STATIC:
	{
		err = sinode_insert_var_static(node);
		break;
	}
	default:
	{
		BUG();
	}
	}

	return err;
}

static struct sinode *sinode_search_by_id(enum sinode_type type, union siid *id)
{
	struct sinode *ret = NULL;
	enum sinode_type type_min, type_max;
	type = siid_get_type(id);
	if (type == TYPE_NONE) {
		type_min = (enum sinode_type)0;
		type_max = TYPE_MAX;
	} else {
		type_min = type;
		type_max = (enum sinode_type)(type + 1);
	}

	read_lock(&si->lock);
	for (enum sinode_type i = type_min; i < type_max;
			i = (enum sinode_type)(i+1)) {
		struct rb_root *root = &si->sinodes[i][RB_NODE_BY_ID];
		struct rb_node **node = &(root->rb_node);
		while (*node) {
			struct sinode *data = container_of(*node, struct sinode,
								SINODE_BY_ID_NODE);
			if (sinode_get_id_whole(data) < siid_get_whole(id))
				node = &((*node)->rb_right);
			else if (sinode_get_id_whole(data) > siid_get_whole(id))
				node = &((*node)->rb_left);
			else {
				ret = data;
				break;
			}
		}
		if (ret) {
			read_unlock(&si->lock);
			return ret;
		}
	}

	read_unlock(&si->lock);
	return NULL;
}

static struct sinode *sinode_search_file(char *name)
{
	struct sinode *ret = NULL;

	read_lock(&si->lock);
	struct rb_root *root = &si->sinodes[TYPE_FILE][RB_NODE_BY_SPEC];
	struct rb_node **node = &(root->rb_node);
	while (*node) {
		struct sinode *data;
		data = container_of(*node, struct sinode, SINODE_BY_SPEC_NODE);

		int res = strcmp(data->name, name);
		if (res > 0) {
			node = &((*node)->rb_left);
		} else if (res < 0) {
			node = &((*node)->rb_right);
		} else {
			ret = data;
			break;
		}
	}

	read_unlock(&si->lock);
	return ret;
}

static struct sinode *_sinode_search_name_loc(enum sinode_type type, long *arg)
{
	void *loc_file = (void *)arg[0];
	int loc_line = (int)arg[1];
	int loc_col = (int)arg[2];
	char *name = (char *)arg[3];

	struct sinode *ret = NULL;
	read_lock(&si->lock);
	struct rb_root *root = &si->sinodes[type][RB_NODE_BY_SPEC];
	struct rb_node **node = &(root->rb_node);
	while (*node) {
		struct sinode *data;
		data = container_of(*node, struct sinode, SINODE_BY_SPEC_NODE);

		int res;
		res = strcmp(data->name, name);
		if (res > 0) {
			node = &((*node)->rb_left);
			continue;
		}

		if (res < 0) {
			node = &((*node)->rb_right);
			continue;
		}

		if ((void *)data->loc_file > loc_file) {
			node = &((*node)->rb_left);
			continue;
		}

		if ((void *)data->loc_file < loc_file) {
			node = &((*node)->rb_right);
			continue;
		}

		if (data->loc_line > loc_line) {
			node = &((*node)->rb_left);
			continue;
		}

		if (data->loc_line < loc_line) {
			node = &((*node)->rb_right);
			continue;
		}

		if (data->loc_col > loc_col) {
			node = &((*node)->rb_left);
			continue;
		}

		if (data->loc_col < loc_col) {
			node = &((*node)->rb_right);
			continue;
		}

		ret = data;
		break;
	}

	read_unlock(&si->lock);
	return ret;
}

static struct sinode *_sinode_search_name_resfile(enum sinode_type type, long *arg)
{
	void *resfile_addr = (void *)arg[0];
	char *name = (char *)arg[1];

	struct sinode *ret = NULL;

	read_lock(&si->lock);
	struct rb_root *root = &si->sinodes[type][RB_NODE_BY_SPEC];
	struct rb_node **node = &(root->rb_node);
	while (*node) {
		struct sinode *data;
		data = container_of(*node, struct sinode, SINODE_BY_SPEC_NODE);

		int res;
		res = strcmp(data->name, name);
		if (res > 0) {
			node = &((*node)->rb_left);
			continue;
		}

		if (res < 0) {
			node = &((*node)->rb_right);
			continue;
		}

		if ((void *)data->buf->rf > resfile_addr) {
			node = &((*node)->rb_left);
			continue;
		}

		if ((void *)data->buf->rf < resfile_addr) {
			node = &((*node)->rb_right);
			continue;
		}

		ret = data;
		break;
	}

	read_unlock(&si->lock);
	return ret;
}

static struct sinode *sinode_search_type(long *arg)
{
	return _sinode_search_name_loc(TYPE_TYPE, arg);
}

static struct sinode *sinode_search_func_global(long *arg)
{
	return _sinode_search_name_resfile(TYPE_FUNC_GLOBAL, arg);
}

static struct sinode *sinode_search_func_static(long *arg)
{
	return _sinode_search_name_loc(TYPE_FUNC_STATIC, arg);
}

static struct sinode *sinode_search_var_global(long *arg)
{
	return _sinode_search_name_resfile(TYPE_VAR_GLOBAL, arg);
}

static struct sinode *sinode_search_var_static(long *arg)
{
	return _sinode_search_name_loc(TYPE_VAR_STATIC, arg);
}

#if 0
static struct sinode *sinode_search_type_name(char *name)
{
	struct sinode *ret = NULL;

	read_lock(&si->lock);
	struct rb_root *root = &si->sinodes[TYPE_TYPE][RB_NODE_BY_SPEC];
	struct rb_node **node = &(root->rb_node);
	while (*node) {
		struct sinode *data;
		data = container_of(*node, struct sinode, SINODE_BY_SPEC_NODE);

		int res;
		res = strcmp(data->name, name);
		if (res > 0) {
			node = &((*node)->rb_left);
			continue;
		}

		if (res < 0) {
			node = &((*node)->rb_right);
			continue;
		}

		struct rb_node **node_tmp;
		struct sinode *data_tmp;

		node_tmp = &((*node)->rb_left);
		if (*node_tmp) {
			data_tmp = container_of(*node_tmp,
						struct sinode,
						SINODE_BY_SPEC_NODE);
			if (unlikely(!strcmp(data_tmp->name, name)))
				BUG();
		}

		node_tmp = &((*node)->rb_right);
		if (*node_tmp) {
			data_tmp = container_of(*node_tmp,
						struct sinode,
						SINODE_BY_SPEC_NODE);
			if (unlikely(!strcmp(data_tmp->name, name)))
				BUG();
		}

		ret = data;
		break;
	}

	read_unlock(&si->lock);
	return ret;
}
#endif

struct sinode *sinode_search(enum sinode_type type, int flag, void *arg)
{
	struct sinode *ret = NULL;
	switch (flag) {
	case SEARCH_BY_ID:
	{
		ret = sinode_search_by_id(type, (union siid *)arg);
		break;
	}
	case SEARCH_BY_SPEC:
	{
		switch (type) {
		case TYPE_FILE:
		{
			ret = sinode_search_file(arg);
			break;
		}
		case TYPE_TYPE:
		{
			ret = sinode_search_type(arg);
			break;
		}
		case TYPE_FUNC_GLOBAL:
		{
			ret = sinode_search_func_global(arg);
			break;
		}
		case TYPE_FUNC_STATIC:
		{
			ret = sinode_search_func_static(arg);
			break;
		}
		case TYPE_VAR_GLOBAL:
		{
			ret = sinode_search_var_global(arg);
			break;
		}
		case TYPE_VAR_STATIC:
		{
			ret = sinode_search_var_static(arg);
			break;
		}
		default:
		{
			BUG();
		}
		}

		break;
	}
#if 0
	case SEARCH_BY_TYPE_NAME:
	{
		ret = sinode_search_type_name(arg);
		break;
	}
#endif
	default:
	{
		BUG();
	}
	}

	return ret;
}

void sinode_iter(struct rb_node *node, void (*cb)(struct rb_node *arg))
{
	if (!node)
		return;

	sinode_iter(node->rb_left, cb);
	cb(node);
	sinode_iter(node->rb_right, cb);
	return;
}
