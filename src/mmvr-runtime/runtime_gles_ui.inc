GlTarget uiTexture, hudTexture, hudSample;
GlTransfer transfer;
bool uiVisible = false;
void GlCopyToSwapchain(const GlImage& source, GLuint image) {
    transfer.ToSwapchain(source, image);
}
static void SourceBlend(void* opaque) {
    if (opaque) {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ZERO, GL_ONE);
    } else
        glDisable(GL_BLEND);
}
void PaintUi(GlImage& target, const UiDrawFrame& frame) {
    GlFramebufferState save;
    GLint viewport[4];
    GLfloat clearColor[4];
    GLboolean mask[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, target.width, target.height);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0, 0, 0,
                 (frame.kind == UiKind::Theater || frame.kind == UiKind::Vision || frame.kind == UiKind::MotionBlur || frame.kind == UiKind::Reveal || frame.kind == UiKind::WearableMask)
                     ? 1.f
                     : 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (drawUi)
        drawUi(frame);
    // One readback per runtime session: prove the headset menu backend painted
    // pixels, independently of the intentionally hidden Android mirror.
    static bool checkedMenu = false;
    if (frame.kind == UiKind::Menu && !checkedMenu) {
        checkedMenu = true;
        unsigned char pixel[4]{};
        glReadPixels(20, target.height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        Log("Quest menu framebuffer sample alpha=" + std::to_string(pixel[3]));
    }

    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glColorMask(mask[0], mask[1], mask[2], mask[3]);
}
GlImage* CompositeSource(GlContext*, GlImage* source, UiKind kind, uintptr_t history = 0, float historyAlpha = 0) {
    hudTexture.Resize(width, height);
    hudSample.Resize(width, height, true);
    transfer.Copy(*source, hudSample.image);
    UiDrawFrame frame{kind, width, height, hudSample.image.texture, SourceBlend,
                      kind == UiKind::Hud ? nullptr : this, currentFov, history, historyAlpha};
    AttachWearableMask(frame);
    PaintUi(hudTexture.image, frame);
    return &hudTexture.image;
}
void VerifyVisionComposition() {
    // Isolated one-shot diagnostic in protected test boots, never during play.
    const char* fixture = std::getenv("MMVR_NATIVE_TEST");
    if (!PrivateDebugTools || !fixture || std::strcmp(fixture, "1") != 0)
        return;
    const auto oldLens = lensVision, oldFade = viewToolFade, oldStreak = speedStreaks;
    const auto oldKind = viewToolKind;
    const auto oldTint = worldTint, oldScreenFade = screenFade;
    const auto oldOpacity = settings.Get(Setting::HudOpacity);
    GlFramebufferState saved;
    GLfloat clear[4];
    GLboolean colorMask[4];
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    GlTarget sample, output;
    sample.Resize(64, 64, true);
    output.Resize(64, 64);
    glBindFramebuffer(GL_FRAMEBUFFER, sample.image.framebuffer);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(180.f / 255, 80.f / 255, 40.f / 255, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    bool telescope = true, lens = true;
    worldTint = {};
    speedStreaks = 0;
    viewToolFade = 0;
    for (int mode : { 1, 2 })
        for (float opacity : { 0.f, 1.f }) {
            settings.Set(Setting::HudOpacity, opacity);
            lensVision = mode == 1 ? 1 : 0;
            viewToolKind = mode == 2 ? 2 : 0;
            PaintUi(output.image, { UiKind::Vision, 64, 64, sample.image.texture, SourceBlend, this, {} });
            glBindFramebuffer(GL_READ_FRAMEBUFFER, output.image.framebuffer);
            for (int y : { 1, 32, 62 })
                for (int x : { 1, 32, 62 }) {
                    unsigned char p[4]{};
                    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
                    bool center = x == 32 && y == 32;
                    bool good = center      ? (std::abs(int(p[0]) - 180) <= 1 && std::abs(int(p[1]) - 80) <= 1 &&
                                               std::abs(int(p[2]) - 40) <= 1)
                                : mode == 2 ? (p[0] == 0 && p[1] == 0 && p[2] == 0)
                                            : (p[0] >= 147 && p[0] <= 151 && p[1] >= 54 && p[1] <= 57 && p[2] >= 68 &&
                                               p[2] <= 71);
                    (mode == 1 ? lens : telescope) &= good && p[3] == 255;
                }
        }
    GlTarget history;
    history.Resize(64, 64, true);
    glBindFramebuffer(GL_FRAMEBUFFER, history.image.framebuffer);
    glClearColor(20.f / 255, 40.f / 255, 200.f / 255, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    PaintUi(output.image,
            { UiKind::MotionBlur, 64, 64, sample.image.texture, SourceBlend, this, {}, history.image.texture, .5f });
    glBindFramebuffer(GL_READ_FRAMEBUFFER, output.image.framebuffer);
    bool blur = true;
    for (int y : { 1, 32, 62 })
        for (int x : { 1, 32, 62 }) {
            unsigned char p[4]{};
            glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
            blur &= std::abs(int(p[0]) - 100) <= 1 && std::abs(int(p[1]) - 60) <= 1 && std::abs(int(p[2]) - 120) <= 1 &&
                    p[3] == 255;
        }
    Log(std::string("GLES motion history: ") + (blur ? "pass" : "FAIL"));
    std::ofstream("mmvr-motion-history.json") << "{\"fullViewAlpha\":" << blur << "}";
    bool fullFade = true;
    for (float opacity : { 0.f, 1.f })
        for (int alpha : { 0, 25, 128, 255 }) {
            settings.Set(Setting::HudOpacity, opacity);
            screenFade = { 1, 1, 1, alpha / 255.f };
            PaintUi(output.image, { UiKind::ScreenFade, 64, 64 });
            glBindFramebuffer(GL_READ_FRAMEBUFFER, output.image.framebuffer);
            unsigned char pixels[64 * 64 * 4]{};
            glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            for (auto value : pixels)
                fullFade &= std::abs(int(value) - alpha) <= 1;
        }
    screenFade = oldScreenFade;
    Log(std::string("GLES full-field fade: ") + (fullFade ? "pass" : "FAIL"));
    std::ofstream("mmvr-screen-fade.json")
        << "{\"allPixelsCovered\":" << fullFade << ",\"hudOpacityIndependent\":true}";
    lensVision = oldLens;
    viewToolFade = oldFade;
    speedStreaks = oldStreak;
    viewToolKind = oldKind;
    worldTint = oldTint;
    settings.Set(Setting::HudOpacity, oldOpacity);
    glClearColor(clear[0], clear[1], clear[2], clear[3]);
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    std::ofstream("mmvr-vision-composition.json")
        << "{\"lensFullEye\":" << lens << ",\"telescopeFullEye\":" << telescope << ",\"hudOpacityIndependent\":true}";
    Log(std::string("GLES vision composition: lens=") + (lens ? "pass" : "FAIL") +
        " telescope=" + (telescope ? "pass" : "FAIL"));
}
void DrawUiLayer(GlContext*) {
    uiVisible = drawUi && (VisibleFullViewFade() || menu.open || selector.open || assignment.open);
    static bool menuProbeDone = false;
    if (drawUi && !menuProbeDone) {
        menuProbeDone = true;
        uiTexture.Resize(1024, 768);
        PaintUi(uiTexture.image, { UiKind::Menu, 1024, 768, maskIcon });
        VerifyVisionComposition();
    }
    if (!uiVisible)
        return;
    const unsigned wantedHeight = menu.open && menu.UsesNativePanel() && !VisibleFullViewFade()
        ? NativeMenuSurfaceHeight : 768;
    if (uiChain && uiHeight != wantedHeight) {
        Check(xrDestroySwapchain(uiChain), "Resize VR UI swapchain");
        uiChain = XR_NULL_HANDLE;
        uiImages.clear();
    }
    uiHeight = wantedHeight;
    uiTexture.Resize(1024, uiHeight);
    if (!uiChain) {
        XrSwapchainCreateInfo info{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
        info.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        info.format = SwapchainFormat;
        info.sampleCount = info.faceCount = info.arraySize = info.mipCount = 1;
        info.width = 1024;
        info.height = uiHeight;
        Check(xrCreateSwapchain(session, &info, &uiChain), "Create VR UI swapchain");
        uint32_t count = 0;
        Check(xrEnumerateSwapchainImages(uiChain, 0, &count, nullptr), "Count UI images");
        uiImages.resize(count, { SwapchainImageType });
        Check(xrEnumerateSwapchainImages(uiChain, count, &count,
                                         reinterpret_cast<XrSwapchainImageBaseHeader*>(uiImages.data())),
              "Get UI images");
    }
    PaintUi(uiTexture.image, { VisibleFullViewFade() ? UiKind::ScreenFade
                               : menu.open       ? UiKind::Menu
                                                 : UiKind::Selector,
                               1024, uiHeight, maskIcon });
    CopyImage(nullptr, &uiTexture.image, uiChain, uiImages);
}
