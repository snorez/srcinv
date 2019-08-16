#ifndef HACKING_H_IEAV6TPU
#define HACKING_H_IEAV6TPU

#include "si_core.h"

DECL_BEGIN

C_SYM struct list_head hacking_module_head;
enum hacking_flag {
	HACKING_FLAG_NONE,
	HACKING_FLAG_STATIC,
	HACKING_FLAG_FUZZ,
	HACKING_FLAG_OTHER,
};

/*
 * handle all hacking modules, fuzz type module are at last
 */
struct hacking_module {
	struct list_head	sibling;
	char			*name;
	enum hacking_flag	flag;
	void			(*callback)(void);
};

static inline struct hacking_module *hacking_module_find(struct list_head *h,
							 struct hacking_module *m)
{
	struct hacking_module *tmp;
	list_for_each_entry(tmp, h, sibling) {
		if (tmp->callback == m->callback)
			return tmp;
	}
	return NULL;
}

static inline void register_hacking_module(struct hacking_module *m)
{
	struct list_head *h = &hacking_module_head;
	if (hacking_module_find(h, m))
		return;
	if (m->flag == HACKING_FLAG_FUZZ)
		list_add_tail(&m->sibling, h);
	else
		list_add(&m->sibling, h);
}

static inline void unregister_hacking_module(struct hacking_module *m)
{
	list_del_init(&m->sibling);
}

DECL_END

#endif /* end of include guard: HACKING_H_IEAV6TPU */
