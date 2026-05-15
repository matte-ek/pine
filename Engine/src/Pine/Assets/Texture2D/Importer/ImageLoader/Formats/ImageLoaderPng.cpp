#ifdef PINE_RUNTIME

namespace Pine::Importer::ImageLoader
{
    void* LoadImagePng(const std::filesystem::path& file, int& width, int& height, int& channels)
    {
        return nullptr;
    }
}

#else

#include <filesystem>
#include <vector>
#include <png.h>

namespace Pine::Importer::ImageLoader
{
    void* LoadImagePng(const std::filesystem::path& file, int& width, int& height, int& channels)
    {
        FILE* fp = fopen(file.c_str(), "rb");
        if (!fp)
        {
            return nullptr;
        }

        unsigned char sig[8];
        fread(sig, 1, 8, fp);
        if (!png_check_sig(sig, 8))
        {
            fclose(fp);
            return nullptr;
        }

        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png)
        {
            fclose(fp);
            return nullptr;
        }

        png_infop info = png_create_info_struct(png);
        if (!info)
        {
            png_destroy_read_struct(&png, nullptr, nullptr);
            fclose(fp);
            return nullptr;
        }

        if (setjmp(png_jmpbuf(png)))
        {
            png_destroy_read_struct(&png, &info, nullptr);
            fclose(fp);
            return nullptr;
        }

        png_init_io(png, fp);
        png_set_sig_bytes(png, 8);

        png_read_info(png, info);

        width = png_get_image_width(png, info);
        height = png_get_image_height(png, info);

        const png_byte color_type = png_get_color_type(png, info);
        const png_byte bit_depth = png_get_bit_depth(png, info);

        // Expand paletted images to RGB
        if (color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_palette_to_rgb(png);

        // Expand grayscale images to 8-bit
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
            png_set_expand_gray_1_2_4_to_8(png);

        // Convert transparency chunks to a full alpha channel
        if (png_get_valid(png, info, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(png);

        // Strip 16-bit channels down to 8-bit
        if (bit_depth == 16)
            png_set_strip_16(png);

        // Add an alpha channel if the image doesn't have one
        if (color_type == PNG_COLOR_TYPE_RGB ||
            color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

        // Expand grayscale to RGB
        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png);

        // Apply transformations and re-read metadata
        png_read_update_info(png, info);

        void* buffer = malloc(width * height * 4);

        std::vector<png_bytep> row_ptrs(height);
        for (int y = 0; y < height; ++y)
        {
            row_ptrs[y] = static_cast<unsigned char *>(buffer) + y * width * 4;
        }

        channels = 4;

        png_read_image(png, row_ptrs.data());
        png_read_end(png, info);
        png_destroy_read_struct(&png, &info, nullptr);

        fclose(fp);

        return buffer;
    }
}

#endif
