#pragma once

#include "../../xr_3da/IRenderable.h"
#include "../../xr_3da/bone.h"
#include "../../Include/xrRender/animation_motion.h"

class CUIWindow;
class IKinematics;
class CBlend;

// the root of the visuals a UI model adds to its tile: no lighting environment of the level (no ROS), the renderer gives UI objects fixed
// hemi / sun values while it renders a tile (CRender::apply_object) and the tile is lit by its own rig
class CUIStaticRenderable : public IRenderable
{
public:
    CUIStaticRenderable() { renderable.pROS_Allowed = FALSE; }

    void renderable_Render() override {}
};

// A 3D model shown by a UI window. The renderer draws it offscreen into a tile of an atlas page (lit, supersampled, IUIModelSource) and the
// window draws the tile as its texture (shader hud\ui3d, premultiplied alpha), so order, clipping, fades and offsets are those of any static.
// Static models are rendered once and again only when something changes (dirty); animated ones, particles and a model rotated by the mouse
// are rendered every frame. Any CUIStatic can own one (CUIStatic::SetModelSection, texture name "$ui3d$<section>"); CUI3dStatic adds mouse
// rotation and zoom. Ported from NS_OGSR_Engine (engine_PLAN_ui_3d_models.md, game_PLAN_ui3d_cell_icons.md)
class CUIModel : public IUIModelSource
{
public:
    struct VisualAttachableAddon
    {
        // config
        shared_str m_sAttachVisual;
        shared_str m_sAttachBone;
        Fvector m_vScaleFactor;

        Fmatrix m_offset{};

        // runtime
        IKinematics* m_model{};
        Fmatrix m_visual_trans;

        void CreateAddon();
        void DestroyAddon();

        void UpdateTransform(IKinematics* root, Fmatrix root_trans);
        void Render(IRenderable* root);
    };

    struct WeaponAttachableAddon
    {
        // config
        shared_str m_sWeaponSection;
        shared_str m_sAttachVisual;

        int boneL{BI_NONE}, boneR{BI_NONE}, boneR2{BI_NONE};

        Fmatrix m_offset{};

        // addons on the weapon's world model (as CWeapon::UpdateAddonsVisibility): a scope section of the weapon or empty, silencer, launcher
        shared_str m_scope;
        bool m_silencer{}, m_launcher{};

        // runtime
        IKinematics* m_model{};
        Fmatrix m_visual_trans;
        xr_vector<VisualAttachableAddon> m_children; // a scope that is a separate model (scope_attach)

        void CreateAddon();
        void DestroyAddon();

        void UpdateTransform(IKinematics* root, Fmatrix root_trans);
        void Render(IRenderable* root);
    };

public:
    CUIModel();
    ~CUIModel() override;

    // for the owner's Draw: renders the tile when needed. true: texture / rect (pixels in the page) hold the image to draw
    bool Prepare(CUIWindow& wnd, shared_str& texture, Frect& rect);

    // zoom: the model fills scale x the window (1 = fits)
    void SetScale(float s)
    {
        m_fItemScale = s;
        clamp(m_fItemScale, 0.01f, 200.f);
        m_dirty = true;
    }
    float GetScale() const { return m_fItemScale; }

    // kept for scripts and configs (ui3d_icon_z_offset): unused since the tile camera frames the model, particles included
    void SetZOffset(float z)
    {
        m_fItemZoffset = z;
        clamp(m_fItemZoffset, 0.f, 20.f);
    }
    float GetZOffset() const { return m_fItemZoffset; }

    void SetFromSection(LPCSTR section);
    void SetFromSection(LPCSTR section, LPCSTR section_line);

    void SetVisual(LPCSTR path, bool pm);
    void SetVisual(IRenderVisual* visual);
    bool HasVisual() const { return m_pCurrentVisual != nullptr; }

    // a looped cycle at once, no blending (kept for scripts and the editor); a name other than "idle" makes the model animated
    void SetAnimation(LPCSTR anim);

