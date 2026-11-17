#include "stdafx.h"
#include "r1a_fog.h"
#include "fstaticrender_rendertarget.h"
#include "../../xr_3da/IGame_Persistent.h"
#include "../../xr_3da/IGame_Level.h"
#include "../../xr_3da/Environment.h"
#include "../xrRender/dxRenderDeviceRender.h"

CR1A R1A;

namespace
{
constexpr u32 cache_magic = 0x474f4648; // "HFOG"
constexpr u32 cache_version = 11; // SOP's version: their caches can be used as they are
constexpr u32 s_sky_lookup = 10;
constexpr u32 s_emitter = 11;

#pragma pack(push, 1)
struct emitter_poly
{
    Fvector v[3];
    u32 flags;
};
struct cache_header
{
    u32 magic;
    u32 version;
    u32 src_crc;
    u32 src_size;
    u32 res;
    float min_x, min_z, max_x, max_z;
    float min_y, max_y;
    float texel_world;
};
#pragma pack(pop)
static_assert(sizeof(emitter_poly) == 40);
static_assert(sizeof(cache_header) == 48);

// 1D squared distance transform (Felzenszwalb & Huttenlocher) with the source index of every sample.
// f[i] = 0 at sources, inf elsewhere (or the squared distance of the previous pass); positions are i * step.
void dt_1d(const float* f, u32 n, float step, float* d, int* src, int* v, float* z)
{
    constexpr float inf = 1e20f;
    int k = 0;
    int first = -1;
    for (u32 q = 0; q < n; q++)
        if (f[q] < inf)
        {
            first = int(q);
            break;
        }
    if (first < 0)
    {
        for (u32 q = 0; q < n; q++)
        {
            d[q] = inf;
            src[q] = -1;
        }
        return;
    }
    v[0] = first;
    z[0] = -inf;
    z[1] = +inf;
    for (u32 q = u32(first) + 1; q < n; q++)
    {
        if (f[q] >= inf)
            continue;
        const float qp = float(q) * step;
        float s;
        for (;;)
        {
            const float vp = float(v[k]) * step;
            s = ((f[q] + qp * qp) - (f[v[k]] + vp * vp)) / (2.f * (qp - vp));
            if (s <= z[k]) // never true for k == 0: z[0] = -inf
                k--;
            else
                break;
        }
        k++;
        v[k] = int(q);
        z[k] = s;
        z[k + 1] = +inf;
    }
    k = 0;
    for (u32 q = 0; q < n; q++)
    {
        const float qp = float(q) * step;
        while (z[k + 1] < qp)
            k++;
        const float vp = float(v[k]) * step;
        d[q] = (qp - vp) * (qp - vp) + f[v[k]];
        src[q] = v[k];
    }
}

// Squared distance (world units) from every texel to the nearest texel with mask == want, plus that texel's index
void edt_2d(const xr_vector<u8>& mask, u8 want, u32 res, float tx, float tz, xr_vector<float>& dist2, xr_vector<int>& nearest)
{
    constexpr float inf = 1e20f;
    xr_vector<float> col_d(size_t(res) * res);
    xr_vector<int> col_src(size_t(res) * res);

    xr_vector<float> f(res), d(res), z(res + 1);
    xr_vector<int> s(res), v(res);

    // columns: along z (rows)
    for (u32 x = 0; x < res; x++)
    {
        for (u32 y = 0; y < res; y++)
            f[y] = (mask[y * res + x] == want) ? 0.f : inf;
        dt_1d(f.data(), res, tz, d.data(), s.data(), v.data(), z.data());
        for (u32 y = 0; y < res; y++)
        {
            col_d[y * res + x] = d[y];
            col_src[y * res + x] = s[y];
        }
    }

    dist2.resize(size_t(res) * res);
    nearest.resize(size_t(res) * res);

    // rows: along x
    for (u32 y = 0; y < res; y++)
    {
        for (u32 x = 0; x < res; x++)
            f[x] = col_d[y * res + x];
        dt_1d(f.data(), res, tx, d.data(), s.data(), v.data(), z.data());
        for (u32 x = 0; x < res; x++)
        {
            dist2[y * res + x] = d[x];
            const int sx = s[x];
            const int sy = (sx >= 0) ? col_src[y * res + sx] : -1;
            nearest[y * res + x] = (sx >= 0 && sy >= 0) ? (sy * int(res) + sx) : -1;
        }
    }
}

// ------------------------------------------------------------------------------------------------
// shader constants
CEnvDescriptor& env() { return *g_pGamePersistent->Environment().CurrentEnv; }

class cl_r1a_fog_params final : public R_constant_setup
{
    void setup(R_constant* C) override
    {
        const float range = _max(ps_r1a_hfog_near_end - ps_r1a_hfog_near_start, 0.01f);
        RCache.set_c(C, ps_r1a_hfog_near_start, 1.f / range, ps_r1a_hfog_near_curve, 1.f);
    }
} binder_r1a_fog_params;

class cl_r1a_fog_height final : public R_constant_setup
{
    void setup(R_constant* C) override
    {
        const bool on = ps_r1a_hfog_enable && R1A.has_emitter();
        RCache.set_c(C, on ? R1A.emitter().min_y + env().emitter_height : 0.f, on ? _max(env().emitter_power, 0.f) : 0.f, 0.f, 0.f);
    }
} binder_r1a_fog_height;

class cl_r1a_hfog_color final : public R_constant_setup
{
    void setup(R_constant* C) override
    {
        const Fvector3& c = env().emitter_color;
        RCache.set_c(C, c.x, c.y, c.z, 0.f);
    }
} binder_r1a_hfog_color;

class cl_r1a_hfog_emitter_bb final : public R_constant_setup
{
    void setup(R_constant* C) override
    {
        const auto& e = R1A.emitter();
        RCache.set_c(C, e.min_x, e.min_z, e.max_x, e.max_z);
    }
} binder_r1a_hfog_emitter_bb;

class cl_r1a_hfog_emitter_params final : public R_constant_setup
{
    void setup(R_constant* C) override { RCache.set_c(C, R1A.has_emitter() ? 1.f : 0.f, ps_r1a_hfog_edge, R1A.emitter().texel_world, 0.f); }
} binder_r1a_hfog_emitter_params;

class cl_r1a_hfog_emitter_height final : public R_constant_setup
{
    void setup(R_constant* C) override
    {
        const auto& e = R1A.emitter();
        // the fog rises emitter_height meters over the emitter surface
        RCache.set_c(C, e.min_y + env().emitter_height, e.max_y - e.min_y, CR1A::emitter_sdf_range, 0.f);
    }
} binder_r1a_hfog_emitter_height;

class cl_r1a_hfog_tune final : public R_constant_setup
{
    void setup(R_constant* C) override
    {
        const CEnvDescriptor& E = env();
        RCache.set_c(C, 0.f, E.emitter_noise_amp, E.emitter_noise_scale, E.emitter_noise_speed);
    }
} binder_r1a_hfog_tune;

class cl_r1a_mipfog_params final : public R_constant_setup
{
    void setup(R_constant* C) override
    {
        const CEnvDescriptor& E = env();
        const bool on = ps_r1a_mipfog_enable && R1A.sky_lookup_valid();
        RCache.set_c(C, on ? 1.f : 0.f, E.mipfog_near_start, E.mipfog_directional, E.mipfog_amount);
    }
} binder_r1a_mipfog_params;
} // namespace

