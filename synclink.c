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

#include "fileutil.h"
#include "runutils.h"

typedef struct fsdata {
	int exists;	// 0 = no, 1 = yes
	int otyp;	// 1 = dir, 2 = file, 0 = any other fs object;
	ino_t ino;	// inode number or 0 = none such;
	mode_t omode;	// not used often, defaults to 0.
} fsdata;

static void recursedir(char *headdir, FILE *fpo);
static char **makefilenamelist(char *prefix, char **argv, int numberof);
static void destroyfilenamelist(char **filenamev);
static void dohelp(int forced);
static void checkdstdirs(struct fdata srcfdat, struct fdata dstfdat);
static void checkdstfiles(struct fdata srcfdat, struct fdata dstfdat);
static void checksrcfiles(struct fdata dstfdat, struct fdata srcfdat);
static void checksrcdirs(struct fdata dstfdat, struct fdata srcfdat);
static fsdata fswhatisit(const char *pathname);
static void domkdir(const char *path, mode_t crmode);
static void checkfilestatus(const char *srcfile, const char *dstfile);
static void myunlink(const char *path);
static void makelink(const char *src, const char *dst);
static void dormdir(const char *path);

static int verbose, delwork;
static char *srcroot, *dstroot;
static mode_t dirmode;
static const char *helpmsg =
  "\n\tUsage:\tsynclink [option] srcdir dstdir\n"
  "\t\tsynclink [option] srclist dstdir"
  "\n\tOptions:\n"
  "\t-h outputs this help message.\n"
  "\t-D Debug mode, don't delete workfiles in /tmp on completion.\n"
  "\t   Workfile names are /tmp/username+argv[0]+0..6.\n"
  "\t-v Set verbose on. Only 1 level of verbosity and it goes to"
  " stdout.\n"
  ;

struct lp {
	char pt1[PATH_MAX];
	char pt2[NAME_MAX];
	int ot;
};

