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

#include "fileops.h"

typedef struct fsdata {
	int exists;	// 0 = no, 1 = yes
	int otyp;	// 1 = dir, 2 = file, 0 = any other fs object;
	ino_t ino;	// inode number or 0 = none such;
	mode_t omode;	// not used often, defaults to 0.
} fsdata;

typedef struct memdat {
	char *from;
	char *to;
	char *limit;
	size_t chunk;
} memdat;

struct lp {
	char pt1[PATH_MAX];
	char pt2[NAME_MAX];
	int ot;
};

static void recursedir(char *headdir, memdat *md);
static void dohelp(int forced);
static void checkdstdirs(size_t srclen, memdat md, char *dsthead);
static void checkdstfiles(size_t srclen, memdat md, char *dsthead);
static void checksrcfiles(size_t dstlen, memdat md, char *srchead);
static void checksrcdirs(size_t dstlen, memdat md, char *srchead);

static fsdata fswhatisit(const char *pathname);
static void myunlink(const char *path);
static void makelink(const char *src, const char *dst);
static void dormdir(const char *path);
static void recordstr(char *line, memdat *md);
static void dumpdirlist(memdat md, char *id, char *progname);
static void pathsplit(char *buf, char **thedir, char **fil);
static ino_t getinode(char *path);
static int verbose, listwork;
static char *srcroot, *dstroot;
static mode_t dirmode;
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
	char srcdir[PATH_MAX], dstdir[PATH_MAX];

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
		verbose++;	// I will only process 3 levels of verbosity, 0 - 2
		if (verbose > 2) verbose = 2;
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
		realpath(argv[optind], srcdir);
	}

	// 2. Check that srcdir exists
	fsdata fsd = fswhatisit(srcdir);
	if (!fsd.exists) {
		fprintf(stderr, "Source dir %s does not exist.\n", srcdir);
		dohelp(EXIT_FAILURE);
	}
	else if (fsd.otyp != 1){
		fprintf(stderr, "Object %s is not a dir.\n", srcdir);
		dohelp(EXIT_FAILURE);
	}
	srcroot = strdup(srcdir);
	dirmode = fsd.omode;

	// Next, the dstdir argument
	optind++;

	// 3. Check that argv[???] exists.
	if (!(argv[optind])) {
		fprintf(stderr, "No destination dir provided\n");
		dohelp(1);
	} else {
		realpath(argv[optind], dstdir);
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

	// make source dir list
	memdat md;
	//md.chunk = 1024 * 1024;	// 1 meg
	md.chunk = PATH_MAX;	// 4096, force realloc() for testing.
	md.from = docalloc(md.chunk, 1, "main()");
	md.to = md.from;
	md.limit = md.from + md.chunk;

	char *progname = strdup(basename(argv[0]));

	recursedir(srcdir, &md);
	if (listwork) dumpdirlist(md, "srcdir", progname);

	// 1. Create destination dirs as needed.
	checkdstdirs(strlen(srcroot), md, dstroot);

	// 2. Create and/or check files in destination.
	checkdstfiles(strlen(srcroot), md, dstroot);

	// process destination dir
	memset(md.from, 0, md.limit - md.from);
	md.to = md.from;
	recursedir(dstdir, &md);
	if (listwork) dumpdirlist(md, "dstdir", progname);

	// 3. Delete files in destination that don't exist in source.
	checksrcfiles(strlen(dstroot), md, srcroot);

	// 4. Delete dirs in destination that don't exist in source.
	checksrcdirs(strlen(dstroot), md, srcroot);

	// free the workfile data
	free(md.from);
	free(progname);
	return 0;
}//main()

void dohelp(int forced)
{
  fputs(helpmsg, stderr);
  exit(forced);
}

