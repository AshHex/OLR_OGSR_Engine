#include "stdafx.h"

#include "UIModels.h"

#if defined(USE_DX10) || defined(USE_DX11)
#include "../xrRenderDX10/StateManager/dx10StateManager.h"
#define UI3D_DX10
#endif

namespace
{
// scratch limits: a tile x SSAA must stay within max_scratch_pixels and max_scratch per side; bigger tiles get less supersampling
constexpr u32 max_scratch = 4096;
constexpr u32 max_scratch_pixels = 2048 * 2048;
// tiles up to this size (the longer side, pixels) get at least r_ui3d_ssaa_small: busy textures and normal maps alias as speckles on small
// icons at 2x, and small tiles are cheap (and usually cached)
constexpr u32 small_tile = 256;

Fvector dir_or(const Fvector& v, const Fvector& fallback)
{
    Fvector d = v;
    if (d.square_magnitude() < EPS_S)
        return fallback;
    return d.normalize();
}

#ifdef UI3D_DX10
using viewport_t = D3D_VIEWPORT;

void set_viewport(u32 x, u32 y, u32 w, u32 h)
{
    D3D_VIEWPORT vp{};
#ifdef USE_DX11
    vp.TopLeftX = static_cast<float>(x);
    vp.TopLeftY = static_cast<float>(y);
    vp.Width = static_cast<float>(w);
    vp.Height = static_cast<float>(h);
#else
    vp.TopLeftX = static_cast<INT>(x);
    vp.TopLeftY = static_cast<INT>(y);
    vp.Width = w;
    vp.Height = h;
#endif
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    HW.pContext->RSSetViewports(1, &vp);
}

void clear_rt(ID3DRenderTargetView* rt, float r, float g, float b, float a)
{
    const FLOAT c[4]{r, g, b, a};
    HW.pContext->ClearRenderTargetView(rt, c);
}

void clear_zb(ID3DDepthStencilView* zb) { HW.pContext->ClearDepthStencilView(zb, D3D_CLEAR_DEPTH | D3D_CLEAR_STENCIL, 1.f, 0); }
#else
void set_viewport(u32 x, u32 y, u32 w, u32 h)
{
    const D3DVIEWPORT9 vp{x, y, w, h, 0.f, 1.f};
    CHK_DX(HW.pDevice->SetViewport(&vp));
}

// DX9 clears the targets bound now, inside the viewport
void clear_bound(u32 flags, u32 color) { CHK_DX(HW.pDevice->Clear(0, nullptr, flags, color, 1.f, 0)); }
#endif

void set_targets(ID3DRenderTargetView* rt0, ID3DRenderTargetView* rt1, ID3DRenderTargetView* rt2, ID3DDepthStencilView* zb)
{
    RCache.set_RT(rt0, 0);
    RCache.set_RT(rt1, 1);
    RCache.set_RT(rt2, 2);
    RCache.set_ZB(zb);
}
} // namespace

bool CUIModels::supported()
{
#ifdef UI3D_DX10
    return !!RImplementation.o.dx10_gbuffer_opt;
#else
    return true;
#endif
}

void CUIModels::set_outline(const Fcolor& color, float width, float softness)
{
    g_ui3d_outline.set(color.r * color.a, color.g * color.a, color.b * color.a, color.a);
    g_ui3d_outline_params.set(std::max(width, 0.f), std::max(softness, 0.f), ui_model_outline_max, 0.f);
    g_ui3d_outline_dirty = true;
}

//////////////////////////////////////////////////////////////////////////
// page allocator

