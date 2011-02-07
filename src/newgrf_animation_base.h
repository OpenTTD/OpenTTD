/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_animation_base.h Function implementations related to NewGRF animation. */

/* No inclusion guards as this file must only be included from .cpp files. */

#include "animated_tile_func.h"
#include "core/random_func.hpp"
#include "date_func.h"
#include "viewport_func.h"
#include "newgrf_animation_type.h"
#include "newgrf_callbacks.h"

/**
 * Helper class for a unified approach to NewGRF animation.
 * @tparam Tbase       Instantiation of this class.
 * @tparam Tspec       NewGRF specification related to the animated tile.
 * @tparam Tobj        Object related to the animated tile.
 * @tparam GetCallback The callback function pointer.
 */
template <typename Tbase, typename Tspec, typename Tobj, uint16 (*GetCallback)(CallbackID callback, uint32 param1, uint32 param2, const Tspec *statspec, const Tobj *st, TileIndex tile)>
struct AnimationBase {
	/**
	 * Animate a single tile.
	 * @param cb          The callback to actually call.
	 * @param spec        Specification related to the tile.
	 * @param obj         Object related to the tile.
	 * @param tile        Tile to animate changes for.
	 * @param random_animation Whether to pass random bits to the "next frame" callback.
	 */
	static void AnimateTile(const Tspec *spec, const Tobj *obj, TileIndex tile, bool random_animation)
	{
		assert(spec != NULL);

		/* Acquire the animation speed from the NewGRF. */
		uint8 animation_speed = spec->animation.speed;
		if (HasBit(spec->callback_mask, Tbase::cbm_animation_speed)) {
			uint16 callback = GetCallback(Tbase::cb_animation_speed, 0, 0, spec, obj, tile);
			if (callback != CALLBACK_FAILED) animation_speed = Clamp(callback & 0xFF, 0, 16);
		}

		/* An animation speed of 2 means the animation frame changes 4 ticks, and
		 * increasing this value by one doubles the wait. 0 is the minimum value
		 * allowed for animation_speed, which corresponds to 30ms, and 16 is the
		 * maximum, corresponding to around 33 minutes. */
		if (_tick_counter % (1 << animation_speed) != 0) return;

		uint8 frame      = GetAnimationFrame(tile);
		uint8 num_frames = spec->animation.frames;

		bool frame_set_by_callback = false;

		if (HasBit(spec->callback_mask, Tbase::cbm_animation_next_frame)) {
			uint16 callback = GetCallback(Tbase::cb_animation_next_frame, random_animation ? Random() : 0, 0, spec, obj, tile);

			if (callback != CALLBACK_FAILED) {
				frame_set_by_callback = true;

				switch (callback & 0xFF) {
					case 0xFF:
						DeleteAnimatedTile(tile);
						break;

					case 0xFE:
						frame_set_by_callback = false;
						break;

					default:
						frame = callback & 0xFF;
						break;
				}

				/* If the lower 7 bits of the upper byte of the callback
				 * result are not empty, it is a sound effect. */
				if (GB(callback, 8, 7) != 0) PlayTileSound(spec->grf_prop.grffile, GB(callback, 8, 7), tile);
			}
		}

		if (!frame_set_by_callback) {
			if (frame < num_frames) {
				frame++;
			} else if (frame == num_frames && spec->animation.status == ANIM_STATUS_LOOPING) {
				/* This animation loops, so start again from the beginning */
				frame = 0;
			} else {
				/* This animation doesn't loop, so stay here */
				DeleteAnimatedTile(tile);
			}
		}

		SetAnimationFrame(tile, frame);
		MarkTileDirtyByTile(tile);
	}

	/**
	 * Check a callback to determine what the next animation step is and
	 * execute that step. This includes stopping and starting animations
	 * as well as updating animation frames and playing sounds.
	 * @param cb          The callback to actually call.
	 * @param spec        Specification related to the tile.
	 * @param obj         Object related to the tile.
	 * @param tile        Tile to consider animation changes for.
	 * @param random_bits Random bits for this update. To be passed as parameter to the NewGRF.
	 * @param trigger     What triggered this update? To be passed as parameter to the NewGRF.
	 */
	static void ChangeAnimationFrame(CallbackID cb, const Tspec *spec, const Tobj *obj, TileIndex tile, uint32 random_bits, uint32 trigger)
	{
		uint16 callback = GetCallback(cb, random_bits, trigger, spec, obj, tile);
		if (callback == CALLBACK_FAILED) return;

		switch (callback & 0xFF) {
			case 0xFD: /* Do nothing. */         break;
			case 0xFE: AddAnimatedTile(tile);    break;
			case 0xFF: DeleteAnimatedTile(tile); break;
			default:
				SetAnimationFrame(tile, callback);
				AddAnimatedTile(tile);
				break;
		}

		/* If the lower 7 bits of the upper byte of the callback
		 * result are not empty, it is a sound effect. */
		if (GB(callback, 8, 7) != 0) PlayTileSound(spec->grf_prop.grffile, GB(callback, 8, 7), tile);
	}
};
