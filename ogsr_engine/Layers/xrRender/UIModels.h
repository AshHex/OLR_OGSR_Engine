#pragma once

// 3D models in UI (IUIModelSource, ported from OGSR_Engine_private): every model is rendered offscreen into its own tile of an atlas page and the
// UI draws the tile as a texture (shader hud\ui3d, premultiplied alpha). Every renderer draws them, each its own way:
//   R3, R4 (DX10 / DX11, the packed G-buffer: normal xy, view z, material) and R2 (DX9, G-buffer of three targets: position, normal, colour):
//     1. G-buffer: the model's visuals with their normal G-buffer shader elements into scratch targets + depth (tile size x SSAA)
//     2. light: one screen quad (ui3d_light): fixed rig of two directional lights + ambient in view space, tonemapped
//     3. forward: priority 1 and strict sorted elements (glass, particles) over the lit colour, depth tested against the G-buffer depth
//     4. resolve: box downsample into the tile of the atlas page (ui3d_resolve), premultiplied alpha, and the outline mask
//   R1 (forward): the model's own lit elements (model_def_lq, the rig through the light constants) drawn twice, over black and over white;
//     the resolve (ui3d_resolve) takes the coverage from the difference of the two (1 - (white - black)), the colour from the black one
// Pages: 2048 x 2048, tiles from a binary 2D buddy allocator (cells split in halves along either axis, merged back on free).
// When all pages are full, tiles not drawn for a few frames (scrolled out, hidden, closed) are evicted; tile ids carry a generation, so an
// owner holding an evicted id sees it gone (tile_alive) and allocates again when it is drawn next.
// Shared tiles (tile_shared, still icons): one tile per image key, reference counted; every model with that key draws the same image. A
// shared tile no model holds any more stays cached with its image (a rebuilt list, a reopened window find it), until room is needed: past
// cache_pages pages, or with all pages full, the least recently drawn of them are evicted first.
class CUIModels : public pureAppActivate
{
public:
    static constexpr u32 page_size = 2048;
    static constexpr u32 max_pages = 8;
    static constexpr u32 min_cell = 16;
    // pages the cache may grow to before cached tiles no model holds give their room to new tiles
    static constexpr u32 cache_pages = 4;

    // the renderer can draw them now (R3 / R4: the packed G-buffer)
    static bool supported();

    u32 tile_alloc(u32 w, u32 h);
    u32 tile_shared(u64 key, u32 w, u32 h);
    void tile_free(u32 id);
    void stats(ui_model_stats& s) const;
    bool tile_alive(u32 id) const { return find(id) != nullptr; }
    bool tile_texture(u32 id, shared_str& texture, Frect& rect);
    void render(u32 id, const ui_model_view& view, IUIModelSource& source);

    // every tile loses its image (device reset: render targets may be recreated)
    void invalidate_all();
    u32 evict_all_cached();
    void destroy();
    // device reset (DX9): the targets this class made itself are released, the images are lost
    void reset_begin();
    // back from alt-tab (the window lost and got the focus, a fullscreen swap chain may have been recreated): the images again, at once
    // and some frames later (tiles rendered in the first frames back could come out black and stay so: a still icon renders once)
    void OnAppActivate() override { invalidate_later(); }
    void invalidate_later();

    // the outline of the tiles drawn now (IRender_interface::ui_model_outline): hud_ui3d's ui3d_outline (premultiplied colour) and
    // ui3d_outline_params (width, softness, max distance of the mask); dxUIRender::SetShader puts them into the shader
    static void set_outline(const Fcolor& color, float width, float softness);

private:
    struct node
    {
        u16 x{}, y{}, w{}, h{};
        s32 parent{-1};
        s32 child{-1}; // first of two consecutive nodes
        enum : u8
        {
            free_leaf,
            used_leaf,
            split,
            released // child pair given back by a merge, waits in free_pairs: not a cell
        } state{free_leaf};
    };

    struct page
    {
        ref_rt rt;
        ref_rt mask; // distance to the model's silhouette / ui_model_outline_max (0 inside) in red, same layout ("<name>_mask")
        shared_str name;
        xr_vector<node> nodes;
        xr_vector<s32> free_pairs; // released child pairs, reused by the next split
        u32 used{};

        s32 alloc(u32 w, u32 h);
        void free(s32 n);
    };

    struct tile
    {
        u32 page{};
        s32 node{-1};
        u32 w{}, h{}; // used size inside the node
        bool used{};
        bool valid{}; // holds an image
        u16 gen{}; // bumped on free / eviction: an old id no longer matches
        u32 last_frame{}; // last frame the tile was drawn or rendered
        u64 key{}; // shared tile: its image key (0: a model's own tile)
        u32 refs{}; // shared tile: models holding it (0: cached)
    };

    // id = generation << 16 | (slot + 1)
    tile* find(u32 id);
    const tile* find(u32 id) const;
    void release(u32 slot);
    u32 evict_stale();
    // evicts cached tiles (shared, no model holds them), least recently drawn first, until a w x h cell is free; false: none made room
    bool evict_cached(u32 w, u32 h, u32& page_id, s32& n);

    xr_vector<page> pages;
    xr_vector<tile> tiles;
    xr_vector<u32> free_tiles;
    xr_unordered_map<u64, u32> keyed; // image key -> slot of its shared tile

    // counters for tests and profiling (ui_model_stats)
    u32 stat_renders{}, stat_shared_hits{}, stat_shared_misses{}, stat_cache_evicted{};

    u32 invalidate_frame{}; // invalidate_later: the frame of the request (0: none)
    bool activate_registered{}; // in Device.seqAppActivate

    // scratch targets, tile size x SSAA, grown on demand. G-buffer renderers: position, (R2: normal), albedo, lit colour; R1: the colour over
    // black (rt_lit) and over white (rt_albedo)
    ref_rt rt_position, rt_normal, rt_albedo, rt_lit;
#if defined(USE_DX10) || defined(USE_DX11)
    ref_rt rt_depth;
    ID3DDepthStencilView* depth_view() const { return rt_depth->pZRT; }
#else
    IDirect3DSurface9* depth_surface{};
    IDirect3DSurface9* depth_view() const { return depth_surface; }
#endif
    u32 scratch_w{}, scratch_h{};

    ref_shader s_light, s_resolve;
    ref_geom g_quad;

    // the light rig and SSAA the tiles were rendered with: a change invalidates them
    struct rig_state
    {
        int ssaa{}, ssaa_small{};
        Fvector key_dir{}, key_color{}, fill_dir{}, fill_color{}, ambient{};
        float exposure{};
    } rig{};

    void create_page();
    void ensure_scratch(u32 w, u32 h);
    void destroy_scratch();
    void check_rig();
    // a quad over the viewport x, y, w, h of a w_rt x h_rt target with the current element (uv 0..1 over the viewport)
    void draw_quad(u32 x, u32 y, u32 w, u32 h);
};
