/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file base_media_graphics.h Generic functions for replacing base graphics data. */

#ifndef BASE_MEDIA_GRAPHICS_H
#define BASE_MEDIA_GRAPHICS_H

#include "base_media_base.h"

/** Types of graphics in the base graphics set */
enum GraphicsFileType : uint8_t {
	GFT_BASE,     ///< Base sprites for all climates
	GFT_LOGOS,    ///< Logos, landscape icons and original terrain generator sprites
	GFT_ARCTIC,   ///< Landscape replacement sprites for arctic
	GFT_TROPICAL, ///< Landscape replacement sprites for tropical
	GFT_TOYLAND,  ///< Landscape replacement sprites for toyland
	GFT_EXTRA,    ///< Extra sprites that were not part of the original sprites
	MAX_GFT,      ///< We are looking for this amount of GRFs
};

/** Blitter type for base graphics sets. */
enum BlitterType : uint8_t {
	BLT_8BPP,       ///< Base set has 8 bpp sprites only.
	BLT_32BPP,      ///< Base set has both 8 bpp and 32 bpp sprites.
};

struct GRFConfig;

template <> struct BaseSetTraits<struct GraphicsSet> {
	static constexpr size_t num_files = MAX_GFT;
	static constexpr bool search_in_tars = true;
	static constexpr std::string_view set_type = "graphics";
};

/** All data of a graphics set. */
struct GraphicsSet : BaseSet<GraphicsSet> {
private:
	mutable std::unique_ptr<GRFConfig> extra_cfg; ///< Parameters for extra GRF
public:
	PaletteType palette{}; ///< Palette of this graphics set
	BlitterType blitter{}; ///< Blitter of this graphics set

	GraphicsSet();
	~GraphicsSet();

	bool FillSetDetails(const IniFile &ini, const std::string &path, const std::string &full_filename);
	GRFConfig *GetExtraConfig() const { return this->extra_cfg.get(); }
	GRFConfig &GetOrCreateExtraConfig() const;
	bool IsConfigurable() const;
	void CopyCompatibleConfig(const GraphicsSet &src);

	static MD5File::ChecksumResult CheckMD5(const MD5File *file, Subdirectory subdir);
};

/** All data/functions related with replacing the base graphics. */
class BaseGraphics : public BaseMedia<GraphicsSet> {
public:
	/** Values loaded from config file. */
	struct Ini {
		std::string name;
		uint32_t shortname;                 ///< unique key for base set
		uint32_t extra_version;             ///< version of the extra GRF
		std::vector<uint32_t> extra_params; ///< parameters for the extra GRF
	};
	static inline Ini ini_data;
};

#endif /* BASE_MEDIA_BASE_H */