s32 CUIModels::page::alloc(const u32 w, const u32 h)
{
    // the smallest free cell that fits
    s32 best = -1;
    u32 best_area = std::numeric_limits<u32>::max();
    for (s32 i = 0; i < static_cast<s32>(nodes.size()); ++i)
    {
        const node& n = nodes[i];
        if (n.state != node::free_leaf || n.w < w || n.h < h)
            continue;
        const u32 area = u32(n.w) * n.h;
        if (area < best_area)
        {
            best_area = area;
            best = i;
        }
    }
    if (best < 0)
        return -1;

    // split it in halves while the tile still fits, along the axis with more room
    for (;;)
    {
        const node n = nodes[best];
        const bool can_x = n.w / 2u >= w && n.w / 2u >= min_cell;
        const bool can_y = n.h / 2u >= h && n.h / 2u >= min_cell;
        if (!can_x && !can_y)
            break;
        const bool split_x = can_x && (!can_y || static_cast<float>(n.w) / w >= static_cast<float>(n.h) / h);

        s32 c;
        if (!free_pairs.empty())
        {
            c = free_pairs.back();
            free_pairs.pop_back();
        }
        else
        {
            c = static_cast<s32>(nodes.size());
            nodes.resize(nodes.size() + 2);
        }

        node a{}, b{};
        a.parent = b.parent = best;
        if (split_x)
        {
            a.x = n.x, a.y = n.y, a.w = n.w / 2, a.h = n.h;
            b.x = n.x + a.w, b.y = n.y, b.w = n.w - a.w, b.h = n.h;
        }
        else
        {
            a.x = n.x, a.y = n.y, a.w = n.w, a.h = n.h / 2;
            b.x = n.x, b.y = n.y + a.h, b.w = n.w, b.h = n.h - a.h;
        }
        nodes[c] = a;
        nodes[c + 1] = b;
        nodes[best].child = c;
        nodes[best].state = node::split;
        best = c;
    }

    nodes[best].state = node::used_leaf;
    ++used;
    return best;
}

void CUIModels::page::free(const s32 n)
{
    VERIFY(nodes[n].state == node::used_leaf);
    nodes[n].state = node::free_leaf;
    --used;

    // merge free siblings back into their parent
    for (s32 p = nodes[n].parent; p >= 0; p = nodes[p].parent)
    {
        const s32 c = nodes[p].child;
        if (nodes[c].state != node::free_leaf || nodes[c + 1].state != node::free_leaf)
            break;
        nodes[c].state = node::released;
        nodes[c + 1].state = node::released;
        free_pairs.push_back(c);
        nodes[p].child = -1;
        nodes[p].state = node::free_leaf;
    }
}

//////////////////////////////////////////////////////////////////////////
// tiles

void CUIModels::create_page()
{
    if (!activate_registered)
    {
        Device.seqAppActivate.Add(this);
        activate_registered = true;
    }
    page& pg = pages.emplace_back();
    string64 name, mask_name;
    xr_sprintf(name, "$user$ui3d_page_%u", static_cast<u32>(pages.size() - 1));
    xr_sprintf(mask_name, "%s_mask", name);
    pg.name = name;
    pg.rt.create(name, page_size, page_size, D3DFMT_A8R8G8B8);
#ifdef UI3D_DX10
    pg.mask.create(mask_name, page_size, page_size, D3DFMT_L8);
    clear_rt(pg.rt->pRT, 0.f, 0.f, 0.f, 0.f);
    clear_rt(pg.mask->pRT, 1.f, 1.f, 1.f, 1.f);
#else
    // DX9: the targets the resolve writes together have the same bit depth (MRT)
    pg.mask.create(mask_name, page_size, page_size, D3DFMT_A8R8G8B8);
    // no need to clear: a tile's rect is written whole by its resolve
#endif
    pg.nodes.push_back(node{0, 0, static_cast<u16>(page_size), static_cast<u16>(page_size)});

    Msg("* [ui3d] atlas page %s created (%u x %u)", pg.name.c_str(), page_size, page_size);
}

CUIModels::tile* CUIModels::find(const u32 id)
{
    const u32 slot = id & 0xffffu;
    if (slot == 0 || slot > tiles.size())
        return nullptr;
    tile& t = tiles[slot - 1];
    return t.used && t.gen == (id >> 16) ? &t : nullptr;
}