void CR1A::create()
{
    auto& R = *dxRenderDeviceRender::Instance().Resources;
    R.RegisterConstantSetup("r1a_fog_params", &binder_r1a_fog_params);
    R.RegisterConstantSetup("r1a_fog_height", &binder_r1a_fog_height);
    R.RegisterConstantSetup("r1a_hfog_color", &binder_r1a_hfog_color);
    R.RegisterConstantSetup("r1a_hfog_emitter_bb", &binder_r1a_hfog_emitter_bb);
    R.RegisterConstantSetup("r1a_hfog_emitter_params", &binder_r1a_hfog_emitter_params);
    R.RegisterConstantSetup("r1a_hfog_emitter_height", &binder_r1a_hfog_emitter_height);
    R.RegisterConstantSetup("r1a_hfog_tune", &binder_r1a_hfog_tune);
    R.RegisterConstantSetup("r1a_mipfog_params", &binder_r1a_mipfog_params);
}

void CR1A::destroy() { level_unload(); }

// ------------------------------------------------------------------------------------------------
// level.emitter
void CR1A::level_load()
{
    level_unload();

    string_path src_name;
    if (!FS.exist(src_name, "$level$", "level.emitter"))
    {
        Msg("* level emitter: level.emitter not found, no height fog on this level");
        return;
    }

    IReader* src = FS.r_open(src_name);
    const u32 src_size = src->length();
    const u32 src_crc = crc32(src->pointer(), src_size);

    string_path cache_rel, cache_name;
    xr_sprintf(cache_rel, "emitters_cache\\%s\\level.emitter_cache", g_pGameLevel->name().c_str());
    FS.update_path(cache_name, "$app_data_root$", cache_rel);

    xr_vector<u32> pixels;
    if (load_cache(cache_name, src_crc, src_size, pixels))
        Msg("* Loading level.emitter from cache: %s", cache_name);
    else
    {
        Msg("* level emitter: building cache: %s", cache_name);
        const u32 t0 = GetTickCount();
        if (build(src, pixels))
        {
            Msg("* level emitter: %ux%u mask in %u ms", emitter_res, emitter_res, GetTickCount() - t0);
            save_cache(cache_name, src_crc, src_size, pixels);
        }
    }
    FS.r_close(src);

    if (m_emitter.valid)
        create_texture(pixels);
}

