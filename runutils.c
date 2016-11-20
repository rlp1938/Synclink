/*      runutils.c
 *
 *	Copyright 23.12.2014 Bob Parker <rlp1938@gmail.com>
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

#include "runutils.h"
#include <unistd.h>

void firstrun(char *frtext, char *srcdir, char *dstdir, ...)
{	/* Display frtext, then for as many srcfiles exist, copy them from
	srcdir to dstdir.
	* NB the last non-optional argument will be srcfile1.
	*/
	va_list argp;
	char from[PATH_MAX], to [PATH_MAX];
	char *cp;
	FILE *fpo;
	int dispcols = 70;

	display(frtext, dispcols);

	// make sure that the destination dir exists.
	if (direxists(dstdir) == -1) {
		sprintf(to, "mkdir -p %s", dstdir);
		dosystem(to);
	}
	// There must be one file spec, maybe more.
	va_start(argp, dstdir);
	while(1) {
		cp = va_arg(argp, char*);
		if (!cp) break;
		sprintf(from, "%s%s", srcdir, cp);
		sprintf(to, "%s%s", dstdir, cp);
		rufd = readfile(from, 0, 1);
		fpo = dofopen(to, "w");
		fwrite(rufd.from, 1, rufd.to - rufd.from, fpo);
		fclose(fpo);
		free(rufd.from);
	}
	va_end(argp);
} // firstrun()

void dogetenv(const char *param, char *result)
{ // getenv with error handling
	char *cp = getenv(param);
	if (!cp) {
		reporterror("dogetenv(): ", param, 1);
	}
	strcpy(result, cp);
} // dogetenv()

void reporterror(const char *module, const char *perrorstr,	int fatal)
{	// enhanced error reporting.
	fputs(module, stderr);
	perror(perrorstr);
	if(fatal) exit(EXIT_FAILURE);
} // reporterror()

void dosetlang(void)
{
	// setenv() constant values with error handling.
	// Must use LC_ALL to stop sort producing garbage.
	// Also makes date() produce English, not Thai.

	if (setenv("LC_ALL", "C", 1) == -1) {
		reporterror("dosetlang(): ", "setenv(LC_ALL, C)", 1);
	}
} // dosetlang()

void display(char *disptext, unsigned int cols)
{	// display text with leading tab max width cols.
	char *cpf, *cpt;
	int len;
	char buf[128];
	char *buf1 = buf + 1;

	buf[0] = '\t';
	cpf = disptext;
	len = strlen(disptext);
	while(1) {
		if (strlen(cpf) <= cols) {
			fprintf(stdout, "\t%s\n", cpf);
			break;
		}
		cpt = cpf + cols;
		while(!isblank(*cpt)) cpt--;
		len = cpt - cpf;
		strncpy(buf1, cpf, len);
		buf1[len] = '\n';
		buf1[len+1] = '\0';
		fputs(buf, stdout);
		cpf = cpt + 1;
	}
} // display()

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

void trace(char *tracefilename, char *tracetext)
{	// record the progress of a running program
	FILE *fpo;

	fpo = dofopen(tracefilename, "a");
	fprintf(fpo, "%s\n", tracetext);
	fclose(fpo);
} // trace()


char *dostrdup(const char *s)
{	// strdup() with error handling
	char *dst = strdup(s);
	if(!dst) {
		reporterror("dostrdup(): ", "strdup", 1);
	}
	return dst;
} // dostrdup()

void stripws(char *target)
{	// strip white space if any from target
	size_t len;
	char *wrk, *cpf, *cpt;

	len = strlen(target);
	wrk = malloc(len + 1);
	if (!wrk) {
		perror("malloc()");
		exit(EXIT_FAILURE);
	}
	cpf = target;
	cpt = wrk;
	while(*cpf) {
		if (isspace(*cpf) == 0) {
			*cpt = *cpf;
			cpt++;
		}
		cpf++;
	} // while()
	*cpt = '\0';
	strcpy(target, wrk);
	free(wrk);
} // stripws()

void *domalloc(size_t sz)
{
	// malloc() with error handling
	void *result = malloc(sz);
	if (!result) {
		reporterror("domalloc(): ", "malloc", 1);
	}
	return result;
} // domalloc()

void dorealpath(char *givenpath, char *resolvedpath)
{	/* Some lunatic has fucked up get_current_dir_name() which I need
	 * to deal with givenpath that has shell short cuts like '.' '..'
	 * etc, so I'll just roll my own realpath().
	* */
	if (givenpath[0] == '/') { // a realpath has been given.
		strcpy(resolvedpath, givenpath);
		return;
	}
	char *cwd = getenv("PWD");
	char *buf = domalloc(strlen(givenpath) + strlen(cwd) + 2);
	strcpy(buf, cwd);
	size_t l = strlen(buf);
	if (buf[l-1] != '/') strcat(buf, "/");
	if (strcmp(givenpath, ".") == 0) {
		strcpy(resolvedpath, buf);
		free(buf);
		return;
	}
	/* Deal with abberations like ../../ etc*/
	if (strncmp("..", givenpath, 2) == 0) {
		buf[l-1] = 0;	// get rid of final '/'
		char *cp = givenpath;
		while (strncmp("..", cp, 2) == 0) {
			char *ep = strrchr(buf, '/');
			if (*ep) *ep = 0; else break;
			cp += 2;
			if (*cp == '/') cp++;
		}
		strcat(buf, "/");
		if(strlen(cp)) strcat(buf, cp);
	} else if (strncmp("./", givenpath, 2) == 0) {
		char *cp = givenpath + 2;
		if (strlen(cp)) strcat(buf, cp);
	} else {
		strcat(buf, givenpath);
	}
	strcpy(resolvedpath, buf);
	free(buf);
} // dorealpath()
