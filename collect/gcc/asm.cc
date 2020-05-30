/*
 * collect .S .s .asm files information
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

static struct plugin_info this_plugin_info = {
	.version = "0.1",
	.help = "collect the .s .S .asm file information",
};
static char *plugin_name = NULL;

static int gcc_ver = __GNUC__;
static int gcc_ver_minor = __GNUC_MINOR__;
static char ver[16];
/* init in plugin_init */
struct plugin_gcc_version version_needed = {
	.basever = "",
};

int plugin_is_GPL_compatible;

static char nodes_mem[MAX_SIZE_PER_FILE];
static char *mem_ptr_start = NULL;
static char *mem_ptr = NULL;
static struct file_content *write_ctx;

static const char *outpath = "/tmp/c_ast";
static int is_kernel = 0;
static int outfd = -1;
static int prepare_outfile(void)
{
	outfd = open(outpath, O_WRONLY | O_APPEND | O_CREAT,// | O_TRUNC,
			S_IRUSR | S_IWUSR);
	if (outfd == -1)
		return -1;

	return 0;
}

static int get_compiling_args(void)
{
	/*
	 * UPDATE:
	 * for file_content, the kernel can not be SI_TYPE_BOTH
	 */
	write_ctx = (struct file_content *)nodes_mem;
	write_ctx->type.binary = SI_TYPE_SRC;
	if (is_kernel)
		write_ctx->type.kernel = SI_TYPE_KERN;
	else
		write_ctx->type.kernel = SI_TYPE_USER;
	write_ctx->type.os_type = SI_TYPE_OS_LINUX;
	write_ctx->type.type_more = SI_TYPE_MORE_GCC_ASM;
	write_ctx->gcc_ver_major = gcc_ver;
	write_ctx->gcc_ver_minor = gcc_ver_minor;

	char *ret = getcwd(write_ctx->path, PATH_MAX);
	if (!ret) {
		fprintf(stderr, "getcwd err\n");
		return -1;
	}

	char *filepath = write_ctx->path;
	size_t dir_len = strlen(filepath);
	size_t file_len = strlen(main_input_filename);

	if ((dir_len + file_len + 1) >= PATH_MAX) {
		fprintf(stderr, "path too long\n");
		return -1;
	}

	if (filepath[dir_len-1] != '/')
		filepath[dir_len] = '/';
	write_ctx->srcroot_len = strlen(filepath);

	/* XXX, the compiling file */
	memcpy(filepath+strlen(filepath), main_input_filename,
			strlen(main_input_filename)+1);
	write_ctx->path_len = strlen(filepath) + 1;

	/* XXX, the compiling args */
	char *ptr = getenv("COLLECT_GCC_OPTIONS");
	if (!ptr)
		write_ctx->cmd_len = 0;
	else {
		/* XXX, drop -fplugin args */
		char *cmd;
		cmd = (char *)fc_cmdptr((void *)nodes_mem);
		char *cur = ptr;
		char *end;
		while (1) {
			if (!*cur)
				break;

			cur = strchr(cur, '\'');
			if (!cur)
				break;
			if (!strncmp(cur+1, "-fplugin", 8)) {
				end = strchr(cur+1, '\'');
				cur = end+1;
				continue;
			}
			end = strchr(cur+1, '\'');
			memcpy(cmd, cur, end+1-cur);
			memcpy(cmd+(end+1-cur), " ", 1);

			cmd += end+1-cur+1;
			write_ctx->cmd_len += end+1-cur+1;
			cur = end + 1;
		}
		write_ctx->cmd_len += 1;	/* the last null byte */
	}
	mem_ptr_start = (char *)fc_pldptr((void *)nodes_mem);
	mem_ptr = mem_ptr_start;
	return 0;
}

static void flush(int fd)
{
	if (mem_ptr == mem_ptr_start)
		return;

	write_ctx->objs_offs = mem_ptr - nodes_mem;
	write_ctx->objs_cnt = 0;
	write_ctx->total_size = mem_ptr - nodes_mem;
	write_ctx->total_size = clib_round_up(write_ctx->total_size, PAGE_SIZE);

	int err = write(fd, nodes_mem, write_ctx->total_size);
	if (unlikely(err == -1))
		BUG();
}