int main(int argc, char **argv)
{
	int opt;
	char srcobj[PATH_MAX], dstdir[PATH_MAX], command[PATH_MAX];
	char **fnv;
	FILE *fpo;
	struct fdata srcfdat, dstfdat;

	// set defaults
	delwork = 0;	// TODO: default to be 1 when this is debugged.
	verbose = 0;

	while((opt = getopt(argc, argv, ":hDv")) != -1) {
		switch(opt){
		case 'h':
			dohelp(0);
		break;
		case 'D': // Debug mode, keep temporary work files.
		delwork = 0;
		break;
		case 'v': // Make verbose.
		verbose++;	// I will only process 3 levels of verbosity, 0 - 2
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

	// 1.Check that argv[???] exists.
	if (!(argv[optind])) {
		fprintf(stderr, "No source dir provided\n");
		dohelp(EXIT_FAILURE);
	} else {
		dorealpath(argv[optind], srcobj);
	}

	// 2. Check that srcdir exists
	fsdata fsd = fswhatisit(srcobj);
	if (!fsd.exists) {
		fprintf(stderr, "Source dir %s does not exist.\n", srcobj);
		dohelp(EXIT_FAILURE);
	}
	else if (fsd.otyp != 1){
		fprintf(stderr, "Object %s is not a dir.\n", srcobj);
		dohelp(EXIT_FAILURE);
	}
	srcroot = strdup(srcobj);
	dirmode = fsd.omode;

	// Next, the dstdir argument
	optind++;

	// 3. Check that argv[???] exists.
	if (!(argv[optind])) {
		fprintf(stderr, "No destination dir provided\n");
		dohelp(1);
	} else {
		dorealpath(argv[optind], dstdir);
	}

	// 4. Check that the FS object exists.
	fsd = fswhatisit(dstdir);
	if (!fsd.exists) {
		fprintf(stderr, "Destination dir %s does not exist.\n", dstdir);
		dohelp(EXIT_FAILURE);
	}
	else if (fsd.otyp != 1){
		fprintf(stderr, "Object %s is not a dir.\n", dstdir);
		dohelp(EXIT_FAILURE);
	}
	dstroot = strdup(dstdir);

	fnv = makefilenamelist("/tmp/", argv, 6);	// workfiles in /tmp/

	dosetlang();	// $LANG to be "C", so sort works in ascii order.

	// process source dir
	fpo = dofopen(fnv[0], "w");
	recursedir(srcobj, fpo);
	fclose(fpo);
	sprintf(command, "sort -u %s > %s", fnv[0], fnv[1]);
	dosystem(command);

	// process destination dir
	fpo = dofopen(fnv[2], "w");
	recursedir(dstdir, fpo);
	fclose(fpo);
	sprintf(command, "sort %s > %s", fnv[2], fnv[3]);
	dosystem(command);

	// read the sorted files into memory
	srcfdat = readfile(fnv[1], 0, 1);
	dstfdat = readfile(fnv[3], 0, 1);

	// convert file data blocks to C strings
	int dummy;
	mem2str(srcfdat.from, srcfdat.to, &dummy);
	mem2str(dstfdat.from, dstfdat.to, &dummy);

	// process the file path lists.
	// 1. Create destination dirs as needed.
	checkdstdirs(srcfdat, dstfdat);

	// 2. Create and/or check files in destination.
	checkdstfiles(srcfdat, dstfdat);

	// 3. Delete files in destination that don't exist in source.
	checksrcfiles(dstfdat, srcfdat);

	// 4. Delete dirs in destination that don't exist in source.
	checksrcdirs(dstfdat, srcfdat);

	// free the workfile data
	free(dstfdat.from);
	free(srcfdat.from);

	// trash workfiles
	if (delwork) {
		int i = 0;
		while(fnv[i]) {
			unlink(fnv[i]);
			i++;
		}
	}

	destroyfilenamelist(fnv);
	return 0;
}//main()

void dohelp(int forced)
{
  fputs(helpmsg, stderr);
  exit(forced);
}

void recursedir(char *headdir, FILE *fpo)
{
	/* open the dir at headdir and process according to file type.
	*/
	DIR *dirp;
	struct dirent *de;

	dirp = opendir(headdir);
	if (!(dirp)) {
		reporterror("recursedir(): ", headdir, 1);
	}

	// put the trailing '/' if it's not there.
	if (headdir[strlen(headdir) - 1] != '/') strcat(headdir, "/");
	fprintf(fpo, "%s\n", headdir);
	while((de = readdir(dirp))) {
		if (strcmp(de->d_name, "..") == 0) continue;
		if (strcmp(de->d_name, ".") == 0) continue;
		switch(de->d_type) {
			char newpath[FILENAME_MAX];
			// Nothing to do for these.
			case DT_BLK:
			case DT_CHR:
			case DT_FIFO:
			case DT_SOCK:
			case DT_LNK:
			break;
			// Just record the other two types
			case DT_REG:
			strcpy(newpath, headdir);
			strcat(newpath, de->d_name);
			// now just record the thing
			fprintf(fpo, "%s\n", newpath);
			break;
			case DT_DIR:
			strcpy(newpath, headdir);
			strcat(newpath, de->d_name);
			recursedir(newpath, fpo);
			break;
			// Just report the error but do nothing else.
			case DT_UNKNOWN:
			fprintf(stderr, "Unknown type:\n%s/%s\n\n", headdir,
					de->d_name);
			break;

		} // switch()
	} // while
	closedir(dirp);
} // recursedir()

char **makefilenamelist(char *prefix, char **argv, int numberof)
{
	// generate a list of file names with NULL terminator
	char fn[PATH_MAX];
	char **fnlist;
	int i;
	char *username, *progname;

	fnlist = domalloc((numberof + 1) * sizeof(char *));
	username = getenv("USER");
	progname = basename(argv[0]);
	for(i=0; i<numberof; i++) {
		sprintf(fn, "%s%s%s%d", prefix, username, progname, i);
		fnlist[i] = dostrdup(fn);
	}
	fnlist[numberof] = (char *)NULL;
	return fnlist;
} // makefilenamelist()

void destroyfilenamelist(char **filenamev)
{
	int i = 0;
	while(filenamev[i]) {
		free(filenamev[i]);
		i++;
	}
	free(filenamev);
} // destroyfilenamelist()

void checkdstdirs(struct fdata srcfdat, struct fdata dstfdat)
{
	// traverse the lists, creating dirs in dst as required.
	fsdata fsd;
	char *srcline = srcfdat.from;
	if (verbose) fprintf(stderr, "Checking destination dirs.\n");

	while (srcline < srcfdat.to) {
		fsd = fswhatisit(srcline);
		if (fsd.otyp == 1) {	// it's a dir
			size_t len = dstfdat.to - dstfdat.from;
			char buf[PATH_MAX];
			sprintf(buf, "%s%s", dstroot, srcline + strlen(srcroot));
			if (!memmem(dstfdat.from, len, buf, strlen(buf))) {
				domkdir(buf, dirmode);
			}
		}
		srcline += strlen(srcline) + 1;
	}
} // checkdstdirs()

fsdata fswhatisit(const char *pathname)
{	/* returns a limited set of information from stat() */
	fsdata fsd = {0};
	struct stat sb;
	if (stat(pathname, &sb) == -1) {
		return fsd;
	}
	fsd.exists = 1;
	fsd.ino = sb.st_ino;
	if (S_ISREG(sb.st_mode)) {
		fsd.otyp = 2;
	} else if (S_ISDIR(sb.st_mode)) {
		fsd.otyp = 1;
	}	// and I don't care about any other types of fsobject found.
	fsd.omode = sb.st_mode;
	return fsd;
} // fswhatisit()

void domkdir(const char *path, mode_t crmode)
{	/* just mkdir with error handling */
	if (verbose)
		fprintf(stderr, "Making destination dir:\n\t%s.\n", path);
	if (mkdir(path, crmode) == -1) {
		perror("path");
		exit(EXIT_FAILURE);
	}
} // domkdir()

void checkdstfiles(struct fdata srcfdat, struct fdata dstfdat)
{
	// traverse the lists, checking files in dst.
	fsdata fsd;
	char *srcline = srcfdat.from;
	if (verbose) fprintf(stderr, "Checking destination files.\n");

	while (srcline < srcfdat.to) {
		fsd = fswhatisit(srcline);
		if (fsd.otyp == 2) {	// it's a file
			size_t len = dstfdat.to - dstfdat.from;
			char buf[PATH_MAX];
			sprintf(buf, "%s%s", dstroot, srcline + strlen(srcroot));
			if (memmem(dstfdat.from, len, buf, strlen(buf))) {
				// the filename exists: copy or link?
				checkfilestatus(srcline, buf);
			} else { // the filename does not exist in dst.
				makelink(srcline, buf);
			}
		}
		srcline += strlen(srcline) + 1;
	}
} // checkdstfiles()

void checkfilestatus(const char *srcfile, const char *dstfile)
{	/* see if the srcfile and dstfile have the same inode, if so do
	 * nothing, but if not delete dstfile and make dstfile a hard link
	 * to srcfile.
	*/
	if (verbose > 1) fprintf(stderr, "Checking: %s\n", dstfile);
	fsdata fsds = fswhatisit(srcfile);
	fsdata fsdd = fswhatisit(dstfile);
	if (fsds.ino == fsdd.ino) return;	// nothing to do.
	myunlink(dstfile);
	makelink(srcfile, dstfile);
} // checkfilestatus()

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
	if (verbose) fprintf(stderr, "Linking:\n\t%s,\n\t%s\n", src, dst);
	if (link(src, dst) == -1) {
		perror(src);
		exit(EXIT_FAILURE);
	}
} // makelink()

void checksrcfiles(struct fdata dstfdat, struct fdata srcfdat)
{	/* if there are any files found in dst that don't exist in src,
	 * delete them.
	*/
	fsdata fsd;
	char *dstline = dstfdat.from;
	if (verbose) fprintf(stderr, "Checking source files.\n");

	while (dstline < dstfdat.to) {
		fsd = fswhatisit(dstline);
		if (fsd.otyp == 2) {	// it's a file
			size_t len = srcfdat.to - srcfdat.from;
			char buf[PATH_MAX];
			sprintf(buf, "%s%s", srcroot, dstline + strlen(dstroot));
			if (!memmem(srcfdat.from, len, buf, strlen(buf))) {
				// Filename does not exist in src?. Delete from dst.
				myunlink(dstline);
			}
		}
		dstline += strlen(dstline) + 1;
	}

} // checksrcfiles()

void checksrcdirs(struct fdata dstfdat, struct fdata srcfdat)
{	/* if there are any dirs found in dst that don't exist in src,
	 * delete them. NB checksrcfiles() must be run first so that these
	 * dirs are empty.
	*/
	fsdata fsd;
	char *dstline = dstfdat.from;
	if (verbose) fprintf(stderr, "Checking source dirs.\n");

	while (dstline < dstfdat.to) {
		fsd = fswhatisit(dstline);
		if (fsd.otyp == 1) {	// it's a dir
			size_t len = srcfdat.to - srcfdat.from;
			char buf[PATH_MAX];
			sprintf(buf, "%s%s", srcroot, dstline + strlen(dstroot));
			if (!memmem(srcfdat.from, len, buf, strlen(buf))) {
				dormdir(dstline);
			}
		}
		dstline += strlen(dstline) + 1;
	}

} // checksrcdirs()

void dormdir(const char *path)
{	/* rmdir() with error handling */
	if (verbose) fprintf(stderr, "Removing dir: %s\n", path);
	if (rmdir(path) == -1) {
		perror(path);
		exit(EXIT_FAILURE);
	}
} // dormdir()