    // animations of a skinned model: the motion on every partition. blend: seconds of the cross-fade from the current pose (0: switch at
    // once); loop false: plays once, then the idle animation (SetIdleAnimation; else the motion that looped before) blends back in with the same
    // blend time; speed: 1 = the motion's own. The model renders every frame while animated. false: the model has no such motion
    bool PlayAnimation(LPCSTR name, bool loop, float blend, float speed);
    void SetIdleAnimation(LPCSTR name) { m_idle_motion = name; }
    // the frame (camera) follows the animated bounds only for settle_ms after the model is set, then stays: an idle character whose
    // gestures would sway the camera (the actor in the inventory). 0: follows always
    void SetFrameLock(u32 settle_ms) { m_frame_lock_ms = settle_ms; }
    // the frame from this box (model space) instead of the visual's bounds, centred on it: models of different sizes (the actor in different
    // outfits) stand the same in the window; turned around the vertical it stays (the cylinder it sweeps). SetVisual clears it
    void SetFixedFrame(const Fbox& box)
    {
        m_fixed_box = box;
        m_fixed_frame = true;
        m_dirty = true;
    }
    // a looped motion on its own partition only (as CActor plays legs and torso apart: "norm_idle_1" + "norm_torso_0_aim_0"); the model
    // renders every frame. false: the model has no such motion
    bool PlayCycle(LPCSTR name);
    bool HasAnimation(LPCSTR name) const;
    // the model's motion names (cycles), sorted; for a script list of them
    u32 GetAnimationCount() const;
    LPCSTR GetAnimationName(u32 index) const;
    LPCSTR GetCurrentAnimation() const { return m_current_motion.c_str(); }
    // a one-shot (loop false) still plays
    bool IsAnimationPlaying() const { return m_oneshot && !m_oneshot_done; }

    void AddAttach(LPCSTR visual, LPCSTR bone, Fvector pos, Fvector rot, Fvector scale);
    // a weapon in the hands (the hand bones weapon_bone0..2 of the model's section, stalker's by default) or on the strap (strap_bone0 / 1 of
    // the weapon); its world model with addons: scope (a scope section of the weapon, scope_name / scope_names; empty: none), silencer, launcher
    void AddWeaponAttach(LPCSTR section, bool strapped);
    void AddWeaponAttach(LPCSTR section, bool strapped, LPCSTR scope, bool silencer, bool launcher);

    // a weapon section shown by its HUD model (the hud section's item_visual, its idle motion) with addons and the HUD addon params, as the
    // weapon in the actor's hands (CWeapon::UpdateHUDAddonsVisibility, LoadScopeAttachParams): scope (a scope section of the weapon or empty),
    // silencer, launcher. Addons that are permanent or disabled for the weapon keep their status. Sections without a hud: the world model
    void SetWeapon(LPCSTR section, LPCSTR scope, bool silencer, bool launcher);
    // the same with the weapon's world model (its visual, as in the world and the item grid; no motion): an inventory icon. A scope that is a
    // separate model (scope_attach) is attached with its world params
    void SetWeaponItem(LPCSTR section, LPCSTR scope, bool silencer, bool launcher);
    // no model any more (HasVisual false): the config and attachments reset too, as before a SetFromSection
    void ModelClear();
    // the bone state of a weapon object's world visual (the same model: its addons and magazine as they are), the laser ray and the torch cone
    // off as SetWeaponItem has them (long beams would set the frame)
    void CopyWeaponBones(IKinematics* src);
    // a bone of the model is shown (tests: a weapon's magazine, addons); false for a bone it does not have
    bool IsBoneVisible(LPCSTR bone) const;

    // an anomaly of a zone section (configs\zones), its particle effects in the zone's cycle (CCustomZone::OnStateSwitch): idle_particles
    // (idle_seconds, in the game until something enters) -> awake_particles (awaking_time) -> blowout_particles at blowout_particles_time
    // (blowout_time) -> accum_particles (accamulate_time) -> idle again. Effects a zone does not have are skipped
    void SetAnomaly(LPCSTR zone_section, float idle_seconds);
    void SetAnomaly(LPCSTR zone_section) { SetAnomaly(zone_section, 3.f); }

