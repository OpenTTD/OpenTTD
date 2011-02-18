/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file getoptdata.cpp Library for parsing command line options. */

#include "../stdafx.h"
#include "getoptdata.h"

/**
 * Find the next option.
 * @return Function returns one
 * - An option letter if it found another option.
 * - -1 if option processing is finished.
 * - -2 if an error was encountered.
 */
int GetOptData::GetOpt()
{
	char *s = this->cont;
	if (s != NULL) {
		goto md_continue_here;
	}

	for (;;) {
		if (--this->numleft < 0) return -1;

		s = *this->argv++;
		if (*s == '-') {
md_continue_here:;
			s++;
			if (*s != 0) {
				const char *r;
				/* Found argument, try to locate it in options. */
				if (*s == ':' || (r = strchr(this->options, *s)) == NULL) {
					/* ERROR! */
					return -2;
				}
				if (r[1] == ':') {
					char *t;
					/* Item wants an argument. Check if the argument follows, or if it comes as a separate arg. */
					if (!*(t = s + 1)) {
						/* It comes as a separate arg. Check if out of args? */
						if (--this->numleft < 0 || *(t = *this->argv) == '-') {
							/* Check if item is optional? */
							if (r[2] != ':') return -2;
							this->numleft++;
							t = NULL;
						} else {
							this->argv++;
						}
					}
					this->opt = t;
					this->cont = NULL;
					return *s;
				}
				this->opt = NULL;
				this->cont = s;
				return *s;
			}
		} else {
			/* This is currently not supported. */
			return -2;
		}
	}
}

