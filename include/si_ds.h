/*
 * XXX: do NOT include this file in source files, it is in si_helper.h.
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

static inline int dsv_get_section(u8 val_type)
{
	switch (val_type) {
	case DSVT_UNK:
	{
		return DSV_SEC_UNK;
	}
	case DSVT_INT_CST:
	case DSVT_REAL_CST:
	{
		return DSV_SEC_1;
	}
	case DSVT_ADDR:
	case DSVT_REF:
	{
		return DSV_SEC_2;
	}
	case DSVT_CONSTRUCTOR:
	{
		return DSV_SEC_3;
	}
	default:
	{
		BUG();
	}
	}
}

static inline void ds_init_base(struct data_state_base *base, u64 addr, u8 type)
{
	base->ref_base = addr;
	base->ref_type = type;
}

static inline struct data_state_base *ds_base_new(u64 addr, u8 type)
{
	struct data_state_base *_new;
	_new = (struct data_state_base *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	ds_init_base(_new, addr, type);
	return _new;
}

static inline void dsv_set_raw(struct data_state_val *dsv, void *raw)
{
	dsv->raw = (u64)raw;
}

static inline struct data_state_rw *ds_rw_new(u64 addr, u8 type, void *raw)
{
	struct data_state_rw *_new;
	_new = (struct data_state_rw *)xmalloc(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	ds_init_base(&_new->base, addr, type);
	dsv_set_raw(&_new->val, raw);
	atomic_set(&_new->refcnt, 1);
	return _new;
}

static inline void ds_drop(struct data_state_rw *ds);
static inline struct data_state_val *dsv_alloc(void);
static inline void dsv_free(struct data_state_val *dsv);
static inline void dsv1_free(struct data_state_val1 *dsv1, u8 parent_subtype);
static inline void dsv_free_data(struct data_state_val *dsv)
{
	switch (dsv_get_section(DSV_TYPE(dsv))) {
	case DSV_SEC_UNK:
	{
		break;
	}
	case DSV_SEC_1:
	{
		free(DSV_SEC1_VAL(dsv));
		DSV_SEC1_VAL(dsv) = NULL;
		break;
	}
	case DSV_SEC_2:
	{
		struct data_state_val_ref *dsvr;
		dsvr = DSV_SEC2_VAL(dsv);
		ds_drop(dsvr->ds);

		free(DSV_SEC2_VAL(dsv));
		DSV_SEC2_VAL(dsv) = NULL;
		break;
	}
	case DSV_SEC_3:
	{
		struct data_state_val1 *tmp, *next;
		slist_for_each_entry_safe(tmp, next, DSV_SEC3_VAL(dsv),
					  sibling) {
			slist_del(&tmp->sibling, DSV_SEC3_VAL(dsv));
			dsv1_free(tmp, dsv->subtype);
		}

		break;
	}
	default:
	{
		BUG();
	}
	}
	DSV_TYPE(dsv) = DSVT_UNK;
	return;
}

static inline
void dsv_alloc_data(struct data_state_val *dsv, u8 val_type,
		    u8 subtype, u32 bytes)
{
	if ((dsv_get_section(val_type) == DSV_SEC_1) &&
	    (DSV_TYPE(dsv) == val_type) &&
	    (dsv->info.v1_info.bytes == bytes)) {
		memset(DSV_SEC1_VAL(dsv), 0, dsv->info.v1_info.bytes);
		return;
	}

	dsv_free_data(dsv);

	switch (dsv_get_section(val_type)) {
	case DSV_SEC_UNK:
	{
		break;
	}
	case DSV_SEC_1:
	{
		BUG_ON(!bytes);
		DSV_SEC1_VAL(dsv) = xmalloc(bytes);
		memset(DSV_SEC1_VAL(dsv), 0, bytes);
		dsv->info.v1_info.bytes = bytes;
		break;
	}
	case DSV_SEC_2:
	{
		bytes = sizeof(struct data_state_val_ref);
		DSV_SEC2_VAL(dsv) = (struct data_state_val_ref *)xmalloc(bytes);
		memset((void *)DSV_SEC2_VAL(dsv), 0, bytes);
		break;
	}
	case DSV_SEC_3:
	{
		break;
	}
	default:
	{
		BUG();
	}
	}

	DSV_TYPE(dsv) = val_type;
	dsv->subtype = subtype;

	return;
}

static inline struct data_state_val *dsv_alloc(void)
{
	struct data_state_val *_new;
	_new = (struct data_state_val *)xmalloc(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline void dsv_free(struct data_state_val *dsv)
{
	dsv_free_data(dsv);
	free(dsv);
}

static inline
struct data_state_val1 *dsv1_alloc(void *raw, s32 offset, u32 bits)
{
	struct data_state_val1 *_new;
	_new = (struct data_state_val1 *)xmalloc(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	dsv_set_raw(&_new->val, raw);
	_new->offset = offset;
	_new->bits = bits;
	return _new;
}

static inline
void dsv1_free(struct data_state_val1 *dsv1, u8 parent_subtype)
{
	dsv_free_data(&dsv1->val);
	free(dsv1);
}

static inline
void __dsv_str_data_add_byte(struct data_state_val *dsv, void *raw,
			     char c, u32 idx)
{
	struct data_state_val1 *tmp;
	u32 this_bytes = sizeof(c);

	tmp = dsv1_alloc(raw, idx * BITS_PER_UNIT, this_bytes * BITS_PER_UNIT);
	dsv_alloc_data(&tmp->val, DSVT_INT_CST, 0, this_bytes);
	clib_memcpy_bits(DSV_SEC1_VAL(&tmp->val), this_bytes * BITS_PER_UNIT,
			 &c, this_bytes * BITS_PER_UNIT);
	slist_add_tail(&tmp->sibling, DSV_SEC3_VAL(dsv));
}

static inline
void dsv_fill_str_data(struct data_state_val *dsv, void *raw,
		       char *str, u32 bytes)
{
	for (u32 i = 0; i < bytes; i++) {
		__dsv_str_data_add_byte(dsv, raw, str[i], i);
	}

	if (bytes && str[bytes-1])
		__dsv_str_data_add_byte(dsv, raw, '\0', bytes);
}

static inline void ds_destroy(struct data_state_rw *ds)
{
	if (!slist_empty(&ds->base.sibling)) {
		WARN();
	}

	dsv_free_data(&ds->val);
	free(ds);
}

static inline struct data_state_rw *ds_hold(struct data_state_rw *ds)
{
	if (!ds)
		return NULL;

	atomic_inc(&ds->refcnt);
	return ds;
}

static inline void ds_drop(struct data_state_rw *ds)
{
	if (!ds)
		return;

	if (atomic_dec_and_test(&ds->refcnt))
		ds_destroy(ds);

	return;
}

static inline
int dsv_find_constructor_elem(struct data_state_val *dsv,
			      s32 offset, u32 bits,
			      struct data_state_val1 **ret_dsv1,
			      struct data_state_val **ret_dsv,
			      struct data_state_val **union_dsv)
{
	*ret_dsv1 = NULL;
	*ret_dsv = NULL;
	*union_dsv = NULL;
	if ((!offset) && (!bits))
		return 0; 

	struct data_state_val *_dsv = dsv;
	int done = 0;
	int err = 0;
	while (1) {
		if ((DSV_TYPE(_dsv) != DSVT_CONSTRUCTOR) ||
		    (!DSV_SEC1_VAL(_dsv))) {
			err = -1;
			break;
		}

		/* record the outer union */
		if ((DSV_TYPE(_dsv) == DSVT_CONSTRUCTOR) &&
		    (_dsv->subtype == DSV_SUBTYPE_UNION)) {
			/* TODO: as of now, we return err on UNION type. */
			*union_dsv = _dsv;
			err = -1;
			break;
		}

		struct data_state_val1 *tmp;
		int has_match = 0;
		slist_for_each_entry(tmp, DSV_SEC3_VAL(_dsv), sibling) {
			/*
			 * [offset, offset+bits] must be a subset of
			 * [tmp->offset, tmp->offset+tmp->bits]
			 *
			 * FIXME: How to find the right field?
			 * for union, it return the first match, which
			 * could be quite a issue.
			 */
			s32 fieldoffset = tmp->offset;
			u32 fieldbits = tmp->bits;
			if (!((offset >= fieldoffset) &&
			      ((offset + bits) <= (fieldoffset + fieldbits))))
				continue;

			has_match = 1;

			if ((DSV_TYPE(&tmp->val) == DSVT_CONSTRUCTOR) &&
			    (tmp->val.subtype == DSV_SUBTYPE_UNION)) {
				/* TODO: has to check UNION_TYPE first */
				_dsv = &tmp->val;
				offset -= fieldoffset;
			} else if ((offset == fieldoffset) &&
				   (bits == fieldbits)) {
				done = 1;
				*ret_dsv1 = tmp;
				*ret_dsv = &tmp->val;
			} else if (DSV_TYPE(&tmp->val) == DSVT_CONSTRUCTOR) {
				_dsv = &tmp->val;
				offset -= fieldoffset;
			} else {
				err = -1;
			}

			break;
		}
		if (!has_match)
			err = -1;

		if (err == -1)
			break;
		if (done)
			break;
	}

	return err;
}

