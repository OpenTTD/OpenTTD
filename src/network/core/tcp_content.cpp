/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tcp_content.cpp Basic functions to receive and send Content packets. */

#include "../../stdafx.h"
#include "../../textfile_gui.h"
#include "../../newgrf_config.h"
#include "../../base_media_base.h"
#include "../../base_media_graphics.h"
#include "../../base_media_music.h"
#include "../../base_media_sounds.h"
#include "../../ai/ai.hpp"
#include "../../game/game.hpp"
#include "../../fios.h"
#include "tcp_content.h"

#include "table/strings.h"

#include "../../safeguards.h"

/**
 * Is the state either selected or autoselected?
 * @return true iff that's the case
 */
bool ContentInfo::IsSelected() const
{
	switch (this->state) {
		case ContentInfo::State::Selected:
		case ContentInfo::State::Autoselected:
		case ContentInfo::State::AlreadyHere:
			return true;

		default:
			return false;
	}
}

/**
 * Is the information from this content info valid?
 * @return true iff it's valid
 */
bool ContentInfo::IsValid() const
{
	return this->state < ContentInfo::State::Invalid && this->type >= CONTENT_TYPE_BEGIN && this->type < CONTENT_TYPE_END;
}

/**
 * Search a textfile file next to this file in the content list.
 * @param type The type of the textfile to search for.
 * @return The filename for the textfile.
 */
std::optional<std::string> ContentInfo::GetTextfile(TextfileType type) const
{
	if (this->state == ContentInfo::State::Invalid) return std::nullopt;
	std::optional<std::string_view> tmp;
	switch (this->type) {
		default: NOT_REACHED();
		case CONTENT_TYPE_AI:
			tmp = AI::GetScannerInfo()->FindMainScript(*this, true);
			break;
		case CONTENT_TYPE_AI_LIBRARY:
			tmp = AI::GetScannerLibrary()->FindMainScript(*this, true);
			break;
		case CONTENT_TYPE_GAME:
			tmp = Game::GetScannerInfo()->FindMainScript(*this, true);
			break;
		case CONTENT_TYPE_GAME_LIBRARY:
			tmp = Game::GetScannerLibrary()->FindMainScript(*this, true);
			break;
		case CONTENT_TYPE_NEWGRF: {
			const GRFConfig *gc = FindGRFConfig(std::byteswap(this->unique_id), FGCM_EXACT, &this->md5sum);
			if (gc != nullptr) tmp = gc->filename;
			break;
		}
		case CONTENT_TYPE_BASE_GRAPHICS:
			tmp = TryGetBaseSetFile(*this, true, BaseGraphics::GetAvailableSets());
			break;
		case CONTENT_TYPE_BASE_SOUNDS:
			tmp = TryGetBaseSetFile(*this, true, BaseSounds::GetAvailableSets());
			break;
		case CONTENT_TYPE_BASE_MUSIC:
			tmp = TryGetBaseSetFile(*this, true, BaseMusic::GetAvailableSets());
			break;
		case CONTENT_TYPE_SCENARIO:
		case CONTENT_TYPE_HEIGHTMAP:
			tmp = FindScenario(*this, true);
			break;
	}
	if (!tmp.has_value()) return std::nullopt;
	return ::GetTextfile(type, GetContentInfoSubDir(this->type), *tmp);
}

/**
 * Handle the given packet, i.e. pass it to the right
 * parser receive command.
 * @param p the packet to handle
 * @return true if we should immediately handle further packets, false otherwise
 */
bool NetworkContentSocketHandler::HandlePacket(Packet &p)
{
	PacketContentType type = static_cast<PacketContentType>(p.Recv_uint8());

	switch (type) {
		case PacketContentType::ClientInfoList: return this->ReceiveClientInfoList(p);
		case PacketContentType::ClientInfoID: return this->ReceiveClientInfoID(p);
		case PacketContentType::ClientInfoExternalID: return this->ReceiveClientInfoExternalID(p);
		case PacketContentType::ClientInfoExternalIDMD5: return this->ReceiveClientInfoExternalIDMD5(p);
		case PacketContentType::ServerInfo: return this->ReceiveServerInfo(p);
		case PacketContentType::ClientContent: return this->ReceiveClientContent(p);
		case PacketContentType::ServerContent: return this->ReceiveServerContent(p);

		default:
			Debug(net, 0, "[tcp/content] Received invalid packet type {}", type);
			return false;
	}
}