const CUIModels::tile* CUIModels::find(const u32 id) const { return const_cast<CUIModels*>(this)->find(id); }

void CUIModels::release(const u32 slot)
{
    tile& t = tiles[slot - 1];
    if (t.key)
        keyed.erase(t.key);
    pages[t.page].free(t.node);
    const u16 gen = t.gen + 1;
    t = {};
    t.gen = gen;
    free_tiles.push_back(slot);
}

u32 CUIModels::evict_stale()
{
    // not drawn for 3 frames: scrolled out, hidden, or a closed dialog not destroyed yet
    u32 count = 0;
    for (u32 slot = 1; slot <= tiles.size(); ++slot)
    {
        const tile& t = tiles[slot - 1];
        if (t.used && Device.dwFrame - t.last_frame > 3)
        {
            release(slot);
            ++count;
        }
    }
    return count;
}

bool CUIModels::evict_cached(const u32 w, const u32 h, u32& page_id, s32& n)
{
    xr_vector<u32> cached;
    for (u32 slot = 1; slot <= tiles.size(); ++slot)
    {
        const tile& t = tiles[slot - 1];
        if (t.used && t.key && !t.refs)
            cached.push_back(slot);
    }
    if (cached.empty())
        return false;
    std::sort(cached.begin(), cached.end(), [this](const u32 a, const u32 b) { return tiles[a - 1].last_frame < tiles[b - 1].last_frame; });

    // one at a time: a freed cell only makes room on its own page, and merges with free neighbours
    u32 count = 0;
    bool done = false;
    for (const u32 slot : cached)
    {
        const u32 p = tiles[slot - 1].page;
        release(slot);
        ++count;
        n = pages[p].alloc(w, h);
        if (n >= 0)
        {
            page_id = p;
            done = true;
            break;
        }
    }
    stat_cache_evicted += count;
    Msg("* [ui3d] %u cached tiles evicted for a %u x %u tile%s", count, w, h, done ? "" : ": not enough");
    return done;
}

u32 CUIModels::tile_alloc(u32 w, u32 h)
{
    w = std::clamp(w, 1u, page_size);
    h = std::clamp(h, 1u, page_size);

    u32 page_id = 0;
    s32 n = -1;
    for (; page_id < pages.size(); ++page_id)
    {
        n = pages[page_id].alloc(w, h);
        if (n >= 0)
            break;
    }
    // past cache_pages, cached tiles give their room before a new page is made
    if (n < 0 && pages.size() >= cache_pages)
        evict_cached(w, h, page_id, n);
    if (n < 0 && pages.size() >= max_pages)
    {
        // full: make room from the tiles nobody draws any more
        if (const u32 evicted = evict_stale())
        {
            Msg("* [ui3d] atlas full: %u tiles not drawn for 3 frames evicted", evicted);
            for (page_id = 0; page_id < pages.size(); ++page_id)
            {
                n = pages[page_id].alloc(w, h);
                if (n >= 0)
                    break;
            }
        }
        if (n < 0)
        {
            static u32 logged_frame = 0;
            if (logged_frame != Device.dwFrame)
                Msg("! [ui3d] no space for a %u x %u tile: all %u atlas pages are full of tiles in use", w, h, max_pages);
            logged_frame = Device.dwFrame;
            return 0;
        }
    }
    if (n < 0)
    {
        create_page();
        page_id = static_cast<u32>(pages.size() - 1);
        n = pages[page_id].alloc(w, h);
        R_ASSERT(n >= 0);
    }

    u32 slot;
    if (!free_tiles.empty())
    {
        slot = free_tiles.back();
        free_tiles.pop_back();
    }
    else
    {
        tiles.emplace_back();
        slot = static_cast<u32>(tiles.size());
        R_ASSERT(slot <= 0xffffu);
    }

    tile& t = tiles[slot - 1];
    t.page = page_id;
    t.node = n;
    t.w = w;
    t.h = h;
    t.used = true;
    t.valid = false;
    t.last_frame = Device.dwFrame;
    return u32(t.gen) << 16 | slot;
}