    // a fixed rotation (ui3d_icon_rotation; no OrientToCell after it): the icon turn applies on top of it
    void SetXYZ(const Fvector& _xyz)
    {
        m_icon_base.identity();
        m_icon_base.setXYZ(_xyz);
        m_oriented = false;
        ApplyIconTurn();
    }
    void SetHPB(const Fvector& _hpb)
    {
        m_icon_base.identity();
        m_icon_base.setHPB(_hpb.x, _hpb.y, _hpb.z);
        m_oriented = false;
        ApplyIconTurn();
    }
    Fvector GetXYZ() const;
    Fvector GetHPB() const;

    // an icon's orientation from the model's own box, for a cell of cell_w x cell_h (an inventory cell: inv_grid_width x inv_grid_height):
    // the longest axis along the cell's longer side, the next along the other, the flattest toward the camera (a rifle lies barrel right,
    // a bottle stands in a tall cell, a flat box is seen from above), then turned by tilt_deg (yaw) and 0.75 x tilt_deg (pitch) for depth.
    // Replaces the rotation (SetHPB); the section's ui3d_icon_turn (degrees, HPB) turns the result. Call after the model is set. up_axis 0 / 1 / 2: that model axis (x / y / z, its + side) goes up on the
    // screen whatever its size (an outfit lies folded with its head along z), the flattest of the other two toward the camera; -1: by size
    void OrientToCell(float cell_w, float cell_h, float tilt_deg, int up_axis);
    void OrientToCell(float cell_w, float cell_h, float tilt_deg) { OrientToCell(cell_w, cell_h, tilt_deg, -1); }

    // an inventory icon of the model's section (after SetFromSection) for a cell of cell_w x cell_h: OrientToCell with the type's rules from
    // the section's config, ui3d_icon_cell (the cell shape to orient for: magazines 1, 2) and ui3d_icon_up_axis (outfits 2, addons 1);
    // nothing with ui3d_icon_rotation. vertical: the finished icon turned a quarter (a vertical_placement list, as its 2D icon)
    void OrientIcon(float cell_w, float cell_h, float tilt_deg, bool vertical);
    void OrientIcon(float cell_w, float cell_h, float tilt_deg) { OrientIcon(cell_w, cell_h, tilt_deg, false); }

    // a fix of the icon orientation by hand (the item grid's buttons): turn by degrees around the screen's axis 0 / 1 / 2 (x right, y up, z
    // into the screen), after the automatic orientation and the turns before; oriented again at once. GetIconTurn: the turn so far as
    // ui3d_icon_turn's HPB degrees (for the section's config); ResetIconTurn: none (also not the section's)
    void TurnIcon(int axis, float degrees);
    Fvector GetIconTurn() const;
    void SetIconTurn(Fvector hpb_deg); // as ui3d_icon_turn (a saved hand fix)
    void ResetIconTurn();

    // radians per second around x, y, z (ui3d_icon_rotation_per_frame, degrees per second): the model turns, it is rendered every frame
    void SetRotationPerFrame(Fvector rot) { m_vRotPerFrame = rot; }
    Fvector GetRotationPerFrame() const { return m_vRotPerFrame; }

    // rendered every frame (an animation that should play); otherwise rendered once and on changes
    void SetAnimated(bool animated) { m_animated = animated; }
    bool IsAnimated() const { return m_animated; }

    // orthographic (default) or perspective with a vertical field of view in degrees (ui3d_icon_projection, ui3d_icon_fov)
    void SetProjection(bool ortho, float fov_deg)
    {
        m_ortho = ortho;
        m_fov = deg2rad(std::clamp(fov_deg, 1.f, 120.f));
        m_dirty = true;
    }
    bool IsOrtho() const { return m_ortho; }
    // the projection it renders with: its own, or the one ui3d_projection forces
    bool IsOrthoEffective() const;