static inline
int __ds_vref_setv(struct data_state_val *t, struct data_state_rw *ds,
		    s32 offset, u32 bits)
{
	if ((DSV_TYPE(t) == DSVT_REF) && (&ds->val == t)) {
		return -1;
	}

	struct data_state_val_ref *dsvr;
	dsvr = DSV_SEC2_VAL(t);
	dsvr->ds = ds;
	dsvr->offset = offset;
	dsvr->bits = bits;
	return 0;
}

static inline
int ds_vref_hold_setv(struct data_state_val *t, struct data_state_rw *ds,
		       s32 offset, u32 bits)
{
	int err = 0;
	err = __ds_vref_setv(t, ds, offset, bits);
	if (!err)
		ds_hold(ds);
	return err;
}

static inline
int ds_vref_setv(struct data_state_val *t, struct data_state_rw *ds,
		  s32 offset, u32 bits)
{
	int err = 0;
	switch (DS_VTYPE(ds)) {
	case DSVT_REF:
	{
		struct data_state_val_ref *dsvr;
		dsvr = DS_SEC2_VAL(ds);
		err = ds_vref_hold_setv(t, dsvr->ds,
					offset + dsvr->offset,
					bits);
		break;
	}
	default:
	{
		err = ds_vref_hold_setv(t, ds, offset, bits);
		break;
	}
	}

	return err;
}