u32 CUIModels::tile_shared(const u64 key, const u32 w, const u32 h)
{
    if (const auto it = keyed.find(key); it != keyed.end())
    {
        tile& t = tiles[it->second - 1];
        // the key holds the size: another size is a hash collision, that model gets its own tile
        if (t.w != std::clamp(w, 1u, page_size) || t.h != std::clamp(h, 1u, page_size))
            return tile_alloc(w, h);
        ++t.refs;
        ++stat_shared_hits;
        return u32(t.gen) << 16 | it->second;
    }

    const u32 id = tile_alloc(w, h);
    if (!id)
        return 0;
    ++stat_shared_misses;
    tile& t = tiles[(id & 0xffffu) - 1];
    t.key = key;
    t.refs = 1;
    keyed.emplace(key, id & 0xffffu);
    return id;
}

void CUIModels::tile_free(const u32 id)
{
    tile* t = find(id);
    if (!t)
        return;
    // a shared tile stays with its image while others hold it, then cached until its room is needed
    if (t->key)
    {
        if (t->refs)
            --t->refs;
        return;
    }
    release(id & 0xffffu);
}

void CUIModels::stats(ui_model_stats& s) const
{
    s = {};
    s.renders = stat_renders;
    s.shared_hits = stat_shared_hits;
    s.shared_misses = stat_shared_misses;
    s.cache_evicted = stat_cache_evicted;
    s.pages = static_cast<u32>(pages.size());
    s.page_size = page_size;
    for (const tile& t : tiles)
    {
        if (!t.used)
            continue;
        ++s.tiles;
        if (t.key && !t.refs)
            ++s.cached;
    }
}

bool CUIModels::tile_texture(const u32 id, shared_str& texture, Frect& rect)
{
    // the second pass of invalidate_later
    if (invalidate_frame && Device.dwFrame > invalidate_frame + 10)
    {
        invalidate_frame = 0;
        invalidate_all();
    }

    tile* tp = find(id);
    if (!tp)
        return false;
    tile& t = *tp;
    t.last_frame = Device.dwFrame;
    if (!t.valid)
        return false;
    const page& pg = pages[t.page];
    const node& n = pg.nodes[t.node];
    texture = pg.name;
    rect.set(static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.x + t.w), static_cast<float>(n.y + t.h));
    return true;
}

void CUIModels::invalidate_all()
{
    for (tile& t : tiles)
        t.valid = false;
}

void CUIModels::invalidate_later()
{
    invalidate_all();
    invalidate_frame = std::max(Device.dwFrame, 1u);
}

u32 CUIModels::evict_all_cached()
{
    u32 count = 0;
    for (u32 slot = 1; slot <= tiles.size(); ++slot)
    {
        const tile& t = tiles[slot - 1];
        if (t.used && t.key && !t.refs)
        {
            release(slot);
            ++count;
        }
    }
    stat_cache_evicted += count;
    return count;
}

void CUIModels::destroy_scratch()
{
    rt_position.destroy();
    rt_normal.destroy();
    rt_albedo.destroy();
    rt_lit.destroy();
#ifdef UI3D_DX10
    rt_depth.destroy();
#else
    _RELEASE(depth_surface);
#endif
    scratch_w = scratch_h = 0;
}

void CUIModels::destroy()
{
#ifdef UI3D_DX10
    // nothing of the tiles left bound or queued in the driver when the device goes (the renderer DLL is unloaded right after)
    if (!pages.empty() && HW.pContext)
    {
        HW.pContext->ClearState();
        HW.pContext->Flush();
    }
#endif
    s_light.destroy();
    s_resolve.destroy();
    g_quad.destroy();
    if (activate_registered)
    {
        Device.seqAppActivate.Remove(this);
        activate_registered = false;
    }
    invalidate_frame = 0;
    destroy_scratch();
    pages.clear();
    tiles.clear();
    free_tiles.clear();
    keyed.clear();
}

