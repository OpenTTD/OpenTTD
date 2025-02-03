/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/* 1.5 adds a game element reference to the news. */
GSNews._Create <- GSNews.Create;
GSNews.Create <- function(type, text, company)
{
    return GSNews._Create(type, text, company, GSNews.NR_NONE, 0);
}

require("compat_1.5.nut")