static inline int dsv_copy_data(struct data_state_val *dst,
				struct data_state_val *src);
static inline
int __dsv_copy_info(struct data_state_val *dst, struct data_state_val *src)
{
	int err = 0;

	/* TODO: trace_id_head */
	memcpy(&dst->info, &src->info, sizeof(src->info));

	return err;
}

static inline
int __dsv_copy_data(struct data_state_val *dst, struct data_state_val *src)
{
	int ret = 0;

	dsv_free_data(dst);

	switch (dsv_get_section(DSV_TYPE(src))) {
	case DSV_SEC_UNK:
	{
		break;
	}
	case DSV_SEC_1:
	{
		size_t sz = src->info.v1_info.bytes;

		dsv_alloc_data(dst, DSV_TYPE(src), 0, sz);
		clib_memcpy_bits(DSV_SEC1_VAL(dst), sz * BITS_PER_UNIT,
				 DSV_SEC1_VAL(src),
				 src->info.v1_info.bytes * BITS_PER_UNIT);
		break;
	}
	case DSV_SEC_2:
	{
		/* FIXME: increase the target ds refcount if DSVT_ADDR */
		dsv_alloc_data(dst, DSV_TYPE(src), 0, 0);
		ret = ds_vref_hold_setv(dst, DSV_SEC2_VAL(src)->ds,
					DSV_SEC2_VAL(src)->offset,
					DSV_SEC2_VAL(src)->bits);
		break;
	}
	case DSV_SEC_3:
	{
		struct data_state_val1 *tmp;
		dsv_alloc_data(dst, DSV_TYPE(src), src->subtype, 0);
		slist_for_each_entry(tmp, DSV_SEC3_VAL(src), sibling) {
			struct data_state_val1 *_tmp;
			void *raw = (void *)(long)tmp->val.raw;
			_tmp = dsv1_alloc(raw, tmp->offset, tmp->bits);
			ret = dsv_copy_data(&_tmp->val, &tmp->val);
			if (ret == -1) {
				dsv1_free(_tmp, src->subtype);
				break;
			}
			slist_add_tail(&_tmp->sibling, DSV_SEC3_VAL(dst));
		}
		break;
	}
	default:
	{
		ret = -1;
		break;
	}
	}

	if (!ret) {
		ret = __dsv_copy_info(dst, src);
		dsv_set_raw(dst, (void *)(long)src->raw);
	}

	return ret;
}

