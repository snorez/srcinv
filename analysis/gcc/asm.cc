/*
 * For now, we only handle call instructions for asm files
 *
 * TODO:
 *	parse sysenter/int for userspace program
 *	variables xrefs
 *	indirect calls, FF /2 and FF /3
 *
 * CALL instruction: (https://www.felixcloutier.com/x86/call)
 *	(https://blog.csdn.net/heron804/article/details/7366898)
 *	opcode: E8 FF 9A
 *	disp: offset to the %rip
 *
 * ELF: https://refspecs.linuxfoundation.org/elf/x86_64-abi-0.99.pdf
 *	https://refspecs.linuxfoundation.org/elf/elf.pdf
 *
 * ModR/M: (https://wiki.osdev.org/X86-64_Instruction_Encoding#ModR.2FM)
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

#include "si_gcc.h"
#include "../analysis.h"

#define CS_ARCH_X86	3
#define	CS_MODE_64	(1 << 3)
#define	CS_MODE_32	(1 << 2)
#include <capstone/x86.h>

/* copy from codecov */
enum jxx_1 {
	JO_OPC0		= 0x70,	//Jbs
	JNO_OPC0,		//Jbs
	JB_OPC0,		//Jbs
	JNB_OPC0,		//Jbs
	JE_OPC0,		//Jbs
	JNE_OPC0,		//Jbs
	JBE_OPC0,		//Jbs
	JA_OPC0,		//Jbs
	JS_OPC0,		//Jbs
	JNS_OPC0,		//Jbs
	JPE_OPC0,		//Jbs
	JPO_OPC0,		//Jbs
	JL_OPC0,		//Jbs
	JGE_OPC0,		//Jbs
	JLE_OPC0,		//Jbs
	JG_OPC0		= 0x7f,	//Jbs
	JCXZ_OPC	= 0xe3,	//Jbs jump if ecx/rcx = 0
	//JMP_OPC	= 0xe9,	//Jvds
	//JMPS_OPC	= 0xeb,	//Jvds
	//JMPF_OPC	= 0xea, //invalid in 64-bit mode
};

enum jxx_2 {
	TWO_OPC		= 0x0f,
	JO_OPC1		= 0x80,	//Jvds
	JNO_OPC1,		//Jvds
	JB_OPC1,		//Jvds
	JNB_OPC1,		//Jvds
	JE_OPC1,		//Jvds
	JNE_OPC1,		//Jvds
	JBE_OPC1,		//Jvds
	JA_OPC1,		//Jvds
	JS_OPC1,		//Jvds
	JNS_OPC1,		//Jvds
	JPE_OPC1,		//Jvds
	JPO_OPC1,		//Jvds
	JL_OPC1,		//Jvds
	JGE_OPC1,		//Jvds
	JLE_OPC1,		//Jvds
	JG_OPC1		= 0x8f,	//Jvds
};

static int parse(struct sibuf *, int);
static int dec(struct sample_state *, struct code_path *);
static struct lang_ops ops;

CLIB_MODULE_NAME(asm);
CLIB_MODULE_NEEDED0();

CLIB_MODULE_INIT()
{
	ops.parse = parse;
	ops.dec = dec;
	ops.type.binary = SI_TYPE_SRC;
	ops.type.kernel = SI_TYPE_BOTH;
	ops.type.os_type = SI_TYPE_OS_LINUX;
	ops.type.data_fmt = SI_TYPE_DF_ASM;
	register_lang_ops(&ops);
	return 0;
}

CLIB_MODULE_EXIT()
{
	unregister_lang_ops(&ops);
	return;
}

static __thread size_t obj_done, obj_cnt;
static __thread struct sibuf *cur_sibuf;
static __thread void *cur_ctx;
static __thread char *cur_srcfile;
static __thread struct sinode *cur_sn;
static __thread struct code_path *cur_cp;

