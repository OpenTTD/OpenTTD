/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file game_info.cpp Functions to convert NetworkGameInfo to Packet and back.
 */

#include "../../stdafx.h"
#include "game_info.h"
#include "../../core/bitmath_func.hpp"
#include "../../company_base.h"
#include "../../date_func.h"
#include "../../debug.h"
#include "../../map_func.h"
#include "../../settings_type.h"
#include "../../string_func.h"
#include "../../rev.h"
#include "../network_func.h"
#include "../network.h"
#include "packet.h"

#include "../../safeguards.h"


/**
 * How many hex digits of the git hash to include in network revision string.
 * Determined as 10 hex digits + 2 characters for -g/-u/-m prefix.
 */
static const uint GITHASH_SUFFIX_LEN = 12;

NetworkServerGameInfo _network_game_info; ///< Information about our game.

/**
 * Get the network version string used by this build.
 * The returned string is guaranteed to be at most NETWORK_REVISON_LENGTH bytes.
 */
const char *GetNetworkRevisionString()
{
	/* This will be allocated on heap and never free'd, but only once so not a "real" leak. */
	static char *network_revision = nullptr;

	if (!network_revision) {
		/* Start by taking a chance on the full revision string. */
		network_revision = stredup(_openttd_revision);
		/* Ensure it's not longer than the packet buffer length. */
		if (strlen(network_revision) >= NETWORK_REVISION_LENGTH) network_revision[NETWORK_REVISION_LENGTH - 1] = '\0';

		/* Tag names are not mangled further. */
		if (_openttd_revision_tagged) {
			DEBUG(net, 1, "Network revision name is '%s'", network_revision);
			return network_revision;
		}

		/* Prepare a prefix of the git hash.
		 * Size is length + 1 for terminator, +2 for -g prefix. */
		assert(_openttd_revision_modified < 3);
		char githash_suffix[GITHASH_SUFFIX_LEN + 1] = "-";
		githash_suffix[1] = "gum"[_openttd_revision_modified];
		for (uint i = 2; i < GITHASH_SUFFIX_LEN; i++) {
			githash_suffix[i] = _openttd_revision_hash[i-2];
		}

		/* Where did the hash start in the original string?
		 * Overwrite from that position, unless that would go past end of packet buffer length. */
		ptrdiff_t hashofs = strrchr(_openttd_revision, '-') - _openttd_revision;
		if (hashofs + strlen(githash_suffix) + 1 > NETWORK_REVISION_LENGTH) hashofs = strlen(network_revision) - strlen(githash_suffix);
		/* Replace the git hash in revision string. */
		strecpy(network_revision + hashofs, githash_suffix, network_revision + NETWORK_REVISION_LENGTH);
		assert(strlen(network_revision) < NETWORK_REVISION_LENGTH); // strlen does not include terminator, constant does, hence strictly less than
		DEBUG(net, 1, "Network revision name is '%s'", network_revision);
	}

	return network_revision;
}

/**
 * Extract the git hash from the revision string.
 * @param revstr The revision string (formatted as DATE-BRANCH-GITHASH).
 * @return The git has part of the revision.
 */
static const char *ExtractNetworkRevisionHash(const char *revstr)
{
	return strrchr(revstr, '-');
}

/**
 * Checks whether the given version string is compatible with our version.
 * First tries to match the full string, if that fails, attempts to compare just git hashes.
 * @param other the version string to compare to
 */
bool IsNetworkCompatibleVersion(const char *other)
{
	if (strncmp(GetNetworkRevisionString(), other, NETWORK_REVISION_LENGTH - 1) == 0) return true;

	/* If this version is tagged, then the revision string must be a complete match,
	 * since there is no git hash suffix in it.
	 * This is needed to avoid situations like "1.9.0-beta1" comparing equal to "2.0.0-beta1".  */
	if (_openttd_revision_tagged) return false;

	const char *hash1 = ExtractNetworkRevisionHash(GetNetworkRevisionString());
	const char *hash2 = ExtractNetworkRevisionHash(other);
	return hash1 != nullptr && hash2 != nullptr && strncmp(hash1, hash2, GITHASH_SUFFIX_LEN) == 0;
}

/**
 * Check if an game entry is compatible with our client.
 */
