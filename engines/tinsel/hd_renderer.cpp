/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "tinsel/hd_renderer.h"
#include "tinsel/object.h"
#include "tinsel/tinsel.h"

#include "common/file.h"
#include "common/path.h"
#include "common/str.h"
#include "common/textconsole.h"
#include "image/png.h"

namespace Tinsel {

HDRenderer::HDRenderer() {
	_pixels.resize(640 * 432 * 4);
	memset(_pixels.data(), 0, _pixels.size() * sizeof(uint32));
	memset(_palette, 0, sizeof(_palette));
	_output.create(1280, 864, Graphics::PixelFormat::createFormatRGBA32(false));
}

HDRenderer::~HDRenderer() {
	for (Common::HashMap<SCNHANDLE, Graphics::Surface *>::iterator it = _images.begin(); it != _images.end(); ++it) {
		if (it->_value) {
			it->_value->free();
			delete it->_value;
		}
	}
	_output.free();
}

void HDRenderer::clearRect(const Common::Rect &rect) {
	Common::Rect clipped = rect;
	clipped.clip(Common::Rect(0, 0, 640, 432));
	if (clipped.isEmpty())
		return;
	for (int y = clipped.top * 2; y < clipped.bottom * 2; ++y)
		memset(&_pixels[y * 1280 + clipped.left * 2], 0, clipped.width() * 2 * sizeof(uint32));
}

void HDRenderer::capture(const DRAWOBJECT &object, DrawSnapshot &snapshot) {
	snapshot.object = object;
	snapshot.rect = Common::Rect(object.xPos, object.yPos,
		object.xPos + object.width - object.leftClip - object.rightClip,
		object.yPos + object.height - object.topClip - object.botClip);
	snapshot.rect.clip(Common::Rect(0, 0, 640, 432));
	if (snapshot.rect.isEmpty())
		return;
	snapshot.before.resize(snapshot.rect.width() * snapshot.rect.height());
	for (int y = snapshot.rect.top; y < snapshot.rect.bottom; ++y) {
		const byte *line = static_cast<const byte *>(_vm->screen().getBasePtr(snapshot.rect.left, y));
		memcpy(&snapshot.before[(y - snapshot.rect.top) * snapshot.rect.width()], line, snapshot.rect.width());
	}
}

Graphics::Surface *HDRenderer::getImage(SCNHANDLE handle) {
	if (!handle)
		return nullptr;
	if (_images.contains(handle))
		return _images[handle];

	Graphics::Surface *image = nullptr;
	debug(2, "Tinsel HD lookup %08x", handle);
#ifdef USE_PNG
	Common::File file;
	Common::String name;
	static const char *const folders[] = { "backgrounds", "sprites", "ui", "fonts", "" };
	uint folderIndex = 0;
	for (; folderIndex < ARRAYSIZE(folders); ++folderIndex) {
		name = folders[folderIndex][0] ? Common::String::format("hd/%s/%08x.png", folders[folderIndex], handle) :
			Common::String::format("hd/%08x.png", handle);
		if (file.open(Common::Path(name)))
			break;
	}
	if (file.isOpen()) {
		debug(1, "Tinsel HD image %08x (%s)", handle, name.c_str());
		_fontImages[handle] = folderIndex == 3;
		Image::PNGDecoder decoder;
		if (decoder.loadStream(file) && decoder.getSurface()) {
			const Graphics::Surface *source = decoder.getSurface();
			if (source->format.bytesPerPixel == 1)
				image = source->convertTo(Graphics::PixelFormat::createFormatRGBA32(), decoder.getPalette().data(), decoder.getPalette().size());
			else
				image = source->convertTo(Graphics::PixelFormat::createFormatRGBA32());
		} else {
			warning("Tinsel HD: cannot decode %s", name.c_str());
		}
	}
#endif
	_images[handle] = image;
	return image;
}

void HDRenderer::putIndexed(int x, int y, byte color) {
	const int pos = y * 2 * 1280 + x * 2;
	_pixels[pos] = _pixels[pos + 1] = _pixels[pos + 1280] = _pixels[pos + 1281] = color;
}

void HDRenderer::putRGB(int x, int y, uint8 r, uint8 g, uint8 b) {
	_pixels[y * 1280 + x] = 0x80000000u | (uint32(r) << 16) | (uint32(g) << 8) | b;
}

void HDRenderer::finish(const DrawSnapshot &snapshot) {
	const DRAWOBJECT &object = snapshot.object;
	const Common::Rect &rect = snapshot.rect;
	if (rect.isEmpty())
		return;

	Graphics::Surface *image = getImage(object.hImg);
	if (image && (image->w != object.width * 2 || image->h != object.height * 2)) {
		warning("Tinsel HD: image %08x is %dx%d, expected %dx%d", object.hImg,
			image->w, image->h, object.width * 2, object.height * 2);
		image = nullptr;
	}

	if (image) {
		const int originX = object.xPos - object.leftClip;
		const int originY = object.yPos - object.topClip;
		for (int y = rect.top * 2; y < rect.bottom * 2; ++y) {
			int sourceY = y - originY * 2;
			for (int x = rect.left * 2; x < rect.right * 2; ++x) {
				int sourceX = x - originX * 2;
				if (object.flags & DMA_FLIPH)
					sourceX = image->w - 1 - sourceX;
				const uint32 pixel = *static_cast<const uint32 *>(image->getBasePtr(sourceX, sourceY));
				uint8 a, r, g, b;
				image->format.colorToARGB(pixel, a, r, g, b);
				if (!a)
					continue;
				if (_fontImages.contains(object.hImg) && _fontImages[object.hImg]) {
					const byte index = *static_cast<const byte *>(_vm->screen().getBasePtr(x / 2, y / 2));
					const byte *rgb = &_palette[index * 3];
					r = rgb[0]; g = rgb[1]; b = rgb[2];
				}
				if (a != 255) {
					const uint32 under = _pixels[y * 1280 + x];
					const byte *indexed = &_palette[(under & 0xff) * 3];
					const uint8 ur = (under & 0x80000000u) ? (under >> 16) & 0xff : indexed[0];
					const uint8 ug = (under & 0x80000000u) ? (under >> 8) & 0xff : indexed[1];
					const uint8 ub = (under & 0x80000000u) ? under & 0xff : indexed[2];
					r = (r * a + ur * (255 - a) + 127) / 255;
					g = (g * a + ug * (255 - a) + 127) / 255;
					b = (b * a + ub * (255 - a) + 127) / 255;
				}
				putRGB(x, y, r, g, b);
			}
		}
		return;
	}

	// An unchanged screen index normally means a transparent sprite pixel.
	// Opaque draw types always replace the underlying HD pixel, even if the
	// resulting palette index happens to match the previous one.
	const int type = object.flags & 0xff;
	const bool opaque = type == 0x08 || type == 0x48 || type == 0x04 || type == 0x44;
	for (int y = rect.top; y < rect.bottom; ++y) {
		const byte *line = static_cast<const byte *>(_vm->screen().getBasePtr(rect.left, y));
		for (int x = rect.left; x < rect.right; ++x) {
			const int i = (y - rect.top) * rect.width() + x - rect.left;
			if (opaque || snapshot.before[i] != line[x - rect.left])
				putIndexed(x, y, line[x - rect.left]);
		}
	}
}

void HDRenderer::setPalette(const byte *colors, uint start, uint count) {
	if (start < 256 && count <= 256 - start)
		memcpy(_palette + start * 3, colors, count * 3);
}

void HDRenderer::present(const Common::Rect &rect, bool originalOnly) {
	Common::Rect clipped = rect;
	clipped.clip(Common::Rect(0, 0, 640, 432));
	if (clipped.isEmpty())
		return;
	for (int y = clipped.top * 2; y < clipped.bottom * 2; ++y) {
		uint32 *dst = static_cast<uint32 *>(_output.getBasePtr(clipped.left * 2, y));
		for (int x = clipped.left * 2; x < clipped.right * 2; ++x) {
			const uint32 pixel = originalOnly ?
				static_cast<const byte *>(_vm->screen().getBasePtr(x / 2, y / 2))[0] : _pixels[y * 1280 + x];
			uint8 r, g, b;
			if (pixel & 0x80000000u) {
				r = (pixel >> 16) & 0xff;
				g = (pixel >> 8) & 0xff;
				b = pixel & 0xff;
			} else {
				const byte *rgb = &_palette[(pixel & 0xff) * 3];
				r = rgb[0]; g = rgb[1]; b = rgb[2];
			}
			dst[x - clipped.left * 2] = _output.format.RGBToColor(r, g, b);
		}
	}
	const int yOffset = (g_system->getHeight() - 864) / 2;
	g_system->copyRectToScreen(_output.getBasePtr(clipped.left * 2, clipped.top * 2), _output.pitch,
		clipped.left * 2, clipped.top * 2 + yOffset, clipped.width() * 2, clipped.height() * 2);
}

} // namespace Tinsel
