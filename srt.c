/*	srt.c
 *
 * Copyright 2017 Robert L (Bob) Parker rlp1938@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

/* The purpose of srt.[h|c] is to provide in memory sort functions.
*/

#include "srt.h"

int
strrcmp(const char *s1, const char *s2)
{/* Sort in reverse order. */
	int res;
	if(strcmp(s1, s2) < 0)
		res = 1;
	else if (strcmp(s1, s2) > 0)
		res = -1;
	else res = 0;
	return res;
} // strrcmp()

int
cmpstringd(const void *p1, const void *p2)
{/* See cmpstringp(). This version is to sort descending. */
	return strrcmp(* (char * const *) p1, * (char * const *) p2);
};	// cmpstringd

int
cmpstringp(const void *p1, const void *p2)
{/* This over complicated mess is to make library function qsort() work.
  * The actual arguments to this function are "pointers to
  * pointers to char", but strcmp(3) arguments are "pointers
  * to char", hence the following cast plus dereference .
  */
	return strcmp(* (char * const *) p1, * (char * const *) p2);
} // cmpstrings()

void
sortmemstr(mdata *md, int direction)
{	/* Uses qsort() to sort a block of C strings in memory.
	 * If direction is 0 the sort is ascending, otherwise descending.
	 * This creates the data structures needed by qsort() and frees
	 * them on completion.
	*/
	size_t count = countmemstr(md);	/* get strings count */
	/* Allocate strings arrays */
	char **strlist = xmalloc(count * sizeof(char *));
	char *cp = md->fro;
	size_t i;
	for (i = 0; i < count; i++) {	/* Fill in strings array */
		strlist[i] = xstrdup(cp);
		cp += strlen(cp) + 1;
	}
	if (direction) { // descending
		qsort(strlist, count, sizeof(char *), cmpstringd);
	} else { // ascending
		qsort(strlist, count, sizeof(char *), cmpstringp);
	}
	cp = md->fro;
	for (i = 0; i < count; i++) {
		strcpy(cp, strlist[i]);
		cp += strlen(cp) + 1;
	}
	/* The strings in strlist[] become unfreeable after qsort(), I guess
	 * because each separate string becomes a different length.
	 * For the moment I will simply let memory leak. It's no big deal
	 * because this is a short run program anyway. */
	// destroystrarray(strlist, count);
	free(strlist);
} // sortmemstr()

void mergesort(char **a, int i, int j, char **work)
{
	int mid;

	if(i < j)
	{
		mid = (i + j) / 2;
		mergesort(a, i, mid, work);	//left recursion
		mergesort(a, mid+1, j, work);	//right recursion
		merge(a, i, mid, mid+1, j, work);	//merge sorted sub-arrays
	}
} // mergesort()

void merge(char **a, int i1, int j1, int i2, int j2, char **work)
{
	char **temp = work;	//array used for merging
	int i, j, k;
	i = i1;	//beginning of the first list
	j = i2;	//beginning of the second list
	k = 0;

	while(i <= j1 && j <= j2)	//while elements in both lists
	{
		if(strcmp(a[i], a[j]) < 0)
			temp[k++] = a[i++];
		else
			temp[k++] = a[j++];
	}

	while(i <= j1)	//copy remaining elements of the first list
		temp[k++] = a[i++];

	while(j <= j2)	//copy remaining elements of the second list
		temp[k++] = a[j++];

	// Transfer elements from temp[] back to a[]
	for(i=i1,j=0; i <= j2; i++,j++)
		a[i] = temp[j];
} // merge()