void CheckGameCompatibility(NetworkGameInfo &ngi)
{
	/* Check if we are allowed on this server based on the revision-check. */
	ngi.version_compatible = IsNetworkCompatibleVersion(ngi.server_revision);
	ngi.compatible = ngi.version_compatible;

	/* Check if we have all the GRFs on the client-system too. */
	for (const GRFConfig *c = ngi.grfconfig; c != nullptr; c = c->next) {
		if (c->status == GCS_NOT_FOUND) ngi.compatible = false;
	}
}

/**
 * Fill a NetworkGameInfo structure with the latest information of the server.
 * @param ngi the NetworkGameInfo struct to fill with data.
 */
void FillNetworkGameInfo(NetworkGameInfo &ngi)
{
	/* Update some game_info */
	ngi.clients_on     = _network_game_info.clients_on;
	ngi.start_date     = ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1);

	ngi.use_password   = !StrEmpty(_settings_client.network.server_password);
	ngi.clients_max    = _settings_client.network.max_clients;
	ngi.companies_on   = (byte)Company::GetNumItems();
	ngi.companies_max  = _settings_client.network.max_companies;
	ngi.spectators_on  = NetworkSpectatorCount();
	ngi.spectators_max = _settings_client.network.max_spectators;
	ngi.game_date      = _date;
	ngi.map_width      = MapSizeX();
	ngi.map_height     = MapSizeY();
	ngi.map_set        = _settings_game.game_creation.landscape;
	ngi.dedicated      = _network_dedicated;
	ngi.grfconfig      = _grfconfig;

	strecpy(ngi.server_name, _settings_client.network.server_name, lastof(ngi.server_name));
	strecpy(ngi.server_revision, GetNetworkRevisionString(), lastof(ngi.server_revision));
}

/**
 * Function that is called for every GRFConfig that is read when receiving
 * a NetworkGameInfo. Only grfid and md5sum are set, the rest is zero. This
 * function must set all appropriate fields. This GRF is later appended to
 * the grfconfig list of the NetworkGameInfo.
 * @param config the GRF to handle.
 */
static void HandleIncomingNetworkGameInfoGRFConfig(GRFConfig *config)
{
	/* Find the matching GRF file */
	const GRFConfig *f = FindGRFConfig(config->ident.grfid, FGCM_EXACT, config->ident.md5sum);
	if (f == nullptr) {
		/* Don't know the GRF, so mark game incompatible and the (possibly)
		 * already resolved name for this GRF (another server has sent the
		 * name of the GRF already */
		config->name = FindUnknownGRFName(config->ident.grfid, config->ident.md5sum, true);
		config->status = GCS_NOT_FOUND;
	} else {
		config->filename = f->filename;
		config->name = f->name;
		config->info = f->info;
		config->url = f->url;
	}
	SetBit(config->flags, GCF_COPY);
}

/**
 * Serializes the NetworkGameInfo struct to the packet.
 * @param p    the packet to write the data to.
 * @param info the NetworkGameInfo struct to serialize from.
 */
void SerializeNetworkGameInfo(Packet *p, const NetworkGameInfo *info)
{
	p->Send_uint8 (NETWORK_GAME_INFO_VERSION);

	/*
	 *              Please observe the order.
	 * The parts must be read in the same order as they are sent!
	 */

	/* Update the documentation in game_info.h on changes
	 * to the NetworkGameInfo wire-protocol! */

	/* NETWORK_GAME_INFO_VERSION = 4 */
	{
		/* Only send the GRF Identification (GRF_ID and MD5 checksum) of
		 * the GRFs that are needed, i.e. the ones that the server has
		 * selected in the NewGRF GUI and not the ones that are used due
		 * to the fact that they are in [newgrf-static] in openttd.cfg */
		const GRFConfig *c;
		uint count = 0;

		/* Count number of GRFs to send information about */
		for (c = info->grfconfig; c != nullptr; c = c->next) {
			if (!HasBit(c->flags, GCF_STATIC)) count++;
		}
		p->Send_uint8 (count); // Send number of GRFs

		/* Send actual GRF Identifications */
		for (c = info->grfconfig; c != nullptr; c = c->next) {
			if (!HasBit(c->flags, GCF_STATIC)) SerializeGRFIdentifier(p, &c->ident);
		}
	}

	/* NETWORK_GAME_INFO_VERSION = 3 */
	p->Send_uint32(info->game_date);
	p->Send_uint32(info->start_date);

	/* NETWORK_GAME_INFO_VERSION = 2 */
	p->Send_uint8 (info->companies_max);
	p->Send_uint8 (info->companies_on);
	p->Send_uint8 (info->spectators_max);

	/* NETWORK_GAME_INFO_VERSION = 1 */
	p->Send_string(info->server_name);
	p->Send_string(info->server_revision);
	p->Send_uint8 (0); // Used to be server-lang.
	p->Send_bool  (info->use_password);
	p->Send_uint8 (info->clients_max);
	p->Send_uint8 (info->clients_on);
	p->Send_uint8 (info->spectators_on);
	p->Send_string(""); // Used to be map-name.
	p->Send_uint16(info->map_width);
	p->Send_uint16(info->map_height);
	p->Send_uint8 (info->map_set);
	p->Send_bool  (info->dedicated);
}

