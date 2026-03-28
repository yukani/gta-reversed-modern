#include "StdInc.h"

#include "MenuManager.h"
#include <extensions/File.hpp>

#ifdef USE_GALLERY
#include "jpeglib.h"
#include "shellapi.h"

extern char (&user_gallery_dir_path)[256];

static auto GetImages() {
    return fs::directory_iterator{ user_gallery_dir_path }
        | rngv::filter([](const auto& entry) { return entry.path().extension() == ".jpg"; })
        | rng::to<std::vector<fs::path>>();
}

static auto GetJpegDimensions(std::span<uint8> buffer) {
    jpeg_error_mgr         jerr{};
    jpeg_decompress_struct cinfo{
        .err = jpeg_std_error(&jerr)
    };
    jerr.error_exit = [](j_common_ptr) {};
    jpeg_create_decompress(&cinfo);

    jpeg_mem_src(&cinfo, buffer.data(), buffer.size());

    auto width{ -1 }, height{ -1 };
    if (jpeg_read_header(&cinfo, TRUE) == JPEG_HEADER_OK) {
        width  = cinfo.image_width;
        height = cinfo.image_height;
    }
    jpeg_destroy_decompress(&cinfo);
    return std::make_pair(width, height);
}

// NOTSA -- reimplementation from PS2
void CMenuManager::DrawGallery() {
    const auto images = GetImages();
    if (images.empty()) {
        SwitchToNewScreen(SCREEN_GALLERY_NO_IMAGES);
        return;
    }

    CSprite2d::DrawRect(
        { StretchX(56.0f),
          StretchY(57.0f),
          StretchX(583.0f),
          StretchY(392.0f) },
        MENU_MAP_BORDER
    );
    CSprite2d::DrawRect(
        { StretchX(59.0f),
          StretchY(60.0f),
          StretchX(580.0f),
          StretchY(389.0f) },
        MENU_BG
    );

    static uint32 s_CurrentImageIdx{}, s_LastImageIdx{UINT32_MAX}, s_KeypressDelay{};
    if (CTimer::GetTimeInMSPauseMode() >= s_KeypressDelay) {
        if (CPad::NewKeyState.left || CPad::NewKeyState.right) {
            if (!s_CurrentImageIdx && CPad::NewKeyState.left) {
                s_CurrentImageIdx = images.size() - 1;
            } else if (s_CurrentImageIdx >= std::size(images) - 1 && CPad::NewKeyState.right) {
                s_CurrentImageIdx = 0;
            }
            else {
                s_CurrentImageIdx += CPad::NewKeyState.left ? -1 : 1;
            }
            s_KeypressDelay = CTimer::GetTimeInMSPauseMode() + 150;
        } else if (CPad::NewKeyState.standardKeys['l'] || CPad::NewKeyState.standardKeys['L']) {
            ShellExecuteA(NULL, "open", user_gallery_dir_path, NULL, NULL, SW_SHOWDEFAULT);
            s_KeypressDelay = CTimer::GetTimeInMSPauseMode() + 150;
        }
    }

    /* drawing */
    static RwRaster* s_DrawingRaster{};
    if (s_CurrentImageIdx != s_LastImageIdx) {
        notsa::File imgFile{ images[s_CurrentImageIdx].string().c_str(), "rb" };
        assert(!!imgFile);
        const auto fileSize = imgFile.GetTotalSize();
        auto       jpegBuffer = std::make_unique<uint8[]>(fileSize);
        imgFile.Read(jpegBuffer.get(), fileSize);

        if (s_DrawingRaster) {
            RwRasterDestroy(std::exchange(s_DrawingRaster, nullptr));
        }
        const auto [width, height] = GetJpegDimensions({ jpegBuffer.get(), (size_t)fileSize });
        assert(width != -1 && height != -1);
        s_DrawingRaster = RwRasterCreate(width, height, 32, rwRASTERFORMAT8888 | rwRASTERTYPETEXTURE);
        JPegDecompressToRasterFromBuffer(s_DrawingRaster, { jpegBuffer.get(), (size_t)fileSize });
        s_LastImageIdx = s_CurrentImageIdx;
    }
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(s_DrawingRaster));

    RwIm2DVertex vertices[4]{};
    {
        const auto nearScreenZ  = RwIm2DGetNearScreenZ();
        const auto recipCameraZ = 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera);

        RwIm2DVertexSetScreenX(&vertices[0], StretchX(60.0f));
        RwIm2DVertexSetScreenY(&vertices[0], StretchY(61.0f));
        RwIm2DVertexSetScreenZ(&vertices[0], nearScreenZ);
        RwIm2DVertexSetRecipCameraZ(&vertices[0], recipCameraZ);
        RwIm2DVertexSetU(&vertices[0], 0.0f, recipCameraZ);
        RwIm2DVertexSetV(&vertices[0], 0.0f, recipCameraZ);
        RwIm2DVertexSetIntRGBA(&vertices[0], 255, 255, 255, 255);

        RwIm2DVertexSetScreenX(&vertices[1], StretchX(60.0f));
        RwIm2DVertexSetScreenY(&vertices[1], StretchY(388.0f));
        RwIm2DVertexSetScreenZ(&vertices[1], nearScreenZ);
        RwIm2DVertexSetRecipCameraZ(&vertices[1], recipCameraZ);
        RwIm2DVertexSetU(&vertices[1], 0.0f, recipCameraZ);
        RwIm2DVertexSetV(&vertices[1], 1.0f, recipCameraZ);
        RwIm2DVertexSetIntRGBA(&vertices[1], 255, 255, 255, 255);

        RwIm2DVertexSetScreenX(&vertices[2], StretchX(579.0f));
        RwIm2DVertexSetScreenY(&vertices[2], StretchY(388.0f));
        RwIm2DVertexSetScreenZ(&vertices[2], nearScreenZ);
        RwIm2DVertexSetRecipCameraZ(&vertices[2], recipCameraZ);
        RwIm2DVertexSetU(&vertices[2], 1.0f, recipCameraZ);
        RwIm2DVertexSetV(&vertices[2], 1.0f, recipCameraZ);
        RwIm2DVertexSetIntRGBA(&vertices[2], 255, 255, 255, 255);

        RwIm2DVertexSetScreenX(&vertices[3], StretchX(579.0f));
        RwIm2DVertexSetScreenY(&vertices[3], StretchY(61.0f));
        RwIm2DVertexSetScreenZ(&vertices[3], nearScreenZ);
        RwIm2DVertexSetRecipCameraZ(&vertices[3], recipCameraZ);
        RwIm2DVertexSetU(&vertices[3], 1.0f, recipCameraZ);
        RwIm2DVertexSetV(&vertices[3], 0.0f, recipCameraZ);
        RwIm2DVertexSetIntRGBA(&vertices[3], 255, 255, 255, 255);
    }
    RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, vertices, 4);

    CFont::SetFontStyle(FONT_PRICEDOWN);
    CFont::SetWrapx(640.0f);
    CFont::SetDropColor(MENU_BG);
    CFont::SetScale(StretchX(0.8f), StretchY(0.8f));
    CFont::SetColor(MENU_TEXT_LIGHT_GRAY);
    CFont::SetEdge(2);
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    char info[32]{};
    notsa::format_to_sz(info, "{}/{}", s_CurrentImageIdx + 1, images.size());
    CFont::PrintString(SCREEN_STRETCH_FROM_RIGHT(90.0f), StretchY(360.0f), GxtCharFromAscii(info));

    /* helper text -- no gxt key so just inline */
    CFont::SetScale(StretchX(0.4f), StretchY(0.6f));
    CFont::SetFontStyle(eFontStyle::FONT_MENU);
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetEdge(0);
    const auto x = StretchX(610.0f);
    const auto y = SCREEN_STRETCH_FROM_BOTTOM(10.0f);
    CFont::SetColor(MENU_TEXT_WHITE);
    CFont::PrintStringFromBottom(x, y, "LEFT: Previous~n~RIGHT: Next~n~L: Open in Explorer"_gxt);
}

// NOTSA
void CMenuManager::DrawGallerySaveMenu() {
    // NOP. We don't need to ask players if they want to save the snap.
}

#endif
