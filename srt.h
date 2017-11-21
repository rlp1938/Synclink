/*    srt.h
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
 * */
#ifndef _SRT_H
#define _SRT_H
#define _GNU_SOURCE 1

#include "str.h"	// should cover all required bases

int
strrcmp(const char *s1, const char *s2);	// sort descending

int
cmpstringp(const void *p1, const void *p2);	// for qsort() ascending

int
cmpstringd(const void *p1, const void *p2);	// for qsort() decsending

void
sortmemstr(mdata *md, int direction);

void
mergesort(char **a, int i, int j, char **work);

void
merge(char **a, int i1, int j1, int i2, int j2, char **work);

#endif