/**
 * Deserializes the NetworkGameInfo struct from the packet.
 * @param p    the packet to read the data from.
 * @param info the NetworkGameInfo to deserialize into.
 */
void DeserializeNetworkGameInfo(Packet *p, NetworkGameInfo *info)
{
	static const Date MAX_DATE = ConvertYMDToDate(MAX_YEAR, 11, 31); // December is month 11

	info->game_info_version = p->Recv_uint8();

	/*
	 *              Please observe the order.
	 * The parts must be read in the same order as they are sent!
	 */

	/* Update the documentation in game_info.h on changes
	 * to the NetworkGameInfo wire-protocol! */

	switch (info->game_info_version) {
		case 4: {
			GRFConfig **dst = &info->grfconfig;
			uint i;
			uint num_grfs = p->Recv_uint8();

			/* Broken/bad data. It cannot have that many NewGRFs. */
			if (num_grfs > NETWORK_MAX_GRF_COUNT) return;

			for (i = 0; i < num_grfs; i++) {
				GRFConfig *c = new GRFConfig();
				DeserializeGRFIdentifier(p, &c->ident);
				HandleIncomingNetworkGameInfoGRFConfig(c);

				/* Append GRFConfig to the list */
				*dst = c;
				dst = &c->next;
			}
			FALLTHROUGH;
		}

		case 3:
			info->game_date      = Clamp(p->Recv_uint32(), 0, MAX_DATE);
			info->start_date     = Clamp(p->Recv_uint32(), 0, MAX_DATE);
			FALLTHROUGH;

		case 2:
			info->companies_max  = p->Recv_uint8 ();
			info->companies_on   = p->Recv_uint8 ();
			info->spectators_max = p->Recv_uint8 ();
			FALLTHROUGH;

		case 1:
			p->Recv_string(info->server_name,     sizeof(info->server_name));
			p->Recv_string(info->server_revision, sizeof(info->server_revision));
			p->Recv_uint8 (); // Used to contain server-lang.
			info->use_password   = p->Recv_bool  ();
			info->clients_max    = p->Recv_uint8 ();
			info->clients_on     = p->Recv_uint8 ();
			info->spectators_on  = p->Recv_uint8 ();
			if (info->game_info_version < 3) { // 16 bits dates got scrapped and are read earlier
				info->game_date    = p->Recv_uint16() + DAYS_TILL_ORIGINAL_BASE_YEAR;
				info->start_date   = p->Recv_uint16() + DAYS_TILL_ORIGINAL_BASE_YEAR;
			}
			while (p->Recv_uint8() != 0) {} // Used to contain the map-name.
			info->map_width      = p->Recv_uint16();
			info->map_height     = p->Recv_uint16();
			info->map_set        = p->Recv_uint8 ();
			info->dedicated      = p->Recv_bool  ();

			if (info->map_set     >= NETWORK_NUM_LANDSCAPES) info->map_set = 0;
	}
}

/**
 * Serializes the GRFIdentifier (GRF ID and MD5 checksum) to the packet
 * @param p    the packet to write the data to.
 * @param grf  the GRFIdentifier to serialize.
 */
void SerializeGRFIdentifier(Packet *p, const GRFIdentifier *grf)
{
	uint j;
	p->Send_uint32(grf->grfid);
	for (j = 0; j < sizeof(grf->md5sum); j++) {
		p->Send_uint8(grf->md5sum[j]);
	}
}

/**
 * Deserializes the GRFIdentifier (GRF ID and MD5 checksum) from the packet
 * @param p    the packet to read the data from.
 * @param grf  the GRFIdentifier to deserialize.
 */
void DeserializeGRFIdentifier(Packet *p, GRFIdentifier *grf)
{
	uint j;
	grf->grfid = p->Recv_uint32();
	for (j = 0; j < sizeof(grf->md5sum); j++) {
		grf->md5sum[j] = p->Recv_uint8();
	}
}