void CUIModels::reset_begin()
{
    // the render targets are recreated by the resource manager; the depth surface made here is not one of them
#ifndef UI3D_DX10
    _RELEASE(depth_surface);
    destroy_scratch();
#endif
    invalidate_later();
}

void CUIModels::ensure_scratch(const u32 w, const u32 h)
{
    if (w <= scratch_w && h <= scratch_h)
        return;

    // grow in steps of 256 px, never shrink
    u32 nw = std::min(max_scratch, std::max(scratch_w, (w + 255u) & ~255u));
    u32 nh = std::min(max_scratch, std::max(scratch_h, (h + 255u) & ~255u));
    // a wide tile then a tall one would grow it to a big square: past 1.5 x the budget, just this tile's size
    if (nw * nh > max_scratch_pixels + max_scratch_pixels / 2)
    {
        nw = std::min(max_scratch, (w + 255u) & ~255u);
        nh = std::min(max_scratch, (h + 255u) & ~255u);
    }

    destroy_scratch();

#if RENDER == R_R1
    // the colour over black and over white
    rt_lit.create("$user$ui3d_lit", nw, nh, D3DFMT_A8R8G8B8);
    rt_albedo.create("$user$ui3d_white", nw, nh, D3DFMT_A8R8G8B8);
#else
    rt_position.create("$user$ui3d_position", nw, nh, D3DFMT_A16B16G16R16F);
#if RENDER == R_R2
    rt_normal.create("$user$ui3d_normal", nw, nh, D3DFMT_A16B16G16R16F);
    // the G-buffer targets with independent bit depths (MRT), else all 64 bit
    rt_albedo.create("$user$ui3d_albedo", nw, nh, RImplementation.o.mrtmixdepth ? D3DFMT_A8R8G8B8 : D3DFMT_A16B16G16R16F);
#else
    rt_albedo.create("$user$ui3d_albedo", nw, nh, D3DFMT_A8R8G8B8);
#endif
    rt_lit.create("$user$ui3d_lit", nw, nh, D3DFMT_A8R8G8B8); // tonemapped sRGB
#endif

#ifdef UI3D_DX10
    rt_depth.create("$user$ui3d_depth", nw, nh, D3DFMT_D24S8);
#else
    CHK_DX(HW.pDevice->CreateDepthStencilSurface(nw, nh, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &depth_surface, nullptr));
#endif

    scratch_w = nw;
    scratch_h = nh;

    Msg("* [ui3d] scratch targets %u x %u", nw, nh);
}

void CUIModels::check_rig()
{
    const rig_state now{ps_r_ui3d_ssaa, ps_r_ui3d_ssaa_small, ps_r_ui3d_key_dir, ps_r_ui3d_key_color, ps_r_ui3d_fill_dir, ps_r_ui3d_fill_color, ps_r_ui3d_ambient, ps_r_ui3d_exposure};
    if (std::memcmp(&now, &rig, sizeof(rig)) != 0)
    {
        rig = now;
        invalidate_all();
    }
}

