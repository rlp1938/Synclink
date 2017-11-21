/*      synclink.c
 *
 * Copyright 2011 Bob Parker <rlp1938@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>
#include <libgen.h>

#include "dirs.h"
#include "files.h"
#include "str.h"
#include "srt.h"

typedef struct fsdata {
	int exists;	// 0 = no, 1 = yes
	int otyp;	// 1 = dir, 2 = file, 0 = any other fs object;
	ino_t ino;	// inode number or 0 = none such;
	mode_t omode;	// not used often, defaults to 0.
} fsdata;

struct lp {
	char pt1[PATH_MAX];
	char pt2[NAME_MAX];
	int ot;
};

static void dohelp(int forced);
static void checkarg(char *in, const char *fail);
static void checkdstdirs(size_t srclen, mdata *md, char *dsthead);
static void checkdstfiles(size_t srclen, mdata *md, char *dsthead);
static void checksrcfiles(size_t dstlen, mdata *md, char *srchead);
static void checksrcdirs(size_t dstlen, mdata *md, char *srchead);
static void myunlink(const char *path);
static void makelink(const char *src, const char *dst);
static void dormdir(const char *path);

static int verbose, listwork;
static char *srcroot, *dstroot;
static const char *helpmsg =
  "\n\tUsage:\tsynclink [option] srcdir dstdir\n"
  "\n\tOptions:\n"
  "\t-h outputs this help message.\n"
  "\t-D Debug mode. List contents of source and target dirs in /tmp,\n"
  "\t   with file names '$USERsynclink$PID[srcdir|dstdir].txt'\n"
  "\t-v Set verbose on. Only 2 level of verbosity and it goes to"
  " stderr.\n"
  ;

int main(int argc, char **argv)
{
	int opt;
	char *srcdir, *dstdir;

	// set defaults
	listwork = 0;
	verbose = 0;

	while((opt = getopt(argc, argv, ":hDv")) != -1) {
		switch(opt){
		case 'h':
			dohelp(0);
		break;
		case 'D': // Debug mode, record src and dst dir lists in /tmp
		listwork = 1;
		break;
		case 'v': // Make verbose.
		verbose++;	// I will only process 3 levels of verbosity, 1 - 3
		if (verbose > 3) verbose = 3;
		break;
		case ':':
			fprintf(stderr, "Option %c requires an argument\n",optopt);
			dohelp(1);
		break;
		case '?':
			fprintf(stderr, "Illegal option: %c\n",optopt);
			dohelp(1);
		break;
		} //switch()
	}//while()

	// now process the non-option arguments
	checkarg(argv[optind], "source dir");
	srcdir = realpath(argv[optind], NULL);
	srcroot = xstrdup(srcdir);
	optind++;
	checkarg(argv[optind], "destination dir");
	dstdir = realpath(argv[optind], NULL);
	dstroot = xstrdup(dstdir);
	// prepare dir recursion.
	mdata *md = init_mdata();
	size_t meminc = 1024 * 1024;	// 1 meg seems good for this app.
	rd_data *rd = init_recursedir((char **)NULL, meminc,
								DT_DIR, DT_REG, 0);
	// make source dir list.
	recursedir(srcdir, md, rd);
	if (listwork) {
		char tfn[PATH_MAX];
		dumpstrblock(mktmpfn("synclink", "source", tfn), md);
	}
	// 1. Create destination dirs as needed.
	checkdstdirs(strlen(srcroot), md, dstroot);
	// 2. Check files in destination; link new, delete and link copies.
	checkdstfiles(strlen(srcroot), md, dstroot);
	// prepare to make destination dir list
	memset(md->fro, 0, md->limit - md->fro);
	md->to = md->fro;
	// make destination dir list.
	recursedir(dstdir, md, rd);
	if (listwork) {
		char tfn[PATH_MAX];
		dumpstrblock(mktmpfn("synclink", "destin", tfn), md);
	}
	// 3. Delete files in destination that don't exist in source.
	checksrcfiles(strlen(dstroot), md, srcroot);
	// 4. Delete dirs in destination that don't exist in source.
	// Must sort destination in reverse order so that deletions work.
	size_t countin = countmemstr(md);
	sortmemstr(md, 1);
	size_t countout = countmemstr(md);
	if (countin != countout) {
		fprintf(stderr, "Sort failed on destination dir: %s\n", dstdir);
		exit(EXIT_FAILURE);
	}
	if (listwork) {
		char tfn[PATH_MAX];
		dumpstrblock(mktmpfn("synclink", "revdst", tfn), md);
	}
	checksrcdirs(strlen(dstroot), md, srcroot);
	// free the workfile data
	free(srcdir);
	free(dstdir);
	free_mdata(md);
	return 0;
}//main()

void dohelp(int forced)
{
  fputs(helpmsg, stderr);
  exit(forced);
}

void checkdstdirs(size_t srclen, mdata *md, char *dsthead)
{
	// traverse md, creating dirs in dsthead as required.
	if (verbose) fprintf(stderr, "Checking destination dirs.\n");
	char *line = md->fro;
	while (line < md->to) {
		if (exists_dir(line)) {	// it's a dir
			char buf[PATH_MAX] = {0};
			sprintf(buf, "%s%s", dsthead, line + srclen);
			if (verbose > 1)
				fprintf (stderr, "Checking dir: %s\n", buf);
			if (!exists_dir(buf)) {	// create the dir
				if (verbose) fprintf(stderr, "Creating dir: %s\n", buf);
				newdir(buf, 0);
			}
		}
		line += strlen(line) + 1;
	}
} // checkdstdirs()

void checkdstfiles(size_t srclen, mdata *md, char *dsthead)
{
	// traverse the lists, checking files in dst.
	char *srcline = md->fro;
	if (verbose) fprintf(stderr, "Checking destination files.\n");
	while (srcline < md->to) {
		if (exists_file(srcline)) {	// it's a file
			char buf[PATH_MAX] = {0};
			sprintf(buf, "%s%s", dsthead, srcline + srclen);
			if(verbose > 1) fprintf(stderr,
					"Checking destination file: %s\n", buf);
			if (exists_file(buf)) {
				// the filename exists: copy or link?
				if (getinode(srcline) != getinode(buf)) { // not a link
					if (verbose) fprintf(stderr,
					"Deleting copy and making link: %s\n", buf);
					myunlink(buf);
					makelink(srcline, buf);
				}
			} else { // the filename does not exist in dst.
				if(verbose) fprintf(stderr,
					"Linking new destination file: %s\n", buf);
				makelink(srcline, buf);
			}
		}
		srcline += strlen(srcline) + 1;
	}
} // checkdstfiles()

void checksrcfiles(size_t dstlen, mdata *md, char *srchead)
{	/* if there are any files found in dst that don't exist in src,
	 * delete them.
	*/
	char *dstline = md->fro;
	if (verbose) fprintf(stderr, "Checking source files.\n");

	while (dstline < md->to) {
		if (exists_file(dstline)) {	// It's a file.
			char buf[PATH_MAX];
			sprintf(buf, "%s%s", srchead, dstline + dstlen);
			// Does it exist in src?
			if (!exists_file(buf)) { // No such file in src.
				// So remove it from dst.
				myunlink(dstline);
			}
		}
		dstline += strlen(dstline) + 1;
	}
} // checksrcfiles()

