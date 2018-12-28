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
	if ((type == TYPE_TYPE_NAME) || (type == TYPE_FUNC_GLOBAL) ||
		(type == TYPE_VAR_GLOBAL))
		INIT_LIST_HEAD(&ret->same_name_head);
	else if ((type == TYPE_TYPE_LOC) || (type == TYPE_FUNC_STATIC) ||
		(type == TYPE_VAR_STATIC))
		INIT_LIST_HEAD(&ret->same_loc_line_head);

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
			err_dbg(0, err_fmt("same id happened"));
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

static int sinode_insert_by_name(struct sinode *node, int behavior)
{
	BUG_ON(!node->namelen);
	enum sinode_type type = sinode_get_id_type(node);

	write_lock(&si->lock);
	struct rb_root *root = &si->sinodes[type][RB_NODE_BY_NAME];
	struct rb_node **newtmp = &(root->rb_node), *parent = NULL;

	while (*newtmp) {
		struct sinode *data = container_of(*newtmp, struct sinode,
							SINODE_BY_NAME_NODE);
		int res = strcmp(data->name, node->name);
		parent = *newtmp;
		if (res > 0)
			newtmp = &((*newtmp)->rb_left);
		else if (res < 0)
			newtmp = &((*newtmp)->rb_right);
		else {
			switch (behavior) {
			case SINODE_INSERT_BH_NONE:
			default:
				err_dbg(0, err_fmt("insert_by_name warning"));
				write_unlock(&si->lock);
				return -1;
			case SINODE_INSERT_BH_REPLACE:
			{
				BUG_ON(node->datalen != data->datalen);

				write_lock(&data->lock);
				sinode_insert_do_replace(data, node);
				write_unlock(&data->lock);

				write_unlock(&si->lock);
				return 0;
			}
			case SINODE_INSERT_BH_SOFT:
			{
				/* two weak symbols */
				write_lock(&data->lock);
				list_add_tail(&node->same_name_sibling,
						&data->same_name_head);
				write_unlock(&data->lock);

				write_unlock(&si->lock);
				return 0;
			}
			}
		}
	}

	rb_link_node(&node->SINODE_BY_NAME_NODE, parent, newtmp);
	rb_insert_color(&node->SINODE_BY_NAME_NODE, root);

	write_unlock(&si->lock);
	return 0;
}

static void sinode_insert_by_same_loc_line(struct sinode *sn, struct list_head *head)
{
	struct sinode *tmp;
	list_for_each_entry(tmp, head, same_loc_line_sibling) {
		BUG_ON(tmp->loc_col == sn->loc_col);
	}

	list_add_tail(&sn->same_loc_line_sibling, head);
}

static void sinode_insert_by_same_loc_file(struct sinode *sn, struct rb_root *root)
{
	struct rb_node **newtmp = &(root->rb_node), *parent = NULL;

	while (*newtmp) {
		struct sinode *data = container_of(*newtmp, struct sinode,
							same_loc_file_node);
		if (data->loc_line > sn->loc_line)
			newtmp = &((*newtmp)->rb_left);
		else if (data->loc_line < sn->loc_line)
			newtmp = &((*newtmp)->rb_right);
		else {
			if (data->loc_col == sn->loc_col)
				BUG();

			write_lock(&data->lock);
			sinode_insert_by_same_loc_line(sn, &data->same_loc_line_head);
			write_unlock(&data->lock);
			return;
		}
	}

	rb_link_node(&sn->same_loc_file_node, parent, newtmp);
	rb_insert_color(&sn->same_loc_file_node, root);

	return;
}

static int sinode_insert_by_loc(struct sinode *sn)
{
	BUG_ON(!sn->loc_file);
	enum sinode_type type = sinode_get_id_type(sn);

	write_lock(&si->lock);
	struct rb_root *root = &si->sinodes[type][RB_NODE_BY_LOC];
	struct rb_node **newtmp = &(root->rb_node), *parent = NULL;

	while (*newtmp) {
		struct sinode *data = container_of(*newtmp, struct sinode,
							SINODE_BY_LOC_NODE);
		parent = *newtmp;
		if (data->loc_file > sn->loc_file)
			newtmp = &((*newtmp)->rb_left);
		else if (data->loc_file < sn->loc_file)
			newtmp = &((*newtmp)->rb_right);
		else {
			if ((data->loc_line == sn->loc_line) &&
				(data->loc_col == sn->loc_col))
				BUG();

			write_lock(&data->lock);
			sinode_insert_by_same_loc_file(sn, &data->same_loc_file_root);
			write_unlock(&data->lock);

			write_unlock(&si->lock);
			return 0;
		}
	}

	rb_link_node(&sn->SINODE_BY_LOC_NODE, parent, newtmp);
	rb_insert_color(&sn->SINODE_BY_LOC_NODE, root);

	write_unlock(&si->lock);
	return 0;
}

#if 0
static void sinode_insert_type_same_code(struct sinode *sn, struct rb_root *root)
{
	struct rb_node **tmp = &(root->rb_node), *parent = NULL;
	struct type_node *tn = (struct type_node *)sn->data;

	while (*tmp) {
		struct sinode *data = container_of(*tmp, struct sinode,
							same_tc_node);
		parent = *tmp;
		struct type_node *tntmp = (struct type_node *)data->data;
		if (tntmp->node > tn->node)
			tmp = &((*tmp)->rb_left);
		else if (tntmp->node < tn->node)
			tmp = &((*tmp)->rb_right);
		else
			BUG();
	}

	rb_link_node(&sn->same_tc_node, parent, tmp);
	rb_insert_color(&sn->same_tc_node, root);

	return;
}