void CUIModels::draw_quad(u32 x, u32 y, u32 w, u32 h)
{
    u32 offset;
    FVF::TL* pv = static_cast<FVF::TL*>(RCache.Vertex.Lock(4, g_quad->vb_stride, offset));
#ifdef UI3D_DX10
    // clip space over the viewport (ui3d_quad.vs); uv 0..1 (top left 0, 0)
    pv->set(-1.f, -1.f, 0.f, 1.f, 0, 0.f, 1.f);
    pv++;
    pv->set(-1.f, 1.f, 0.f, 1.f, 0, 0.f, 0.f);
    pv++;
    pv->set(1.f, -1.f, 0.f, 1.f, 0, 1.f, 1.f);
    pv++;
    pv->set(1.f, 1.f, 0.f, 1.f, 0, 1.f, 0.f);
    pv++;
#else
    // pre-transformed pixels of the target (the viewport clips), half a pixel back: texel centres
    const float x0 = static_cast<float>(x) - 0.5f, y0 = static_cast<float>(y) - 0.5f;
    const float x1 = x0 + static_cast<float>(w), y1 = y0 + static_cast<float>(h);
    pv->set(x0, y1, 0.f, 1.f, 0, 0.f, 1.f);
    pv++;
    pv->set(x0, y0, 0.f, 1.f, 0, 0.f, 0.f);
    pv++;
    pv->set(x1, y1, 0.f, 1.f, 0, 1.f, 1.f);
    pv++;
    pv->set(x1, y0, 0.f, 1.f, 0, 1.f, 0.f);
    pv++;
#endif
    RCache.Vertex.Unlock(4, g_quad->vb_stride);

    RCache.set_Geometry(g_quad);
    RCache.Render(D3DPT_TRIANGLELIST, offset, 0, 4, 0, 2);
}

//////////////////////////////////////////////////////////////////////////
// render

void CUIModels::render(const u32 id, const ui_model_view& view, IUIModelSource& source)
{
    tile* tp = find(id);
    if (!tp || !supported())
        return;

    check_rig();

    tile& t = *tp;
    t.last_frame = Device.dwFrame;
    ++stat_renders;

    u32 ss = static_cast<u32>(std::clamp(ps_r_ui3d_ssaa, 1, 4));
    if (std::max(t.w, t.h) <= small_tile)
        ss = std::max(ss, static_cast<u32>(std::clamp(ps_r_ui3d_ssaa_small, 1, 4)));
    while (ss > 1 && (t.w * ss > max_scratch || t.h * ss > max_scratch || t.w * ss * t.h * ss > max_scratch_pixels))
        --ss;
    const u32 sw = std::min(t.w * ss, max_scratch);
    const u32 sh = std::min(t.h * ss, max_scratch);

    ensure_scratch(sw, sh);
    if (!s_light)
    {
#if RENDER != R_R1
        s_light.create("ui3d_light");
#endif
        s_resolve.create("ui3d_resolve");
        g_quad.create(FVF::F_TL, RCache.Vertex.Buffer(), RCache.QuadIB);
    }

    CRender& R = RImplementation;

    // UI state, restored at the end
    ID3DRenderTargetView* saved_rt[3]{RCache.get_RT(0), RCache.get_RT(1), RCache.get_RT(2)};
    ID3DDepthStencilView* saved_zb = RCache.get_ZB();
#ifdef UI3D_DX10
    UINT n_vp = 1;
    D3D_VIEWPORT saved_vp{};
    HW.pContext->RSGetViewports(&n_vp, &saved_vp);
    UINT n_sc = 1;
    RECT saved_sc{};
    HW.pContext->RSGetScissorRects(&n_sc, &saved_sc);
    // the UI's scissor (a clipped list) would cut the tile's passes
    const bool sc_override = StateManager.IsScissorOverridden();
    const BOOL sc_value = StateManager.GetScissorOverrideValue();
    StateManager.OverrideScissoring(true, FALSE);
#else
    D3DVIEWPORT9 saved_vp{};
    CHK_DX(HW.pDevice->GetViewport(&saved_vp));
    DWORD sc_enable = FALSE;
    CHK_DX(HW.pDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &sc_enable));
    RECT saved_sc{};
    CHK_DX(HW.pDevice->GetScissorRect(&saved_sc));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE));