    // a looped particle effect is shown this many seconds into its play (default 1): it does not build up on screen first. Before its first draw
    void SetParticleWarmup(float seconds) { particle_warmup_ms = static_cast<u32>(std::clamp(seconds, 0.f, 10.f) * 1000.f); }
    // an effect that has ended (stopped, or no particle alive: one-shot blowouts) plays again half a second later (default on); off: it plays
    // once and the tile stays empty after
    void SetParticleReplay(bool on) { particle_replay = on; }

        // outline of the model's silhouette, drawn behind it in the UI (hover, selection, quest highlight): colour (alpha: strength), width and
    // softness in screen pixels (up to ui_model_outline_max = 8; softness 1: a sharp antialiased line, more: a glow); width 0 hides it.
    // The first call makes the tile keep the silhouette distance mask and an 8 pixel margin (rendered once more); later changes are free
    void SetOutline(u32 color, float width, float softness)
    {
        m_outline_color = color;
        m_outline_width = std::clamp(width, 0.f, ui_model_outline_max);
        m_outline_softness = std::clamp(softness, 0.f, ui_model_outline_max);
        if (!m_outline_mask)
        {
            m_outline_mask = true;
            m_dirty = true;
        }
    }
    void SetOutline(u32 color, float width) { SetOutline(color, width, 1.f); }
    bool HasOutline() const { return m_outline_mask && m_outline_width > 0.f; }
    u32 GetOutlineColor() const { return m_outline_color; }
    float GetOutlineWidth() const { return m_outline_width; }
    float GetOutlineSoftness() const { return m_outline_softness; }

    // the model's look changed outside (bones hidden by a script, ...): render it again
    void Invalidate() { m_dirty = true; }

    void SetBonesVisible(IKinematics* pVisual);

    // user (mouse) rotation and zoom, applied after the configured rotation
    void UserRotate(float dx, float dy, bool free);
    // steps: wheel steps, +1 = 10 % closer
    void UserZoom(float steps);
    // range of the user (mouse) zoom, 1 = fits (default 0.25 .. 4)
    void SetZoomRange(float min_zoom, float max_zoom)
    {
        m_user_zoom_min = std::clamp(min_zoom, 0.01f, 1.f);
        m_user_zoom_max = std::clamp(max_zoom, 1.f, 100.f);
        m_fItemUserScale = std::clamp(m_fItemUserScale, m_user_zoom_min, m_user_zoom_max);
        m_dirty = true;
    }
    // rendered every frame while the user drags it
    void SetUserRotating(bool rotating)
    {
        if (m_user_rotating && !rotating)
            m_dirty = true; // once more into the cached tile
        m_user_rotating = rotating;
    }

    // IUIModelSource
    void ui_model_add_visuals() override;

    u32 tile() const { return m_tile; }

    // for the editor (embedded_editor_ui3d)
    const shared_str& Section() const { return m_section; }
    shared_str VisualName() const;
    bool IsDynamicModel() const { return IsDynamic(); }
    bool IsParticles() const { return particle_mode; }
    bool IsAnomaly() const { return m_anomaly; }
    u32 TileWidth() const { return m_tile_w; }
    u32 TileHeight() const { return m_tile_h; }
    u64 TileImageKey() const { return m_tile_key; }
    // the models alive now (any widget's), in creation order
    static const xr_vector<CUIModel*>& Live();
    // the model of the inventory cell under the cursor (CUICellItem::Update3DIcon), recently: nullptr when none
    static CUIModel* Hovered();
    void MarkHovered();

private:
    IRenderVisual* m_pCurrentVisual{};
    MotionID m_currentMotionID;

    // animations (PlayAnimation)
    shared_str m_section; // the section the model came from (SetFromSection, SetWeapon): weapon_bone0..2 for weapons in the hands
    shared_str m_current_motion, m_idle_motion;
    MotionID m_last_loop_id; // the last looped motion: a one-shot returns to it when no idle animation is set
    shared_str m_last_loop_name;
    float m_oneshot_blend{};
    bool m_oneshot{}, m_oneshot_done{};
    static void OnOneshotEnd(CBlend* blend);
    bool PlayMotion(MotionID motion, bool loop, float blend, float speed);