void CR1A::level_unload()
{
    _RELEASE(m_emitter_tex);
    m_emitter = {};
    if (HW.pDevice)
        HW.pDevice->SetTexture(s_emitter, nullptr);
}

bool CR1A::load_cache(LPCSTR cache_name, u32 src_crc, u32 src_size, xr_vector<u32>& pixels)
{
    if (!FS.exist(cache_name))
        return false;
    IReader* F = FS.r_open(cache_name);
    if (!F)
        return false;
    bool ok = false;
    const size_t px_bytes = size_t(emitter_res) * emitter_res * 4;
    if (F->length() == sizeof(cache_header) + px_bytes)
    {
        cache_header H;
        F->r(&H, sizeof(H));
        if (H.magic == cache_magic && H.version == cache_version && H.src_crc == src_crc && H.src_size == src_size && H.res == emitter_res)
        {
            pixels.resize(size_t(emitter_res) * emitter_res);
            F->r(pixels.data(), u32(px_bytes));
            m_emitter.valid = true;
            m_emitter.min_x = H.min_x;
            m_emitter.min_z = H.min_z;
            m_emitter.max_x = H.max_x;
            m_emitter.max_z = H.max_z;
            m_emitter.min_y = H.min_y;
            m_emitter.max_y = H.max_y;
            m_emitter.texel_world = H.texel_world;
            ok = true;
        }
        else
            Msg("* level emitter: cache mismatch, rebuilding");
    }
    FS.r_close(F);
    return ok;
}

void CR1A::save_cache(LPCSTR cache_name, u32 src_crc, u32 src_size, const xr_vector<u32>& pixels)
{
    IWriter* W = FS.w_open(cache_name);
    if (!W)
    {
        Msg("! level.emitter_cache: can't write cache [%s]", cache_name);
        return;
    }
    cache_header H{cache_magic, cache_version, src_crc, src_size, emitter_res,
                   m_emitter.min_x, m_emitter.min_z, m_emitter.max_x, m_emitter.max_z, m_emitter.min_y, m_emitter.max_y, m_emitter.texel_world};
    W->w(&H, sizeof(H));
    W->w(pixels.data(), u32(pixels.size() * sizeof(u32)));
    FS.w_close(W);
}

