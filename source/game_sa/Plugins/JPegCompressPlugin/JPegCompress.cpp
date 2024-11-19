#include "StdInc.h"
#include "jpeglib.h"

static std::array<uint8, 204'800> g_ScreenshotFileBuf    = StaticRef<std::array<uint8, 204'800>>(0xBD0B78);
static CRGBA*&                    g_JpegDecodingToRaster = StaticRef<CRGBA*>(0xBD0160);

// size unknown
static std::array<CRGBA, 10'000>& g_JpegDecodingBuffer   = StaticRef<std::array<CRGBA, 10'000>>(0xBD0170);

void JPegPlugin::InjectHooks() {
    RH_ScopedNamespace(JPegPlugin);
    RH_ScopedCategory("Plugins");

    RH_ScopedGlobalInstall(JPegCompressScreen, 0x5D0470);
    RH_ScopedGlobalInstall(JPegCompressScreenToFile, 0x5D0820);
    RH_ScopedGlobalInstall(JPegCompressScreenToBuffer, 0x5D0740);
    RH_ScopedGlobalInstall(RwRgbToPixel, 0x803740);
    RH_ScopedGlobalInstall(WriteScanlineToRaster, 0x5D0570);
    RH_ScopedGlobalInstall(JPegDecompressToRaster, 0x5D05F0);
    RH_ScopedGlobalInstall(JPegDecompressToVramFromBuffer, 0x5D0320);
}

// 0x5D0470
void JPegCompressScreen(RwCamera* camera, jpeg_destination_mgr& dst) {
    jpeg_error_mgr       errmgr{};
    jpeg_compress_struct compress{
        .err = jpeg_std_error(&errmgr)
    };
    jpeg_CreateCompress(&compress, 0x3E, sizeof(jpeg_compress_struct));
    jpeg_set_defaults(&compress);

    const auto image = RsGrabScreen(camera);
    compress.image_width = image->width;
    compress.image_height = image->height;
    compress.dest = &dst;
    jpeg_start_compress(&compress, true);

    for (auto line = 0u; line < image->height; line++) {
        auto* pixels = &image->cpPixels[line * image->width * sizeof(RwRGBA)];
        if (jpeg_write_scanlines(&compress, &pixels, 1u) != 1) {
            break;
        }
    }
    jpeg_finish_compress(&compress);
    jpeg_destroy_compress(&compress);
    RwImageDestroy(image);
}

// 0x5D0820
void JPegCompressScreenToFile(RwCamera* camera, const char* path) {
    static FILESTREAM g_ScreenshotFileHandle{};

    if (g_ScreenshotFileHandle = CFileMgr::OpenFile(path, "wb")) {
        jpeg_destination_mgr dst{};

        // InitDestination (0x5D03E0)
        dst.init_destination = [](j_compress_ptr c) {
            c->dest->next_output_byte = g_ScreenshotFileBuf.data();
            c->dest->free_in_buffer = g_ScreenshotFileBuf.size();
        };

        // HDEmptyOutputBuffer (0x5D0400)
        dst.empty_output_buffer = [](j_compress_ptr c) -> boolean {
            CFileMgr::Write(g_ScreenshotFileHandle, g_ScreenshotFileBuf.data(), g_ScreenshotFileBuf.size());
            c->dest->next_output_byte = g_ScreenshotFileBuf.data();
            c->dest->free_in_buffer   = g_ScreenshotFileBuf.size();
            return true;
        };

        // HDTermDestination (0x5D0440)
        dst.term_destination = [](j_compress_ptr c) {
            // NOTSA: signed -> unsigned
            const auto totalProcessed = (uintptr)c->dest->next_output_byte - (uintptr)g_ScreenshotFileBuf.data();
            if (totalProcessed > 0) {
                CFileMgr::Write(g_ScreenshotFileHandle, g_ScreenshotFileBuf.data(), totalProcessed);
            }
        };

        JPegCompressScreen(camera, dst);
        CFileMgr::CloseFile(g_ScreenshotFileHandle);
    }
}