void checksrcdirs(size_t dstlen, mdata *md, char *srchead)
{	/* if there are any dirs found in dst that don't exist in src,
	 * delete them. NB checksrcfiles() must be run first so that these
	 * dirs are empty.
	*/
	char *dstline = md->fro;
	if (verbose) fprintf(stderr, "Checking source dirs.\n");
	while (dstline < md->to) {
		if (exists_dir(dstline)) {	// It's a dir.
			char buf[PATH_MAX];
			sprintf(buf, "%s%s", srchead, dstline + dstlen);
			// Does it exist in src?
			if (!exists_dir(buf)) {	// No such dir in src.
				// So remove it from dst.
				dormdir(dstline);
			}
		}
		dstline += strlen(dstline) + 1;
	}
} // checksrcdirs()

void myunlink(const char *path)
{	/* just unlink() with error handling */
	if (verbose) fprintf(stderr, "Unlinking: %s\n", path);
	if (unlink(path) == -1) {
		perror(path);
		exit(EXIT_FAILURE);
	}
} // myunlink()

void makelink(const char *src, const char *dst)
{	/* link() with error handling */
	if (verbose) fprintf(stderr, "Linking:\n\t%s =>\n\t%s\n", src, dst);
	if (link(src, dst) == -1) {
		perror(src);
		exit(EXIT_FAILURE);
	}
} // makelink()

void dormdir(const char *path)
{	/* rmdir() with error handling */
	if (verbose) fprintf(stderr, "Removing dir: %s\n", path);
	if (rmdir(path) == -1) {
		perror(path);
		exit(EXIT_FAILURE);
	}
} // dormdir()

void checkarg(char *in, const char *fail)
{/* in must exist and then test for type */
	if (!in) {
		fprintf(stderr, "%s argument does not exist.\n", fail);
		dohelp(EXIT_FAILURE);
	}
	if (!exists_dir(in)) {
		fprintf(stderr, "%s does not exist or is not a dir.\n", fail);
		dohelp(EXIT_FAILURE);
	}
} // checkarg()
