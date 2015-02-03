#ifndef _FILEUTIL_H
#define _FILEUTIL_H 1

#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>
#include <libgen.h>


struct fdata {
	char *from;
	char *to;
};
struct fdata readfile(const char *filename, off_t extra, int fatal);
FILE *dofopen(const char *path, const char *mode);
FILE *dofreopen(const char *path, const char *mode, FILE *fp);
int filexists(const char *path, off_t *fsize);
int direxists(const char *path);
void dorename(char *oldname, char *newname);
void writefile(const char *fname, char *fro, const char *to);
void dounlink(const char *fname);
void mem2str(char *fr, char *to, int *lcount);
void cpfile(const char *fr, const char *to);
#endif /* fileutil.h */
