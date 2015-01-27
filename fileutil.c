/*      fileutil.c
 *
 *	Copyright 2014 Bob Parker <rlp1938@gmail.com>
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

#include "fileutil.h"

#include <stdlib.h>
#include <string.h>

struct fdata readfile(const char *filename, off_t extra, int fatal)
{
	FILE *fpi;
	off_t bytesread;
	char *from, *to;
	struct fdata data;
	struct stat sb;

	if (stat(filename, &sb) == -1) {
		if (fatal){
			perror(filename);
			exit(EXIT_FAILURE);
		} else {
			data.from = (char *)NULL;
			data.to = (char *)NULL;
			return data;
		}
	}
	fpi = fopen(filename, "r");
	if(!(fpi)) {
		perror(filename);
		exit(EXIT_FAILURE);
	}

	from = malloc(sb.st_size + extra);
	if (!(from)) {
		perror("malloc failure");
		exit(EXIT_FAILURE);
	}

	bytesread = fread(from, 1, sb.st_size, fpi);
	if (bytesread != sb.st_size) {
		perror("fread");
		exit(EXIT_FAILURE);
	}

        fclose (fpi);

	to = from + bytesread + extra;
	// zero the extra space
	memset(from+bytesread, 0, to-(from+bytesread));
	data.from = from;
	data.to = to;
	return data;
} // readfile()

FILE *dofopen(const char *path, const char *mode)
{
	// fopen with error handling
	FILE *fp = fopen(path, mode);
	if(!(fp)){
		perror(path);
		exit(EXIT_FAILURE);
	}
	return fp;
} // dofopen()

int filexists(const char *path, off_t *fsize)
{	// returns 0 if, -1 otherwise.
	struct stat sb;
	int res;

	res = stat(path, &sb);
	if (res == -1) return -1;
	/* This will return true only if path looks at a regular file
	 * or a symlink pointing to one such.
	*/
	if (S_ISREG(sb.st_mode)) {
		// if symlink then mode is that of the target
		*fsize = sb.st_size;
		return 0;
	}
	return -1;
} // filexists()

int direxists(const char *path)
{	// return 0 if so, -1 otherwise.
	struct stat sb;

	if (stat(path, &sb) == -1) return -1;
	/* This will return true only if path looks at a dir.
	*/
	if (S_ISDIR(sb.st_mode)) {
		return 0;
	}
	return -1;
} // direxists()

void dorename(char *oldname, char *newname)
{	// just rename() with error handling.
	if (rename(oldname, newname) == -1) {
		perror(newname);
		exit(EXIT_FAILURE);
	}
} // dorename()

void writefile(const char *fname, char *fro, const char *to)
{	// invokes fwrite()
	/* TODO: Consider that I should bring mode into this, w|a so I can
	 * append as well as write, or whether I have appendfile()
	 * function instead.
	* */
	FILE *fpo;
	size_t siz, result;

	siz = to - fro;
	if (strcmp(fname, "-") == 0) {
		/* I have had some impossibility writing to stdout after
		 * redirecting it using freopen(). I worked around that by
		 * explicitly opening a named file to take some output.
		 * I should just use fwrite() on this also.
		*/
		char *cp = fro;
		while (cp < to) {
			putchar(*cp);
			cp++;
		}
	} else {
		fpo = dofopen(fname, "w");
		result = fwrite(fro, 1, siz, fpo);
		if (result != siz) {
			fprintf(stderr, "Size discrepancy in fwrite: %s %zu, %zu",
				fname, siz, result);
			perror(fname);	// might produce something useful.
			exit(EXIT_FAILURE);
		}
	fclose(fpo);
	}
} // writefile()


FILE *dofreopen(const char *path, const char *mode, FILE *fp)
{	// just freopen() with error handling.
	FILE *fpo = freopen(path, mode, fp);
	if(!(fp)){
		perror(path);
		exit(EXIT_FAILURE);
	}
	return fpo;
} // dofreopen()

void dounlink(const char *fname)
{
	if (unlink(fname) == -1) {
		perror(fname);
		exit(EXIT_FAILURE);
	}
} // dounlink()

void mem2str(char *fr, char *to, int *lcount)
{	// replace all '\n' with '\0' and count the replacements.
	int lc;
	char *cp;

	cp = fr;
	lc = 0;
	while (cp < to) {
		if (*cp == '\n') {
			*cp = '\0';
			lc++;
		}
		cp++;
	}
	*lcount = lc;
} // mem2str()

void cpfile(const char *fr, const char *to)
{
	struct fdata frdat = readfile(fr, 0, 1);
	writefile(to, frdat.from, frdat.to);
	free(frdat.from);
}
