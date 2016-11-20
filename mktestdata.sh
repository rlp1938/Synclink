#!/bin/bash
#
# mktestdata.sh - script to generate test data for synclink.
#
# Copyright 2016 Robert L (Bob) Parker rlp1938@gmail.com
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.# See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA 02110-1301, USA.
#
if [[ -d Test ]]
then
	rm -rf Test
fi

# set up dirs
mkdir -p Test/from Test/to
mkdir Test/from/Copied Test/to/Copied
mkdir Test/from/Linked Test/to/Linked
mkdir Test/from/Tolink
mkdir -p Test/from/Level1/Level2
mkdir Test/to/DeleteThis/


# create files
echo copied > Test/from/Copied/copied
echo copied > Test/to/Copied/copied
echo linked > Test/from/Linked/linked
ln Test/from/Linked/linked Test/to/Linked
echo tolink > Test/from/Tolink/tolink
echo deletethis > Test/to/DeleteThis/deletethis
echo level2 > Test/from/Level1/Level2/level2
