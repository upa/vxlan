/*_
 * Copyright 2009 Scyphus Solutions Co.,Ltd. All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id: error.c,v 3fd7b0a2108d 2011/02/11 09:36:32 Hirochika $ */

#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

int error_syslog = 0;

static void
error_printf(int, int, const char *, va_list);

/* Print error message. */
void
error_msg(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_printf(0, LOG_INFO, fmt, ap);
    va_end(ap);
}

/* Print system error message. */
void
error_sys_msg(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_printf(1, LOG_INFO, fmt, ap);
    va_end(ap);
}

/* Print notice message. */
void
error_notice(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_printf(0, LOG_NOTICE, fmt, ap);
    va_end(ap);
}

/* Print system warning message. */
void
error_sys_notice(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_printf(1, LOG_NOTICE, fmt, ap);
    va_end(ap);
}

/* Print warning message. */
void
error_warn(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_printf(0, LOG_WARNING, fmt, ap);
    va_end(ap);
}

/* Print system warning message. */
void
error_sys_warn(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_printf(1, LOG_WARNING, fmt, ap);
    va_end(ap);
}

/* Print error message and quit the program. */
void
error_quit(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_printf(0, LOG_ERR, fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

/* Print system error message and quit the program. */
void
error_sys(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_printf(1, LOG_ERR, fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

/* Print error message and quit the program with a status. */
void
error_exit(int status, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_printf(0, LOG_ERR, fmt, ap);
    va_end(ap);
    exit(status);
}

/* Print error message and abort. */
void
error_dump(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_printf(1, LOG_ERR, fmt, ap);
    va_end(ap);
    abort();            /* dump core and terminate */
    exit(EXIT_FAILURE); /* shouldn't get here */
}

/**
 * error_printf
 */
static void
error_printf(int errnoflag, int level, const char *fmt, va_list ap)
{
    int errno_save, n;
    char buf[MAXLINE];

    errno_save = errno;
    vsnprintf(buf, sizeof(buf), fmt, ap);

    n = strlen(buf);
    if ( errnoflag ) {
        snprintf(buf+n, sizeof(buf)-n, ": %s", strerror(errno_save));
    }
    strcat(buf, "\n");

    if ( error_syslog ) {
        /* output to syslog */
        syslog(level, buf);
    } else {
        fflush(stdout);         /* in case stdout and stderr are the same */
        fputs(buf, stderr);
        fflush(stderr);
    }
}

void
error_enable_syslog(void)
{
    error_syslog = 1;
}

void
error_disable_syslog(void)
{
    error_syslog = 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