    // anomaly (SetAnomaly): the stage effects (idle, awake, blowout, accum; m_pCurrentVisual is the first of them, the others are owned here)
    bool m_anomaly{};
    IRenderVisual* m_stage_ps[4]{};
    xr_vector<IRenderVisual*> m_extra_ps;
    u32 m_stage_ms[4]{};
    u32 m_blowout_at_ms{};
    int m_stage{};
    u32 m_stage_t0{};
    bool m_blowout_fired{};
    void AnomalyEnter(int stage);
    void AnomalyUpdate();
    void AnomalyClear();

    // particle start: plays the effect, simulates it unseen for its box (m_particle_scan_box): a looped one its warm up (left playing), a
    // one-shot one through (then from the start)
    void ParticleScan(IRenderVisual* visual);

    void ApplySectionConfig(LPCSTR section);

    xr_vector<VisualAttachableAddon> m_visual_attachments;
    xr_vector<WeaponAttachableAddon> m_weapon_attachments;

    // config settings
    Fmatrix mStaticRotate{};
    float m_fItemScale{1.f};
    float m_fItemZoffset{1.f};
    Fvector m_vRotPerFrame{};
    bool m_animated{};
    bool m_ortho{true};
    float m_fov{deg2rad(25.f)};
    bool m_rendered_ortho{true}; // the projection of the last render: a ui3d_projection change makes the tile render again

    // user settings
    float m_fItemUserScale{1.f};
    float m_user_zoom_min{0.25f}, m_user_zoom_max{4.f};
    Fmatrix mUserRotate{};
    float mCurrentAngleX{}, mCurrentAngleY{};
    bool m_user_rotating{};

    // tile
    u32 m_tile{};
    u32 m_tile_w{}, m_tile_h{};
    u64 m_tile_key{}; // the image key of a shared tile, 0: an own tile
    bool m_dirty{true};
    Fmatrix m_model_xform{}; // model -> tile camera world space, for ui_model_add_visuals

    // box the particle effect has covered since it started (the camera frame grows with it, never shrinks: no jitter)
    Fbox m_particle_box{};
    bool m_particle_box_valid{};

    // particle runtime specific params
    bool particle_mode{};
    u32 particle_last_update{};
    bool particle_started{};
    bool particle_xform{true}; // xform update works faster and better scale
    u32 particle_warmup_ms{1000}; // a looped effect starts this far into its play (SetParticleWarmup)
    bool particle_replay{true}; // an ended effect plays again (SetParticleReplay)
    u32 particle_stop_time{}; // since when the effect has been ended (0: playing)
    bool particle_seen{}; // the effect has had particles since it started playing

    // the camera frame (centre and size of the bounds) of a tile rendered every frame eases to the new bounds instead of jumping: a particle
    // box that grows, an animation that moves the bounds. Static tiles snap (rendered once)
    Fvector m_frame_center{}, m_frame_size{};

    // ui3d_icon_turn of the section as a rotation: OrientToCell turns its result by it (a config fix of one item or a base section; TurnIcon
    // adds to it); the last OrientToCell arguments, to orient again after a turn. Without OrientToCell it turns the fixed rotation m_icon_base
    // (SetHPB / SetXYZ, ui3d_icon_rotation)
    Fmatrix m_icon_turn{Fidentity};
    Fmatrix m_icon_base{Fidentity};
    void ApplyIconTurn();
    float m_orient_w{}, m_orient_h{}, m_orient_tilt{};
    int m_orient_up{-1};
    bool m_orient_vertical{}; // OrientIcon for a vertical list: turned a quarter
    bool m_oriented{};
    bool m_frame_valid{};
    u32 m_frame_lock_ms{}, m_frame_since{}; // SetFrameLock
    Fvector m_center_lock{}; // the model centre while the frame is locked
    bool m_fixed_frame{}; // SetFixedFrame
    Fbox m_fixed_box{};
    bool FrameLocked() const { return m_frame_valid && m_frame_lock_ms && Device.dwTimeContinual - m_frame_since > m_frame_lock_ms; }

