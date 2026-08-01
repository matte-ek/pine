#include <cstring>
#include <setjmp.h>
#ifdef PINE_RUNTIME

namespace Pine::Importer::ImageLoader
{
    void* LoadImageJpeg(const std::filesystem::path& file, int& width, int& height, int& channels)
    {
        return nullptr;
    }
}

#else

#include <filesystem>
#include <vector>
#include <jpeglib.h>

namespace
{
    struct JpegErrorManager
    {
        jpeg_error_mgr base;
        jmp_buf jmp;
    };

    static void jpeg_error_callback(const j_common_ptr cinfo)
    {
        auto* err = reinterpret_cast<JpegErrorManager*>(cinfo->err);
        longjmp(err->jmp, 1);
    }
}

namespace Pine::Importer::ImageLoader
{
    void* LoadImageJpeg(const std::filesystem::path& file, int& width, int& height, int& channels)
    {
        FILE* fp = fopen(file.c_str(), "rb");
        if (!fp)
        {
            return nullptr;
        }

        jpeg_decompress_struct cinfo{};
        JpegErrorManager jerr{};

        cinfo.err = jpeg_std_error(&jerr.base);
        jerr.base.error_exit = jpeg_error_callback;

        if (setjmp(jerr.jmp))
        {
            jpeg_destroy_decompress(&cinfo);
            fclose(fp);
            return nullptr;
        }

        jpeg_create_decompress(&cinfo);
        jpeg_stdio_src(&cinfo, fp);

        jpeg_read_header(&cinfo, TRUE);

        cinfo.out_color_space = JCS_EXT_RGBA; // always decode to RGBA

        jpeg_start_decompress(&cinfo);

        width      = cinfo.output_width;
        height     = cinfo.output_height;
        channels   = cinfo.output_components;

        void* buffer = malloc(width * height * channels);

        int row_stride = width * channels;
        std::vector<unsigned char> row(row_stride);
        JSAMPROW row_ptr[1] = { row.data() };

        while (cinfo.output_scanline < static_cast<unsigned>(height))
        {
            int y = cinfo.output_scanline;
            jpeg_read_scanlines(&cinfo, row_ptr, 1);

            memcpy(static_cast<unsigned char *>(buffer) + y * row_stride, row.data(), row_stride);
        }

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose(fp);

        return buffer;
    }
}

#endif