bool CR1A::build(IReader* src, xr_vector<u32>& pixels)
{
    src->seek(0);
    IReader* S = src->open_chunk(1);
    if (!S)
    {
        Msg("! level emitter: no polygons in level.emitter");
        return false;
    }

    xr_vector<emitter_poly> polys;
    polys.resize(S->length() / sizeof(emitter_poly));
    if (!polys.empty())
        S->r(polys.data(), u32(polys.size() * sizeof(emitter_poly)));
    S->close();
    if (polys.empty())
        return false;

    Fbox bb;
    bb.invalidate();
    for (const auto& P : polys)
        for (const auto& v : P.v)
            bb.modify(v);

    const u32 res = emitter_res;
    const float pad = emitter_sdf_range * 1.5f;
    m_emitter.min_x = bb.min.x - pad;
    m_emitter.min_z = bb.min.z - pad;
    m_emitter.max_x = bb.max.x + pad;
    m_emitter.max_z = bb.max.z + pad;
    m_emitter.min_y = bb.min.y;
    m_emitter.max_y = _max(bb.max.y, bb.min.y + 0.01f);

    const float tx = (m_emitter.max_x - m_emitter.min_x) / float(res);
    const float tz = (m_emitter.max_z - m_emitter.min_z) / float(res);
    m_emitter.texel_world = _max(tx, tz);

    // rasterise: texel centres inside a triangle (XZ) get the triangle's height, the highest one wins
    xr_vector<u8> inside(size_t(res) * res, 0);
    xr_vector<float> height(size_t(res) * res, -flt_max);

    for (const auto& P : polys)
    {
        const Fvector &a = P.v[0], &b = P.v[1], &c = P.v[2];
        const float den = (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
        if (_abs(den) < 1e-8f)
            continue;

        const float x0 = _min(a.x, _min(b.x, c.x)), x1 = _max(a.x, _max(b.x, c.x));
        const float z0 = _min(a.z, _min(b.z, c.z)), z1 = _max(a.z, _max(b.z, c.z));
        const int ix0 = clampr(iFloor((x0 - m_emitter.min_x) / tx - 0.5f), 0, int(res) - 1);
        const int ix1 = clampr(iCeil((x1 - m_emitter.min_x) / tx - 0.5f), 0, int(res) - 1);
        const int iz0 = clampr(iFloor((z0 - m_emitter.min_z) / tz - 0.5f), 0, int(res) - 1);
        const int iz1 = clampr(iCeil((z1 - m_emitter.min_z) / tz - 0.5f), 0, int(res) - 1);

        for (int iz = iz0; iz <= iz1; iz++)
        {
            const float pz = m_emitter.min_z + (float(iz) + 0.5f) * tz;
            for (int ix = ix0; ix <= ix1; ix++)
            {
                const float px = m_emitter.min_x + (float(ix) + 0.5f) * tx;
                const float w1 = ((b.z - c.z) * (px - c.x) + (c.x - b.x) * (pz - c.z)) / den;
                const float w2 = ((c.z - a.z) * (px - c.x) + (a.x - c.x) * (pz - c.z)) / den;
                const float w3 = 1.f - w1 - w2;
                if (w1 < -1e-4f || w2 < -1e-4f || w3 < -1e-4f)
                    continue;
                const size_t i = size_t(iz) * res + ix;
                inside[i] = 1;
                height[i] = _max(height[i], w1 * a.y + w2 * b.y + w3 * c.y);
            }
        }
    }

    // signed distance to the edge of the emitter area, the height outside comes from the nearest emitter texel
    xr_vector<float> d_to_in, d_to_out;
    xr_vector<int> n_in, n_out;
    edt_2d(inside, 1, res, tx, tz, d_to_in, n_in);
    edt_2d(inside, 0, res, tx, tz, d_to_out, n_out);

    pixels.resize(size_t(res) * res);
    const float y_range = m_emitter.max_y - m_emitter.min_y;
    for (size_t i = 0, n = pixels.size(); i < n; i++)
    {
        float sd;
        float h;
        if (inside[i])
        {
            sd = _sqrt(d_to_out[i]);
            h = height[i];
        }
        else
        {
            sd = -_sqrt(d_to_in[i]);
            h = (n_in[i] >= 0) ? height[n_in[i]] : m_emitter.min_y;
        }
        const float s01 = clampr(sd / emitter_sdf_range, -1.f, 1.f) * 0.5f + 0.5f;
        const float h01 = clampr((h - m_emitter.min_y) / y_range, 0.f, 1.f);
        const u32 s16 = u32(clampr(iFloor(s01 * 65535.f + 0.5f), 0, 65535));
        const u32 h16 = u32(clampr(iFloor(h01 * 65535.f + 0.5f), 0, 65535));
        // A8R8G8B8: A = sdf hi, R = sdf lo, G = height hi, B = height lo
        pixels[i] = ((s16 >> 8) << 24) | ((s16 & 0xff) << 16) | ((h16 >> 8) << 8) | (h16 & 0xff);
    }

    m_emitter.valid = true;
    return true;
}

void CR1A::create_texture(const xr_vector<u32>& pixels)
{
    _RELEASE(m_emitter_tex);
    if (FAILED(HW.pDevice->CreateTexture(emitter_res, emitter_res, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &m_emitter_tex, nullptr)))
    {
        Msg("! level emitter: can't create the mask texture");
        m_emitter.valid = false;
        return;
    }
    D3DLOCKED_RECT R;
    if (SUCCEEDED(m_emitter_tex->LockRect(0, &R, nullptr, 0)))
    {
        for (u32 y = 0; y < emitter_res; y++)
            CopyMemory((u8*)R.pBits + y * R.Pitch, pixels.data() + size_t(y) * emitter_res, emitter_res * 4);
        m_emitter_tex->UnlockRect(0);
    }
}

// ------------------------------------------------------------------------------------------------
// sky lookup and texture binding
static void set_lookup_sampler(u32 stage)
{
    HW.pDevice->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    HW.pDevice->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    HW.pDevice->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    HW.pDevice->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    HW.pDevice->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    HW.pDevice->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
}

// The frame right after the sky and clouds (R1 draws them after the opaque level) is shrunk into the lookup and used by the
// next frame: distant fully fogged geometry takes the colour that is already there, so it dissolves into the horizon
// band of the sky as in SOP. Starting from fog_color (no lookup yet) the far fog stays the weather's colour.
void CR1A::capture_sky_lookup()
{
    CRenderTarget* T = RImplementation.Target;
    if (!ps_r1a_mipfog_enable || !T || !T->RT_sky_lookup)
    {
        m_sky_valid = false;
        return;
    }
    IDirect3DSurface9* cur{};
    if (FAILED(HW.pDevice->GetRenderTarget(0, &cur)) || !cur)
        return;
    // by halves: every linear step averages 2x2 texels, the result is a box-filtered 1/32 of the frame
    bool ok = true;
    IDirect3DSurface9* src = cur;
    for (u32 i = 0; ok && i < CRenderTarget::sky_chain_count; ++i)
    {
        IDirect3DSurface9* dst = T->RT_sky_chain[i] ? T->RT_sky_chain[i]->pRT : nullptr;
        ok = dst && SUCCEEDED(HW.pDevice->StretchRect(src, nullptr, dst, nullptr, D3DTEXF_LINEAR));
        src = dst;
    }
    m_sky_valid = ok && SUCCEEDED(HW.pDevice->StretchRect(src, nullptr, T->RT_sky_lookup->pRT, nullptr, D3DTEXF_LINEAR));
    cur->Release();
    m_sky_owner = T;
}

void CR1A::bind_textures()
{
    CRenderTarget* T = RImplementation.Target;
    // a new render target (vid_restart) holds no image yet
    if (T != m_sky_owner)
        m_sky_valid = false;
    HW.pDevice->SetTexture(s_sky_lookup, (m_sky_valid && T && T->RT_sky_lookup) ? T->RT_sky_lookup->pSurface : nullptr);
    set_lookup_sampler(s_sky_lookup);
    HW.pDevice->SetTexture(s_emitter, m_emitter_tex);
    set_lookup_sampler(s_emitter);
}