    // outline (SetOutline): the tile's mask is kept once asked for
    u32 m_outline_color{0xffffffff};
    float m_outline_width{}, m_outline_softness{1.f};
    bool m_outline_mask{};

    // box of the visible particles seen when the effect started (the warm up of a looped effect, the unseen play through of a one-shot one;
    // effect space): the first frame already frames it (merged into m_particle_box by RenderTile)
    Fbox m_particle_scan_box{};
    bool m_particle_scan_valid{};
    u32 m_frame_time{};

    void ModelSet(LPCSTR visual_name);
    void ModelSet(IRenderVisual* visual);
    void ModelDelete();
    void SetupWeapon(LPCSTR section, LPCSTR scope, bool silencer, bool launcher, bool hud);
    void TileFree();
    void Reinit();
    bool IsDynamic() const;
    bool SpinsAroundVertical() const; // turns by itself (SetRotationPerFrame) around the screen's vertical only
    void RenderTile(u32 tile_w, u32 tile_h);
    // the image key of a still model (IRender_interface::ui_model_tile_shared): everything its image depends on, the tile size included;
    // 0: not shared (moving, animated, particles, an anomaly, attachments, g_3d_static_debug, ui3d_tile_cache off)
    u64 TileKey(u32 tile_w, u32 tile_h) const;
};

// the root of every UI model's visuals (renderable_UI)
CUIStaticRenderable& ui_static_renderable();

extern BOOL m_b3dStaticDebug;

// static models rendered per frame at most (a dialog with many new icons fills them in over a few frames)
extern int g_ui3d_static_per_frame;

// ui3d_tile_cache: still models with the same image share one tile, kept cached after the last one goes (a rebuilt list, a reopened window
// draw it without a render). Off: every model renders its own tile, freed with it
extern BOOL g_ui3d_tile_cache;

// ui3d_projection: the projection of every UI model. 0 model: its own (SetProjection, ui3d_icon_projection), 1 ortho, 2 perspective (with
// the model's field of view, 25 degrees unless set). A change re-renders the cached tiles
extern u32 g_ui3d_projection;

// bumped when the icon settings change (the editor's fixes, the cells' tilt): inventory cells make their models again (CUICellItem)
extern u32 g_ui3d_icons_gen;

// The editor's fixes of a section's icon keys (embedded_editor_ui3d): read instead of the section's own lines while set, for the session;
// saved to $app_data_root$\ui3d_icon_fixes.ltx (and loaded from it on request) to be copied into the item configs
struct ui3d_icon_fix
{
    bool turn_set{};
    Fvector turn{}; // ui3d_icon_turn, degrees HPB
    bool scale_set{};
    float scale{1.f}; // ui3d_icon_scale
    bool cell_scale_set{};
    float cell_scale{1.f}; // ui3d_cell_scale
    bool up_axis_set{};
    int up_axis{-1}; // ui3d_icon_up_axis (-1: by size)

    bool empty() const { return !turn_set && !scale_set && !cell_scale_set && !up_axis_set; }
};

namespace ui3d_icon_fixes
{
xr_map<shared_str, ui3d_icon_fix>& all();
const ui3d_icon_fix* find(const shared_str& section);
// the fix of a section, made when missing; changed() after editing it
ui3d_icon_fix& edit(const shared_str& section);
void remove(const shared_str& section);
void clear();
// the icons use the fixes now (g_ui3d_icons_gen)
void changed();
// the file in app data; false: not written / not found
bool save(string_path& path);
bool load(string_path& path);
// the config lines of a section's fix
xr_string lines(const shared_str& section, const ui3d_icon_fix& fix);

// a section's key with the fix on top of the config
Fvector turn_deg(const shared_str& section);
float scale(const shared_str& section);
float cell_scale(const shared_str& section);
int up_axis(const shared_str& section);
} // namespace ui3d_icon_fixes
