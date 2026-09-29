/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef TINSEL_HD_RENDERER_H
#define TINSEL_HD_RENDERER_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/rect.h"
#include "graphics/surface.h"
#include "tinsel/graphics.h"

namespace Tinsel {

/** Optional 2x presentation layer. The original indexed screen remains the game state. */
class HDRenderer {
public:
	struct DrawSnapshot {
		DRAWOBJECT object;
		Common::Rect rect;
		Common::Array<byte> before;
	};

	HDRenderer();
	~HDRenderer();

	void clearRect(const Common::Rect &rect);
	void capture(const DRAWOBJECT &object, DrawSnapshot &snapshot);
	void finish(const DrawSnapshot &snapshot);
	void setPalette(const byte *colors, uint start, uint count);
	void present(const Common::Rect &rect, bool originalOnly = false);

private:
	Graphics::Surface *getImage(SCNHANDLE handle);
	void putIndexed(int x, int y, byte color);
	void putRGB(int x, int y, uint8 r, uint8 g, uint8 b);

	Common::Array<uint32> _pixels;
	Graphics::Surface _output;
	byte _palette[256 * 3];
	Common::HashMap<SCNHANDLE, Graphics::Surface *> _images;
	Common::HashMap<SCNHANDLE, bool> _fontImages;
};

} // namespace Tinsel

#endif
