/*
 * runutils.h
 * 	Copyright 23.12.2014 Bob Parker <rlp1938@gmail.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *	MA 02110-1301, USA.
*/

#ifndef _RUNUTILS_H
#define _RUNUTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>
#include <libgen.h>

#include "fileutil.h"

char progname[NAME_MAX];

char *dostrdup(const char *s);
void *domalloc(size_t sz);
void firstrun(char *frtext, char *srcdir, char *dstdir, ...);
void dogetenv(const char *param, char *result);
void reporterror(const char *module, const char *perrorstr,
							int fatal);
void dosetlang(void);
void display(char *disptext, unsigned int cols);
void dosystem(const char *cmd);
void trace(char *tracefilename, char *tracetext);
void stripws(char *target);
void dorealpath(char *givenpath, char *resolvedpath);

struct fdata rufd;

#endif
