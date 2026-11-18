#pragma once

// R1A: the look of SOP's static renderer (renderer_r1a) on R1.
// - height fog over the fog emitters of the level (level.emitter: triangles laid over lowlands, same format as level.hom).
//   They are rasterised into a 2048x2048 A8R8G8B8 mask over the XZ plane: R/A = signed distance to the emitter edge
//   (+inside, -outside, +-32 m), G/B = height of the emitter surface between minY and maxY. The mask is cached in
//   $app_data_root$\emitters_cache\<level>\level.emitter_cache (SOP's "HFOG" format) and bound to sampler s11.
// - "mipfog": the frame after the sky is shrunk to 1/8 (s10) and the next frame's far fog takes its colour from the pixel's
//   place in it, so distant geometry dissolves into the sky's horizon colour.
// - bloom (r1a_bloom_* shaders) in CRenderTarget::End.
// The weather parameters (emitter_*, mipfog_*) live in CEnvDescriptor, the console variables are r1a_*.
class CR1A
{
public:
    static constexpr u32 emitter_res = 2048;
    static constexpr float emitter_sdf_range = 32.f; // world meters for sdf -1..+1

    struct emitter_info
    {
        bool valid{};
        float min_x{}, min_z{}, max_x{}, max_z{};
        float min_y{}, max_y{};
        float texel_world{};
    };

    void create();
    void destroy();

    void level_load();
    void level_unload();

    // the textures are bound to s10/s11 for the whole frame; the sky lookup is captured after the sky for the next frame
    void bind_textures();
    void capture_sky_lookup();

    bool has_emitter() const { return m_emitter.valid && m_emitter_tex; }
    const emitter_info& emitter() const { return m_emitter; }
    bool sky_lookup_valid() const { return m_sky_valid; }

    // HUD passes draw with the camera at the origin: positions there are camera-relative, so the height fog (world height,
    // emitter mask, distance to eye_position) can't be computed for them and is switched off
    void set_hud_pass(bool b) { m_hud_pass = b; }
    bool hud_pass() const { return m_hud_pass; }

private:
    bool load_cache(LPCSTR cache_name, u32 src_crc, u32 src_size, xr_vector<u32>& pixels);
    void save_cache(LPCSTR cache_name, u32 src_crc, u32 src_size, const xr_vector<u32>& pixels);
    bool build(IReader* src, xr_vector<u32>& pixels);
    void create_texture(const xr_vector<u32>& pixels);

    emitter_info m_emitter;
    IDirect3DTexture9* m_emitter_tex{};
    bool m_sky_valid{};
    bool m_hud_pass{};
    const void* m_sky_owner{};
};

extern CR1A R1A;
