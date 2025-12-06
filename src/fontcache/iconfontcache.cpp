/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file spritefontcache.cpp Sprite fontcache implementation. */

#include "../stdafx.h"

#include "iconfontcache.h"

#include "../cargotype.h"
#include "../company_func.h"
#include "../engine_gui.h"
#include "../vehicle_gui.h"
#include "../fontcache.h"
#include "../gfx_layout.h"
#include "../iconglyphs.h"
#include "../newgrf_badge.h"
#include "../zoom_func.h"

#include "../safeguards.h"

static uint GetIconGlyphWidth(const IconGlyph &ig)
{
	struct visitor {
		uint operator()(CargoType)
		{
			return GetLargestCargoIconSize().width;
		}

		uint operator()(BadgeID badge_index)
		{
			const Badge *badge = GetBadge(badge_index);
			if (badge == nullptr) return 0;

			auto [img, _] = GetBadgeSprite(*badge, GSF_DEFAULT, std::nullopt, PAL_NONE);
			return GetScaledSpriteSize(img).width;
		}

		uint operator()(CompanyID)
		{
			return GetScaledSpriteSize(SPR_COMPANY_ICON).width;
		}

		uint operator()(EngineID engine)
		{
			auto size = GetVehicleImageCellSize(Engine::Get(engine)->type, EIT_PURCHASE);
			return (size.extend_left + size.extend_right + ScaleGUITrad(1));
		}
	};

	return std::visit(visitor{}, ig);
}

static void DrawIconGlyph(const IconGlyph ig, const Rect &r)
{
	struct visitor {
		const Rect &r;

		void operator()(CargoType cargotype)
		{
			SpriteID img = CargoSpec::Get(cargotype)->GetCargoIcon();
			DrawSpriteIgnorePadding(img, PAL_NONE, r, SA_CENTER);
		}

		void operator()(BadgeID badge_index)
		{
			const Badge *badge = GetBadge(badge_index);
			if (badge == nullptr) return;

			auto [img, pal] = GetBadgeSprite(*badge, GSF_DEFAULT, std::nullopt, PAL_NONE);
			DrawSpriteIgnorePadding(img, pal, r, SA_CENTER);
		}

		void operator()(CompanyID company)
		{
			DrawSpriteIgnorePadding(SPR_COMPANY_ICON, GetCompanyPalette(company), r, SA_CENTER);
		}

		void operator()(EngineID engine)
		{
			DrawVehicleEngine(r.left, r.right, CentreBounds(r.left, r.right, 0), CentreBounds(r.top, r.bottom, 0), engine, PAL_NONE, EIT_PREVIEW);
		}
	};

	std::visit(visitor{r}, ig);
}

/**
 * Create a new icon font cache.
 * @param fs The font size to create the cache for.
 */
IconFontCache::IconFontCache(FontSize fs) : FontCache(fs)
{
	this->UpdateMetrics();
}

void IconFontCache::UpdateMetrics()
{
	this->height = ScaleGUITrad(DEFAULT_FONT_HEIGHT[this->fs]);
	this->ascender = ScaleFontTrad(DEFAULT_FONT_ASCENDER[fs]);
	this->descender = ScaleFontTrad(DEFAULT_FONT_ASCENDER[fs] - DEFAULT_FONT_HEIGHT[fs]);
}

void IconFontCache::ClearFontCache()
{
	Layouter::ResetFontCache(this->fs);
	this->UpdateMetrics();
}

void IconFontCache::DrawGlyph(GlyphID key, const Rect &r)
{
	const IconGlyph *ig = _icon_glyphs.GetGlyph(static_cast<char32_t>(key));
	if (ig != nullptr) return DrawIconGlyph(*ig, r);
}

uint IconFontCache::GetGlyphWidth(GlyphID key)
{
	const IconGlyph *ig = _icon_glyphs.GetGlyph(static_cast<char32_t>(key));
	if (ig != nullptr) return GetIconGlyphWidth(*ig);

	return 0;
}

GlyphID IconFontCache::MapCharToGlyph(char32_t key)
{
	const IconGlyph *ig = _icon_glyphs.GetGlyph(key);
	if (ig != nullptr) return static_cast<GlyphID>(key);

	return 0;
}

class IconFontCacheFactory : public FontCacheFactory {
public:
	IconFontCacheFactory() : FontCacheFactory("icon", "Icon font provider") {}

	std::unique_ptr<FontCache> LoadFont(FontSize fs, FontType fonttype, bool, const std::string &, std::span<const std::byte>) const override
	{
		if (fonttype != FontType::Icon) return nullptr;

		/* Icons are only supported for normal text. */
		if (fs != FS_NORMAL) return nullptr;

		return std::make_unique<IconFontCache>(fs);
	}

	bool FindFallbackFont(const std::string &, FontSizes, class MissingGlyphSearcher *) const override
	{
		return false;
	}

private:
	static IconFontCacheFactory instance;
};

/* static */ IconFontCacheFactory IconFontCacheFactory::instance;
