#pragma once

#include "../xrRender/ColorMapManager.h"

class CRenderTarget : public IRender_Target
{
public:
    ref_rt RT_SecondVP; //--#SM+#-- +SecondVP+
    ref_rt RT_BeforeUi; // Кадр до рендера UI (для пда)
    // R1A: the previous frame after the sky, 1/32 of the screen, for the sky tinted fog.
    // It is shrunk by halves through RT_sky_chain (1/2 .. 1/16): one linear StretchRect straight to a small target
    // reads only 2x2 of every block, the far fog then shimmers while moving and carries cloud detail onto the horizon.
    static constexpr u32 sky_chain_count = 4;
    ref_rt RT_sky_chain[sky_chain_count];
    ref_rt RT_sky_lookup;

private:
    // R1A bloom: 1/4 of the render target
    ref_rt RT_bloom[3];
    ref_shader s_bloom_prefilter;
    ref_shader s_bloom_blur_h;
    ref_shader s_bloom_blur_v;
    ref_shader s_bloom_combine;
    ref_geom g_bloom;
    bool NeedBloom();
    void phase_bloom();
    void draw_bloom_quad(u32 w, u32 h);

    BOOL bAvailable;
    u32 rtWidth;
    u32 rtHeight;

    u32 curWidth;
    u32 curHeight;

    ref_rt RT;
    ref_rt RT_color_map;
    ref_rt RT_distort;
    IDirect3DSurface9* ZB;

    //	Can't implement in a single pass of a shader since
    //	should be compiled only for the hardware that supports it.
    ref_shader s_postprocess[2]; //	Igor: 0 - plain, 1 - colormapped
    ref_shader s_postprocess_D[2]; //	Igor: 0 - plain, 1 - colormapped
    ref_geom g_postprocess;

    float im_noise_time;
    u32 im_noise_shift_w;
    u32 im_noise_shift_h;

    float param_blur;
    float param_gray;
    float param_duality_h;
    float param_duality_v;
    float param_noise;
    float param_noise_scale;
    float param_noise_fps;

    //	Color mapping
    float param_color_map_influence;
    float param_color_map_interpolate;
    ColorMapManager color_map_manager;

    u32 param_color_base;
    u32 param_color_gray;
    Fvector param_color_add;

    u32 frame_distort;

public:
    IDirect3DSurface9* pTempZB;

    //	Igor: for async screenshots
    IDirect3DSurface9* pFB; // 32bit		(r,g,b,a) is situated in the system memory

private:
    BOOL Create();
    bool NeedColorMapping();
    BOOL NeedPostProcess();
    BOOL Available() { return bAvailable; }
    BOOL Perform();

    void calc_tc_noise(Fvector2& p0, Fvector2& p1);
    void calc_tc_duality_ss(Fvector2& r0, Fvector2& r1, Fvector2& l0, Fvector2& l1);
    void phase_distortion();

public:
    CRenderTarget();
    ~CRenderTarget();

    void Begin();
    void End();

    void DoAsyncScreenshot();

    virtual void set_blur(float f) { param_blur = f; }
    virtual void set_gray(float f) { param_gray = f; }
    virtual void set_duality_h(float f) { param_duality_h = _abs(f); }
    virtual void set_duality_v(float f) { param_duality_v = _abs(f); }
    virtual void set_noise(float f) { param_noise = f; }
    virtual void set_noise_scale(float f) { param_noise_scale = f; }
    virtual void set_noise_fps(float f) { param_noise_fps = _abs(f) + EPS_S; }

    virtual void set_color_base(u32 f) { param_color_base = f; }
    virtual void set_color_gray(u32 f) { param_color_gray = f; }
    virtual void set_color_add(const Fvector& f) { param_color_add = f; }

    virtual void set_cm_imfluence(float f) { param_color_map_influence = f; }
    virtual void set_cm_interpolate(float f) { param_color_map_interpolate = f; }
    virtual void set_cm_textures(const shared_str& tex0, const shared_str& tex1) { color_map_manager.SetTextures(tex0, tex1); }

    virtual u32 get_width() { return curWidth; }
    virtual u32 get_height() { return curHeight; }

    u32 get_rtwidth() { return rtWidth; }
    u32 get_rtheight() { return rtHeight; }
};
