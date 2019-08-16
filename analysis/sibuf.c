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
#include "si_core.h"

struct sibuf *sibuf_new(void)
{
	void *ret = src_buf_get(sizeof(struct sibuf));
	return (struct sibuf *)ret;
}

void sibuf_insert(struct sibuf *b)
{
	write_lock(&si->lock);
	list_add_tail(&b->sibling, &si->sibuf_head);
	write_unlock(&si->lock);
}

void sibuf_remove(struct sibuf *b)
{
	write_lock(&si->lock);
	list_del(&b->sibling);
	write_unlock(&si->lock);
}

static void sibuf_type_node_insert_same_tc(struct rb_root *root,
						struct sibuf_type_node *stn)
{
	struct rb_node **node = &(root->rb_node), *parent = NULL;

	while (*node) {
		struct sibuf_type_node *data = container_of(*node,
							struct sibuf_type_node,
							same_tc_node);
		parent = *node;
		struct type_node *tn = &data->type;
		if (tn->node > stn->type.node)
			node = &((*node)->rb_left);
		else if (tn->node < stn->type.node)
			node = &((*node)->rb_right);
		else
			BUG();
	}

	rb_link_node(&stn->same_tc_node, parent, node);
	rb_insert_color(&stn->same_tc_node, root);

	return;
}

int sibuf_type_node_insert(struct sibuf *b, struct sibuf_type_node *stn)
{
	write_lock(&b->lock);

	struct rb_root *root = &b->file_types;
	struct rb_node **node = &(root->rb_node), *parent = NULL;

	while (*node) {
		struct sibuf_type_node *data = container_of(*node,
							struct sibuf_type_node,
							tc_node);
		parent = *node;
		struct type_node *tn = &data->type;
		if (tn->type_code > stn->type.type_code)
			node = &((*node)->rb_left);
		else if (tn->type_code < stn->type.type_code)
			node = &((*node)->rb_right);
		else {
			BUG_ON(tn->node == stn->type.node);
			sibuf_type_node_insert_same_tc(&data->same_tc_root, stn);
			write_unlock(&b->lock);
			return 0;
		}
	}

	rb_link_node(&stn->tc_node, parent, node);
	rb_insert_color(&stn->tc_node, root);

	write_unlock(&b->lock);
	return 0;
}

static struct type_node *sibuf_type_node_search_same_tc(struct rb_root *root,
							void *addr)
{
	struct rb_node **node = &(root->rb_node);

	while (*node) {
		struct sibuf_type_node *data = container_of(*node,
							struct sibuf_type_node,
							same_tc_node);
		struct type_node *tn = &data->type;

		if (tn->node > addr)
			node = &((*node)->rb_left);
		else if (tn->node < addr)
			node = &((*node)->rb_right);
		else
			return tn;
	}

	return NULL;
}

struct type_node *sibuf_type_node_search(struct sibuf *b, int tc, void *addr)
{
	struct type_node *ret = NULL;
	read_lock(&b->lock);

	struct rb_root *root = &b->file_types;
	struct rb_node **node = &(root->rb_node);

	while (*node) {
		struct sibuf_type_node *data = container_of(*node,
							struct sibuf_type_node,
							tc_node);
		struct type_node *tn = &data->type;
		if (tn->type_code > tc)
			node = &((*node)->rb_left);
		else if (tn->type_code < tc)
			node = &((*node)->rb_right);
		else {
			if (tn->node == addr) {
				ret = tn;
				break;
			}

			ret = sibuf_type_node_search_same_tc(&data->same_tc_root,
								addr);
			break;
		}
	}

	read_unlock(&b->lock);
	return ret;
}