void recursedir(char *headdir, memdat *md)
{
	/* open the dir at headdir and process according to file type.
	*/
	DIR *dirp;
	struct dirent *de;

	dirp = opendir(headdir);
	if (!(dirp)) {
		perror("opendir");
		exit(EXIT_FAILURE);
	}

	// put the trailing '/' if it's not there.
	if (headdir[strlen(headdir) - 1] != '/') strcat(headdir, "/");
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
			recordstr(newpath, md);
			break;
			case DT_DIR:
			strcpy(newpath, headdir);
			strcat(newpath, de->d_name);
			recordstr(newpath, md);
			recursedir(newpath, md);
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

void checkdstdirs(size_t srclen, memdat md, char *dsthead)
{
	// traverse md, creating dirs in dsthead as required.
	if (verbose) fprintf(stderr, "Checking destination dirs.\n");

	char *line = md.from;
	while (line < md.to) {
		if (direxists(line) == 0) {	// it's a dir
			char buf[PATH_MAX] = {0};
			sprintf(buf, "%s%s", dsthead, line + srclen);
			sync();
			int de = direxists(buf);
			if (verbose > 1) {
				fprintf (stderr, "Checking dir: %s\n", buf);
			}

			if (de == -1) {	// create the dir
				char *hd = 0;
				char *nd = 0;
				if (verbose) fprintf(stderr, "Creating dir: %s\n",
										buf);
				pathsplit(buf, &hd, &nd);
				do_mkdir(hd, nd);
			}
		}
		line += strlen(line) + 1;
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

void checkdstfiles(size_t srclen, memdat md, char *dsthead)
{
	// traverse the lists, checking files in dst.
	char *srcline = md.from;
	if (verbose) fprintf(stderr, "Checking destination files.\n");

	while (srcline < md.to) {
		if (fileexists(srcline) == 0) {	// it's a file
			char buf[PATH_MAX];
			sprintf(buf, "%s%s", dsthead, srcline + srclen);
			if (fileexists(buf) == 0) {
				// the filename exists: copy or link?
				if (getinode(srcline) != getinode(buf)) { // not a link
					myunlink(buf);
					makelink(srcline, buf);
				}
			} else { // the filename does not exist in dst.
				makelink(srcline, buf);
			}
		}
		srcline += strlen(srcline) + 1;
	}
} // checkdstfiles()

void checksrcfiles(size_t dstlen, memdat md, char *srchead)
{	/* if there are any files found in dst that don't exist in src,
	 * delete them.
	*/
	char *dstline = md.from;
	if (verbose) fprintf(stderr, "Checking source files.\n");

	while (dstline < md.to) {
		if (fileexists(dstline) == 0) {	// It's a file.
			char buf[PATH_MAX];
			sprintf(buf, "%s%s", srchead, dstline + dstlen);
			// Does it exist in src?
			if (fileexists(buf) == -1) { // No such file in src.
				// So remove it from dst.
				myunlink(dstline);
			}
		}
		dstline += strlen(dstline) + 1;
	}
} // checksrcfiles()

void checksrcdirs(size_t dstlen, memdat md, char *srchead)
{	/* if there are any dirs found in dst that don't exist in src,
	 * delete them. NB checksrcfiles() must be run first so that these
	 * dirs are empty.
	*/
	char *dstline = md.from;
	if (verbose) fprintf(stderr, "Checking source dirs.\n");
	while (dstline < md.to) {
		if (direxists(dstline) == 0) {	// It's a dir.
			char buf[PATH_MAX];
			sprintf(buf, "%s%s", srchead, dstline + dstlen);
			// Does it exist in src?
			if (direxists(buf) == -1) {	// No such dir in src.
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

void recordstr(char *line, memdat *md)
{	/* copy line into the blob described by md, taking care of
	 * memory reallocations as required.
	 * Foe safety md->chunk should be PATH_MAX minimum.
	*/
	size_t len = strlen(line);
	size_t avail = md->limit - md->to;
	size_t used = md->to - md->from;
	if (len >= avail) {	// make more space
		size_t current = md->limit - md->from;
		md->from = realloc(md->from, current + md->chunk);
		md->to = md->from + used;
		md->limit = md->from + current + md->chunk;
		memset(md->to, 0, md->limit - md->to);
	}
	strcpy(md->to, line);
	md->to += len + 1;
} // recordstr()

void dumpdirlist(memdat md, char *id, char *progname)
{	/* write the data described by md to a file in /tmp */
	char name[PATH_MAX];
	sprintf(name, "/tmp/%s%s%d%s.txt", getenv("USER"), progname,
				getpid(), id);
	FILE *fpo = dofopen(name, "w");
	char *cp = md.from;
	while (cp < md.to) {
		fprintf(fpo, "%s\n", cp);
		cp += strlen(cp) + 1;
	}
	dofclose(fpo);
}

static void pathsplit(char *buf, char **thedir, char **fil)
{	/* If buf ends with '/' get rid of it.
	 * 0 the last '/' and return fil = *last '/' + 1
	 * thedir = buf.
	*/
	size_t len = strlen(buf);
	if (buf[len-1] == '/') {
		buf[len-1] = 0;
	}
	*thedir = *fil = NULL;
	char *sep = strrchr(buf, '/');
	if (sep) {
		*sep = 0;
		*thedir = buf;
		*fil = sep + 1;
	}
} // pathsplit()

ino_t getinode(char *path)
{	/* return the inode number if the path exists, if not abort */
	struct stat sb;
	if (stat(path, &sb) == -1) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	return sb.st_ino;
} // getinode()