#endif

    // the dsgraph state: both priorities in the normal phase, the tile's own lists
    const u32 saved_phase = R.phase;
    const bool saved_pmask[2]{R.pmask[0], R.pmask[1]}, saved_pmask_wmark = R.pmask_wmark;
    IRenderable* saved_object = R.val_pObject;
    Fmatrix* saved_xform = R.val_pTransform;
    const BOOL saved_hud = R.val_bHUD, saved_invisible = R.val_bInvisible;
    R.phase = CRender::PHASE_NORMAL;
    R.r_pmask(true, true);
    R.val_bHUD = FALSE;
    R.val_bInvisible = FALSE;

    // the tile camera (shader constants read Device: eye position, ...)
    const Fmatrix saved_view = Device.mView, saved_project = Device.mProject, saved_full = Device.mFullTransform;
    const Fvector saved_pos = Device.vCameraPosition, saved_dir = Device.vCameraDirection, saved_top = Device.vCameraTop, saved_right = Device.vCameraRight;

    Fmatrix inv_view;
    inv_view.invert(view.view);
    Device.mView = view.view;
    Device.mProject = view.project;
    Device.mFullTransform.mul(view.project, view.view);
    Device.vCameraPosition = inv_view.c;
    Device.vCameraDirection = inv_view.k;
    Device.vCameraTop = inv_view.j;
    Device.vCameraRight = inv_view.i;
    RCache.set_xform_view(Device.mView);
    RCache.set_xform_project(Device.mProject);

    R.val_bUI = TRUE;
    RCache.unbind_ps_textures();

    const page& pg = pages[t.page];
    const node& n = pg.nodes[t.node];

#if RENDER == R_R1
    // R1 (forward): the model's own lit elements, over black and over white
    for (int pass = 0; pass < 2; ++pass)
    {
        set_targets(pass == 0 ? rt_lit->pRT : rt_albedo->pRT, nullptr, nullptr, depth_view());
        set_viewport(0, 0, sw, sh);
        clear_bound(D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, pass == 0 ? 0x00000000 : 0xffffffff);

        R.marker++;
        source.ui_model_add_visuals();
        R.r_dsgraph_render_ui(0);
        R.r_dsgraph_render_ui(1);
        R.r_dsgraph_render_ui_sorted();
    }
#else
    // 1. G-buffer
    {
#ifdef UI3D_DX10
        set_targets(rt_position->pRT, rt_albedo->pRT, nullptr, depth_view());
        set_viewport(0, 0, sw, sh);
        clear_rt(rt_position->pRT, 0.f, 0.f, 0.f, 0.f);
        clear_rt(rt_albedo->pRT, 0.f, 0.f, 0.f, 0.f);
        clear_zb(depth_view());
#else
        set_targets(rt_position->pRT, rt_normal->pRT, rt_albedo->pRT, depth_view());
        set_viewport(0, 0, sw, sh);
        clear_bound(D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0x00000000);
#endif
        RCache.set_Stencil(FALSE);

        R.marker++;
        source.ui_model_add_visuals();
        R.r_dsgraph_render_ui(0);
    }

    // 2. light
    {
        set_targets(rt_lit->pRT, nullptr, nullptr, nullptr);
        set_viewport(0, 0, sw, sh);
        RCache.set_CullMode(CULL_NONE);
        RCache.set_Stencil(FALSE);
        RCache.set_Element(s_light->E[0]);

        const Fvector key = dir_or(ps_r_ui3d_key_dir, Fvector{0.45f, -0.55f, 0.7f});
        const Fvector fill = dir_or(ps_r_ui3d_fill_dir, Fvector{-0.75f, 0.1f, 0.65f});
#ifdef UI3D_DX10
        RCache.set_c("ui3d_tile", static_cast<float>(sw) / scratch_w, static_cast<float>(sh) / scratch_h, 1.f / scratch_w, 1.f / scratch_h);
#else
        // DX9 reads by pixel (VPOS): the tile's size and the texel size of the scratch targets
        RCache.set_c("ui3d_tile", static_cast<float>(sw), static_cast<float>(sh), 1.f / scratch_w, 1.f / scratch_h);
#endif
        RCache.set_c("ui3d_proj", view.ortho ? 1.f : 0.f, view.tan_half_fov_x, view.tan_half_fov_y, 0.f);
        RCache.set_c("ui3d_l0_dir", key.x, key.y, key.z, 0.f);
        RCache.set_c("ui3d_l0_color", ps_r_ui3d_key_color.x, ps_r_ui3d_key_color.y, ps_r_ui3d_key_color.z, 0.f);
        RCache.set_c("ui3d_l1_dir", fill.x, fill.y, fill.z, 0.f);
        RCache.set_c("ui3d_l1_color", ps_r_ui3d_fill_color.x, ps_r_ui3d_fill_color.y, ps_r_ui3d_fill_color.z, 0.f);
        RCache.set_c("ui3d_ambient", ps_r_ui3d_ambient.x, ps_r_ui3d_ambient.y, ps_r_ui3d_ambient.z, ps_r_ui3d_exposure);

        draw_quad(0, 0, sw, sh);
    }

    // 3. forward: priority 1 and strict sorted over the lit colour
    {
        set_targets(rt_lit->pRT, nullptr, nullptr, depth_view());
        set_viewport(0, 0, sw, sh);
        R.r_dsgraph_render_ui(1);
        R.r_dsgraph_render_ui_sorted();
    }
