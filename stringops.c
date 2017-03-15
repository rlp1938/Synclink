/*      stringops.c
 *
 *	Copyright 2016 Bob Parker Bob Parker rlp1938@gmail.com
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

#include "stringops.h"
#include "fileops.h"
char *dostrdup(const char *str)
{	/* strdup() with error handling */
	char *dup = strdup(str);
	if (!dup) {
		perror(str);
		exit(EXIT_FAILURE);
	}
	return dup;
} // dostrdup()

char *getcfgvalue(const char *cfgname, char **cfglines)
{
	int i = 0;
	size_t len = strlen(cfgname);
	while (cfglines[i]) {
		if (strncmp(cfgname, cfglines[i], len) == 0) {
			return dostrdup(cfglines[i] + len + 1);
		}
		i++;
	}
	fprintf(stderr, "No parameter name: %s\n", cfgname);
	exit(EXIT_FAILURE);
} // getcfgvalue()

strdata getdatafromtagnames(char *fro, char *to, char *tagname)
{
	char tagfro[NAME_MAX], tagto[NAME_MAX];
	sprintf(tagfro, "<%s>", tagname);
	sprintf(tagto, "</%s>", tagname);
	strdata sd;
	sd.from = memmem(fro, to - fro, tagfro, strlen(tagfro));
	if (!sd.from) {
		fprintf(stderr, "Tag not found: %s", tagfro);
		exit(EXIT_FAILURE);
	}
	sd.from += strlen(tagfro);	// point to actual data
	sd.to = memmem(sd.from, to - sd.from, tagto,
							strlen(tagto));
	if (!sd.to) {
		fprintf(stderr, "Tag not found: %s", tagto);
		exit(EXIT_FAILURE);
	}
	return sd;
} // getdatafromtagnames()

void trace(const char *fn, char *fmt, ...)
{
	va_list ap;
	char buf[PATH_MAX];
	char line[NAME_MAX];
	char format[NAME_MAX];
	char *bp, *pp, *lp;

	va_start(ap, fmt);
	buf[0] = 0;	// strcat everything
	bp = fmt;
	lp = bp + strlen(fmt);
	while (1) {
		pp = strchr(bp+1, '%');
		int done = 0;
		if (!pp) {
			pp = lp;	// sweep up whatever is at the end of fmt
			done = 1;	// force exit after format identification.
		}
		if (*(pp + 1) == '%') {	// "%%"
			strcat(format, "%");
			pp++;
		}
		strncpy(format, bp, pp - bp);
		format[pp - bp] = 0;
		int thetype = getdatatype(format);
		int ii; long int li; long long int lli; double d;
		long double ld; char *s; void *vp;
		switch (thetype)
		{
			case 0:	// print whatever it is
				strcpy(line, format);
				break;
			case 1:	// integer types
				ii = va_arg(ap, int);
				sprintf(line, format, ii);
				break;
			case 2:	// long int types
				li = va_arg(ap, long int);
				sprintf(line, format, li);
				break;
			case 3:	// long long int types
				lli = va_arg(ap, long long int);
				sprintf(line, format, lli);
				break;
			case 4:	// doubles
				d = va_arg(ap, double);
				sprintf(line, format, d);
				break;
			case 5:	// long doubles
				ld = va_arg(ap, long double);
				sprintf(line, format, ld);
				break;
			case 6:	// char *
				s = va_arg(ap, char *);
				sprintf(line, format, s);
				break;
			case 7:	// void *
				vp = va_arg(ap, void *);
				sprintf(line, format, vp);
				break;
			case 8:	// just print '%'
				strcpy(line, "%");
				break;
			default:	// there are no defaults
				break;
		}	// switch()
		strcat(buf, line);
		if(done) break;
		bp = pp;	// move on to the next
	} // while(1)
	writefile(fn, buf, NULL, "a");
	va_end(ap);
} // trace()

/* trace() - function to trace program execution.
 * writes formatted data to named file, stdout if "-", stderr if "+",
 * otherwise whatever the file is named.
 * Format chars supported:
 * %d	int
 * %c	int
 * %u	unsigned int
 * %ld	long int
 * %lld	long long int
 * %lu	long unsigned int
 * %llu	long long unsigned int
 * %f	float
 * %g	float
 * %e	float
 * %lg	double
 * %lf	double
 * %le	double
 * */
int getdatatype(char *partformat)
{	/* must get past modifiers for most types but afaik the first 2
		don't use them.
	*/
	if (strstr(partformat, "%p")) return 7;	// void *
	if (strstr(partformat, "%%")) return 8;	// just print '%'

	char *cp = partformat;	// reach past modifiers
	cp = strchr(cp, '%');
	if (!cp) return 0;	// just print it all.
	cp++;	// get past '%'
	while (isdigit(*cp)) cp++;	// field width
	if (*cp == '.') cp++;
	while (isdigit(*cp)) cp++;	// precision

	// the next group single char only
	if (*cp == 'd') return 1;	// integer types
	if (*cp == 'u') return 1;
	if (*cp == 'c') return 1;
	if (*cp == 'o') return 1;
	if (*cp == 'x') return 1;
	if (*cp == 'X') return 1;

	if (strncmp(cp, "ld", 2) == 0 ) return 2;	// long integer types
	if (strncmp(cp, "lu", 2) == 0 ) return 2;
	if (strncmp(cp, "lo", 2) == 0 ) return 2;
	if (strncmp(cp, "lx", 2) == 0 ) return 2;
	if (strncmp(cp, "lX", 2) == 0 ) return 2;

	if (strncmp(cp, "lld", 3 ) == 0 )return 3;	// long long types
	if (strncmp(cp, "llu", 3 ) == 0 )return 3;
	if (strncmp(cp, "llo", 3 ) == 0 )return 3;
	if (strncmp(cp, "llx", 3 ) == 0 )return 3;
	if (strncmp(cp, "llX", 3 ) == 0 )return 3;

	if (strncmp(cp, "e", 1 ) == 0 ) return 4;	// double types promoted
	if (strncmp(cp, "f", 1 ) == 0 ) return 4;
	if (strncmp(cp, "g", 1 ) == 0 ) return 4;
	if (strncmp(cp, "le", 2 ) == 0 ) return 4;	// double types native
	if (strncmp(cp, "lE", 2 ) == 0 ) return 4;
	if (strncmp(cp, "lf", 2 ) == 0 ) return 4;
	if (strncmp(cp, "lg", 2 ) == 0 ) return 4;
	if (strncmp(cp, "lG", 2 ) == 0 ) return 4;

	if (strncmp(cp, "lle", 3 ) == 0 ) return 5;	// long double types
	if (strncmp(cp, "llE", 3 ) == 0 ) return 5;
	if (strncmp(cp, "llf", 3 ) == 0 ) return 5;
	if (strncmp(cp, "llg", 3 ) == 0 ) return 5;
	if (strncmp(cp, "llG", 3 ) == 0 ) return 5;

	if (strncmp(cp, "s" , 1) == 0 ) return 6;	// char *
	if (strncmp(cp, "ls", 2) == 0 ) return 6;

	return 0;	// print whatever garbage is in the buffer.
} // getdatatype()

void trace_init(const char *fn)
{	/* If fn == "-" does nothing, else it creates or truncates fn */
	if (strcmp(fn, "-") == 0) return;
	FILE *fp = dofopen(fn, "w");
	dofclose(fp);
}