static void getbase(void)
{
	CLIB_DBG_FUNC_ENTER();

	struct sinode *sn_new = NULL, *sn_tmp = NULL, *loc_file = NULL;
	enum sinode_type type;
	int err = 0;
	elf_file *ef;
	struct list_head syms_head;
	INIT_LIST_HEAD(&syms_head);

	loc_file = analysis__sinode_search(TYPE_FILE, SEARCH_BY_SPEC,
						(void *)cur_srcfile);
	if (loc_file) {
		si_log1_emer("Same srcfile, not supposed\n");
		goto out;
	}
	loc_file = analysis__sinode_new(TYPE_FILE, cur_srcfile,
					strlen(cur_srcfile)+1,
					NULL, 0);
	BUG_ON(analysis__sinode_insert(loc_file, SINODE_INSERT_BH_NONE));

	ef = elf_parse_data(cur_ctx);
	if (!ef) {
		si_log1_emer("elf_parse_data err\n");
		goto out;
	}

	err = elf_get_syms(ef, &syms_head);
	if (err == -1) {
		si_log1_emer("elf_get_syms err\n");
		goto ef_out;
	}

	struct _elf_sym_full *tmp;
	list_for_each_entry(tmp, &syms_head, sibling) {
		int behavior = SINODE_INSERT_BH_NONE;
		get_sym_detail(ef, tmp);

		if ((tmp->type == STT_FUNC) && (tmp->bind == STB_LOCAL))
			type = TYPE_FUNC_STATIC;
		else if ((tmp->type == STT_FUNC) &&
			 ((tmp->bind == STB_GLOBAL) ||
			  (tmp->bind == STB_WEAK)))
			type = TYPE_FUNC_GLOBAL;
		else if ((tmp->type == STT_OBJECT) &&
			 (tmp->bind == STB_LOCAL))
			type = TYPE_VAR_STATIC;
		else if ((tmp->type == STT_OBJECT) &&
			 ((tmp->bind == STB_GLOBAL) ||
			  (tmp->bind == STB_WEAK)))
			type = TYPE_VAR_GLOBAL;
		else if ((tmp->type == STT_NOTYPE) &&
			 (tmp->name) &&
			 (tmp->load_addr)) {
			/* detect which section this sym in */
			if (is_section_text(ef, tmp)) {
				if ((tmp->bind == STB_GLOBAL) ||
					(tmp->bind == STB_WEAK))
					type = TYPE_FUNC_GLOBAL;
				else if (tmp->bind == STB_LOCAL)
					type = TYPE_FUNC_STATIC;
				else
					continue;
			} else if (is_section_data(ef, tmp)) {
				if ((tmp->bind == STB_GLOBAL) ||
					(tmp->bind == STB_WEAK))
					type = TYPE_VAR_GLOBAL;
				else if (tmp->bind == STB_LOCAL)
					type = TYPE_VAR_STATIC;
				else
					continue;
			} else {
				continue;
			}
		} else
			continue;

		mutex_lock(&getbase_lock);
		if ((type == TYPE_FUNC_GLOBAL) ||
			(type == TYPE_VAR_GLOBAL)) {
			long args[3];
			args[0] = (long)cur_sibuf->rf;
			args[1] = (long)get_builtin_resfile();
			args[2] = (long)tmp->name;
			sn_tmp = analysis__sinode_search(type,
							 SEARCH_BY_SPEC,
							 (void *)args);
		} else if ((type == TYPE_FUNC_STATIC) ||
				(type == TYPE_VAR_STATIC)) {
			long args[4];
			args[0] = (long)loc_file;
			args[1] = 0;
			args[2] = 0;
			args[3] = (long)tmp->name;
			sn_tmp = analysis__sinode_search(type,
							 SEARCH_BY_SPEC,
							 (void *)args);
		}

		if (sn_tmp) {
			int weak = (tmp->bind == STB_WEAK);
			if (weak < sn_tmp->weak_flag) {
				behavior = SINODE_INSERT_BH_REPLACE;
			} else {
				mutex_unlock(&getbase_lock);
				continue;
			}
		}

		sn_new = analysis__sinode_new(type, tmp->name,
					      strlen(tmp->name) + 1,
					      NULL, 0);
		BUG_ON(!sn_new);
		sn_new->buf = cur_sibuf;
		sn_new->loc_file = loc_file;

		if ((type == TYPE_VAR_GLOBAL) || (type == TYPE_VAR_STATIC)) {
			struct var_node *vn;
			vn = var_node_new(tmp->load_addr);
			vn->name = sn_new->name;
			vn->size = tmp->size;
			sn_new->data = (char *)vn;
			sn_new->datalen = sizeof(*vn);
		} else if ((type == TYPE_FUNC_GLOBAL) ||
				(type == TYPE_FUNC_STATIC)) {
			struct func_node *fn = NULL;
			fn = func_node_new((void *)tmp->load_addr);
			fn->name = sn_new->name;
			fn->size = tmp->size;
			sn_new->data = (char *)fn;
			sn_new->datalen = sizeof(*fn);
		}
		sn_new->data_fmt = SI_TYPE_DF_ASM;
		sn_new->weak_flag = (tmp->bind == STB_WEAK);

		BUG_ON(analysis__sinode_insert(sn_new, behavior));
		mutex_unlock(&getbase_lock);
	}

	elf_drop_syms(&syms_head);
ef_out:
	elf_cleanup(ef);
out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void get_var_detail(struct sinode *sn)
{
	CLIB_DBG_FUNC_ENTER();

	struct var_node *vn;
	vn = (struct var_node *)sn->data;

	/* TODO: what should we do here? */
	vn->detailed = 1;

	CLIB_DBG_FUNC_EXIT();
	return;
}

static inline void calc_addr_jmp(unsigned long addr, struct insn *insn,
				 unsigned long *next, unsigned long *jmp,
				 int opcs)
{
	*next = addr + insn->length;
	*jmp = 0;
	s32 offset;

	switch (opcs) {
	case 1:
	{
		offset = (s32)(insn->immediate.bytes[0]);
		*jmp = *next + offset;
		break;
	}
	case 2:
	{
		switch (insn->immediate.nbytes) {
		case 1:
		{
			offset = (s32)(insn->immediate.bytes[0]);
			break;
		}
		case 2:
		{
			offset = *(short *)(insn->immediate.bytes);
			break;
		}
		case 4:
		{
			offset = *(int *)(insn->immediate.bytes);
			break;
		}
		default:
		{
			offset = 0;
			break;
		}
		}

		*jmp = *next + offset;
		break;
	}
	default:
	{
		break;
	}
	}
}

static void get_func_detail(struct sinode *sn)
{
	struct func_node *fn;
	fn = (struct func_node *)sn->data;

	struct insn insn;
	unsigned long addr, i = 0;
	unsigned long size = fn->size;
	addr = (unsigned long)fn->node;
	unsigned long next_addr;
	unsigned long addr_jmp;

	if (!size)
		return;

	CLIB_DBG_FUNC_ENTER();

	struct clib_bitmap *bm = NULL;
	unsigned long entries = 1;
	bm = clib_bitmap_create(size);
	clib_bitmap_set(bm, 0);

	i = 0;
	while (i < size) {
		int jmpflag = 0;
		unsigned char this_bytes, opc0, opc1;
		/* FIXME: we just assume that the x86_64 arg is true */
		insn_init(&insn, (void *)(addr+i), MAX_INSN_SIZE, 1);
		insn_get_length(&insn);
		if (!insn.length)
			break;

		this_bytes = insn.opcode.nbytes;
		opc0 = insn.opcode.bytes[0];
		opc1 = insn.opcode.bytes[1];

		if ((this_bytes == 1) &&
			((opc0 == JCXZ_OPC) ||
			 ((opc0 <= JG_OPC0) && (opc0 >= JO_OPC0)))) {
			calc_addr_jmp(addr+i, &insn, &next_addr,
					&addr_jmp, 1);
			jmpflag = 1;
		} else if ((this_bytes == 2) && (opc0 == TWO_OPC) &&
				((opc1 <= JG_OPC1) && (opc1 >= JO_OPC1))) {
			calc_addr_jmp(addr+i, &insn, &next_addr,
					&addr_jmp, 2);
			jmpflag = 1;
		}

		i += insn.length;

		if (!jmpflag)
			continue;

		if ((next_addr > addr) && (next_addr < (addr + size))) {
			if (!clib_bitmap_set(bm, next_addr-addr))
				entries++;
		} else {
			si_log1_todo("next_addr not in func %s\n",
					sn->name);
		}

		if ((addr_jmp > addr) && (addr_jmp < (addr + size))) {
			if (!clib_bitmap_set(bm, addr_jmp-addr))
				entries++;
		} else {
			si_log1_todo("addr_jmp not in func %s\n",
					sn->name);
		}
	}

	struct code_path **__cps;
	__cps = (struct code_path **)
		src_buf_get(entries * sizeof(struct code_path *));
	struct code_path **cps = __cps;
	s64 start = 0;
	for (i = 0; i < entries; i++) {
		cps[i] = code_path_new(fn, 0);
		start = clib_bitmap_nonzero(bm, start);
		BUG_ON(start == -1);
		cps[i]->cp = (void *)(addr + start);
		start += 1;
	}
	clib_bitmap_destroy(bm);

	i = 0;
	unsigned long prev_insn_addr = addr;
	unsigned long cp_idx = 0;
	while (i < size) {
		int jmpflag = 0;
		unsigned char this_bytes, opc0, opc1;
		insn_init(&insn, (void *)(addr+i), MAX_INSN_SIZE, 1);
		insn_get_length(&insn);
		if (!insn.length)
			break;

		this_bytes = insn.opcode.nbytes;
		opc0 = insn.opcode.bytes[0];
		opc1 = insn.opcode.bytes[1];

		if ((this_bytes == 1) &&
			((opc0 == JCXZ_OPC) ||
			 ((opc0 <= JG_OPC0) && (opc0 >= JO_OPC0)))) {
			calc_addr_jmp(addr+i, &insn, &next_addr,
					&addr_jmp, 1);
			jmpflag = 1;
		} else if ((this_bytes == 2) && (opc0 == TWO_OPC) &&
				((opc1 <= JG_OPC1) && (opc1 >= JO_OPC1))) {
			calc_addr_jmp(addr+i, &insn, &next_addr,
					&addr_jmp, 2);
			jmpflag = 1;
		}

		prev_insn_addr = addr + i;
		i += insn.length;

		if (!jmpflag)
			continue;

		if ((next_addr > addr) && (next_addr < (addr + size))) {
			cps[cp_idx]->next[0] = cps[cp_idx+1];
		}

		if ((addr_jmp > addr) && (addr_jmp < (addr + size))) {
			for (unsigned k = 0; k < entries; k++) {
				if (cps[k]->cp != (void *)addr_jmp)
					continue;
				cps[cp_idx]->next[1] = cps[k];
			}
		}
		cps[cp_idx]->cond_head = (void *)prev_insn_addr;
		cp_idx++;
	}
	cps[cp_idx]->cond_head = (void *)prev_insn_addr;

#if 0
	/* do this in phase4 */
	for (i = 0; i < entries; i++) {
		analysis__init_cp_state(SI_TYPE_DF_ASM, cps[i]);
	}
#endif

	fn->cps = __cps;
	fn->cp_cnt = entries;
	fn->detailed = 1;

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void getdetail_match_cb(struct sinode *sn, void *arg)
{
	if (sn->buf != cur_sibuf)
		return;

	CLIB_DBG_FUNC_ENTER();

	enum sinode_type type;
	type = sinode_idtype(sn);

	switch (type) {
	case TYPE_VAR_STATIC:
	case TYPE_VAR_GLOBAL:
	{
		get_var_detail(sn);
		break;
	}
	case TYPE_FUNC_STATIC:
	case TYPE_FUNC_GLOBAL:
	{
		get_func_detail(sn);
		break;
	}
	default:
	{
		si_log1_emer("Not supposed to happen\n");
		break;
	}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void getdetail(void)
{
	CLIB_DBG_FUNC_ENTER();

	/*
	 * We just traverse the sinode, match sinode.buf with cur_sibuf
	 */
	analysis__sinode_match("var_global", getdetail_match_cb, NULL);
	analysis__sinode_match("var_static", getdetail_match_cb, NULL);
	analysis__sinode_match("func_global", getdetail_match_cb, NULL);
	analysis__sinode_match("func_static", getdetail_match_cb, NULL);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void phase4_func(struct sinode *sn)
{
	struct func_node *fn;
	fn = (struct func_node *)sn->data;

	for (int i = 0; i < fn->cp_cnt; i++) {
		cur_cp = fn->cps[i];
		cur_cp->state = cp_state_new();
		cur_cp->state->data_fmt = SI_TYPE_DF_ASM;
		cur_cp->state->status = CSS_EMPTY;
	}
}

static void phase4_match_cb(struct sinode *sn, void *arg)
{
	if (sn->buf != cur_sibuf)
		return;

	enum sinode_type type;
	type = sinode_idtype(sn);

	switch (type) {
	case TYPE_FUNC_STATIC:
	case TYPE_FUNC_GLOBAL:
	{
		phase4_func(sn);
		break;
	}
	default:
	{
		break;
	}
	}

	return;
}

static void phase4(void)
{
	CLIB_DBG_FUNC_ENTER();

	/*
	 * We just traverse the sinode, match sinode.buf with cur_sibuf
	 */
	analysis__sinode_match("var_global", phase4_match_cb, NULL);
	analysis__sinode_match("var_static", phase4_match_cb, NULL);
	analysis__sinode_match("func_global", phase4_match_cb, NULL);
	analysis__sinode_match("func_static", phase4_match_cb, NULL);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static __thread struct sinode *addr2sinode_val;
static void addr2sinode_match(struct sinode *sn, void *arg)
{
	if (sn->data_fmt != SI_TYPE_DF_ASM)
		return;

	CLIB_DBG_FUNC_ENTER();

	struct func_node *fn;
	fn = (struct func_node *)sn->data;

	if (!fn)
		goto out;

	if (fn->node != arg)
		goto out;

	addr2sinode_val = sn;

out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void addr2sinode_rel(char *addr)
{
	int err = 0;
	elf_file *ef;
	struct list_head syms_head;
	INIT_LIST_HEAD(&syms_head);
	struct _elf_sym_full *tmp, *target_sym = NULL;
	unsigned long shdr_off = 0;
	char *shdrname = NULL;
	char *target_symname = NULL;
	long args[3];

	ef = elf_parse_data(cur_ctx);
	if (!ef) {
		si_log1_emer("elf_parse_data err\n");
		return;
	}

	err = elf_get_syms(ef, &syms_head);
	if (err == -1) {
		si_log1_emer("elf_get_syms err\n");
		goto ef_out;
	}

	/* get current text section name first */
	list_for_each_entry(tmp, &syms_head, sibling) {
		get_sym_detail(ef, tmp);

		struct func_node *fn;
		fn = (struct func_node *)cur_sn->data;
		if ((void *)tmp->load_addr != fn->node)
			continue;

		target_sym = tmp;
		break;
	}
	if (!target_sym) {
		si_log1_todo("target_sym not found\n");
		goto syms_out;
	}

	switch (ef->elf_bits) {
	case 32:
	{
		Elf32_Sym *sym = &target_sym->data.sym0;
		Elf32_Shdr *shdr;
		shdr = (Elf32_Shdr *)get_sh_by_id(ef, sym->st_shndx);
		if (shdr) {
			shdrname = (char *)((long)ef->shstrtab +
					    shdr->sh_name);
			shdr_off = shdr->sh_offset;
		}
		break;
	}
	case 64:
	{
		Elf64_Sym *sym = &target_sym->data.sym1;
		Elf64_Shdr *shdr;
		shdr = (Elf64_Shdr *)get_sh_by_id(ef, sym->st_shndx);
		if (shdr) {
			shdrname = (char *)((long)ef->shstrtab +
					    shdr->sh_name);
			shdr_off = shdr->sh_offset;
		}
		break;
	}
	default:
	{
		si_log1_todo("miss elf_bits: %d\n", ef->elf_bits);
		break;
	}
	}
	if (!shdrname) {
		si_log1_todo("section header name not found\n");
		goto syms_out;
	}

	/*
	 * XXX: .rel or .rela?
	 * https://stevens.netmeister.org/631/elf.html
	 * platform-dependent. For x86_32, it is .rel and for x86_64, .rela
	 */
	for (int i = 0; i < 2; i++) {
		char rel_shdr_name[strlen(shdrname) + 6];
		if (!i)
			sprintf(rel_shdr_name, "%s%s", ".rel", shdrname);
		else
			sprintf(rel_shdr_name, "%s%s", ".rela", shdrname);

		unsigned long off = addr - (char *)cur_ctx - shdr_off;
		target_symname = get_relentname_by_offset(ef,
							  rel_shdr_name,
							  i,
							  off);
		if (target_symname)
			break;
	}

	if (!target_symname) {
		si_log1_todo("target symbol name not found\n");
		goto syms_out;
	}

	args[0] = (long)cur_sibuf->rf;
	args[1] = (long)get_builtin_resfile();
	args[2] = (long)target_symname;
	addr2sinode_val = analysis__sinode_search(TYPE_FUNC_GLOBAL,
						  SEARCH_BY_SPEC,
						  (void *)args);

syms_out:
	elf_drop_syms(&syms_head);
ef_out:
	elf_cleanup(ef);
	return;
}

static struct sinode *addr2sinode(char *addr, int flag)
{
	CLIB_DBG_FUNC_ENTER();
	addr2sinode_val = NULL;

	switch (flag) {
	case 0:
	{
		analysis__sinode_match("func_global", addr2sinode_match,
				       addr);
		if (addr2sinode_val)
			break;

		analysis__sinode_match("func_static", addr2sinode_match,
				       addr);
		break;
	}
	case 1:
	{
		/* check the .rel/.rela sections */
		addr2sinode_rel(addr);
		break;
	}
	default:
		BUG();
	}

	CLIB_DBG_FUNC_EXIT();
	return addr2sinode_val;
}

static void indcfg1_do_call(char *ip, char *opcodes, int len)
{
	CLIB_DBG_FUNC_ENTER();

	char *next_ip = (char *)ip + len;
	unsigned value = 0;
	struct sinode *target_sn = NULL;

	if (len == 3)
		value = *(unsigned short *)&opcodes[1];
	else if (len == 5)
		value = *(unsigned *)&opcodes[1];
	else if ((unsigned char)opcodes[0] == 0xff) {
		;
	} else {
		si_log1_todo("Call(%x) ins length(%d) not right? in %s\n",
				opcodes[0], len, cur_sn->name);
		goto out;
	}

	switch ((unsigned char)opcodes[0]) {
	case 0xe8:
	{
		if (value)
			target_sn = addr2sinode(next_ip + value, 0);
		else
			target_sn = addr2sinode(ip + 1, 1);
		break;
	}
	case 0x9a:
	{
		if (value)
			target_sn = addr2sinode((char *)(long)value, 0);
		else
			target_sn = addr2sinode(ip + 1, 1);

		break;
	}
	case 0xff:
	{
		/*
		 * XXX: check intel manual
		 * 2.1.5: Addressing-Mode Encoding of ModR/M and SIB Bytes
		 */
		unsigned char modrm = opcodes[1];
		unsigned char digit = (modrm >> 3) & 7;
		unsigned char mod = (modrm >> 6) & 3;
		unsigned char rm = (modrm & 0x7);
		if ((digit != 2) && (digit != 3)) {
			si_log1_todo("Call is not FF /2 or FF /3\n");
			break;
		}
		if ((len == 6) && (mod == 0) && (rm == 5)) {
			/* disp32 */
			value = *(unsigned *)&opcodes[2];
			si_log1_todo("Call(FF) disp32: %x in %s\n",
					value, cur_sn->name);
		} else if ((len == 4) && (mod == 0) && (rm == 6)) {
			/* disp16 */
			value = *(unsigned short *)&opcodes[2];
			si_log1_todo("Call(FF) disp16: %x in %s\n",
					value, cur_sn->name);
		} else {
			si_log1_todo("Call(FF) with mod(%x) R/M(%x) in %s\n",
					mod, rm, cur_sn->name);
		}
		break;
	}
	default:
		si_log1_todo("Unknown call opcode(%x) in %s\n",
				opcodes[0], cur_sn->name);
		break;
	}

	if (!target_sn) {
		si_log1_todo("target called sinode not found in %s\n",
				cur_sn->name);
		goto out;
	}

	analysis__add_callee(cur_sn, target_sn, ip, SI_TYPE_DF_ASM);
	analysis__add_caller(target_sn, cur_sn, NULL);

out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void indcfg1_match_cb(struct sinode *sn, void *arg)
{
	if (sn->buf != cur_sibuf)
		return;

	CLIB_DBG_FUNC_ENTER();

	cur_sn = sn;

	struct func_node *fn;
	fn = (struct func_node *)sn->data;

	/* XXX: we get the direct/indirect calls here */
	int bits = elf_bits((char *)cur_ctx);

	/*
	 * disas the function
	 * look for call instruction,
	 * decode it
	 * get the callee function and its sinode
	 * add_caller/add_callee
	 */
	char *addr = (char *)fn->node;
	while (addr < ((char *)fn->node + fn->size)) {
		char buf[X86_X64_OPCODE_MAXLEN];
		unsigned int opcode;
		int bytes = disas_next(CS_ARCH_X86,
					bits == 32 ? CS_MODE_32 : CS_MODE_64,
					addr,
					buf,
					X86_X64_OPCODE_MAXLEN,
					&opcode);
		if (bytes == -1) {
			si_log1_emer("disas_next err in %s\n", sn->name);
			goto out;
		}

		if (bytes > X86_X64_OPCODE_MAXLEN) {
			si_log1_emer("buffer too small, need %d\n", bytes);
			goto out;
		}

		if (opcode == X86_INS_CALL) {
			indcfg1_do_call(addr, buf, bytes);
		}

		addr += bytes;
	}

out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void getindcfg1(void)
{
	CLIB_DBG_FUNC_ENTER();

	/*
	 * We just traverse the sinode, match sinode.buf with cur_sibuf
	 */
	analysis__sinode_match("var_global", indcfg1_match_cb, NULL);
	analysis__sinode_match("var_static", indcfg1_match_cb, NULL);
	analysis__sinode_match("func_global", indcfg1_match_cb, NULL);
	analysis__sinode_match("func_static", indcfg1_match_cb, NULL);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void indcfg2_match_cb(struct sinode *sn, void *arg)
{
	if (sn->buf != cur_sibuf)
		return;

	/* TODO */
}

static void getindcfg2(void)
{
	CLIB_DBG_FUNC_ENTER();

	/*
	 * We just traverse the sinode, match sinode.buf with cur_sibuf
	 */
	analysis__sinode_match("var_global", indcfg2_match_cb, NULL);
	analysis__sinode_match("var_static", indcfg2_match_cb, NULL);
	analysis__sinode_match("func_global", indcfg2_match_cb, NULL);
	analysis__sinode_match("func_static", indcfg2_match_cb, NULL);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static __thread long show_progress_arg[0x10];
static int show_progress_timeout = 3;
static void this_show_progress(int signo, siginfo_t *si, void *arg, int last)
{
	long *args = (long *)arg;
	char *parsing_file = (char *)args[1];
	long cur = *(long *)args[2];
	long goal = *(long *)args[3];
	pthread_t id = (pthread_t)args[4];

	long percent = (cur) * 100 / (goal);
	mt_print1(id, "0x%lx %s (%ld%%)\n", id, parsing_file, percent);
}

static int gcc_ver_major = __GNUC__;
static int gcc_ver_minor = __GNUC_MINOR__;
static int parse(struct sibuf *buf, int parse_mode)
{
	CLIB_DBG_FUNC_ENTER();

	struct file_content *fc = (struct file_content *)buf->load_addr;
	if ((fc->gcc_ver_major != gcc_ver_major) ||
		(fc->gcc_ver_minor > gcc_ver_minor)) {
		si_log1_emer("gcc version not match, need %d.%d\n",
				fc->gcc_ver_major, fc->gcc_ver_minor);
		CLIB_DBG_FUNC_EXIT();
		return -1;
	}

	cur_sibuf = buf;
	cur_ctx = fc_pldptr(fc);
	cur_srcfile = fc->path;
	obj_done = 0;
	obj_cnt = 1;

	char *sp_path = fc->path + fc->srcroot_len;

	switch (parse_mode) {
	case MODE_ADJUST:
	{
		show_progress_arg[0] = (long)"ADJUST";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_done;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, this_show_progress,
				show_progress_arg, 0, 1);

		sleep(1);
		obj_done = 1;

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETBASE:
	{
		show_progress_arg[0] = (long)"GETBASE";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_done;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, this_show_progress,
				show_progress_arg, 0, 1);

		getbase();
		obj_done = 1;

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETDETAIL:
	{
		show_progress_arg[0] = (long)"GETDETAIL";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_done;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, this_show_progress,
				show_progress_arg, 0, 1);

		getdetail();
		obj_done = 1;

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETSTEP4:
	{
		show_progress_arg[0] = (long)"GETSTEP4";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_done;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, this_show_progress,
				show_progress_arg, 0, 1);

		phase4();
		obj_done = 1;

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETINDCFG1:
	{
		show_progress_arg[0] = (long)"GETINDCFG1";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_done;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, this_show_progress,
				show_progress_arg, 0, 1);

		getindcfg1();
		obj_done = 1;

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETINDCFG2:
	{
		show_progress_arg[0] = (long)"GETINDCFG2";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_done;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, this_show_progress,
				show_progress_arg, 0, 1);

		getindcfg2();
		obj_done = 1;

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	default:
	{
		BUG();
	}
	}

	CLIB_DBG_FUNC_EXIT();
	return 0;
}

static int dec(struct sample_state *, struct code_path *)
{
	/* TODO */
	return 0;
}
