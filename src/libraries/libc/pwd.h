#ifndef _LIBC_PWD_H
#define _LIBC_PWD_H

#include <sys/types.h>

struct passwd
{
	char *pw_name;	 // User's login name.
	uid_t pw_uid;	 // Numerical user ID.
	gid_t pw_gid;	 // Numerical group ID.
	char *pw_dir;	 // Initial working directory.
	char *pw_shell;	 // Program to use as shell.
};

#endif
