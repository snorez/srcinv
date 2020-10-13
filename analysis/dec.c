/*
 * decode code in sinode.
 *	SI_TYPE_DF_GIMPLE: gimple
 *	SI_TYPE_DF_ASM: binary
 *
 * We need to know what the code does, the input should be a code_path, and
 * its type, the output is a cp_state list.
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

int dec_next(struct sample_set *sset, int idx)
{
	int ret = 0;
	struct sibuf *buf;
	struct file_content *fc;
	struct lang_ops *ops;
	struct sinode *entry_fsn = sample_state_cur_entry(sset->samples[idx]);

	if ((!entry_fsn) || (!entry_fsn->data))
		return 0;

	if (!sset) {
		si_log1_err("Param sample_state must not be NULL\n");
		return -1;
	}

	struct func_node *entry_fn = (struct func_node *)entry_fsn->data;
	buf = find_target_sibuf(entry_fn->node);
	if (!buf) {
		si_log1_todo("find_target_sibuf return NULL\n");
		return -1;
	}

	fc = (struct file_content *)buf->load_addr;
	int held = analysis__sibuf_hold(buf);
	ops = lang_ops_find(&analysis_lang_ops_head, &fc->type);
	if (!held)
		analysis__sibuf_drop(buf);
	if (!ops) {
		si_log1_todo("lang_ops_find return NULL\n");
		return -1;
	}

	ret = ops->dec(sset, idx, entry_fn);
	return ret;
}

#define	SPECIAL_CALL_HANDLED	(0)
#define	SPECIAL_CALL_NOTHANDLED	(-1)

static int dec_linux_kernel_kmalloc(struct sample_set *sset, int idx,
				    struct fn_list *fnl,
				    struct func_node *call_fn)
{
	/* TODO */
	return SPECIAL_CALL_NOTHANDLED;
}

static int dec_linux_kernel_kzalloc(struct sample_set *sset, int idx,
				    struct fn_list *fnl,
				    struct func_node *call_fn)
{
	/* TODO */
	return SPECIAL_CALL_NOTHANDLED;
}

static int dec_linux_kernel_kmem_cache_alloc(struct sample_set *sset, int idx,
					     struct fn_list *fnl,
					     struct func_node *call_fn)
{
	/* TODO */
	return SPECIAL_CALL_NOTHANDLED;
}

static int dec_linux_kernel_kfree(struct sample_set *sset, int idx,
				  struct fn_list *fnl,
				  struct func_node *call_fn)
{
	/* TODO */
	return SPECIAL_CALL_NOTHANDLED;
}

static int dec_linux_kernel_kzfree(struct sample_set *sset, int idx,
				   struct fn_list *fnl,
				   struct func_node *call_fn)
{
	/* TODO */
	return SPECIAL_CALL_NOTHANDLED;
}

static int dec_linux_kernel_kmem_cache_free(struct sample_set *sset, int idx,
					    struct fn_list *fnl,
					    struct func_node *call_fn)
{
	/* TODO */
	return SPECIAL_CALL_NOTHANDLED;
}

static struct {
	char	*funcname;
	int	(*cb)(struct sample_set *sset, int idx,
			struct fn_list *fnl, struct func_node *call_fn);
} linux_kernel_special_call_cbs[] = {
	{"kmalloc", dec_linux_kernel_kmalloc},
	{"kzalloc", dec_linux_kernel_kzalloc},
	{"kmem_cache_alloc", dec_linux_kernel_kmem_cache_alloc},

	{"kfree", dec_linux_kernel_kfree},
	{"kzfree", dec_linux_kernel_kzfree},
	{"kmem_cache_free", dec_linux_kernel_kmem_cache_free},
};

static int dec_linux_kernel_special_call(struct sample_set *sset, int idx,
					 struct fn_list *fnl,
					 struct func_node *call_fn)
{
	int ret = SPECIAL_CALL_NOTHANDLED;

	int cnt = sizeof(linux_kernel_special_call_cbs) /
			sizeof(linux_kernel_special_call_cbs[0]);
	for (int i = 0; i < cnt; i++) {
		if (strcmp(linux_kernel_special_call_cbs[i].funcname,
				call_fn->name))
			continue;

		if (linux_kernel_special_call_cbs[i].cb)
			ret = linux_kernel_special_call_cbs[i].cb(sset, idx,
								  fnl, call_fn);
		break;
	}

	return ret;
}

/*
 * @dec_special_call: handle some special calls.
 * Return value:
 *	0: this call is properly handled.
 *	-1: this call is not handled, caller handle it itself.
 *
 * We need to setup the retval, if no return value, set VOID_RETVAL.
 * The retval is a data_state_rw.
 * Check the refcount if freeing an object.
 */
int dec_special_call(struct sample_set *sset, int idx, struct fn_list *fnl,
			struct func_node *call_fn)
{
	int ret = SPECIAL_CALL_NOTHANDLED;

	if (src_is_linux_kernel()) {
		ret = dec_linux_kernel_special_call(sset, idx, fnl, call_fn);
	}

	return ret;
}
