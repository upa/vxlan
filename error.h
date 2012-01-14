/*_
 * Copyright 2009 Scyphus Solutions Co.,Ltd. All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id: error.h,v f045f3d6e083 2011/02/12 10:50:28 Hirochika $ */

#ifndef _ERROR_H
#define _ERROR_H

#ifndef MAXLINE
#define MAXLINE 256
#endif

#include <stdio.h>
#include <stdarg.h>

extern int error_syslog;

static const struct {
    const char *nomem;          /* Memory error */
    const char *server;         /* Server error */
} errstr = {
    .nomem = "No enough memory.",
    .server = "Server error: %s"
};

#ifdef __cplusplus
extern "C" {
#endif

    void error_msg(const char *, ...);
    void error_sys_msg(const char *, ...);
    void error_notice(const char *, ...);
    void error_sys_notice(const char *, ...);
    void error_warn(const char *, ...);
    void error_sys_warn(const char *, ...);
    void error_quit(const char *, ...);
    void error_sys(const char *, ...);
    void error_exit(int, const char *, ...);
    void error_dump(const char *fmt, ...);

    void error_enable_syslog(void);
    void error_disable_syslog(void);

#ifdef __cplusplus
}
#endif

#endif /* _ERROR_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