#endif

    // 4. resolve into the atlas page
    {
        RCache.unbind_ps_textures();
        // colour and the outline mask (w: its search radius in tile pixels; 0: not wanted, written as "far")
        set_targets(pg.rt->pRT, pg.mask->pRT, nullptr, nullptr);
        set_viewport(n.x, n.y, t.w, t.h);
        RCache.set_CullMode(CULL_NONE);
        RCache.set_Stencil(FALSE);
        RCache.set_Element(s_resolve->E[0]);
        RCache.set_c("ui3d_resolve", static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(ss), view.outline_mask ? ui_model_outline_max : 0.f);
        // the tile's size in supersampled pixels (the outline search stays inside it: the scratch targets past it keep older tiles'
        // colour) and the texel size of the scratch targets (DX9 reads by uv)
        RCache.set_c("ui3d_resolve_size", static_cast<float>(t.w * ss), static_cast<float>(t.h * ss), 1.f / scratch_w, 1.f / scratch_h);
        draw_quad(n.x, n.y, t.w, t.h);
    }

    R.val_bUI = FALSE;

    R.phase = saved_phase;
    R.r_pmask(saved_pmask[0], saved_pmask[1], saved_pmask_wmark);
    R.val_pObject = saved_object;
    R.val_pTransform = saved_xform;
    R.val_bHUD = saved_hud;
    R.val_bInvisible = saved_invisible;

    // restore
    Device.mView = saved_view;
    Device.mProject = saved_project;
    Device.mFullTransform = saved_full;
    Device.vCameraPosition = saved_pos;
    Device.vCameraDirection = saved_dir;
    Device.vCameraTop = saved_top;
    Device.vCameraRight = saved_right;
    RCache.set_xform_view(Device.mView);
    RCache.set_xform_project(Device.mProject);

    RCache.unbind_ps_textures();
    set_targets(saved_rt[0], saved_rt[1], saved_rt[2], saved_zb);
#ifdef UI3D_DX10
    if (n_vp)
        HW.pContext->RSSetViewports(1, &saved_vp);
    if (sc_override)
    {
        StateManager.OverrideScissoring(true, sc_value);
        if (n_sc)
            HW.pContext->RSSetScissorRects(1, &saved_sc);
    }
    else
        StateManager.OverrideScissoring(false);
#else
    CHK_DX(HW.pDevice->SetViewport(&saved_vp));
    CHK_DX(HW.pDevice->SetScissorRect(&saved_sc));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, sc_enable));
#endif

    t.valid = true;
}
