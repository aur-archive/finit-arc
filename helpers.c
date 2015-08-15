
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef BUILTIN_RUNPARTS
#include <dirent.h>
#include <stdarg.h>
#include <sys/wait.h>
#endif

#include <grp.h>

#include "helpers.h"

/*
 * Helpers to replace system() calls
 */

int makepath(char *p)
{
	char *x, path[PATH_MAX];
	int ret;
	
	x = path;

	do {
		do { *x++ = *p++; } while (*p && *p != '/');
		*x = 0;
		ret = mkdir(path, 0777);
	} while (*p && (*p != '/' || *(p + 1))); /* ignore trailing slash */

	return ret;
}

#define BUF_SIZE 4096

void copyfile(char *src, char *dst, int size)
{
	char buffer[BUF_SIZE];
	int s, d, n;

	/* Size == 0 means copy entire file */
	if (size == 0)
		size = INT_MAX;

	if ((s = open(src, O_RDONLY)) >= 0) {
		if ((d = open(dst, O_WRONLY | O_CREAT, 0644)) >= 0) {
			do {
				int csize = size > BUF_SIZE ?  BUF_SIZE : size;
		
				if ((n = read(s, buffer, csize)) > 0)
					write(d, buffer, n);
				size -= csize;
			} while (size > 0 && n == BUF_SIZE);
			close(d);
		}
		close(s);
	}
}

#ifdef BUILTIN_RUNPARTS

#define NUM_SCRIPTS 128		/* ought to be enough for anyone */
#define NUM_ARGS 16

static int cmp(const void *s1, const void *s2)
{
	return strcmp(*(char **)s1, *(char **)s2);
}

int run_parts(char *dir, ...)
{
	DIR *d;
	struct dirent *e;
	struct stat st;
	char *ent[NUM_SCRIPTS];
	int i, num = 0, argnum = 1;
	char *args[NUM_ARGS];
	va_list ap;

	if (chdir(dir))
		return -1;

	if ((d = opendir(dir)) == NULL)
		return -1;

	va_start(ap, dir);
	while (argnum < NUM_ARGS && (args[argnum++] = va_arg(ap, char *)));
	va_end(ap);

	while ((e = readdir(d))) {
		if (e->d_type == DT_REG && stat(e->d_name, &st) == 0) {
			if (st.st_mode & S_IXUSR) {
				ent[num++] = strdup(e->d_name);
				if (num >= NUM_SCRIPTS)
					break;
			}
		}
	}

	closedir(d);

	if (num == 0)
		return 0;

	qsort(ent, num, sizeof(char *), cmp);

	for (i = 0; i < num; i++) {
		args[0] = ent[i];
		if (!fork()) {
			execv(ent[i], args);
			exit(0);
		}
		wait(NULL);
		free(ent[i]);
	}

	return 0;
}

int getgroup(char *s)
{
	struct group *grp;

	if ((grp = getgrnam(s)) == NULL)
		return -1;

	return grp->gr_gid;
}

#endif