/**
 * Receive a packet at TCP level
 * @return Whether at least one packet was received.
 */
bool NetworkContentSocketHandler::ReceivePackets()
{
	/*
	 * We read only a few of the packets. This as receiving packets can be expensive
	 * due to the re-resolving of the parent/child relations and checking the toggle
	 * state of all bits. We cannot do this all in one go, as we want to show the
	 * user what we already received. Otherwise, it can take very long before any
	 * progress is shown to the end user that something has been received.
	 * It is also the case that we request extra content from the content server in
	 * case there is an unknown (in the content list) piece of content. These will
	 * come in after the main lists have been requested. As a result, we won't be
	 * getting everything reliably in one batch. Thus, we need to make subsequent
	 * updates in that case as well.
	 *
	 * As a result, we simple handle an arbitrary number of packets in one cycle,
	 * and let the rest be handled in subsequent cycles. These are ran, almost,
	 * immediately after this cycle so in speed it does not matter much, except
	 * that the user interface will appear better responding.
	 *
	 * What arbitrary number to choose is the ultimate question though.
	 */
	std::unique_ptr<Packet> p;
	static const int MAX_PACKETS_TO_RECEIVE = 42;
	int i = MAX_PACKETS_TO_RECEIVE;
	while (--i != 0 && (p = this->ReceivePacket()) != nullptr) {
		bool cont = this->HandlePacket(*p);
		if (!cont) return true;
	}

	return i != MAX_PACKETS_TO_RECEIVE - 1;
}


/**
 * Helper for logging receiving invalid packets.
 * @param type The received packet type.
 * @return Always false, as it's an error.
 */
bool NetworkContentSocketHandler::ReceiveInvalidPacket(PacketContentType type)
{
	Debug(net, 0, "[tcp/content] Received illegal packet type {}", type);
	return false;
}

bool NetworkContentSocketHandler::ReceiveClientInfoList(Packet &) { return this->ReceiveInvalidPacket(PacketContentType::ClientInfoList); }
bool NetworkContentSocketHandler::ReceiveClientInfoID(Packet &) { return this->ReceiveInvalidPacket(PacketContentType::ClientInfoID); }
bool NetworkContentSocketHandler::ReceiveClientInfoExternalID(Packet &) { return this->ReceiveInvalidPacket(PacketContentType::ClientInfoExternalID); }
bool NetworkContentSocketHandler::ReceiveClientInfoExternalIDMD5(Packet &) { return this->ReceiveInvalidPacket(PacketContentType::ClientInfoExternalIDMD5); }
bool NetworkContentSocketHandler::ReceiveServerInfo(Packet &) { return this->ReceiveInvalidPacket(PacketContentType::ServerInfo); }
bool NetworkContentSocketHandler::ReceiveClientContent(Packet &) { return this->ReceiveInvalidPacket(PacketContentType::ClientContent); }
bool NetworkContentSocketHandler::ReceiveServerContent(Packet &) { return this->ReceiveInvalidPacket(PacketContentType::ServerContent); }

/**
 * Helper to get the subdirectory a #ContentInfo is located in.
 * @param type The type of content.
 * @return The subdirectory the content is located in.
 */
Subdirectory GetContentInfoSubDir(ContentType type)
{
	switch (type) {
		default: return NO_DIRECTORY;
		case CONTENT_TYPE_AI:           return AI_DIR;
		case CONTENT_TYPE_AI_LIBRARY:   return AI_LIBRARY_DIR;
		case CONTENT_TYPE_GAME:         return GAME_DIR;
		case CONTENT_TYPE_GAME_LIBRARY: return GAME_LIBRARY_DIR;
		case CONTENT_TYPE_NEWGRF:       return NEWGRF_DIR;

		case CONTENT_TYPE_BASE_GRAPHICS:
		case CONTENT_TYPE_BASE_SOUNDS:
		case CONTENT_TYPE_BASE_MUSIC:
			return BASESET_DIR;

		case CONTENT_TYPE_SCENARIO:     return SCENARIO_DIR;
		case CONTENT_TYPE_HEIGHTMAP:    return HEIGHTMAP_DIR;
	}
}
