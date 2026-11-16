/* Shared helpers for the recorder harnesses: a private scratch STORAGEDIR that is emptied before
 * every input, so each input's result depends only on that input. */
#ifndef OTR_FUZZ_H
#define OTR_FUZZ_H

#define _GNU_SOURCE
#include <ftw.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

extern char STORAGEDIR[];

static char otr_root[PATH_MAX];

static int otr_rm_cb(const char *path, const struct stat *sb, int flag, struct FTW *ftw)
{
	(void)sb; (void)flag;
	if (ftw->level > 0)
		(void)remove(path);
	return 0;
}

/* Remove everything below the scratch root, keep the root itself. */
static void __attribute__((unused)) otr_reset_storage(void)
{
	nftw(otr_root, otr_rm_cb, 16, FTW_DEPTH | FTW_PHYS);
}

/* Create the scratch STORAGEDIR under $TMPDIR and point the recorder at it. */
static void otr_init_storage(void)
{
	const char *tmp = getenv("TMPDIR");

	if (tmp == NULL || *tmp == 0)
		tmp = "/tmp";
	snprintf(otr_root, sizeof(otr_root), "%s/otr-fuzz-XXXXXX", tmp);
	if (mkdtemp(otr_root) == NULL) {
		perror("mkdtemp");
		exit(1);
	}
	snprintf(STORAGEDIR, BUFSIZ, "%s", otr_root);

	/* olog() goes to syslog; nothing listens in the fuzzing container */
	setlogmask(LOG_MASK(LOG_EMERG));
	/* gcache_open() maps 5 GiB by default; 10 MiB is the minimum it accepts */
	setenv("OTR_LMDBSIZE", "10485760", 1);
}

#endif
