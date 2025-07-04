//////////////////////////////////////////////////
//  Copyright (c) 2020 Nara Hiero
//
// This file is licensed under GPLv3+
// Refer to the `License.txt` file included.
//////////////////////////////////////////////////

#include "pch.h"
#include <CTLib/Image.hpp>

#include <SOIL2/stb_image.h>
#include <stb/stb_image_resize.h>
#include <stb/stb_image_write.h>

#include <CTLib/Utilities.hpp>

namespace CTLib
{

uint8_t RGBAColour::operator[](size_t index) const
{
    switch (index)
    {
    case 0:
        return r;
    
    case 1:
        return g;

    case 2:
        return b;

    case 3:
        return a;

    default:
        throw ImageError("Illegal index! (index > 3)");
    }
}

Image::Image(uint32_t width, uint32_t height, const Buffer& data, RGBAColour colour) :
    width{width},
    height{height}
{
    buffer = Buffer(width * height * 4);
    size_t min = buffer.remaining() > data.remaining() ? data.remaining() : buffer.remaining();
    for (size_t i = 0; i < min; ++i)
    {
        buffer[i] = data[i];
    }
    size_t fill = buffer.remaining() - min;
	for ( size_t i = 0; i < fill; ++i )
    {
        buffer[i + min] = colour[i & 3];
    }
}

Image::Image(uint32_t width, uint32_t height, RGBAColour colour) :
    width{width},
    height{height}
{
    buffer = Buffer(width * height * 4);
    while (buffer.hasRemaining())
    {
        buffer.put(colour.r).put(colour.g).put(colour.b).put(colour.a);
    }
    buffer.clear();
}

Image::Image(uint32_t width, uint32_t height, uint8_t* data) :
    width{width},
    height{height}
{
    buffer = Buffer(width * height * 4);
    buffer.putArray(data, buffer.capacity());
    buffer.flip();
}

Image::Image(const Image& src) :
    width{src.width},
    height{src.height},
    buffer{src.buffer}
{

}

Image::Image(Image&& src) :
    width{src.width},
    height{src.height},
    buffer{std::move(src.buffer)}
{

}

uint8_t* Image::operator*() const
{
    return *buffer;
}

bool Image::operator==(const Image& src) const
{
    if (this == &src)
    {
        return true;
    }
    if (width != src.width)
    {
        return false;
    }
    if (height != src.height)
    {
        return false;
    }
    if (buffer.capacity() != src.buffer.capacity()) // if an image was moved
    {
        return false;
    }
    for (size_t i = 0; i < buffer.capacity(); ++i)
    {
        if (buffer[i] != src.buffer[i])
        {
            return false;
        }
    }
    return true;
}

uint32_t Image::getWidth() const
{
    return width;
}

uint32_t Image::getHeight() const
{
    return height;
}

Buffer Image::getData() const
{
    return buffer.duplicate();
}

size_t Image::offsetFor(uint32_t x, uint32_t y) const
{
    return ((y * width) + x) * 4;
}

Image Image::resize(uint32_t nw, uint32_t nh) const
{
    Image out(nw, nh);
    stbir_resize_uint8(*buffer, width, height, 0, *out, nw, nh, 0, 4);
    return out;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////
////   IMAGE IO STUFF
////
////

Image ImageIO::read(const char* filename)
{
    uint32_t err = 0;
    Buffer data = IO::readFile(filename, &err);
    if (err)
    {
        throw ImageError("File not found!");
    }

    int width, height, c;
    uint8_t* img = stbi_load_from_memory(*data, (int)data.capacity(), &width, &height, &c, 4);
    if (img == nullptr)
    {
        throw ImageError("Invalid or corrupted image data!");
    }

    Image image(static_cast<uint32_t>(width), static_cast<uint32_t>(height), img);

    stbi_image_free(img);

    return image;
}

Image ImageIO::read(const std::string& filename)
{
    return read(filename.c_str());
}

void ImageIO::write(const char* filename, const Image& image, ImageIOFormat format)
{
    const int width = static_cast<int>(image.getWidth()),
        height = static_cast<int>(image.getHeight());
    
    int retval;
    switch (format)
    {
    case ImageIOFormat::BMP:
        retval = stbi_write_bmp(filename, width, height, 4, *image);
        break;
    
    case ImageIOFormat::JPEG:
        retval = stbi_write_jpg(filename, width, height, 4, *image, 50 /*arbitrary, not tested*/);
        break;

    case ImageIOFormat::PNG:
        retval = stbi_write_png(filename, width, height, 4, *image, 0);
        break;
    
    case ImageIOFormat::TGA:
        retval = stbi_write_tga(filename, width, height, 4, *image);
        break;

    default:
        // should never happen
        throw CTLib::ImageError("Invalid format!");
    }

    if (retval == 0)
    {
        throw ImageError("Could not write image file!");
    }
}

void ImageIO::write(const std::string& filename, const Image& image, ImageIOFormat format)
{
    return write(filename.c_str(), image, format);
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////
////   IMAGE ERROR STUFF
////
////

ImageError::ImageError(const char* msg) : std::runtime_error(msg)
{

}

ImageError::ImageError(const std::string& msg) : std::runtime_error(msg)
{

}
}