// 0x5D0740
void JPegCompressScreenToBuffer(RwCamera* cam, uint8** buffer, size_t& bufferSizeInOut) {
    static uint8**& Buffer = StaticRef<uint8**>(0xC02B78);
    static size_t& BufferSize = StaticRef<size_t>(0xC02B7C);
    static size_t& TotalProcessed = StaticRef<size_t>(0xC02B80);

    Buffer = buffer;
    BufferSize = bufferSizeInOut;
    TotalProcessed = 0;

    jpeg_destination_mgr dst{};

    // 0x5D0260
    dst.init_destination = [](j_compress_ptr c) {
        c->dest->next_output_byte = g_ScreenshotFileBuf.data();
        c->dest->free_in_buffer   = g_ScreenshotFileBuf.size();
    };

    // 0x5D0280
    dst.empty_output_buffer = [](j_compress_ptr c) -> boolean {
        if (TotalProcessed + g_ScreenshotFileBuf.size() < BufferSize) {
            std::memcpy(
                *Buffer + TotalProcessed,
                g_ScreenshotFileBuf.data(),
                g_ScreenshotFileBuf.size()
            );
            TotalProcessed += g_ScreenshotFileBuf.size();
        }
        c->dest->next_output_byte = g_ScreenshotFileBuf.data();
        c->dest->free_in_buffer   = g_ScreenshotFileBuf.size();

        return true;
    };

    // 0x5D02D0
    dst.term_destination = [](j_compress_ptr c) {
        if (c->dest->next_output_byte + TotalProcessed - g_ScreenshotFileBuf.data() < BufferSize) {
            std::memcpy(
                *Buffer + TotalProcessed,
                g_ScreenshotFileBuf.data(),
                g_ScreenshotFileBuf.size()
            );
            TotalProcessed += g_ScreenshotFileBuf.size();
        }
    };

    bufferSizeInOut = TotalProcessed;
}

// Converts RGB color values to the given internal format via RW.
// 0x803740
static uint32 RwRgbToPixel(uint32 rgb, uint32 format) {
    uint32 out{};
    RwEngineInstance->stdFunc[rwSTANDARDRGBTOPIXEL](&out, &rgb, format);
    return out;
}

// 0x5D0570
static void WriteScanlineToRaster(RwRaster* raster, int32, uint32 begin, uint32 end) {
    for (auto i = begin; i < end; i++) {
        const auto rgb = g_JpegDecodingBuffer[i].ToRGB(); // Discard alpha.
        *g_JpegDecodingToRaster++ = RwRgbToPixel(rgb.ToInt(), RwRasterGetFormat(raster));
    }
}

// 0x5D05F0 -- PS2 leftover.
void JPegDecompressToRaster(RwRaster* raster, jpeg_source_mgr& src) {
    jpeg_error_mgr       errmgr{};
    jpeg_decompress_struct decompress{
        .err = jpeg_std_error(&errmgr)
    };
    jpeg_CreateDecompress(&decompress, 0x3E, sizeof(jpeg_decompress_struct));
    decompress.src = &src;

    if (jpeg_read_header(&decompress, true) == 1 && jpeg_start_decompress(&decompress)) {
        g_JpegDecodingToRaster = reinterpret_cast<CRGBA*>(RwRasterLock(raster, 0, rwRASTERLOCKWRITE));

        // Not even sure if it works tbh.
        bool success{};
        size_t scanline{};
        while (true) {
            if (!jpeg_start_output(&decompress, 1)) {
                break;
            }
            success = true;

            // no jpeg_read_scanlines?

            static constexpr auto PAL_NUM_SCANLINES = 576u;
            WriteScanlineToRaster(raster, scanline++, 64, PAL_NUM_SCANLINES);
        }
        success = false;
    }
    jpeg_destroy_decompress(&decompress);
}

// 0x5D0320 -- PS2 leftover
void JPegDecompressToVramFromBuffer(RwRaster* raster, bool unk) {
    static size_t&                     g_ScreenshotFilePointer = StaticRef<size_t>(0xBD0B70);
    static std::array<uint8, 204'800>& g_ScreenshotFileBuf = StaticRef<std::array<uint8, 204'800>>(0xBD0B78);

    if (!unk) {
        return;
    }

    std::memcpy(g_ScreenshotFileBuf.data(), FrontEndMenuManager.m_GalleryImgBuffer, g_ScreenshotFileBuf.size());
    g_ScreenshotFilePointer = 0;

    jpeg_source_mgr src{};
    src.init_source = src.term_source = [](j_decompress_ptr) { /* nop */ };
    src.skip_input_data = [](j_decompress_ptr, long) { /* nop */ };

    src.resync_to_restart = jpeg_resync_to_restart;

    // 0x5D0320
    src.fill_input_buffer = [](j_decompress_ptr d) -> boolean {
        std::memcpy(
            g_ScreenshotFileBuf.data(),
            &FrontEndMenuManager.m_GalleryImgBuffer[g_ScreenshotFilePointer],
            g_ScreenshotFileBuf.size()
        );

        d->src->next_input_byte = g_ScreenshotFileBuf.data();
        d->src->bytes_in_buffer = g_ScreenshotFileBuf.size();
        g_ScreenshotFilePointer += g_ScreenshotFileBuf.size();
        return true;
    };

    src.next_input_byte = g_ScreenshotFileBuf.data();
    JPegDecompressToRaster(raster, src);
}
