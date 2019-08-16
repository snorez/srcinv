#ifndef ANALYSIS_H_AJXNFAL9
#define ANALYSIS_H_AJXNFAL9

#include "si_core.h"

DECL_BEGIN

#define	STEP1			MODE_ADJUST
#define	STEP2			MODE_GETBASE
#define	STEP3			MODE_GETDETAIL
#define	STEP4			MODE_GETXREFS
#define	STEP5			MODE_GETINDCFG1
#define	STEP6			MODE_GETINDCFG2
#define	STEPMAX			MODE_MAX

C_SYM struct list_head analysis_lang_ops_head;

struct lang_ops {
	struct list_head	sibling;
	struct si_type		type;
	int			(*callback)(struct sibuf *buf, int parse_mode);
};

static inline struct lang_ops *lang_ops_find(struct list_head *h,struct si_type *type)
{
	struct lang_ops *tmp;
	list_for_each_entry(tmp, h, sibling) {
		if (si_type_match(&tmp->type, type))
			return tmp;
	}
	return NULL;
}

static inline void register_lang_ops(struct lang_ops *ops)
{
	struct list_head *h = &analysis_lang_ops_head;
	if (lang_ops_find(h, &ops->type))
		return;
	list_add_tail(&ops->sibling, h);
}

static inline void unregister_lang_ops(struct lang_ops *ops)
{
	list_del_init(&ops->sibling);
}

DECL_END

#endif /* end of include guard: ANALYSIS_H_AJXNFAL9 */
