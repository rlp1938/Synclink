/*      synclink.c
 *
 *	Copyright 2011 Bob Parker <rlp1938@gmail.com>
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

static void recursedir(char *headdir, FILE *fpo);
static char **makefilenamelist(char *prefix, char **argv, int numberof);
static void destroyfilenamelist(char **filenamev);
static void *domalloc(size_t sz);
static char *dostrdup(const char *s);
static void dorealpath(char *givenpath, char *resolvedpath);
static void dosystem(const char *cmd);
static void dohelp(int forced);
static void dosetlang(void);
static void pass1(struct fdata srcfdat, struct fdata dstfdat,
					FILE *fpo);
static void lineparts(char *origin, char *root, char *fsobject,
						int *objtyp);
static void addobject(char *dstroot, char *srcpath, char *srcptr,
						int objtyp);
static void inocheck(char *srcline, char *dstline, int objtyp);
static void twoparts(char *line, char *path, int *fsobj);
static void pass2(struct fdata delfdat, char acton);
static void reporterror(const char *module, const char *perrorstr,
							int fatal);

static const char *pathend = "   !";	// Maybe no one is stupid
										// enough to use any of these
										// (int)32X3+! in a file name.
static int verbose;

static const char *helpmsg =
  "\n\tUsage: synclink [option] srcdir dstdir\n"
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
	int opt, delwork;
	char srcdir[PATH_MAX], dstdir[PATH_MAX], command[PATH_MAX];
	struct stat sb;
	char **fnv;
	FILE *fpo;
	struct fdata srcfdat, dstfdat, delfdat;

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
		verbose = 1;
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
		dohelp(1);
	} else {
		dorealpath(argv[optind], srcdir);
	}

	// 2. Check that dir exists.
	if (stat(srcdir, &sb) == -1) {
		perror(srcdir);
		dohelp(EXIT_FAILURE);	// no return from dohelp()
	}

	// 3. Ensure that this is a dir
	if (!S_ISDIR(sb.st_mode)) {
		fprintf(stderr, "%s is not a direcory.\n", srcdir);
		dohelp(EXIT_FAILURE);	// no return from dohelp()
	}

	// Next, the dstdir argument
	optind++;

	// 4. Check that argv[???] exists.
	if (!(argv[optind])) {
		fprintf(stderr, "No destination dir provided\n");
		dohelp(1);
	} else {
		dorealpath(argv[optind], dstdir);
	}

	// 5. Check that dir exists.
	if (stat(dstdir, &sb) == -1) {
		perror(dstdir);
		dohelp(EXIT_FAILURE);	// no return from dohelp()
	}

	// 6. Ensure that this is a dir
	if (!S_ISDIR(sb.st_mode)) {
		fprintf(stderr, "%s is not a direcory.\n", dstdir);
		dohelp(EXIT_FAILURE);	// no return from dohelp()
	}

	fnv = makefilenamelist("/tmp/", argv, 6);	// workfiles in /tmp/

	// process source dir
	fpo = dofopen(fnv[0], "w");
	recursedir(srcdir, fpo);
	fclose(fpo);
	dosetlang();	// $LANG to be "C", so sort works in ascii order.
	sprintf(command, "sort %s > %s", fnv[0], fnv[1]);
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

	// process the file path lists
	fpo = dofopen(fnv[4], "w");	// candidates for deletion.
	if (verbose) fprintf(stdout, "Pass1\n");
	pass1(srcfdat, dstfdat, fpo);
	fclose(fpo);
	sprintf(command, "sort -r %s > %s", fnv[4], fnv[5]);
	dosystem(command);	// reverse order so files are presented before
						// containing directories.
	// free the workfile data
	free(dstfdat.from);
	free(srcfdat.from);

	// deal with the deletions

	/* Necessary change:
	 * Because any char except '/' can be used in a filename, it is
	 * forever impossible to create a pathend string that will not
	 * fubar on some system because a dir is to be deleted before it's
	 * contained file. So, I will do 2 passes over the existing list,
	 * the first deleting only file/symlink objects, and the second
	 * removing the empty dirs.
	*/

	delfdat = readfile(fnv[5], 0, 1);
	mem2str(delfdat.from, delfdat.to, &dummy);
	if (verbose) fprintf(stdout, "Pass2\n");
	pass2(delfdat, ' ');	// only files and symlinks.
	pass2(delfdat, 'd');	// only dirs.
	free(delfdat.from);

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
	fprintf(fpo, "%s%s %c\n", headdir, pathend, 'd');
	while((de = readdir(dirp))) {
		if (strcmp(de->d_name, "..") == 0) continue;
		if (strcmp(de->d_name, ".") == 0) continue;

		switch(de->d_type) {
			char newpath[FILENAME_MAX];
			char ftyp;
			// Nothing to do for these.
			case DT_BLK:
			case DT_CHR:
			case DT_FIFO:
			case DT_SOCK:
			break;
			// For these I just list them with appropriate flags
			case DT_LNK:
			case DT_REG:
			strcpy(newpath, headdir);
			strcat(newpath, de->d_name);
			// symlinks are simply recorded, broken?? I don't care here.
			if (de->d_type == DT_LNK){
				ftyp = 's';
			} else if (de->d_type == DT_REG){
				ftyp = 'f';
			} else ftyp = 0;	// stops gcc warning.
			// now just record the thing
			fprintf(fpo, "%s%s %c\n", newpath, pathend, ftyp);
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

static void *domalloc(size_t sz)
{
	// malloc() with error handling
	void *result = malloc(sz);
	if (!result) {
		reporterror("domalloc(): ", "malloc", 1);
	}
	return result;
} // domalloc()

char *dostrdup(const char *s)
{	// strdup() with error handling
	char *dst = strdup(s);
	if(!dst) {
		reporterror("dostrdup(): ", "strdup", 1);
	}
	return dst;
} // dostrdup()

void dorealpath(char *givenpath, char *resolvedpath)
{	// realpath() witherror handling.
	if(!(realpath(givenpath, resolvedpath))) {
		reporterror("dorealpath(): ", givenpath, 1);
	}
} // dorealpath()

void dosystem(const char *cmd)
{
    const int status = system(cmd);

    if (status == -1) {
        fprintf(stderr, "System to execute: %s\n", cmd);
        exit(EXIT_FAILURE);
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        fprintf(stderr, "%s failed with non-zero exit\n", cmd);
        exit(EXIT_FAILURE);
    }

    return;
} // dosystem()

void dosetlang(void)
{
	// setenv() constant values with error handling.
	// Must use LC_ALL to stop sort producing garbage.

	if (setenv("LC_ALL", "C", 1) == -1) {
		reporterror("dosetlang(): ", "setenv(LC_ALL, C)", 1);
	}
} // dosetlang()

void pass1(struct fdata srcfdat, struct fdata dstfdat, FILE *fpo)
{
	// traverse the lists, creating dirs in dst as required
	// linking files in dst as required including deleting files
	// and linking if inodes differ. Also make a list of candidates
	// for deletion in the destination.

	char *srcline, *dstline;
	char srcobject[NAME_MAX], dstobject[NAME_MAX],
		srcroot[PATH_MAX], dstroot[PATH_MAX];
	int srcobjtyp, dstobjtyp, srlen, drlen;

	// source and destination path roots and length of them.
	srcline = srcfdat.from;
	dstline = dstfdat.from;
	lineparts(srcline, srcroot, srcobject, &srcobjtyp);
	lineparts(dstline, dstroot, dstobject, &dstobjtyp);

	srlen = strlen(srcroot);
	drlen = strlen(dstroot);

	while(srcline < srcfdat.to && dstline < dstfdat.to) {
		char srcpath[PATH_MAX], dstpath[PATH_MAX];
		char *srcptr, *dstptr;
		int srcobj, dstobj;

		twoparts(srcline, srcpath, &srcobj);	// sometimes redundant
		twoparts(dstline, dstpath, &dstobj);

		srcptr = srcpath + srlen;
		dstptr = dstpath + drlen;
		if (strcmp(srcptr, dstptr) < 0) {
			// source has something that dest does not.
			do {
				if (verbose) fprintf(stdout, "Adding: %s %c\n",
									srcpath, srcobj);
				addobject(dstroot, srcpath, srcptr, srcobj);
				srcline += strlen(srcline) + 1;
				if (srcline >= srcfdat.to) break;
				twoparts(srcline, srcpath, &srcobj);
				srcptr = srcpath + srlen;
			} while (strcmp(srcptr, dstptr) < 0);
		} else if (strcmp(srcptr, dstptr) > 0) {
			// dest has stuff that source does not.
			do {
				if (verbose) fprintf(stdout, "Deleting: %s\n", dstline);
				fprintf(fpo, "%s\n", dstline);
				dstline += strlen(dstline) + 1;
				if (dstline >= dstfdat.to) break;
				twoparts(dstline, dstpath, &dstobj);
				dstptr = dstpath + drlen;
			} while (strcmp(srcptr, dstptr) > 0);
		} else {
			// source and dest have the same named object, check link.
			do {
				if (srcobj != 'd') {	// nothing to do for dirs.
					if (verbose) fprintf(stdout, "Checking:"
					"%s %c\n%s %c\n", srcpath, srcobj, dstpath, dstobj);
					inocheck(srcpath, dstpath, srcobj);
				}
				srcline += strlen(srcline) + 1;
				dstline += strlen(dstline) + 1;
				if (srcline >= srcfdat.to ||
					dstline >= dstfdat.to) break;
				twoparts(srcline, srcpath, &srcobj);
				srcptr = srcpath + srlen;
				twoparts(dstline, dstpath, &dstobj);
				dstptr = dstpath + drlen;
			} while (strcmp(srcptr, dstptr) == 0);
		} // if (strcmp ...
	} // while(srcline...)

	// If source and dest have identical structure neither of the next 2
	// loops will execute, otherwise just 1 of the 2 will.

	while (srcline < srcfdat.to ) { // source has stuff not in dest.
		char srcpath[PATH_MAX];
		char *srcptr;
		int srcobj;

		if (verbose) fprintf(stdout, "Adding: %s %c\n",
								srcpath, srcobj);
		twoparts(srcline, srcpath, &srcobj);
		srcptr = srcpath + srlen;
		addobject(dstroot, srcpath, srcptr, srcobj);
		srcline += strlen(srcline) + 1;
	} // while(srcline...)

	while (dstline < dstfdat.to ) { // dest has stuff not in source.
		if (verbose) fprintf(stdout, "Deleting: %s\n", dstline);
		fprintf(fpo, "%s\n", dstline);	// list of deletions
		dstline += strlen(dstline) + 1;
	} // while(dstline...)
} // pass1()

void lineparts(char *origin, char *root, char *fsobject,
						int *objtyp)
{
	//
	char *cp, *dtp;
	int dt, len;

	strcpy(root, origin);
	// "ÿÿÿÿ" f
	cp = strstr(root, pathend);
	if (!cp) {
		fprintf(stderr, "Corrupt file line: %s\n", root);
		exit(EXIT_FAILURE);
	}
	*cp = '\0';	// path1 now a fully path to file|dir|symlink
	cp += strlen(pathend) + 1;
	dt = cp[0];	// the object type s|d|f
	*objtyp = dt;

	len = strlen(root);
	if (root[len - 1] == '/') root[len-1] = '\0';
	dtp = strrchr(root, '/');	// don't believe it can fail but..
	if (!dtp) {
		strcpy(fsobject, root);
		root[0] = '\0';
	} else {
		strcpy(fsobject, (dtp + 1));
		*(dtp + 1) = '\0';
	}

	// terminate a final dir with '/'
	if (dt == 'd') {
		len = strlen(fsobject);
		if (fsobject[len - 1] !=  '/') {
			strcat(fsobject, "/");
		}
	}
} // lineparts()

void addobject(char *dstroot, char *srcpath, char *srcptr, int objtyp)
{	// add a dir or make a hard link.
	struct stat sb;
	mode_t themode;
	char buf[PATH_MAX];

	sprintf(buf, "%s%s", dstroot, srcptr);
	switch (objtyp) {
		case 'd':	// make a new dir
		if (stat(srcpath, &sb) == -1) {
			reporterror("Failure, addobject()/stat(): ", srcpath, 0);
		} else {
			themode = sb.st_mode;
			if (mkdir(buf, themode) == -1) {
				reporterror("Failure, addobject()/mkdir(): ",
							srcpath, 0);
			}
		}
		break;
		case 's':
		case 'f':
		if (link(srcpath, buf) == -1) {
			reporterror("Failure, addobject()/link(): ", srcpath, 0);
		}
		break;
		default:
		fprintf(stderr, "Failure: Corrupt source record: %s %c\n",
					srcpath, objtyp);
		break;
	} // switch()
} // addobject()

void inocheck(char *srcpath, char *dstpath, int objtyp)
{	// check inode numbers for both paths, if they differ delete the
	// destination path and then link it from source.
	struct stat sb;
	ino_t srcino, dstino;
	srcino = dstino = 0;

	switch (objtyp) {
		case 'f':
		if (stat(srcpath, &sb) == -1) {
			reporterror("Failure, inocheck()/stat(): ", srcpath, 0);
		} else {
			srcino = sb.st_ino;
		}
		if (stat(dstpath, &sb) == -1) {
			reporterror("Failure, inocheck()/stat(): ", dstpath, 0);
		} else {
			dstino = sb.st_ino;
		}
		break;
		case 's':
		if (lstat(srcpath, &sb) == -1) {
			reporterror("Failure, inocheck()/lstat(): ", srcpath, 0);
		} else {
			srcino = sb.st_ino;
		}
		if (lstat(dstpath, &sb) == -1) {
			reporterror("Failure, inocheck()/lstat(): ", dstpath, 0);
		} else {
			dstino = sb.st_ino;
		}
		break;
		default:
		fprintf(stderr, "Data has invalid object type: %s %s %c\n",
					srcpath, dstpath, objtyp);
		exit(EXIT_FAILURE);
		break;
	}
	if (srcino && dstino) {
		if (srcino != dstino) {
			if (unlink(dstpath) == -1) {
				reporterror("Failure, inocheck()/unlink(): ",
							dstpath, 0);
			}
			sync();	/* TODO,try removing this and see if the link()
					fails. */
			if (link(srcpath, dstpath) == -1) {
				reporterror("Failure, inocheck()/link(): ", srcpath, 0);
			}
		}
	}
} // inocheck()

void twoparts(char *line, char *path, int *fsobj)
{	// separate the path from the object type.
	char buf[PATH_MAX];
	int fsot;
	char *cp;

	strcpy(buf, line);
	cp = strstr(buf, pathend);
	if (!cp) {
		fprintf(stderr, "Corrupt data line in twoparts(): %s\n", buf);
		exit(EXIT_FAILURE);
	}
	*cp = '\0';
	strcpy(path, buf);

	cp += strlen(pathend) + 1;
	fsot = *cp;
	*fsobj = fsot;
} // twoparts()

void pass2(struct fdata delfdat, char acton)
{
	// data is sorted in reverse ascii order so file objects will be
	// presented before their containing dirs.
	char delpath[PATH_MAX];
	char *line;
	int objtyp;

	line = delfdat.from;
	while (line < delfdat.to) {
		twoparts(line, delpath, &objtyp);
		switch (objtyp) {
			case 'd':
			if (acton != 'd') break;
			if (rmdir(delpath) == -1) {
				reporterror("Failure, pass2()/rmdir(): ", delpath, 0);
			}
			break;
			case 'f':
			case 's':
			if (acton == 'd') break;
			if (unlink(delpath) == -1) {
				reporterror("Failure, pass2()/unlink(): ", delpath, 0);
			}
			break;
			default:
			fprintf(stderr, "Corrupt data line in pass2\n%s\n", line);
			exit(EXIT_FAILURE);
			break;
		} // switch()
		line += strlen(line) + 1;
	}
} // pass2()

void reporterror(const char *module, const char *perrorstr,	int fatal)
{	// enhanced error reporting.
	fputs(module, stderr);
	perror(perrorstr);
	if(fatal) exit(EXIT_FAILURE);
} // reporterror()