static char objfile[PATH_MAX];
static void finish(void *gcc_data, void *user_data)
{
	/*
	 * the objfile may not exists at the moment, so
	 * we use a child process to wait for the objfile
	 */
	int pid;
	if ((pid = fork()) < 0) {
		fprintf(stderr, "fork err\n");
		goto out;
	} else if (pid == 0) {
		int fd = -1;
		int maxtry = 9000;	/* total time: 9s */
		while (fd == -1) {
			if (!maxtry)
				break;
			fd = open(objfile, O_RDONLY);
			usleep(1000);
			maxtry--;
		}
		if (fd == -1) {
			fprintf(stderr, "Open %s err\n", objfile);
			close(outfd);
			exit(0);
		}

		struct stat st;
		int err = fstat(fd, &st);
		if (err == -1) {
			fprintf(stderr, "fstat err\n");
			close(fd);
			close(outfd);
			exit(0);
		}

		size_t len = st.st_size;
		if ((mem_ptr + len) > ((char *)nodes_mem + sizeof(nodes_mem)))
			BUG();
		ssize_t rlen = read(fd, mem_ptr, len);
		if (rlen != (ssize_t)len) {
			fprintf(stderr, "read err, %ld bytes read\n", rlen);
			close(fd);
			close(outfd);
			exit(0);
		}

		mem_ptr += len;
		close(fd);

		flush(outfd);
		fprintf(stderr, "%s: child[%d] write res done\n", plugin_name,
				getpid());
		exit(0);
	}

	fprintf(stderr, "%s: child[%d] wait to write res\n", plugin_name, pid);
out:
	close(outfd);
	return;
}

int plugin_init(struct plugin_name_args *plugin_info,
		struct plugin_gcc_version *version)
{
	int err = 0;

	/* XXX: match the current gcc version with version_needed */
	memset(ver, 0, sizeof(ver));
	snprintf(ver, sizeof(ver), "%d.%d", gcc_ver, gcc_ver_minor);
	version_needed.basever = ver;
	if (strncmp(version->basever, version_needed.basever,
				strlen(version_needed.basever))) {
		fprintf(stderr, "version not match: needed(%s) given(%s)\n",
				version_needed.basever,
				version->basever);
		return -1;
	}

	plugin_name = plugin_info->base_name;

	if (plugin_info->argc != 2) {
		fprintf(stderr, "plugin %s usage:\n"
				" -fplugin=.../%s.so"
				" -fplugin-arg-%s-outpath=..."
				" -fplugin-arg-%s-kernel=[0|1]\n",
				plugin_name,
				plugin_name,
				plugin_name,
				plugin_name);
		return -1;
	}
	outpath = plugin_info->argv[0].value;
	is_kernel = !!atoi(plugin_info->argv[1].value);

	const char *file = main_input_filename;
	/* this is for .s .S .asm detection */
	if (strlen(file) > 4) {
		if (strcmp(file+strlen(file)-4, ".asm") &&
			strcmp(file+strlen(file)-2, ".s") &&
			strcmp(file+strlen(file)-2, ".S"))
			return 0;
	} else if (strlen(file) > 2) {
		if (strcmp(file+strlen(file)-2, ".s") &&
			strcmp(file+strlen(file)-2, ".S"))
			return 0;
	}
	char *last_dot_pos = strrchr((char *)file, '.');
	BUG_ON(!last_dot_pos);
	memcpy(objfile, file, last_dot_pos - file);
	memcpy(objfile + (last_dot_pos - file), ".o", 3);

	err = get_compiling_args();
	if (err) {
		fprintf(stderr, "get_compiling_args err\n");
		return -1;
	}

	/* kicking out some folders */
	const char *exclude_folders[] = {
		"scripts/", "Documentation/", "debian/", "debian.master/",
		"tools/", "samples/", "spl/", "usr/",
	};
	unsigned int i = 0;
	for (i = 0; i < sizeof(exclude_folders) / sizeof(char *); i++) {
		if (strstr(write_ctx->path, exclude_folders[i]))
			return 0;
	}

	err = prepare_outfile();
	if (err) {
		fprintf(stderr, "prepare_outfile err\n");
		return -1;
	}

	register_callback(plugin_name, PLUGIN_INFO, NULL,
				&this_plugin_info);

	/* collect data, flush data to file */
	register_callback(plugin_name, PLUGIN_FINISH, finish, NULL);

	return 0;
}
