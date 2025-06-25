/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2006 Gabriel Somlo <somlo at cmu.edu>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include "busybox.h" /* for APPLET_IS_NOEXEC */

/* check if path points to an executable file;
 * return 1 if found;
 * return 0 otherwise;
 */
int FAST_FUNC file_is_executable(const char *name)
{
	struct stat s;
	return (!access(name, X_OK) && !stat(name, &s) && S_ISREG(s.st_mode));
}

/* search (*PATHp) for an executable file;
 * return allocated string containing full path if found;
 *  PATHp points to the component after the one where it was found
 *  (or NULL if found in last component),
 *  you may call find_executable again with this PATHp to continue
 * return NULL otherwise (PATHp is undefined)
 */
char* FAST_FUNC find_executable(const char *name, const char **PATHp)
{
	/* About empty components in $PATH:
	 * http://pubs.opengroup.org/onlinepubs/009695399/basedefs/xbd_chap08.html
	 * 8.3 Other Environment Variables - PATH
	 * A zero-length prefix is a legacy feature that indicates the current
	 * working directory. It appears as two adjacent colons ( "::" ), as an
	 * initial colon preceding the rest of the list, or as a trailing colon
	 * following the rest of the list.
	 */
	char *p = (char*) *PATHp;

	if (!p)
		return NULL;
	while (1) {
		const char *end = strchrnul(p, ':');
		int sz = end - p;

		if (sz != 0) {
			p = xasprintf("%.*s/%s", sz, p, name);
		} else {
			/* We have xxx::yyy in $PATH,
			 * it means "use current dir" */
			p = xstrdup(name);
// A bit of discrepancy wrt the path used if file is found here.
// bash 5.2.15 "type" returns "./NAME".
// GNU which v2.21 returns "/CUR/DIR/NAME".
// With -a, both skip over all colons: xxx::::yyy is the same as xxx::yyy,
// current dir is not tried the second time.
		}
		if (file_is_executable(p)) {
			*PATHp = (*end ? end+1 : NULL);
			return p;
		}
		free(p);
		if (*end == '\0')
			return NULL;
		p = (char *) end + 1;
	}
}

/* search $PATH for an executable file;
 * return 1 if found;
 * return 0 otherwise;
 */
int FAST_FUNC executable_exists(const char *name)
{
	const char *path = getenv("PATH");
	char *ret = find_executable(name, &path);
	free(ret);
	return ret != NULL;
}

int FAST_FUNC applet_execve(const char *name, char *const argv[], char *const envp[])
{
#if ENABLE_FEATURE_PREFER_APPLETS
	/* used to copy argv to heap */
	char **copied_argv;

# if ENABLE_FEATURE_TRY_BASENAME_APPLETS
	/* find applet by basename */
	int applet = find_applet_by_name(bb_basename(name));
# else
	/* find applet by given name*/
	int applet = find_applet_by_name(name);
# endif

	/* no matching applet was found */
	if (applet < 0) {
		errno = ENOENT;
		return -1;
	}

	/* NOMMU targets only support vfork().
	 * since vfork() requires the child to exec() or _exit() for the
	 * parent to resume, running applets with NOEXEC and vfork()
	 * may result in deadlocks, as exec() will never be called.
	 * these applets must be executed using the exec syscall. */
	if (!BB_MMU || !APPLET_IS_NOEXEC(applet))
		return execve(bb_busybox_exec_path, argv, envp);

	/* since run_noexec_applet_and_exit takes char **argv,
	 * we need to copy argv to a new heap-allocated array. */
	copied_argv = clone_string_array(argv);

	/* since exec will not be called, we need to manually
	 * reset some signal handlers. */
	reset_all_signals();

	/* since exec will not be called, we then need to close
	 * all FDs with FD_CLOEXEC manually. */
	close_cloexec_fds();

	/* if non-default environ was passed, replace environ */
	if (envp != environ) {
		clearenv();

		/* envp is NULL terminated. */
		while (*envp)
			putenv(*envp++);
	}

	/* this should never return. */
	run_noexec_applet_and_exit(applet, name, copied_argv);

	/* free duplicated argv array */
	while (*copied_argv)
		free(*copied_argv++);

	/* if this is reached, error out */
	errno = ENOEXEC;
	return -1;
#else
	/* applets are not prefered */
	return -1;
#endif
}

/* just like the real execve, but we might try to launch an applet named 'pathname' first */
int FAST_FUNC bb_execve(const char *pathname, char *const argv[], char *const envp[])
{
	/* try executing using applet_execve */
	int status = applet_execve(pathname, argv, envp);

	/* if status is valid, should not fall back */
	if (status >= 0)
		return status;

#if ENABLE_FEATURE_FORCE_APPLETS
	/* no external programs are allowed, error out */
	errno = ENOENT;
	return -1;
#endif

	/* fall back to execve */
	return execve(pathname, argv, envp);
}

/* just like bb_execve, but we keep the existing environment by passing environ */
int FAST_FUNC bb_execv(const char *pathname, char *const argv[])
{
	return bb_execve(pathname, argv, environ);
}

/* just like bb_execve, but we pass the basename of file and fall back to execvp */
int FAST_FUNC bb_execvp(const char *file, char *const argv[])
{
	/* try executing using applet_execve with the basename of file.
	 * this emulates a PATH search, since applet names are always basenames. */
	int status = applet_execve(bb_basename(file), argv, environ);

	/* if status is valid, should not fall back */
	if (status >= 0)
		return status;

#if ENABLE_FEATURE_FORCE_APPLETS
	/* no external programs are allowed, error out */
	errno = ENOENT;
	return -1;
#endif

	/* fall back to execvp */
	return execvp(file, argv);
}