static inline
int dsv_copy_data(struct data_state_val *dst, struct data_state_val *src)
{
	int ret = 0;

	if (dst == src)
		return ret;

	struct data_state_val *_dst = dst;
	if ((DSV_TYPE(_dst) == DSVT_CONSTRUCTOR) &&
			(_dst->subtype == DSV_SUBTYPE_UNION)) {
		/* TODO: we should do the sync here */
	}

	if ((DSV_TYPE(_dst) == DSVT_CONSTRUCTOR) &&
			(DSV_TYPE(src) != DSVT_CONSTRUCTOR)) {
		return -1;
	}

	return __dsv_copy_data(_dst, src);
}

static inline
struct data_state_rw *ds_dup_base(struct data_state_base *base)
{
	struct data_state_rw *_new;
	void *raw = NULL;
	switch (base->ref_type) {
	case DSRT_VN:
	{
		struct var_node *vn;
		vn = (struct var_node *)(long)base->ref_base;
		raw = vn->node;
		break;
	}
	case DSRT_FN:
	{
		struct func_node *fn;
		fn = (struct func_node *)(long)base->ref_base;
		raw = fn->node;
		break;
	}
	case DSRT_RAW:
	{
		raw = (void *)(long)base->ref_base;
		break;
	}
	default:
	{
		/* should not happen here */
		err_dbg(0, "miss %d\n", base->ref_type);
		BUG();
	}
	}
	_new = ds_rw_new(base->ref_base, base->ref_type, raw);
	return _new;
}

static inline
struct data_state_base *__ds_find(struct slist_head *head, u64 addr, u8 type)
{
	struct data_state_base *tmp;
	slist_for_each_entry(tmp, head, sibling) {
		if ((tmp->ref_base == addr) && (tmp->ref_type == type))
			return tmp;
	}

	return NULL;
}

static inline
struct data_state_base *fn_ds_find(struct func_node *fn, u64 addr, u8 type)
{
	return __ds_find(&fn->data_state_list, addr, type);
}

static inline
struct data_state_base *fn_ds_add(struct func_node *fn, u64 addr, u8 type)
{
	struct data_state_base *ret;

	ret = fn_ds_find(fn, addr, type);
	if (ret)
		return ret;

	/* insert into func_node, use src_buf_get to alloc */
	ret = ds_base_new(addr, type);
	slist_add_tail(&ret->sibling, &fn->data_state_list);

	return ret;
}

static inline
struct data_state_rw *fnl_ds_find(struct fn_list *fnl, u64 addr, u8 type)
{
	struct data_state_base *base;
	struct data_state_rw *ret;
	base = __ds_find(&fnl->data_state_list, addr, type);
	if (!base)
		return NULL;

	ret = container_of(base, struct data_state_rw, base);
	ds_hold(ret);
	return ret;
}

