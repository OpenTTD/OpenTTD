/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file splash.h Functions to support splash screens for OSX. */

#ifndef SPLASH_H
#define SPLASH_H

#define SPLASH_IMAGE_FILE		"splash.png"

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

	void DisplaySplashImage();

#ifdef __cplusplus
}
#endif //__cplusplus

#endif /* SPLASH_H */