static int sinode_insert_by_addr(struct sinode *sn)
{
	enum sinode_type type = sinode_get_id_type(sn);
	BUG_ON(type != TYPE_TYPE);

	write_lock(&si->lock);
	struct rb_root *root = &si->sinodes[TYPE_TYPE][RB_NODE_BY_ADDR];
	struct rb_node **tmp = &(root->rb_node), *parent = NULL;
	struct type_node *tn = (struct type_node *)sn->data;
	int err = 0;

	while (*tmp) {
		struct sinode *data = container_of(*tmp, struct sinode,
							SINODE_BY_ADDR_NODE);
		struct type_node *tntmp = (struct type_node *)data->data;
		parent = *tmp;
		if (tntmp->type_code > tn->type_code)
			tmp = &((*tmp)->rb_left);
		else if (tntmp->type_code < tn->type_code)
			tmp = &((*tmp)->rb_right);
		else {
			write_lock(&data->lock);
			sinode_insert_type_same_code(sn, &data->same_tc_root);
			write_unlock(&data->lock);

			write_unlock(&si->lock);
			return 0;
		}
	}

	rb_link_node(&sn->SINODE_BY_NODE_NODE, parent, tmp);
	rb_insert_color(&sn->SINODE_BY_NODE_NODE, root);

	write_unlock(&si->lock);
	return err;
}
#endif

/*
 * if the behavior is SINODE_INSERT_BH_REPLACE, then we should not insert the ID
 */
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
		err = sinode_insert_by_name(node, SINODE_INSERT_BH_NONE);
		break;
	}
	case TYPE_TYPE_NAME:
	{
		err = sinode_insert_by_name(node, behavior);
		break;
	}
	case TYPE_TYPE_LOC:
	{
		err = sinode_insert_by_loc(node);
		break;
	}
	case TYPE_FUNC_GLOBAL:
	{
		err = sinode_insert_by_name(node, behavior);
		break;
	}
	case TYPE_FUNC_STATIC:
	{
		err = sinode_insert_by_loc(node);
		break;
	}
	case TYPE_VAR_GLOBAL:
	{
		err = sinode_insert_by_name(node, behavior);
		break;
	}
	case TYPE_VAR_STATIC:
	{
		err = sinode_insert_by_loc(node);
		break;
	}
	default:
	{
		BUG();
	}
	}

	if (err) {
		err_dbg(0, err_fmt("sinode_insert_by_* err"));
		return -1;
	}

	return 0;
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

static struct sinode *sinode_search_by_name(enum sinode_type type, char *name)
{
	struct sinode *ret = NULL;
	enum sinode_type type_min, type_max;
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
		struct rb_root *root = &si->sinodes[i][RB_NODE_BY_NAME];
		struct rb_node **node = &(root->rb_node);
		while (*node) {
			struct sinode *data = container_of(*node, struct sinode,
								SINODE_BY_NAME_NODE);
			int res = strcmp(data->name, name);
			if (res > 0)
				node = &((*node)->rb_left);
			else if (res < 0)
				node = &((*node)->rb_right);
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

static struct sinode *sinode_search_by_same_loc_line(struct list_head *head,
							int col)
{
	struct sinode *ret;
	list_for_each_entry(ret, head, same_loc_line_sibling) {
		if (ret->loc_col == col)
			return ret;
	}

	return NULL;
}

static struct sinode *sinode_search_by_same_loc_file(struct rb_root *root,
							int line,
							int col)
{
	struct sinode *ret = NULL;
	struct rb_node **node = &(root->rb_node);
	while (*node) {
		struct sinode *data = container_of(*node, struct sinode,
							same_loc_file_node);
		if (data->loc_line > line)
			node = &((*node)->rb_left);
		else if (data->loc_line < line)
			node = &((*node)->rb_right);
		else {
			if (data->loc_col == col) {
				ret = data;
				break;
			}

			read_lock(&data->lock);
			ret = sinode_search_by_same_loc_line(
							&data->same_loc_line_head,
							col);
			read_unlock(&data->lock);
			break;
		}
	}

	return ret;
}

static struct sinode *sinode_search_by_loc(enum sinode_type type, void *arg)
{
	unsigned long *args = (unsigned long *)arg;
	char *file = (char *)args[0];
	int line = (int)args[1];
	int col = (int)args[2];
	BUG_ON(!file);

	struct sinode *loc_file = NULL;
	loc_file = sinode__sinode_search(TYPE_FILE, SEARCH_BY_NAME, file);
	BUG_ON(!loc_file);

	read_lock(&si->lock);
	struct rb_root *root = &si->sinodes[type][RB_NODE_BY_LOC];
	struct rb_node **node = &(root->rb_node);
	struct sinode *ret = NULL;
	while (*node) {
		struct sinode *data = container_of(*node, struct sinode,
							SINODE_BY_LOC_NODE);
		if (data->loc_file > loc_file)
			node = &((*node)->rb_left);
		else if (data->loc_file < loc_file)
			node = &((*node)->rb_right);
		else {
			if ((data->loc_line == line) && (data->loc_col == col)) {
				ret = data;
				break;
			}

			read_lock(&data->lock);
			ret = sinode_search_by_same_loc_file(
							&data->same_loc_file_root,
							line, col);
			read_unlock(&data->lock);
			break;
		}
	}

	read_unlock(&si->lock);
	return ret;
}

struct sinode *sinode_search(enum sinode_type type, int flag, void *arg)
{
	struct sinode *ret = NULL;
	switch (flag) {
	case SEARCH_BY_ID:
	{
		ret = sinode_search_by_id(type, (union siid *)arg);
		break;
	}
	case SEARCH_BY_NAME:
	{
		ret = sinode_search_by_name(type, (char *)arg);
		break;
	}
	case SEARCH_BY_LOC:
	{
		ret = sinode_search_by_loc(type, arg);
		break;
	}
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