static inline
struct data_state_rw *fnl_ds_add(struct fn_list *fnl,
				 u64 addr, u8 type, void *raw)
{
	struct data_state_rw *ret;

	ret = fnl_ds_find(fnl, addr, type);
	if (ret)
		return ret;

	ret = ds_rw_new(addr, type, raw);
	if (slist_add_tail_check(&ret->base.sibling, &fnl->data_state_list)) {
		ds_drop(ret);
		return NULL;
	} else {
		ds_hold(ret);
	}

	return ret;
}

static inline
struct data_state_base *__global_ds_base_find(u64 addr, u8 type)
{
	return __ds_find(&si->global_data_states, addr, type);
}

static inline
struct data_state_base *global_ds_base_find(u64 addr, u8 type)
{
	struct data_state_base *ret;
	si_lock_r();
	ret = __global_ds_base_find(addr, type);
	si_unlock_r();
	return ret;
}

static inline
struct data_state_base *global_ds_base_add(u64 addr, u8 type)
{
	struct data_state_base *ret;

	si_lock_w();
	ret = __global_ds_base_find(addr, type);
	if (!ret) {
		/* likewise */
		ret = ds_base_new(addr, type);
		slist_add_tail(&ret->sibling, &si->global_data_states);
	}
	si_unlock_w();

	return ret;
}

static inline void init_global_ds_rw(void)
{
	struct data_state_base *base;
	slist_for_each_entry(base, &si->global_data_states, sibling) {
		struct data_state_rw *tmp;
		tmp = ds_dup_base(base);
		slist_add_tail(&tmp->base.sibling, &si->global_data_rw_states);
	}
}

static inline
struct data_state_rw *__global_ds_rw_find(u64 addr, u8 type)
{
	struct data_state_base *base;
	struct data_state_rw *ret;
	base = __ds_find(&si->global_data_rw_states, addr, type);
	if (!base)
		return NULL;

	ret = container_of(base, struct data_state_rw, base);
	ds_hold(ret);
	return ret;
}

static inline
struct data_state_rw *global_ds_rw_find(u64 addr, u8 type)
{
	struct data_state_rw *ret;
	si_lock_w();
	if (unlikely(slist_empty(&si->global_data_rw_states)))
		init_global_ds_rw();
	ret = __global_ds_rw_find(addr, type);
	si_unlock_w();

	return ret;
}

static inline
struct data_state_rw *global_ds_rw_add(u64 addr, u8 type, void *raw)
{
	struct data_state_rw *ret;

	si_lock_w();
	if (unlikely(slist_empty(&si->global_data_rw_states)))
		init_global_ds_rw();
	ret = __global_ds_rw_find(addr, type);
	if (!ret) {
		ret = ds_rw_new(addr, type, raw);
		if (slist_add_tail_check(&ret->base.sibling,
					 &si->global_data_rw_states)) {
			ds_drop(ret);
			ret = NULL;
		} else {
			ds_hold(ret);
		}
	}
	si_unlock_w();

	return ret;
}

/*
 * @data_state_find: search for data_state_*
 * 1st, search in the global data_state_*
 * 2nd, Optional, search in the sample set allocated_data_states
 * 3rd, search in the local func_node
 */
static inline
struct data_state_base *ds_base_find(struct sample_set *sset, int idx,
				     struct func_node *fn, u64 addr, u8 type)
{
	struct data_state_base *ret;

	si_lock_r();
	ret = __global_ds_base_find(addr, type);
	si_unlock_r();

	if (ret)
		return ret;

	if (fn)
		ret = fn_ds_find(fn, addr, type);

	return ret;
}

static inline
struct data_state_rw *ds_rw_find(struct sample_set *sset, int idx,
				 struct fn_list *fnl, u64 addr, u8 type)
{
	struct data_state_rw *ret;

	ret = global_ds_rw_find(addr, type);
	if (ret)
		return ret;

	if (sset) {
		struct data_state_base *base;
		base = __ds_find(&sset->allocated_data_states,
					 addr, type);
		if (base) {
			ret = container_of(base, struct data_state_rw, base);
			ds_hold(ret);
			return ret;
		}
	}

	if (fnl)
		ret = fnl_ds_find(fnl, addr, type);

	return ret;
}
