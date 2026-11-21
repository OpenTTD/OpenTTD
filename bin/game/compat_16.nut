/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/* This file contains code to downgrade the API from 17 to 16. */

GSCompany.SetPrimaryLiveryColour <- function(scheme, colour) { return GSCompany.SetLiveryColour(scheme, 0, colour); }
GSCompany.SetSecondaryLiveryColour <- function(scheme, colour) { return GSCompany.SetLiveryColour(scheme, 1, colour); }
GSCompany.GetPrimaryLiveryColour <- function(scheme) { return GSCompany.GetLiveryColour(scheme, 0); }
GSCompany.GetSecondaryLiveryColour <- function(scheme) { return GSCompany.GetLiveryColour(scheme, 1); }

GSGroup.SetPrimaryColour <- function(group_id, colour) { return GSGroup.SetColour(group_id, 0, colour); }
GSGroup.SetSecondaryColour <- function(group_id, colour) { return GSGroup.SetColour(group_id, 1, colour); }
GSGroup.GetPrimaryColour <- function(group_id) { return GSGroup.GetColour(group_id, 0); }
GSGroup.GetSecondaryColour <- function(group_id) { return GSGroup.GetColour(group_id, 1); }
