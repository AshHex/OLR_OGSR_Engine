#include "stdafx.h"
#include "UIModel.h"

#include "UIWindow.h"
#include "../ui_base.h"
#include "../inventory_item.h"

#include "../../Include/xrRender/ParticleCustom.h"
#include "../../Include/xrRender/Kinematics.h"
#include "../../Include/xrRender/KinematicsAnimated.h"
#include "../../xr_3da/vismask.h"
#include "../../xr_3da/SkeletonMotions.h"

BOOL m_b3dStaticDebug = FALSE; // g_3d_static_debug: free mouse rotation without limits, rotation logged, shift + wheel scale (R6)

int g_ui3d_static_per_frame = 8;

BOOL g_ui3d_tile_cache = TRUE;

u32 g_ui3d_projection = 0;

bool CUIModel::IsOrthoEffective() const { return g_ui3d_projection == 1 || (g_ui3d_projection != 2 && m_ortho); }

CUIStaticRenderable& ui_static_renderable()
{
    // never destroyed: ~IRenderable calls into the renderer, which may be gone at the end of the static destruction
    static CUIStaticRenderable* r = new CUIStaticRenderable();
    return *r;
}

u32 g_ui3d_icons_gen = 0;

//////////////////////////////////////////////////////////////////////
// the editor's fixes of the icon keys (embedded_editor_ui3d)
//////////////////////////////////////////////////////////////////////

namespace ui3d_icon_fixes
{
namespace
{
constexpr LPCSTR fixes_file = "ui3d_icon_fixes.ltx";

xr_map<shared_str, ui3d_icon_fix>& fixes()
{
    static xr_map<shared_str, ui3d_icon_fix> m;
    return m;
}
} // namespace

xr_map<shared_str, ui3d_icon_fix>& all() { return fixes(); }

const ui3d_icon_fix* find(const shared_str& section)
{
    if (fixes().empty() || !section.size())
        return nullptr;
    const auto it = fixes().find(section);
    return it != fixes().end() ? &it->second : nullptr;
}

ui3d_icon_fix& edit(const shared_str& section) { return fixes()[section]; }

void remove(const shared_str& section)
{
    fixes().erase(section);
    changed();
}

void clear()
{
    fixes().clear();
    changed();
}

void changed() { ++g_ui3d_icons_gen; }

xr_string lines(const shared_str& section, const ui3d_icon_fix& fix)
{
    xr_string s;
    string256 line;
    xr_sprintf(line, "[%s]\n", section.c_str());
    s += line;
    if (fix.turn_set)
    {
        xr_sprintf(line, "ui3d_icon_turn = %g, %g, %g\n", fix.turn.x, fix.turn.y, fix.turn.z);
        s += line;
    }
    if (fix.scale_set)
    {
        xr_sprintf(line, "ui3d_icon_scale = %g\n", fix.scale);
        s += line;
    }
    if (fix.cell_scale_set)
    {
        xr_sprintf(line, "ui3d_cell_scale = %g\n", fix.cell_scale);
        s += line;
    }
    if (fix.up_axis_set)
    {
        xr_sprintf(line, "ui3d_icon_up_axis = %d\n", fix.up_axis);
        s += line;
    }
    return s;
}

bool save(string_path& path)
{
    FS.update_path(path, "$app_data_root$", fixes_file);
    IWriter* w = FS.w_open(path);
    if (!w)
        return false;
    w->w_string("; 3D icon fixes made in the editor (ImGui \"3D Icons\"): copy the lines into the item sections of the configs");
    for (const auto& [section, fix] : fixes())
    {
        if (fix.empty())
            continue;
        // a line at a time: w_string ends each with \r\n, as the rest of the file
        const xr_string s = lines(section, fix);
        for (size_t b = 0, e; b < s.size(); b = e + 1)
        {
            e = s.find('\n', b);
            if (e == xr_string::npos)
                e = s.size();
            w->w_string(s.substr(b, e - b).c_str());
        }
    }
    FS.w_close(w);
    return true;
}

bool load(string_path& path)
{
    FS.update_path(path, "$app_data_root$", fixes_file);
    if (!FS.exist(path))
        return false;
    CInifile ini(path, TRUE, TRUE, FALSE);
    for (const auto& [name, sect] : ini.sections())
    {
        ui3d_icon_fix& f = edit(name);
        if (ini.line_exist(name, "ui3d_icon_turn"))
        {
            f.turn_set = true;
            f.turn = ini.r_fvector3(name, "ui3d_icon_turn");
        }
        if (ini.line_exist(name, "ui3d_icon_scale"))
        {
            f.scale_set = true;
            f.scale = ini.r_float(name, "ui3d_icon_scale");
        }
        if (ini.line_exist(name, "ui3d_cell_scale"))
        {
            f.cell_scale_set = true;
            f.cell_scale = ini.r_float(name, "ui3d_cell_scale");
        }
        if (ini.line_exist(name, "ui3d_icon_up_axis"))
        {
            f.up_axis_set = true;
            f.up_axis = ini.r_s32(name, "ui3d_icon_up_axis");
        }
    }
    changed();
    return true;
}

Fvector turn_deg(const shared_str& section)
{
    if (const ui3d_icon_fix* f = find(section); f && f->turn_set)
        return f->turn;
    if (!section.size())
        return Fvector{};
    if (pSettings->line_exist(section, "ui3d_icon_turn"))
        return pSettings->r_fvector3(section, "ui3d_icon_turn");
    // the table of turns of the item icons (external.ltx: [ui3d_icon_turns] <item section> = h, p, b), in one place for the items whose
    // automatic orientation is off (a label upside down, a device seen from above instead of its front)
    constexpr LPCSTR table = "ui3d_icon_turns";
    if (pSettings->section_exist(table) && pSettings->line_exist(table, section))
        return pSettings->r_fvector3(table, section.c_str());
    return Fvector{};
}

float scale(const shared_str& section)
{
    if (const ui3d_icon_fix* f = find(section); f && f->scale_set)
        return f->scale;
    return section.size() ? READ_IF_EXISTS(pSettings, r_float, section, "ui3d_icon_scale", 1.f) : 1.f;
}

float cell_scale(const shared_str& section)
{
    if (const ui3d_icon_fix* f = find(section); f && f->cell_scale_set)
        return f->cell_scale;
    return section.size() ? READ_IF_EXISTS(pSettings, r_float, section, "ui3d_cell_scale", 1.f) : 1.f;
}

int up_axis(const shared_str& section)
{
    if (const ui3d_icon_fix* f = find(section); f && f->up_axis_set)
        return f->up_axis;
    return section.size() ? READ_IF_EXISTS(pSettings, r_s32, section, "ui3d_icon_up_axis", -1) : -1;
}
} // namespace ui3d_icon_fixes

//////////////////////////////////////////////////////////////////////
// the models alive (for the editor)
//////////////////////////////////////////////////////////////////////

namespace
{
xr_vector<CUIModel*>& live_models()
{
    static xr_vector<CUIModel*> v;
    return v;
}

CUIModel* hovered_model{};
u32 hovered_frame{};
} // namespace

const xr_vector<CUIModel*>& CUIModel::Live() { return live_models(); }

CUIModel* CUIModel::Hovered()
{
    // marked this frame or the one before (the cells update before the editor builds its frame, or after it)
    if (!hovered_model || Device.dwFrame - hovered_frame > 1)
        return nullptr;
    const auto& v = live_models();
    return std::find(v.begin(), v.end(), hovered_model) != v.end() ? hovered_model : nullptr;
}

void CUIModel::MarkHovered()
{
    hovered_model = this;
    hovered_frame = Device.dwFrame;
}

shared_str CUIModel::VisualName() const { return m_pCurrentVisual ? m_pCurrentVisual->getDebugName() : shared_str{}; }

namespace
{
const Fvector zero_vel{0.f, 0.f, 0.f}; // a particle effect's parent velocity (UpdateParent)

// the renderer can still take calls: a model destroyed at the exit after the device (a window kept by a static) would call into the unloaded
// renderer DLL; its visuals and tile are gone with the device anyway
bool ui_render_alive() { return ::Render && Device.b_is_Ready; }

// a visual into the tile being rendered (inside IUIModelSource::ui_model_add_visuals): the renderer copies the transform when it adds it
void ui_add_visual(IRenderable* root, IRenderVisual* visual, const Fmatrix& xform)
{
    if (!visual)
        return;
    Fmatrix m = xform;
    ::Render->set_Object(root);
    ::Render->set_Transform(&m);
    ::Render->add_Visual(visual);
}

// 64-bit hash of bytes (the tile cache's image keys, CUIModel::TileKey)
u64 hash_bytes(const void* data, const size_t size) { return std::hash<std::string_view>{}(std::string_view{static_cast<const char*>(data), size}); }

// a character (stalker, monster) skeleton: its front is +z, the tile camera looks along +z, so by default it would show its back
bool is_character(IRenderVisual* v)
{
    IKinematics* k = v ? v->dcast_PKinematics() : nullptr;
    return k && v->dcast_PKinematicsAnimated() && (k->LL_BoneID("bip01_spine") != BI_NONE || k->LL_BoneID("bip01_head") != BI_NONE);
}

// the UI models' clock: it runs while the game is paused (the main menu over a level), unlike dwTimeGlobal / fTimeDelta, which stand still
// then and would freeze animations and particles (a particle effect started paused never emits)
u32 ui_time_ms() { return Device.dwTimeContinual; }
float ui_time_delta()
{
    if (!Device.Paused())
        return Device.fTimeDelta;
    // paused: the frame time by the continual clock (once per frame), at most 0.1 s
    static u32 frame = 0, prev = 0;
    static float dt = 0.f;
    if (frame != Device.dwFrame)
    {
        frame = Device.dwFrame;
        dt = prev ? std::min(static_cast<float>(Device.dwTimeContinual - prev) * 0.001f, 0.1f) : 0.f;
        prev = Device.dwTimeContinual;
    }
    return dt;
}

// the model file of a visual name exists (as CModelPool::Instance_Load looks for it: the game's meshes, then the level; a missing one is
// FATAL there): a config error (an item's visual that is not in the game) logs and leaves the model empty instead
bool model_exists(LPCSTR name)
{
    if (!name || !name[0])
        return false;
    string_path n;
    if (!strext(name))
        xr_strconcat(n, name, ".ogf");
    else
        xr_strcpy(n, name);
    if (FS.exist("$game_meshes$", n) || FS.exist("$level$", n))
        return true;
    Msg("! [ui3d] no model file [%s]", n);
    return false;
}

// a comma separated list of a line; false when the line is missing or empty
bool ini_list(const shared_str& section, LPCSTR line, xr_vector<shared_str>& out)
{
    out.clear();
    if (!section.size() || !pSettings->line_exist(section, line))
        return false;
    LPCSTR S = pSettings->r_string(section, line);
    if (!S || !S[0])
        return false;
    string128 item{};
    const int count = _GetItemCount(S);
    for (int i = 0; i < count; ++i)
        out.emplace_back(_GetItem(S, i, item));
    return true;
}

void set_bones_visible(IKinematics* k, const xr_vector<shared_str>& bones, bool visible)
{
    for (const auto& name : bones)
    {
        const u16 id = k->LL_BoneID(name);
        if (id != BI_NONE && !!k->LL_GetBoneVisible(id) != visible)
            k->LL_SetBoneVisible(id, visible, true);
    }
}

// a scope that is a separate model on the weapon (CWeapon::LoadScopeAttachParams): visual, bone, offset, scale
struct weapon_scope_model
{
    shared_str visual, bone;
    Fvector pos{}, rot{};
    float scale{};
};

// a weapon section's addons on its HUD (hud) or world model, as CWeapon::UpdateHUDAddonsVisibility / UpdateAddonsVisibility with the loaded
// state (the first of mag_bones shown) and LoadScopeAttachParams. scope: a scope section of the weapon (scope_name / scope_names) or empty.
// Returns the scope model to attach (visual empty: none)
weapon_scope_model apply_weapon_addons(IKinematics* k, const shared_str& section, bool hud, LPCSTR scope, bool silencer, bool launcher)
{
    enum
    {
        addon_disabled = 0,
        addon_permanent = 1,
        addon_attachable = 2
    };
    const shared_str hud_sect = hud ? READ_IF_EXISTS(pSettings, r_string, section, "hud", "") : "";
    const int scope_status = READ_IF_EXISTS(pSettings, r_s32, section, "scope_status", addon_disabled);
    const int silencer_status = READ_IF_EXISTS(pSettings, r_s32, section, "silencer_status", addon_disabled);
    const int launcher_status = READ_IF_EXISTS(pSettings, r_s32, section, "grenade_launcher_status", addon_disabled);

    // a list of the hud section, else of the weapon section, else the default
    xr_vector<shared_str> list;
    const auto bones_of = [&](LPCSTR line, LPCSTR def) {
        if (!ini_list(hud_sect, line, list) && !ini_list(section, line, list) && def)
            list.assign(1, shared_str{def});
        return list;
    };

    // the scope: one of the weapon's scopes (attachable), or its own (permanent)
    bool scope_on = false;
    // scopes_sect (CWeapon::m_scopes): the entry whose scope_name is this scope item carries its model and mount
    shared_str scope_entry;
    if (scope_status == addon_attachable && scope && scope[0])
    {
        xr_vector<shared_str> entries;
        ini_list(section, "scopes_sect", entries);
        for (const shared_str& e : entries)
            if (pSettings->section_exist(e) && !xr_strcmp(READ_IF_EXISTS(pSettings, r_string, e, "scope_name", ""), scope))
                scope_entry = e;
        // the entry itself given (CWeapon::m_scopes[m_cur_scope], as an older saved main menu state has it): its scope_name is the scope
        if (!scope_entry.size())
            for (const shared_str& e : entries)
                if (!xr_strcmp(e, scope) && pSettings->section_exist(e) && pSettings->line_exist(e, "scope_name"))
                {
                    scope_entry = e;
                    scope = pSettings->r_string(e, "scope_name");
                    break;
                }
        // one scope (scope_name in the weapon section): the weapon section is its entry (CWeapon::m_scopes), with a model
        if (entries.empty() && !xr_strcmp(READ_IF_EXISTS(pSettings, r_string, section, "scope_name", ""), scope) &&
            (pSettings->line_exist(section, "attach_hud_visual") || pSettings->line_exist(section, "scope_hud_visual")))
            scope_entry = section;
    }
    if (scope_entry.size())
        scope_on = true;
    else if (scope_status == addon_attachable && scope && scope[0])
    {
        xr_vector<shared_str> names;
        ini_list(section, "scope_names", names);
        if (pSettings->line_exist(section, "scope_name"))
            names.emplace_back(pSettings->r_string(section, "scope_name"));
        scope_on = std::find(names.begin(), names.end(), shared_str{scope}) != names.end();
        if (!scope_on)
            Msg("! [apply_weapon_addons] [%s] is not a scope of [%s]", scope, section.c_str());
    }

    // scope params section (CWeapon::GetScopeParamSection): scope_params_<scope>, or scope_params, or the weapon section
    xr_string key = "scope_params";
    if (scope_on)
        key.append("_").append(scope);
    const shared_str params = pSettings->line_exist(section, key.c_str()) ? shared_str{pSettings->r_string(section, key.c_str())} : section;

    // the scope as a separate model (scope_attach = true and the scope section has scope_hud_visual)
    weapon_scope_model model{};
    const auto load_scope_model = [&](const shared_str& main, const shared_str& scope_sect) {
        if (!READ_IF_EXISTS(pSettings, r_bool, main, "scope_attach", false) || !pSettings->line_exist(scope_sect, "scope_hud_visual"))
            return;
        const shared_str hud_visual = pSettings->r_string(scope_sect, "scope_hud_visual");
        const shared_str hud_bone = READ_IF_EXISTS(pSettings, r_string, main, "scope_hud_attach_bone", "wpn_scope");
        const Fvector hud_pos = READ_IF_EXISTS(pSettings, r_fvector3, main, "scope_hud_attach_pos", Fvector{});
        const Fvector hud_rot = READ_IF_EXISTS(pSettings, r_fvector3, main, "scope_hud_attach_rot", Fvector{});
        const float hud_scale = READ_IF_EXISTS(pSettings, r_float, main, "scope_hud_attach_scale", 0.6666f);
        if (hud)
            model = {hud_visual, hud_bone, hud_pos, hud_rot, hud_scale};
        else
            model = {READ_IF_EXISTS(pSettings, r_string, scope_sect, "scope_visual", hud_visual.c_str()), READ_IF_EXISTS(pSettings, r_string, main, "scope_attach_bone", hud_bone.c_str()),
                     READ_IF_EXISTS(pSettings, r_fvector3, main, "scope_attach_pos", hud_pos), READ_IF_EXISTS(pSettings, r_fvector3, main, "scope_attach_rot", hud_rot),
                     READ_IF_EXISTS(pSettings, r_float, main, "scope_attach_scale", hud_scale)};
    };
    // a scopes_sect entry (as CWeapon::LoadScopeAttachParams): attach_hud_visual / attach_bone_hud / hud_attach_offset ... or the
    // scope_hud_* keys, each from the entry, else from the weapon section
    const auto load_entry_model = [&](const shared_str& entry) {
        if (!READ_IF_EXISTS(pSettings, r_bool, section, "scope_attach", false))
            return;
        const auto find = [&](LPCSTR key, LPCSTR alt) -> std::pair<LPCSTR, LPCSTR> {
            for (LPCSTR sect : {entry.c_str(), section.c_str()})
            {
                if (pSettings->line_exist(sect, key))
                    return {sect, key};
                if (pSettings->line_exist(sect, alt))
                    return {sect, alt};
            }
            return {nullptr, nullptr};
        };
        const auto str = [&](LPCSTR key, LPCSTR alt, LPCSTR def) { const auto [s, kk] = find(key, alt); return s ? pSettings->r_string(s, kk) : def; };
        const auto vec = [&](LPCSTR key, LPCSTR alt, const Fvector& def) { const auto [s, kk] = find(key, alt); return s ? pSettings->r_fvector3(s, kk) : def; };
        const auto flt = [&](LPCSTR key, LPCSTR alt, float def) { const auto [s, kk] = find(key, alt); return s ? pSettings->r_float(s, kk) : def; };

        LPCSTR hud_visual = nullptr;
        if (pSettings->line_exist(entry, "attach_hud_visual"))
            hud_visual = pSettings->r_string(entry, "attach_hud_visual");
        else if (pSettings->line_exist(entry, "scope_hud_visual"))
            hud_visual = pSettings->r_string(entry, "scope_hud_visual");
        if (!hud_visual)
            return;

        const LPCSTR hud_bone = str("attach_bone_hud", "scope_hud_attach_bone", "wpn_scope");
        const Fvector hud_pos = vec("hud_attach_offset", "scope_hud_attach_pos", Fvector{});
        const Fvector hud_rot = vec("hud_attach_rotation", "scope_hud_attach_rot", Fvector{});
        const float hud_scale = flt("hud_attach_scale", "scope_hud_attach_scale", 1.f);
        if (hud)
            model = {hud_visual, hud_bone, hud_pos, hud_rot, hud_scale};
        else
            model = {str("attach_world_visual", "scope_visual", hud_visual), str("attach_bone_world", "scope_attach_bone", hud_bone), vec("world_attach_offset", "scope_attach_pos", hud_pos),
                     vec("world_attach_rotation", "scope_attach_rot", hud_rot), flt("world_attach_scale", "scope_attach_scale", hud_scale)};
    };

    if (scope_status == addon_permanent)
        load_scope_model(section, section);
    else if (scope_entry.size())
        load_entry_model(scope_entry);
    else if (scope_on)
        load_scope_model(params, READ_IF_EXISTS(pSettings, r_string, params, "scope_name", scope));

    VisMask all;
    all.set(u64(-1), u64(-1));
    k->LL_SetBonesVisible(all);

    // scope, silencer, launcher bones by status; a separate scope model hides the scope bones
    const auto addon = [&](const xr_vector<shared_str>& bones, int status, bool on) {
        set_bones_visible(k, bones, status == addon_attachable ? on : status == addon_permanent);
    };
    addon(bones_of("scope_bone", "wpn_scope"), !model.visual.size() ? scope_status : addon_disabled, scope_on);
    addon(bones_of("silencer_bone", "wpn_silencer"), silencer_status, silencer);
    addon(bones_of("launcher_bone", "wpn_launcher"), launcher_status, launcher);

    // magazine: loaded, the first of mag_bones
    bones_of("mag_bones", nullptr);
    if (!list.empty())
    {
        const xr_vector<shared_str> rest(list.begin() + 1, list.end());
        set_bones_visible(k, rest, false);
    }

    set_bones_visible(k, bones_of("hidden_bones", nullptr), false);
    set_bones_visible(k, bones_of("shown_bones", nullptr), true);

    // the laser ray and the tactical torch's cone off (CWeapon with the laser / torch off): long beams, they would make the box and the frame
    set_bones_visible(k, bones_of("laser_ray_bones", nullptr), false);
    set_bones_visible(k, bones_of("torch_cone_bones", nullptr), false);

    // a scopes_sect entry: its mount bones (CWeapon: m_cur_scope_hide_bones / m_cur_scope_show_bones)
    if (scope_entry.size())
    {
        ini_list(scope_entry, "hide_bones", list);
        set_bones_visible(k, list, false);
        ini_list(scope_entry, "show_bones", list);
        set_bones_visible(k, list, true);
    }
    // the scope's own hidden / shown bones: its params section, for the HUD its aim params section (the params' hud line) first
    else if (scope_on)
    {
        const shared_str aim = hud ? READ_IF_EXISTS(pSettings, r_string, params, "hud", "") : "";
        if (!ini_list(aim, "hidden_bones", list))
            ini_list(params, "hidden_bones", list);
        set_bones_visible(k, list, false);
        if (!ini_list(aim, "shown_bones", list))
            ini_list(params, "shown_bones", list);
        set_bones_visible(k, list, true);
    }

    k->CalculateBones_Invalidate();
    k->CalculateBones(true);
    return model;
}

// a skinned model's box is the union of its bones' boxes (CKinematics): a bone parked far away (the vepr HUD model's joint9, 10 m in front of
// it, to hide a part in the game) makes the box, and the camera frame, 30 times the model. Bones farther than max(1 m, 3 x the median
// distance) from the bones' median position are left out of the box; nothing changes when there are none
// bones parked far from the rest (the vepr HUD model's joint9, 10 m in front of it, hides a part in the game): farther than max(1 m, 3 x
// the median distance) from the bones' median position. Empty when there are none
xr_vector<bool> parked_bones(IKinematics* k)
{
    const u16 count = k->LL_BoneCount();
    if (count < 3)
        return {};

    xr_vector<float> xs, ys, zs;
    for (u16 i = 0; i < count; ++i)
    {
        const Fvector& p = k->LL_GetTransform(i).c;
        xs.push_back(p.x);
        ys.push_back(p.y);
        zs.push_back(p.z);
    }
    const auto median = [](xr_vector<float>& v) {
        std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
        return v[v.size() / 2];
    };
    const Fvector center{median(xs), median(ys), median(zs)};

    xr_vector<float> dist;
    for (u16 i = 0; i < count; ++i)
        dist.push_back(k->LL_GetTransform(i).c.distance_to(center));
    xr_vector<float> sorted = dist;
    const float limit = std::max(1.f, 3.f * median(sorted));

    xr_vector<bool> parked(count, false);
    bool any = false;
    for (u16 i = 0; i < count; ++i)
        any |= parked[i] = dist[i] > limit;
    return any ? parked : xr_vector<bool>{};
}

// the box of a bone's obb now (bone space by the bone transform) into box
void add_bone_box(IKinematics* k, u16 i, Fbox& box)
{
    const Fobb& obb = k->LL_GetBox(i);
    if (fis_zero(obb.m_halfsize.square_magnitude()))
        return;
    const Fmatrix& m = k->LL_GetTransform(i);
    for (int c = 0; c < 8; ++c)
    {
        Fvector q = obb.m_translate;
        q.mad(obb.m_rotate.i, (c & 1 ? 1.f : -1.f) * obb.m_halfsize.x);
        q.mad(obb.m_rotate.j, (c & 2 ? 1.f : -1.f) * obb.m_halfsize.y);
        q.mad(obb.m_rotate.k, (c & 4 ? 1.f : -1.f) * obb.m_halfsize.z);
        Fvector w;
        m.transform_tiny(w, q);
        box.modify(w);
    }
}

// a skinned model's box is the union of its bones' boxes (CKinematics): parked bones make it, and the camera frame, many times the model.
// Without them: the other visible bones' boxes; nothing changes when there are none
void skeleton_box_without_outliers(IKinematics* k, Fbox& box)
{
    const xr_vector<bool> parked = parked_bones(k);
    if (parked.empty())
        return;
    Fbox tight;
    tight.invalidate();
    for (u16 i = 0; i < k->LL_BoneCount(); ++i)
        if (!parked[i] && k->LL_GetBoneVisible(i))
            add_bone_box(k, i, tight);
    if (tight.is_valid())
        box = tight;
}

// the box of a still skinned model (an item, a weapon): the visible bones' boxes (a hidden bone's geometry, the torch's light cone, does not
// count; parked bones left out), and the meshes' own boxes when no bone is hidden (the bones' boxes do not always cover the mesh: the 12
// gauge magazine, one bone whose box is half the model; the mesh boxes include hidden parts, so not then)
void still_model_box(IKinematics* k, Fbox& box)
{
    const xr_vector<bool> parked = parked_bones(k);

    // the meshes in the pose drawn (vertices skinned with the bones now; hidden and parked bones left out): exact, also when an idle motion
    // moves a part off the bind pose (the bobbing artefacts af_kol_*, af_fireball, af_pudd_*: framed a fifth of the cell up or down)
    Fbox posed;
    if (k->GetPosedMeshBox(posed, parked.empty() ? nullptr : &parked))
    {
        box = posed;
        return;
    }

    // no vertex copy (not expected for skinned models): the bones' and the meshes' boxes
    skeleton_box_without_outliers(k, box);
    bool hidden = false;
    Fbox bones;
    bones.invalidate();
    for (u16 i = 0; i < k->LL_BoneCount(); ++i)
    {
        if (!k->LL_GetBoneVisible(i))
            hidden = true;
        else if (parked.empty() || !parked[i])
            add_bone_box(k, i, bones);
    }
    if (hidden && bones.is_valid())
    {
        box = bones;
        return;
    }

    // no hidden bones: the meshes' box (their geometry, in the bind pose) moved by the root bone from its bind pose to its pose now (an
    // item's idle motion can carry it away: the glowsticks, 0.6 m), instead of the bones' boxes (a helmet's one bone box is off its mesh)
    Fbox mesh;
    if (k->GetMeshBox(mesh))
    {
        xr_vector<Fmatrix> bind;
        k->LL_GetBindTransform(bind);
        if (!bind.empty())
        {
            Fmatrix inv, move;
            inv.invert(bind[0]);
            move.mul(k->LL_GetTransform(0), inv);
            mesh.xform(move);
        }
        box = mesh;
    }
}

void init_bones_from_config(IKinematics* model, const shared_str& section)
{
    if (pSettings->line_exist(section, "hidden_bones"))
    {
        LPCSTR S = pSettings->r_string(section, "hidden_bones");
        if (S && strlen(S))
        {
            const int count = _GetItemCount(S);
            string128 bone_name{};
            for (int it = 0; it < count; ++it)
            {
                _GetItem(S, it, bone_name);

                u16 bone_id = model->LL_BoneID(bone_name);
                if (bone_id != BI_NONE && model->LL_GetBoneVisible(bone_id))
                    model->LL_SetBoneVisible(bone_id, false, true);
            }
        }
    }

    if (pSettings->line_exist(section, "mag_bones"))
    {
        LPCSTR S = pSettings->r_string(section, "mag_bones");
        if (S && strlen(S))
        {
            const int count = _GetItemCount(S);
            string128 bone_name{};
            for (int it = 1; it < count; ++it) // hide all expect first
            {
                _GetItem(S, it, bone_name);

                u16 bone_id = model->LL_BoneID(bone_name);
                if (bone_id != BI_NONE && model->LL_GetBoneVisible(bone_id))
                    model->LL_SetBoneVisible(bone_id, false, true);
            }
        }
    }
}
} // namespace

// a weapon's world model with its addons for a model outside the UI (the main menu actor, MenuScene.cpp); the scope as a separate model:
// its visual (empty: none), the weapon's bone and the offset from that bone (scale included)
void weapon_world_addons(IKinematics* k, const shared_str& section, LPCSTR scope, bool silencer, bool launcher, shared_str& scope_visual, shared_str& scope_bone, Fmatrix& scope_offset)
{
    const weapon_scope_model m = apply_weapon_addons(k, section, false, scope, silencer, launcher);
    scope_visual = model_exists(m.visual.c_str()) ? m.visual : shared_str{};
    scope_bone = m.bone;

    Fvector ypr = m.rot;
    ypr.mul(PI / 180.f);
    Fmatrix offset, scale;
    offset.setHPB(ypr.x, ypr.y, ypr.z);
    offset.translate_over(m.pos);
    scale.scale(m.scale, m.scale, m.scale);
    scope_offset.mul_43(offset, scale);
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void CUIModel::VisualAttachableAddon::CreateAddon()
{
    if (m_sAttachVisual.size() && model_exists(m_sAttachVisual.c_str()))
    {
        m_model = smart_cast<IKinematics*>(::Render->model_Create(m_sAttachVisual.c_str()));
    }
}

void CUIModel::VisualAttachableAddon::DestroyAddon()
{
    if (m_model)
    {
        IRenderVisual* v = m_model->dcast_RenderVisual();
        if (ui_render_alive())
            ::Render->model_Delete(v);
        m_model = nullptr;

        // Msg("Destroyed addon with visual = %s", m_sAttachVisual.c_str());
    }
}

void CUIModel::VisualAttachableAddon::UpdateTransform(IKinematics* root, Fmatrix root_trans)
{
    if (!root)
        return;

    if (!m_model)
    {
        CreateAddon();
    }

    // a bone the model doesn't have: attached to the model root (BI_NONE went to LL_GetTransform, an out of range read; R6)
    const u16 bone = root->LL_BoneID(m_sAttachBone);
    Fmatrix temp;
    if (bone != BI_NONE)
        temp.mul(root_trans, root->LL_GetTransform(bone));
    else
        temp = root_trans;
    m_visual_trans.mul(temp, m_offset);

    //-- Scaling model
    Fmatrix scale, t;
    t = m_visual_trans;
    scale.scale(m_vScaleFactor);
    m_visual_trans.mul(t, scale);

    // Msg("--Addon no bone position = [x = %f,y = %f,z = %f]", m_visual_trans.c.x, m_visual_trans.c.y, m_visual_trans.c.z);
}

void CUIModel::VisualAttachableAddon::Render(IRenderable* root)
{
    if (m_model)
    {
        ui_add_visual(root, m_model->dcast_RenderVisual(), m_visual_trans);
    }
}

void CUIModel::WeaponAttachableAddon::CreateAddon()
{
    if (m_sAttachVisual.size() && model_exists(m_sAttachVisual.c_str()))
    {
        m_model = smart_cast<IKinematics*>(::Render->model_Create(m_sAttachVisual.c_str()));
        if (!m_model)
            return;

        // the world model with its addons; a scope that is a separate model hangs on it
        const weapon_scope_model scope = apply_weapon_addons(m_model, m_sWeaponSection, false, m_scope.c_str(), m_silencer, m_launcher);
        if (scope.visual.size())
        {
            auto& child = m_children.emplace_back();
            child.m_sAttachVisual = scope.visual;
            child.m_sAttachBone = scope.bone;
            child.m_vScaleFactor.set(scope.scale, scope.scale, scope.scale);
            Fvector ypr = scope.rot;
            ypr.mul(PI / 180.f);
            child.m_offset.setHPB(ypr.x, ypr.y, ypr.z);
            child.m_offset.translate_over(scope.pos);
        }
    }
}

void CUIModel::WeaponAttachableAddon::DestroyAddon()
{
    if (m_model)
    {
        IRenderVisual* v = m_model->dcast_RenderVisual();
        if (ui_render_alive())
            ::Render->model_Delete(v);
        m_model = nullptr;
    }
    for (auto& child : m_children)
        child.DestroyAddon();
    m_children.clear();
}

void CUIModel::WeaponAttachableAddon::UpdateTransform(IKinematics* root, Fmatrix root_trans)
{
    if (!root)
        return;

    if (!m_model)
    {
        CreateAddon();
    }

    // bones the model doesn't have (strap_bone0 / 1 or the hand bones): at the model root (R6)
    if (boneL == BI_NONE || boneR == BI_NONE)
    {
        m_visual_trans.mul(root_trans, m_offset);
        for (auto& child : m_children)
            child.UpdateTransform(m_model, m_visual_trans);
        return;
    }

    const Fmatrix& mL = root->LL_GetTransform(static_cast<u16>(boneL));
    const Fmatrix& mR = root->LL_GetTransform(static_cast<u16>(boneR));

    Fmatrix mRes;
    Fvector R, D, N;
    D.sub(mL.c, mR.c);

    if (fis_zero(D.magnitude()))
    {
        mRes.set(root_trans);
        mRes.c.set(mR.c);
    }
    else
    {
        D.normalize();
        R.crossproduct(mR.j, D);

        N.crossproduct(D, R);
        N.normalize();

        mRes.set(R, N, D, mR.c);
        mRes.mulA_43(root_trans);
    }

    m_visual_trans.mul(mRes, m_offset);

    for (auto& child : m_children)
        child.UpdateTransform(m_model, m_visual_trans);
}

void CUIModel::WeaponAttachableAddon::Render(IRenderable* root)
{
    if (m_model)
    {
        ui_add_visual(root, m_model->dcast_RenderVisual(), m_visual_trans);
    }
    for (auto& child : m_children)
        child.Render(root);
}

CUIModel::CUIModel()
{
    mStaticRotate.identity();
    mUserRotate.identity();
    m_model_xform.identity();
    live_models().push_back(this);
}

CUIModel::~CUIModel()
{
    auto& live = live_models();
    if (const auto it = std::find(live.begin(), live.end(), this); it != live.end())
        live.erase(it);
    if (hovered_model == this)
        hovered_model = nullptr;

    for (auto& addon : m_visual_attachments)
        addon.DestroyAddon();
    for (auto& addon : m_weapon_attachments)
        addon.DestroyAddon();

    AnomalyClear();
    ModelDelete();
    TileFree();
}

void CUIModel::TileFree()
{
    if (m_tile)
        if (ui_render_alive())
            ::Render->ui_model_tile_free(m_tile);
    m_tile = 0;
    m_tile_w = m_tile_h = 0;
    m_tile_key = 0;
}

u64 CUIModel::TileKey(const u32 tile_w, const u32 tile_h) const
{
    if (!g_ui3d_tile_cache || !m_pCurrentVisual || IsDynamic() || m_anomaly || m_b3dStaticDebug || !m_weapon_attachments.empty())
        return 0;

    // what RenderTile and the resolve read: the visual, its visible bones (hidden_bones, addons shown on the model), the pose's motion, the
    // rotations (config, orientation, icon turn, user), zoom, projection, outline mask and the tile size. Zeroed first: padding hashes as 0
    struct key_data
    {
        u64 visual;
        u64 bones[2];
        Fmatrix rotate, user_rotate;
        float zoom, fov;
        u32 w, h, motion, flags;
    } d;
    std::memset(&d, 0, sizeof(d));

    const shared_str name = m_pCurrentVisual->getDebugName();
    d.visual = hash_bytes(name.c_str(), name.size());
    if (auto* k = m_pCurrentVisual->dcast_PKinematics())
    {
        const VisMask mask = k->LL_GetBonesVisible();
        d.bones[0] = mask._visimask.flags;
        d.bones[1] = mask._visimask_ex.flags;
    }
    d.rotate = mStaticRotate;
    d.user_rotate = mUserRotate;
    d.zoom = m_fItemScale * m_fItemUserScale;
    const bool ortho = IsOrthoEffective();
    d.fov = ortho ? 0.f : m_fov;
    d.w = tile_w;
    d.h = tile_h;
    d.motion = m_currentMotionID.val;
    d.flags = (ortho ? 1u : 0u) | (m_outline_mask ? 2u : 0u);

    u64 key = hash_bytes(&d, sizeof(d));

    // models attached to it (a weapon's scope_attach scope): visual, bone, offset, scale
    for (const auto& addon : m_visual_attachments)
    {
        struct
        {
            u64 visual, bone, prev;
            Fmatrix offset;
            Fvector scale;
        } a;
        std::memset(&a, 0, sizeof(a));
        a.visual = hash_bytes(addon.m_sAttachVisual.c_str(), addon.m_sAttachVisual.size());
        a.bone = hash_bytes(addon.m_sAttachBone.c_str(), addon.m_sAttachBone.size());
        a.prev = key;
        a.offset = addon.m_offset;
        a.scale = addon.m_vScaleFactor;
        key = hash_bytes(&a, sizeof(a));
    }
    return key ? key : 1;
}

void CUIModel::Reinit()
{
    for (auto& addon : m_visual_attachments)
        addon.DestroyAddon();
    m_visual_attachments.clear();

    for (auto& addon : m_weapon_attachments)
        addon.DestroyAddon();
    m_weapon_attachments.clear();

    m_fItemScale = 1.f;
    m_fItemZoffset = 1.f;
    m_animated = false;
    m_ortho = true;
    m_fov = deg2rad(25.f);
    m_vRotPerFrame = {};
    m_fixed_frame = false;

    AnomalyClear();
    particle_mode = false;
    particle_started = false;
    m_particle_box_valid = false;
    m_frame_valid = false;
    m_particle_scan_valid = false;

    mStaticRotate.identity();
    mUserRotate.identity();
    mCurrentAngleX = mCurrentAngleY = 0.f;
    m_fItemUserScale = 1.f;

    m_currentMotionID.set(0, 0);
    m_last_loop_id.set(0, 0);
    m_icon_turn.identity();
    m_icon_base.identity();
    m_oriented = false;
    m_orient_vertical = false;
    m_last_loop_name = nullptr;
    m_section = nullptr;
    m_current_motion = nullptr;
    m_idle_motion = nullptr;
    m_oneshot = m_oneshot_done = false;

    m_dirty = true;
}

void CUIModel::ModelSet(LPCSTR visual_name)
{
    ModelDelete();

    if (visual_name[0])
    {
        if (particle_mode)
            m_pCurrentVisual = ::Render->model_CreateParticles(visual_name);
        else
            m_pCurrentVisual = model_exists(visual_name) ? ::Render->model_Create(visual_name) : nullptr;

        if (!m_pCurrentVisual)
        {
            Msg("! [%s] can't create [%s]", __FUNCTION__, visual_name);
            return;
        }

        // the effect's own small world: no bouncing off the level around the origin (none in the main menu)
        if (auto* ps = m_pCurrentVisual->dcast_ParticleCustom())
            ps->SetWorldCollision(false);

        SetAnimation("idle");
    }
    m_dirty = true;
}

void CUIModel::ModelSet(IRenderVisual* visual)
{
    ModelDelete();

    if (visual)
    {
        m_pCurrentVisual = ::Render->model_Duplicate(visual);

        if (visual->dcast_PKinematics() && m_pCurrentVisual->dcast_PKinematics())
            m_pCurrentVisual->dcast_PKinematics()->LL_SetBonesVisible(visual->dcast_PKinematics()->LL_GetBonesVisible());
        if (auto* ps = m_pCurrentVisual ? m_pCurrentVisual->dcast_ParticleCustom() : nullptr)
            ps->SetWorldCollision(false);

        SetAnimation("idle");
    }
    m_dirty = true;
}

void CUIModel::ModelDelete()
{
    if (m_pCurrentVisual)
    {
        IRenderVisual* v = m_pCurrentVisual;
        if (ui_render_alive())
            ::Render->model_Delete(v, TRUE);

        m_pCurrentVisual = nullptr;
    }
    m_dirty = true;
}

void CUIModel::SetBonesVisible(IKinematics* pVisual)
{
    if (!m_pCurrentVisual)
        return;

    auto* k = m_pCurrentVisual->dcast_PKinematics();
    if (k)
    {
        k->LL_SetBonesVisible(pVisual->LL_GetBonesVisible());

        k->CalculateBones_Invalidate();
        k->CalculateBones(true);
    }
    m_dirty = true;
}

void CUIModel::UserRotate(float dx, float dy, bool free)
{
    const float deltaX = -dy / 150.f;
    const float deltaY = -dx / 150.f;

    if (free)
    {
        // g_3d_static_debug: unlimited, into the configured rotation, logged
        Fmatrix rotX, rotY;
        rotX.identity();
        rotX.rotateX(deltaX);
        rotY.identity();
        rotY.rotateY(deltaY);

        mStaticRotate.mulA_44(rotY);
        mStaticRotate.mulA_44(rotX);

        Fvector hpb;
        mStaticRotate.getHPB(hpb);
        hpb.mul(180.f / PI);
        Msg("Rotation = HPB degrees[%f, %f, %f]", hpb.x, hpb.y, hpb.z);
    }
    else
    {
        mCurrentAngleX = std::clamp(mCurrentAngleX - deltaX, -PI_DIV_6, PI_DIV_6);
        mCurrentAngleY = std::clamp(mCurrentAngleY - deltaY, -PI_DIV_6, PI_DIV_6);

        mUserRotate.setXYZ(mCurrentAngleX, mCurrentAngleY, 0.f);
    }
    m_dirty = true;
}

void CUIModel::UserZoom(float steps)
{
    // multiplicative: every wheel step is the same visible change over a wide range
    m_fItemUserScale = std::clamp(m_fItemUserScale * std::pow(1.1f, steps), m_user_zoom_min, m_user_zoom_max);
    m_dirty = true;
}

bool CUIModel::IsDynamic() const { return particle_mode || m_animated || m_user_rotating || !fis_zero(m_vRotPerFrame.square_magnitude()); }

bool CUIModel::SpinsAroundVertical() const
{
    return !particle_mode && !m_user_rotating && !fis_zero(m_vRotPerFrame.y) && fis_zero(m_vRotPerFrame.x) && fis_zero(m_vRotPerFrame.z);
}

bool CUIModel::Prepare(CUIWindow& wnd, shared_str& texture, Frect& rect)
{
    if (!m_pCurrentVisual)
        return false;

    // off the screen: nothing to render (in screen pixels)
    Frect rect_unscaled;
    wnd.GetAbsoluteRect(rect_unscaled);
    Fvector2 lt{rect_unscaled.x1, rect_unscaled.y1}, rb{rect_unscaled.x2, rect_unscaled.y2};
    UI()->ClientToScreenScaled(lt);
    UI()->ClientToScreenScaled(rb);
    if (lt.x > static_cast<float>(Device.dwWidth) || rb.x < 0.f || lt.y > static_cast<float>(Device.dwHeight) || rb.y < 0.f)
        return false;

    // tile: the window's size on the screen
    Fvector2 size{wnd.GetWidth(), wnd.GetHeight()};
    UI()->ClientToScreenScaled(size);
    const u32 tile_w = static_cast<u32>(std::max(1, iCeil(size.x - EPS_L)));
    const u32 tile_h = static_cast<u32>(std::max(1, iCeil(size.y - EPS_L)));
    // ui3d_projection changed the projection: render the cached tile again (a shared one: the key changes)
    if (IsOrthoEffective() != m_rendered_ortho)
    {
        m_rendered_ortho = !m_rendered_ortho;
        m_dirty = true;
    }

    // evicted while not drawn (the atlas was full): allocate again
    if (m_tile && !::Render->ui_model_tile_alive(m_tile))
    {
        m_tile = 0;
        m_dirty = true;
    }
    // a still model takes the shared tile of its image key, again when something changed: a model with the same image may have drawn it
    // already (a rebuilt list, a reopened window, a stack of the same items), then nothing renders
    const bool resized = tile_w != m_tile_w || tile_h != m_tile_h;
    if (!m_tile || m_dirty || resized)
    {
        const u64 key = TileKey(tile_w, tile_h);
        if (!m_tile || resized || key != m_tile_key)
        {
            TileFree();
            m_tile = key ? ::Render->ui_model_tile_shared(key, tile_w, tile_h) : ::Render->ui_model_tile_alloc(tile_w, tile_h);
            m_tile_key = key;
            m_tile_w = tile_w;
            m_tile_h = tile_h;
            // a shared tile renders only while it has no image (below)
            m_dirty = !key;
            if (!m_tile)
                return false;
        }
        else if (key)
            m_dirty = false; // the same image: the shared tile holds it already
    }

    // rotation per second (ui3d_icon_rotation_per_frame)
    if (!fis_zero(m_vRotPerFrame.square_magnitude()))
    {
        Fmatrix r;
        const float dt = ui_time_delta();
        r.setXYZ(m_vRotPerFrame.x * dt, m_vRotPerFrame.y * dt, m_vRotPerFrame.z * dt);
        mStaticRotate.mulA_44(r);
    }

    // particles play while drawn; they advance inside the tile pass (ui_model_add_visuals)
    if (particle_mode && !particle_started)
    {
        // the box of the visible particles while simulating unseen: the camera frames the effect from its first frame (no zoom out from a
        // tiny first box). An anomaly: every stage's effect, then its cycle starts with the idle one (warmed up, left playing)
        m_particle_scan_valid = false;
        if (m_anomaly)
        {
            for (IRenderVisual* v : m_extra_ps)
            {
                ParticleScan(v);
                v->dcast_ParticleCustom()->Stop(false);
            }
            ParticleScan(m_pCurrentVisual);
            AnomalyEnter(0);
        }
        else
            ParticleScan(m_pCurrentVisual);

        // the warm up built the sprites facing the scene camera: the first frame in the tile (ui_model_add_visuals) gets a 1 ms step, which
        // rebuilds them under the tile camera without moving the simulation on
        particle_last_update = ui_time_ms() - 1;
        particle_started = true;
        particle_stop_time = 0;
        particle_seen = false;
        m_particle_box_valid = false;
        m_frame_valid = false;
    }
    else if (m_anomaly)
        AnomalyUpdate();
    else if (particle_mode && particle_replay)
    {
        // an effect that has ended plays again after 0.5 s; the camera keeps its frame (the box only grows). Ended: stopped, or no particle
        // alive after some were (a group without a time limit counts as looped, but its one-shot emitters just die out: most blowouts; an
        // effect whose emitters start late is not restarted before its first particle)
        auto* ps = m_pCurrentVisual->dcast_ParticleCustom();
        const bool alive = ps->ParticlesCount() > 0;
        particle_seen |= alive;
        if (!ps->IsPlaying() || (particle_seen && !alive))
        {
            if (!particle_stop_time)
                particle_stop_time = ui_time_ms();
            else if (ui_time_ms() - particle_stop_time >= 500)
            {
                particle_stop_time = 0;
                particle_seen = false;
                ps->Stop(false);
                ps->Play();
                particle_last_update = ui_time_ms() - 1;
            }
        }
        else
            particle_stop_time = 0;
    }

    // a one-shot animation has ended: back to the idle one
    if (m_oneshot && m_oneshot_done)
    {
        m_oneshot = false;
        if (m_idle_motion.size())
            PlayAnimation(m_idle_motion.c_str(), true, m_oneshot_blend, 1.f);
        else if (m_last_loop_id.valid())
        {
            PlayMotion(m_last_loop_id, true, m_oneshot_blend, 1.f);
            m_current_motion = m_last_loop_name;
        }
    }

    bool have = ::Render->ui_model_tile_texture(m_tile, texture, rect);

    bool render = !have || m_dirty || IsDynamic();
    if (render && have && !IsDynamic())
    {
        // the per-frame budget of static models: the rest keep their old image and come next frame
        static u32 budget_frame = 0, budget_used = 0;
        if (budget_frame != Device.dwFrame)
        {
            budget_frame = Device.dwFrame;
            budget_used = 0;
        }
        if (budget_used >= static_cast<u32>(g_ui3d_static_per_frame))
            render = false;
        else
            ++budget_used;
    }

    if (render)
    {
        RenderTile(tile_w, tile_h);
        m_dirty = false;
        have = ::Render->ui_model_tile_texture(m_tile, texture, rect);
    }

    return have;
}

void CUIModel::RenderTile(const u32 tile_w, const u32 tile_h)
{
    // model transform: centre of the bounding sphere (meshes) to the origin, then the rotations
    auto& vis_data = m_pCurrentVisual->getVisData();

    Fmatrix xform;
    xform.identity();
    if (!particle_mode)
    {
        // the sphere follows the pose (hands together pull it aside): with the frame locked the centre stays too, or the model slides
        if (m_fixed_frame)
            m_fixed_box.getcenter(m_center_lock);
        else if (!FrameLocked())
            m_center_lock = vis_data.sphere.P;
        Fmatrix t;
        t.translate(Fvector(m_center_lock).invert());
        xform.mulA_44(t);
    }
    xform.mulA_44(mStaticRotate);
    xform.mulA_44(mUserRotate);
    m_model_xform = xform;

    if (auto* k = !particle_mode ? m_pCurrentVisual->dcast_PKinematics() : nullptr)
    {
        // paused: the motions advance by the UI clock here (UpdateTracks inside CalculateBones counts game time, which stands still)
        if (auto* ka = Device.Paused() ? m_pCurrentVisual->dcast_PKinematicsAnimated() : nullptr)
            ka->LL_UpdateTracks(ui_time_delta(), false, false);
        k->CalculateBones_Invalidate();
        k->CalculateBones(true);
    }

    // bounds in the tile's world space
    Fbox box;
    if (particle_mode)
    {
        // the visible particles (colour x alpha >= 0.08), not the effect's whole box: sparse effects (a bright core in a wide cloud of faint
        // sprites) would look tiny; the effect's box only until something is visible
        // plus what the effect covered while simulated unseen (Prepare): known from the first frame
        if (m_pCurrentVisual->dcast_ParticleCustom()->GetVisibleBox(box, 0.08f))
        {
            if (m_particle_scan_valid)
                box.merge(m_particle_scan_box);
        }
        else
            box = m_particle_scan_valid ? m_particle_scan_box : vis_data.box;
        if (particle_xform)
            box.xform(m_model_xform);
        if (box.is_valid() && !fis_zero(box.getvolume()))
        {
            if (m_particle_box_valid)
                m_particle_box.merge(box);
            else
                m_particle_box = box;
            m_particle_box_valid = true;
        }
        if (m_particle_box_valid)
            box = m_particle_box;
        else
            box.set(-0.25f, -0.25f, -0.25f, 0.25f, 0.25f, 0.25f);
    }
    else
    {
        // a box of a broken size (not finite, over a kilometre): a box around the bounding sphere instead
        box = vis_data.box;
        if (auto* k = m_pCurrentVisual->dcast_PKinematics())
        {
            // a still model (an item, a weapon): its visible bones and meshes (still_model_box); an animated one its bones, parked ones
            // left out (the bind pose of the meshes is not the pose drawn)
            // (a still model turning by itself, SetRotationPerFrame, is still: its meshes are the frame)
            if (m_animated || m_user_rotating)
                skeleton_box_without_outliers(k, box);
            else
                still_model_box(k, box);

            // a scope that is a separate model (SetWeapon, SetWeaponItem: scope_attach) is in the frame too
            for (auto& addon : m_visual_attachments)
            {
                addon.UpdateTransform(k, Fidentity);
                if (addon.m_model)
                {
                    Fbox ab = addon.m_model->dcast_RenderVisual()->getVisData().box;
                    ab.xform(addon.m_visual_trans);
                    box.merge(ab);
                }
            }
        }
        if (m_fixed_frame)
            box = m_fixed_box; // SetFixedFrame: the same frame for every visual
        Fvector bs;
        box.getsize(bs);
        if (!_valid(bs) || bs.magnitude() > 1000.f)
        {
            box.set(vis_data.sphere.P, vis_data.sphere.P);
            box.grow(vis_data.sphere.R);
        }
        if (SpinsAroundVertical() || m_fixed_frame)
        {
            // turning around the screen's vertical by itself (or a fixed frame, turned by the wheel): the frame is the cylinder the turn
            // sweeps (the radius of the box corners around that axis), the same at every angle; the box of the turned box grew and shrank with the angle and the model breathed
            Fbox turned;
            turned.invalidate();
            float r2 = 0.f;
            for (int i = 0; i < 8; ++i)
            {
                Fvector p;
                box.getpoint(i, p);
                m_model_xform.transform_tiny(p);
                r2 = std::max(r2, p.x * p.x + p.z * p.z);
                turned.modify(p);
            }
            const float r = _sqrt(r2);
            turned.min.x = turned.min.z = -r;
            turned.max.x = turned.max.z = r;
            box = turned;
        }
        else
            box.xform(m_model_xform);
    }

    Fvector center, size;
    box.getcenter(center);
    box.getsize(size);
    size.x = std::max(size.x, 0.001f);
    size.y = std::max(size.y, 0.001f);
    size.z = std::max(size.z, 0.001f);

    // a dynamic tile eases its frame to the new bounds (time constant 0.25 s): no jump when a particle box grows or an animation moves the
    // bounds. The first frame, static tiles and a frame far off the bounds take the bounds as they are: 4 x bigger, or more than 2 x their
    // size away (a skinned model's first frame can be garbage: the boars' bounding sphere centre is about 1e18 m off after the first
    // CalculateBones; easing back from there took seconds)
    const float size_m = size.magnitude();
    if (FrameLocked())
        ; // locked (SetFrameLock)
    else if (m_frame_valid && IsDynamic() && _valid(m_frame_size) && _valid(m_frame_center) && m_frame_size.magnitude() < size_m * 4.f &&
        m_frame_center.distance_to(center) < size_m * 2.f)
    {
        const float dt = std::min(static_cast<float>(ui_time_ms() - m_frame_time) * 0.001f, 0.25f);
        const float k = 1.f - expf(-dt / 0.25f);
        m_frame_center.lerp(m_frame_center, center, k);
        m_frame_size.lerp(m_frame_size, size, k);
    }
    else
    {
        m_frame_center = center;
        m_frame_size = size;
        if (!m_frame_valid)
            m_frame_since = ui_time_ms();
        m_frame_valid = true;
    }
    m_frame_time = ui_time_ms();
    center = m_frame_center;
    size = m_frame_size;

    const float aspect = static_cast<float>(tile_w) / static_cast<float>(tile_h); // w / h
    const float zoom = std::max(m_fItemScale * m_fItemUserScale, 0.01f);
    // margin around the model; with an outline the mask range in pixels on each side as well (the outline is not cut by the tile)
    const float pad = m_outline_mask ? 2.f * (ui_model_outline_max + 1.f) / static_cast<float>(std::max(1u, std::min(tile_w, tile_h))) : 0.f;
    const float fill = std::max(0.95f - pad, 0.5f);

    ui_model_view v{};
    const bool ortho = IsOrthoEffective();
    v.ortho = ortho;
    if (ortho)
    {
        // camera in front of the box, looking along +z
        const float margin = 0.1f + size.z * 0.05f;
        const float z0 = std::min(box.min.z, center.z - size.z * 0.5f);
        const float z1 = std::max(box.max.z, center.z + size.z * 0.5f);
        const Fvector pos{center.x, center.y, z0 - margin};
        v.view.build_camera_dir(pos, Fvector{0.f, 0.f, 1.f}, Fvector{0.f, 1.f, 0.f});

        const float h = std::max(size.y, size.x / aspect) / (fill * zoom);
        const float w = h * aspect;
        v.project.build_projection_ortho(w, h, margin * 0.5f, margin + (z1 - z0) + margin);
    }
    else
    {
        const float radius = std::max(size.magnitude() * 0.5f, 0.001f);
        // the distance fits the box at zoom 1 as the ortho camera does: its front face (the nearest plane) fills the view, the rest is farther
        // and so smaller. A fit of the bounding sphere left long, flat models (a car, a rifle) small in the middle. The zoom only narrows the
        // field of view (a distance from the zoomed view cancelled the zoom)
        const float tan_base = tanf(m_fov * 0.5f);
        const float half_h = std::max(size.y, size.x / aspect) * 0.5f / fill;
        const float dist = std::max(size.z * 0.5f + half_h / tan_base, 0.001f);
        const float tan_y = tan_base / zoom;
        const float tan_x = tan_y * aspect;

        const Fvector pos{center.x, center.y, center.z - dist};
        v.view.build_camera_dir(pos, Fvector{0.f, 0.f, 1.f}, Fvector{0.f, 1.f, 0.f});

        // depth range: the eased frame and the real bounds (the frame lags behind them)
        Fvector real_center;
        box.getcenter(real_center);
        const float depth = std::max(radius, real_center.distance_to(center) + box.getradius());
        const float zn = std::max(dist - depth * 1.05f, dist * 0.01f);
        const float zf = dist + depth * 1.05f;
        v.project.build_projection(2.f * atanf(tan_y), 1.f / aspect, zn, zf);
        v.tan_half_fov_x = tan_x;
        v.tan_half_fov_y = tan_y;
    }

    if (m_b3dStaticDebug)
    {
        static u32 last_log = 0;
        if (ui_time_ms() - last_log > 1000)
        {
            last_log = ui_time_ms();
            Msg("* [ui3d] [%s] tile %u (%u x %u), box [%f, %f, %f]-[%f, %f, %f], sphere [%f, %f, %f] r %f, ortho %d", m_pCurrentVisual->getDebugName().c_str(), m_tile, tile_w,
                tile_h, box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z, vis_data.sphere.P.x, vis_data.sphere.P.y, vis_data.sphere.P.z, vis_data.sphere.R, ortho ? 1 : 0);
        }
    }

    // particle effects: the ground plane under them (the effect's origin and up axis, where the game places them), in view space; only when
    // the camera is above it. particle.ps fades only the sprites lying parallel to it (common\soft_particles.h)
    if (particle_mode)
    {
        Fvector n, o;
        v.view.transform_dir(n, m_model_xform.j);
        v.view.transform_tiny(o, m_model_xform.c);
        n.normalize_safe();
        const float d = -n.dotproduct(o);
        if (d > 0.f)
            v.ground.set(n.x, n.y, n.z, d);
    }

    v.outline_mask = m_outline_mask;
    ::Render->ui_model_render(m_tile, v, *this);
}

void CUIModel::ui_model_add_visuals()
{
    const auto root = &ui_static_renderable();

    Fmatrix xform = m_model_xform;
    if (particle_mode)
    {
        // under the tile camera: OnFrame builds the sprites (UpdateCache) facing Device's camera and culls them against its frustum
        // and the other effects of an anomaly (stopped ones have no particles and draw nothing)
        const u32 dt = ui_time_ms() - particle_last_update;
        particle_last_update = ui_time_ms();
        const auto update = [&](IRenderVisual* v) {
            auto* ps = v->dcast_ParticleCustom();
            ps->UpdateParent(m_model_xform, zero_vel, particle_xform);
            if (dt)
                ps->OnFrame(dt);
        };
        update(m_pCurrentVisual);
        for (IRenderVisual* v : m_extra_ps)
            update(v);
        if (!particle_xform)
            xform.identity();
    }

    ui_add_visual(root, m_pCurrentVisual, xform);
    for (IRenderVisual* v : m_extra_ps)
        ui_add_visual(root, v, xform);

    for (auto& child : m_visual_attachments)
    {
        child.UpdateTransform(m_pCurrentVisual->dcast_PKinematics(), m_model_xform);
        child.Render(root);
    }

    for (auto& child : m_weapon_attachments)
    {
        child.UpdateTransform(m_pCurrentVisual->dcast_PKinematics(), m_model_xform);
        child.Render(root);
    }

}

void CUIModel::SetFromSection(LPCSTR section) { SetFromSection(section, "visual"); }

void CUIModel::SetFromSection(LPCSTR section, LPCSTR section_line)
{
    Reinit();

    if (!pSettings->section_exist(section))
    {
        Msg("! [%s] no section [%s]", __FUNCTION__, section);
        ModelDelete();
        return;
    }

    ApplySectionConfig(section);

    LPCSTR visual = READ_IF_EXISTS(pSettings, r_string, section, section_line, "");
    if (!visual || !visual[0])
    {
        Msg("! [%s] section [%s] has no [%s]", __FUNCTION__, section, section_line);
        ModelDelete();
        return;
    }

    ModelSet(visual);
    m_section = section;

    // a weapon's world model shows every addon bone: as in the inventory, the addons the weapon has without any attached (permanent ones
    // shown); other items their hidden_bones
    if (m_pCurrentVisual && m_pCurrentVisual->dcast_PKinematics())
    {
        IKinematics* k = m_pCurrentVisual->dcast_PKinematics();
        if (!xr_strcmp(section_line, "visual") && pSettings->line_exist(section, "scope_status"))
            apply_weapon_addons(k, m_section, false, "", false, false);
        else
            init_bones_from_config(k, section);

        // a torch: its light cone (light_trace_bone) hidden, as CTorch with the light off
        if (pSettings->line_exist(section, "light_trace_bone"))
        {
            const u16 bone = k->LL_BoneID(pSettings->r_string(section, "light_trace_bone"));
            if (bone != BI_NONE && k->LL_GetBoneVisible(bone))
            {
                k->LL_SetBoneVisible(bone, false, true);
                k->CalculateBones_Invalidate();
                k->CalculateBones(true);
            }
        }
    }

    // characters face the camera unless the section sets a rotation
    if (!pSettings->line_exist(section, "ui3d_icon_rotation") && is_character(m_pCurrentVisual))
        SetHPB(Fvector{PI, 0.f, 0.f});
}

// the ui3d_icon_* lines of a section: scale, rotation, rotation per second, projection, fov
void CUIModel::ApplySectionConfig(LPCSTR section)
{
    // a turn after OrientToCell's orientation (degrees, HPB: h turns around the screen's vertical); the editor's fix wins (ui3d_icon_fixes)
    Fvector turn = ui3d_icon_fixes::turn_deg(section);
    turn.mul(PI / 180.f);
    m_icon_turn.setHPB(turn.x, turn.y, turn.z);

    SetScale(ui3d_icon_fixes::scale(section));
    SetZOffset(READ_IF_EXISTS(pSettings, r_float, section, "ui3d_icon_z_offset", 1.f));

    Fvector static_rotate = READ_IF_EXISTS(pSettings, r_fvector3, section, "ui3d_icon_rotation", Fvector{});
    static_rotate.mul(PI / 180.f);
    SetHPB(static_rotate);

    static_rotate = READ_IF_EXISTS(pSettings, r_fvector3, section, "ui3d_icon_rotation_per_frame", Fvector{});
    static_rotate.mul(PI / 180.f);
    SetRotationPerFrame(static_rotate);

    const shared_str projection = READ_IF_EXISTS(pSettings, r_string, section, "ui3d_icon_projection", "ortho");
    SetProjection(projection != "perspective", READ_IF_EXISTS(pSettings, r_float, section, "ui3d_icon_fov", 25.f));
}

void CUIModel::SetVisual(LPCSTR path, bool pm)
{
    Reinit();

    particle_mode = pm;

    ModelSet(path);

    // characters face the camera; particle effects are seen a little from above and from the side, not straight from the front like in an
    // editor (SetHPB afterwards replaces it)
    if (!pm && is_character(m_pCurrentVisual))
        SetHPB(Fvector{PI, 0.f, 0.f});
    else if (pm)
        SetHPB(Fvector{deg2rad(25.f), deg2rad(20.f), 0.f});
}

void CUIModel::SetVisual(IRenderVisual* visual)
{
    Reinit();

    particle_mode = visual && visual->dcast_ParticleCustom() != nullptr;

    ModelSet(visual);
}

void CUIModel::SetAnimation(LPCSTR anim)
{
    if (!m_pCurrentVisual)
        return;

    if (auto* pKa = m_pCurrentVisual->dcast_PKinematicsAnimated())
    {
        m_currentMotionID = pKa->ID_Cycle_Safe(anim);

        if (!m_currentMotionID.valid())
        {
            if (strcmp(anim, "idle") != 0)
                Msg("~~ [%s] Animation [%s] is not available for visual [%s], playing default...", __FUNCTION__, anim, m_pCurrentVisual->getDebugName().c_str());

            m_currentMotionID = pKa->ID_Cycle_Safe("idle");

            if (!m_currentMotionID.valid())
                m_currentMotionID.set(0, 0);
        }

        for (u16 partition_id = 0; partition_id < pKa->partitions().count(); ++partition_id)
        {
            CBlend* B = pKa->PlayCycle(partition_id, m_currentMotionID);
            if (B)
                B->stop_at_end = false;
        }
        m_current_motion = anim;
        m_last_loop_id = m_currentMotionID;
        m_last_loop_name = anim;
        m_oneshot = m_oneshot_done = false;

        auto* k = m_pCurrentVisual->dcast_PKinematics();

        k->CalculateBones_Invalidate();
        k->CalculateBones(true);

        // a named animation plays: render every frame (idle stays a still pose; SetAnimated changes it)
        if (strcmp(anim, "idle") != 0)
            m_animated = true;
    }
    m_dirty = true;
}

void CUIModel::AddAttach(LPCSTR visual, LPCSTR bone, Fvector pos, Fvector rot, Fvector scale)
{
    if (particle_mode)
        return;

    auto& attach = m_visual_attachments.emplace_back();

    attach.m_sAttachVisual = visual;
    attach.m_sAttachBone = bone;
    attach.m_vScaleFactor = scale;

    Fvector ymp = rot;
    ymp.mul(PI / 180.f);
    attach.m_offset.identity();
    attach.m_offset.setHPB(ymp.x, ymp.y, ymp.z);
    attach.m_offset.translate_over(pos);

    m_dirty = true;
}

void CUIModel::AddWeaponAttach(LPCSTR section, bool strapped) { AddWeaponAttach(section, strapped, "", false, false); }

void CUIModel::AddWeaponAttach(LPCSTR section, bool strapped, LPCSTR scope, bool silencer, bool launcher)
{
    if (particle_mode || !m_pCurrentVisual)
        return;

    IKinematics* k = m_pCurrentVisual->dcast_PKinematics();
    if (!k)
        return;

    if (!pSettings->section_exist(section) || !pSettings->line_exist(section, "visual"))
    {
        Msg("! [%s] [%s] is not a weapon section with a visual", __FUNCTION__, section);
        return;
    }
    if (strapped && (!pSettings->line_exist(section, "strap_bone0") || !pSettings->line_exist(section, "strap_bone1")))
    {
        Msg("! [%s] [%s] has no strap bones", __FUNCTION__, section);
        return;
    }

    auto& attach = m_weapon_attachments.emplace_back();

    attach.m_sWeaponSection = section;
    attach.m_sAttachVisual = pSettings->r_string(section, "visual");
    attach.m_scope = scope ? scope : "";
    attach.m_silencer = silencer;
    attach.m_launcher = launcher;

    if (strapped)
    {
        attach.boneR = k->LL_BoneID(pSettings->r_string(section, "strap_bone0"));
        attach.boneR2 = k->LL_BoneID(pSettings->r_string(section, "strap_bone1"));
        attach.boneL = attach.boneR2;
    }
    else
    {
        // the hand bones of the model's section (weapon_bone0..2, as CObjectHandler / CActor), else the stalker's: bip01_r_finger1,
        // bip01_l_finger1, bip01_r_finger11
        const bool own = m_section.size() && pSettings->line_exist(m_section, "weapon_bone0");
        attach.boneR = k->LL_BoneID(own ? pSettings->r_string(m_section, "weapon_bone0") : "bip01_r_finger1");
        attach.boneL = k->LL_BoneID(own ? READ_IF_EXISTS(pSettings, r_string, m_section, "weapon_bone1", "bip01_l_finger1") : "bip01_l_finger1");
        attach.boneR2 = k->LL_BoneID(own ? READ_IF_EXISTS(pSettings, r_string, m_section, "weapon_bone2", "bip01_r_finger11") : "bip01_r_finger11");
    }

    const auto eHandDependence = static_cast<EHandDependence>(READ_IF_EXISTS(pSettings, r_s32, section, "hand_dependence", hd2Hand));
    if (eHandDependence == hd1Hand)
        attach.boneL = attach.boneR2;

    const Fvector pos = READ_IF_EXISTS(pSettings, r_fvector3, section, strapped ? "strap_position" : "position", Fvector{});
    Fvector ymp = READ_IF_EXISTS(pSettings, r_fvector3, section, strapped ? "strap_orientation" : "orientation", Fvector{});
    ymp.mul(PI / 180.f);
    attach.m_offset.identity();
    attach.m_offset.setHPB(ymp.x, ymp.y, ymp.z);
    attach.m_offset.translate_over(pos);

    m_dirty = true;
}

Fvector CUIModel::GetXYZ() const
{
    Fvector rotate_vector{};
    mStaticRotate.getXYZ(rotate_vector);
    return rotate_vector;
}

Fvector CUIModel::GetHPB() const
{
    Fvector rotate_vector{};
    mStaticRotate.getHPB(rotate_vector);
    return rotate_vector;
}

void CUIModel::TurnIcon(int axis, float degrees)
{
    Fmatrix r;
    r.identity();
    const float a = deg2rad(degrees);
    if (axis == 0)
        r.rotateX(a);
    else if (axis == 1)
        r.rotateY(a);
    else
        r.rotateZ(a);
    Fmatrix t;
    t.mul(r, m_icon_turn);
    m_icon_turn = t;
    ApplyIconTurn();
}

// the icon turn on the model: orient to the cell again, or (a fixed rotation, no OrientToCell) the turn after that rotation
void CUIModel::ApplyIconTurn()
{
    if (m_oriented)
    {
        OrientToCell(m_orient_w, m_orient_h, m_orient_tilt, m_orient_up);
        return;
    }
    mStaticRotate.mul(m_icon_turn, m_icon_base);
    // a vertical list (OrientIcon of a section with a fixed rotation): the icon turned a quarter, as OrientToCell does
    if (m_orient_vertical)
    {
        Fmatrix quarter, t;
        quarter.rotateZ(PI_DIV_2);
        t.mul(quarter, mStaticRotate);
        mStaticRotate = t;
    }
    m_dirty = true;
}

Fvector CUIModel::GetIconTurn() const
{
    Fvector hpb;
    m_icon_turn.getHPB(hpb);
    hpb.mul(180.f / PI);
    // round to a tenth: 90 degree turns compose to exact angles, the float noise is not a value
    for (float* v : {&hpb.x, &hpb.y, &hpb.z})
        *v = std::round(*v * 10.f) / 10.f + 0.f;
    return hpb;
}

void CUIModel::SetIconTurn(Fvector hpb_deg)
{
    hpb_deg.mul(PI / 180.f);
    m_icon_turn.setHPB(hpb_deg.x, hpb_deg.y, hpb_deg.z);
    ApplyIconTurn();
}

void CUIModel::ResetIconTurn()
{
    m_icon_turn.identity();
    ApplyIconTurn();
}

void CUIModel::OrientToCell(float cell_w, float cell_h, float tilt_deg, int up_axis)
{
    m_orient_w = cell_w;
    m_orient_h = cell_h;
    m_orient_tilt = tilt_deg;
    m_orient_up = up_axis;
    m_oriented = true;

    if (!m_pCurrentVisual || particle_mode)
        return;

    // the model's own box (a skinned one in its current pose, parked bones left out)
    Fbox box = m_pCurrentVisual->getVisData().box;
    if (auto* k = m_pCurrentVisual->dcast_PKinematics())
    {
        k->CalculateBones_Invalidate();
        k->CalculateBones(true);
        box = m_pCurrentVisual->getVisData().box;
        if (IsDynamic())
            skeleton_box_without_outliers(k, box);
        else
            still_model_box(k, box);
    }
    Fvector size;
    box.getsize(size);
    const float s[3] = {size.x, size.y, size.z};

    // axes by size: longest, middle, flattest
    int ax[3] = {0, 1, 2};
    std::sort(std::begin(ax), std::end(ax), [&](int a, int b) { return s[a] > s[b]; });

    // where each model axis goes: the longest along the cell's longer side, the flattest along the view (+z)
    // a square cell: the longest axis lies along it, unless the model clearly stands on its own up axis (y 1.25 x the next: a can, a
    // grenade). The flattest axis's positive side faces the camera (-z): a flat item lying on its +y face (a carpet, a newspaper) shows it,
    // not its back (culled: the item was not drawn)
    const bool square = fsimilar(cell_w, cell_h);
    const bool upright = square && ax[0] == 1 && s[1] > 1.25f * s[ax[1]];
    const bool wide = upright ? false : cell_w >= cell_h;
    Fvector img[3];
    img[ax[0]] = wide ? Fvector{1.f, 0.f, 0.f} : Fvector{0.f, 1.f, 0.f};
    img[ax[1]] = wide ? Fvector{0.f, 1.f, 0.f} : Fvector{1.f, 0.f, 0.f};
    img[ax[2]] = Fvector{0.f, 0.f, -1.f};

    // a given up axis: it goes up, the flatter of the other two to the camera, the last across
    if (up_axis >= 0 && up_axis <= 2)
    {
        int rest[2], n = 0;
        for (int a = 0; a < 3; ++a)
            if (a != up_axis)
                rest[n++] = a;
        const int flat = s[rest[0]] <= s[rest[1]] ? rest[0] : rest[1];
        const int across = flat == rest[0] ? rest[1] : rest[0];
        img[up_axis] = Fvector{0.f, 1.f, 0.f};
        img[flat] = Fvector{0.f, 0.f, -1.f};
        img[across] = Fvector{1.f, 0.f, 0.f};
        ax[1] = across; // the axis the mirror fix flips
    }


    // a rotation, not a mirror: flip the middle axis when needed (the face to the camera stays)
    Fvector jk;
    jk.crossproduct(img[1], img[2]);
    if (img[0].dotproduct(jk) < 0.f)
        img[ax[1]].invert();

    Fmatrix base;
    base.identity();
    base.i.set(img[0]);
    base.j.set(img[1]);
    base.k.set(img[2]);

    Fmatrix tilt;
    tilt.setHPB(deg2rad(tilt_deg), deg2rad(0.75f * tilt_deg), 0.f);

    // the section's ui3d_icon_turn (and the hand turns), then the tilt
    Fmatrix turned;
    turned.mul(m_icon_turn, base);
    mStaticRotate.mul(tilt, turned);

    // a vertical list (CUIDragDropListEx vertical_placement): the finished icon turned a quarter around the screen's axis, as the 2D icon is
    if (m_orient_vertical)
    {
        Fmatrix quarter;
        quarter.rotateZ(PI_DIV_2);
        Fmatrix t;
        t.mul(quarter, mStaticRotate);
        mStaticRotate = t;
    }
    m_dirty = true;
}

void CUIModel::OrientIcon(float cell_w, float cell_h, float tilt_deg, bool vertical)
{
    if (!m_section.size())
        return;
    // a fixed rotation of the section wins (ApplySectionConfig set it; weapons): only the quarter turn of a vertical list on top
    if (pSettings->line_exist(m_section, "ui3d_icon_rotation"))
    {
        m_orient_vertical = vertical;
        ApplyIconTurn();
        return;
    }
    // the type's rules from the config (base sections): the cell shape to orient for (magazines stand: 1, 2; addons lie: 2, 1) and the
    // model axis that goes up (outfits and helmets lie folded, head along z: 2; addons, modelled in the weapon's frame: 1)
    const Fvector2 cell = READ_IF_EXISTS(pSettings, r_fvector2, m_section, "ui3d_icon_cell", (Fvector2{cell_w, cell_h}));
    const int up_axis = ui3d_icon_fixes::up_axis(m_section);
    m_orient_vertical = vertical;
    OrientToCell(cell.x, cell.y, tilt_deg, up_axis);
}

//////////////////////////////////////////////////////////////////////
// Animations
//////////////////////////////////////////////////////////////////////

void CUIModel::OnOneshotEnd(CBlend* blend)
{
    if (auto* self = static_cast<CUIModel*>(blend->CallbackParam))
        self->m_oneshot_done = true;
}

bool CUIModel::PlayMotion(MotionID motion, bool loop, float blend, float speed)
{
    auto* pKa = m_pCurrentVisual ? m_pCurrentVisual->dcast_PKinematicsAnimated() : nullptr;
    if (!pKa || !motion.valid())
        return false;

    // the motion's own speed scaled; a blend of t seconds is the rate 1 / t (CBlend: blendAmount += dt * accrue); the old cycles fade out
    // at the same rate
    const CMotionDef* def = pKa->LL_GetMotionDef(motion);
    const float rate = blend > EPS_L ? 1.f / blend : 0.f;
    const float motion_speed = (def ? def->Speed() : 1.f) * std::max(speed, 0.01f);
    for (u16 partition_id = 0; partition_id < pKa->partitions().count(); ++partition_id)
        pKa->LL_PlayCycle(partition_id, motion, rate > 0.f, rate, rate, motion_speed, !loop, loop ? nullptr : &OnOneshotEnd, loop ? nullptr : this);

    m_currentMotionID = motion;
    if (loop)
        m_last_loop_id = motion;
    m_dirty = true;
    return true;
}

bool CUIModel::PlayAnimation(LPCSTR name, bool loop, float blend, float speed)
{
    auto* pKa = m_pCurrentVisual ? m_pCurrentVisual->dcast_PKinematicsAnimated() : nullptr;
    if (!pKa || !name || !name[0])
        return false;

    const MotionID motion = pKa->ID_Cycle_Safe(name);
    if (!motion.valid())
    {
        Msg("~ [%s] no animation [%s] in [%s]", __FUNCTION__, name, m_pCurrentVisual->getDebugName().c_str());
        return false;
    }

    if (loop)
        m_last_loop_name = name;
    m_oneshot = !loop;
    m_oneshot_done = false;
    m_oneshot_blend = blend;
    m_current_motion = name;
    m_animated = true;

    return PlayMotion(motion, loop, blend, speed);
}

bool CUIModel::PlayCycle(LPCSTR name)
{
    auto* pKa = m_pCurrentVisual ? m_pCurrentVisual->dcast_PKinematicsAnimated() : nullptr;
    if (!pKa || !name || !name[0])
        return false;

    const MotionID motion = pKa->ID_Cycle_Safe(name);
    if (!motion.valid())
        return false;

    if (CBlend* B = pKa->PlayCycle(motion))
        B->stop_at_end = false;
    m_current_motion = name;
    m_oneshot = m_oneshot_done = false;
    m_animated = true;
    m_dirty = true;
    return true;
}

bool CUIModel::HasAnimation(LPCSTR name) const
{
    auto* pKa = m_pCurrentVisual ? m_pCurrentVisual->dcast_PKinematicsAnimated() : nullptr;
    return pKa && name && pKa->ID_Cycle_Safe(name).valid();
}

namespace
{
// the cycle names of a skinned model, sorted (every motion slot)
xr_vector<shared_str> motion_names(IRenderVisual* v)
{
    xr_vector<shared_str> names;
    auto* pKa = v ? v->dcast_PKinematicsAnimated() : nullptr;
    if (!pKa)
        return names;
    for (u16 slot = 0; slot < pKa->LL_MotionsSlotCount(); ++slot)
        for (const auto& [name, idx] : *const_cast<shared_motions&>(pKa->LL_MotionsSlot(slot)).cycle())
            names.push_back(name);
    std::sort(names.begin(), names.end(), [](const shared_str& a, const shared_str& b) { return xr_strcmp(a, b) < 0; });
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}
} // namespace

u32 CUIModel::GetAnimationCount() const { return static_cast<u32>(motion_names(m_pCurrentVisual).size()); }

LPCSTR CUIModel::GetAnimationName(u32 index) const
{
    const auto names = motion_names(m_pCurrentVisual);
    return index < names.size() ? names[index].c_str() : "";
}

//////////////////////////////////////////////////////////////////////
// Weapon (HUD model with addons)
//////////////////////////////////////////////////////////////////////

void CUIModel::SetWeapon(LPCSTR section, LPCSTR scope, bool silencer, bool launcher) { SetupWeapon(section, scope, silencer, launcher, true); }

void CUIModel::SetWeaponItem(LPCSTR section, LPCSTR scope, bool silencer, bool launcher) { SetupWeapon(section, scope, silencer, launcher, false); }

void CUIModel::ModelClear()
{
    Reinit();
    ModelDelete();
}

bool CUIModel::IsBoneVisible(LPCSTR bone) const
{
    auto* k = m_pCurrentVisual ? m_pCurrentVisual->dcast_PKinematics() : nullptr;
    if (!k || !bone)
        return false;
    const u16 id = k->LL_BoneID(bone);
    return id != BI_NONE && !!k->LL_GetBoneVisible(id);
}

void CUIModel::CopyWeaponBones(IKinematics* src)
{
    auto* k = m_pCurrentVisual ? m_pCurrentVisual->dcast_PKinematics() : nullptr;
    if (!k || !src || k->LL_BoneCount() != src->LL_BoneCount())
        return;
    k->LL_SetBonesVisible(src->LL_GetBonesVisible());
    xr_vector<shared_str> list;
    for (LPCSTR line : {"laser_ray_bones", "torch_cone_bones"})
        if (ini_list(m_section, line, list))
            set_bones_visible(k, list, false);
    k->CalculateBones_Invalidate();
    k->CalculateBones(true);
    m_dirty = true;
}

void CUIModel::SetupWeapon(LPCSTR section, LPCSTR scope, bool silencer, bool launcher, bool want_hud)
{
    Reinit();

    if (!pSettings->section_exist(section) || !pSettings->line_exist(section, "visual"))
    {
        Msg("! [%s] [%s] is not a weapon section with a visual", __FUNCTION__, section);
        ModelDelete();
        return;
    }

    ApplySectionConfig(section);

    // the HUD model (the hud section's item_visual), else the world one
    const shared_str hud_sect = READ_IF_EXISTS(pSettings, r_string, section, "hud", "");
    const bool hud = want_hud && hud_sect.size() && pSettings->line_exist(hud_sect, "item_visual");
    ModelSet(hud ? pSettings->r_string(hud_sect, "item_visual") : pSettings->r_string(section, "visual"));
    m_section = section;

    auto* k = m_pCurrentVisual ? m_pCurrentVisual->dcast_PKinematics() : nullptr;
    if (!k)
        return;

    // the item's idle motion: anm_idle = <hands motion>[, <item motion>]; the item plays the second, else the same name. Not animated (the
    // pose of its first frame; PlayAnimation plays it)
    if (hud && pSettings->line_exist(hud_sect, "anm_idle"))
    {
        string128 hands{}, item{};
        LPCSTR S = pSettings->r_string(hud_sect, "anm_idle");
        _GetItem(S, 0, hands);
        if (_GetItemCount(S) > 1)
            _GetItem(S, 1, item);
        auto* pKa = m_pCurrentVisual->dcast_PKinematicsAnimated();
        const MotionID motion = pKa ? pKa->ID_Cycle_Safe(item[0] ? item : hands) : MotionID{};
        if (motion.valid())
        {
            PlayMotion(motion, true, 0.f, 1.f);
            m_current_motion = item[0] ? item : hands;
        }
    }

    const weapon_scope_model model = apply_weapon_addons(k, m_section, hud, scope, silencer, launcher);
    if (model.visual.size())
    {
        auto& attach = m_visual_attachments.emplace_back();
        attach.m_sAttachVisual = model.visual;
        attach.m_sAttachBone = model.bone;
        attach.m_vScaleFactor.set(model.scale, model.scale, model.scale);
        Fvector ypr = model.rot;
        ypr.mul(PI / 180.f);
        attach.m_offset.setHPB(ypr.x, ypr.y, ypr.z);
        attach.m_offset.translate_over(model.pos);
    }
    m_dirty = true;
}

//////////////////////////////////////////////////////////////////////
// Particles: start scan, anomaly cycle
//////////////////////////////////////////////////////////////////////

void CUIModel::ParticleScan(IRenderVisual* visual)
{
    auto* ps = visual->dcast_ParticleCustom();
    ps->UpdateParent(m_model_xform, zero_vel, particle_xform);
    ps->SetHudMode(false);
    ps->Play();

    const auto scan = [&] {
        Fbox b;
        if (!ps->GetVisibleBox(b, 0.08f))
            return;
        if (m_particle_scan_valid)
            m_particle_scan_box.merge(b);
        else
            m_particle_scan_box = b;
        m_particle_scan_valid = true;
    };

    // a looped effect is shown already running: simulate its first second at once (one step per call, OnFrame does at most 3), so it doesn't
    // build up on screen for a second. A one-shot effect (blowout) is played through once unseen for its box (up to its time limit, at most
    // 5 s, or until its particles are gone) and then starts from the beginning
    if (ps->IsLooped())
    {
        for (u32 t = 0; t < particle_warmup_ms; t += 33)
        {
            ps->OnFrame(33);
            scan();
        }
    }
    else
    {
        const float limit_s = ps->GetTimeLimit();
        const u32 limit = limit_s > 0.f ? std::min(static_cast<u32>(limit_s * 1000.f), 5000u) : 5000u;
        bool seen = false;
        for (u32 t = 0; t < limit; t += 33)
        {
            ps->OnFrame(33);
            scan();
            const bool alive = ps->ParticlesCount() > 0;
            seen |= alive;
            if (!ps->IsPlaying() || (seen && !alive))
                break;
        }
        ps->Stop(false);
        ps->Play();
    }
}

void CUIModel::AnomalyClear()
{
    for (IRenderVisual* v : m_extra_ps)
    {
        IRenderVisual* d = v;
        if (ui_render_alive())
            ::Render->model_Delete(d, TRUE);
    }
    m_extra_ps.clear();
    std::fill(std::begin(m_stage_ps), std::end(m_stage_ps), nullptr);
    m_anomaly = false;
}

void CUIModel::SetAnomaly(LPCSTR zone_section, float idle_seconds)
{
    Reinit();

    if (!pSettings->section_exist(zone_section))
    {
        Msg("! [%s] no section [%s]", __FUNCTION__, zone_section);
        ModelDelete();
        return;
    }

    // the stage effects; the same name twice shares one visual. The first found is the main visual (m_pCurrentVisual), the rest are owned here
    constexpr LPCSTR keys[4] = {"idle_particles", "awake_particles", "blowout_particles", "accum_particles"};
    shared_str names[4];
    particle_mode = true;
    ModelDelete();
    for (int i = 0; i < 4; ++i)
    {
        names[i] = READ_IF_EXISTS(pSettings, r_string, zone_section, keys[i], "");
        if (!names[i].size())
            continue;
        for (int j = 0; j < i && !m_stage_ps[i]; ++j)
            if (names[j] == names[i])
                m_stage_ps[i] = m_stage_ps[j];
        if (m_stage_ps[i])
            continue;
        IRenderVisual* v = ::Render->model_CreateParticles(names[i].c_str());
        if (!v)
        {
            Msg("! [%s] [%s] can't create [%s]", __FUNCTION__, zone_section, names[i].c_str());
            continue;
        }
        v->dcast_ParticleCustom()->SetWorldCollision(false);
        if (!m_pCurrentVisual)
            m_pCurrentVisual = v;
        else
            m_extra_ps.push_back(v);
        m_stage_ps[i] = v;
    }
    if (!m_pCurrentVisual)
    {
        Msg("! [%s] zone [%s] has no particle effects", __FUNCTION__, zone_section);
        particle_mode = false;
        return;
    }

    // times of the zone's states (ms), the idle one given (in the game it lasts until something enters)
    m_stage_ms[0] = static_cast<u32>(std::max(idle_seconds, 0.f) * 1000.f);
    m_stage_ms[1] = static_cast<u32>(std::max(READ_IF_EXISTS(pSettings, r_s32, zone_section, "awaking_time", 0), 0));
    m_stage_ms[2] = static_cast<u32>(std::max(READ_IF_EXISTS(pSettings, r_s32, zone_section, "blowout_time", 0), 0));
    m_stage_ms[3] = static_cast<u32>(std::max(READ_IF_EXISTS(pSettings, r_s32, zone_section, "accamulate_time", 0), 0));
    m_blowout_at_ms = std::min(READ_IF_EXISTS(pSettings, r_u32, zone_section, "blowout_particles_time", 0u), m_stage_ms[2]);
    m_anomaly = true;

    // seen a little from above and from the side, as single effects (SetVisual)
    SetHPB(Fvector{deg2rad(25.f), deg2rad(20.f), 0.f});
    m_dirty = true;
}

void CUIModel::AnomalyEnter(int stage)
{
    m_stage = stage;
    m_stage_t0 = ui_time_ms();
    m_blowout_fired = false;

    const auto play = [&](IRenderVisual* v) {
        if (!v)
            return;
        auto* ps = v->dcast_ParticleCustom();
        ps->UpdateParent(m_model_xform, zero_vel, particle_xform);
        ps->Stop(false);
        ps->Play();
    };
    const auto stop = [](IRenderVisual* v) {
        if (v)
            v->dcast_ParticleCustom()->Stop(false);
    };

    // as CCustomZone::OnStateSwitch / PlayIdleParticles: entering idle stops the other effects and plays the idle one (left running when it
    // already plays: the start, warmed up by the scan); leaving idle stops it; awaking and accumulate play their effect at once, the blowout
    // effect comes at blowout_particles_time (AnomalyUpdate)
    switch (stage)
    {
    case 0:
        for (int i = 1; i < 4; ++i)
            if (m_stage_ps[i] != m_stage_ps[0])
                stop(m_stage_ps[i]);
        if (m_stage_ps[0] && !m_stage_ps[0]->dcast_ParticleCustom()->IsPlaying())
            play(m_stage_ps[0]);
        break;
    case 1:
        stop(m_stage_ps[0]);
        play(m_stage_ps[1]);
        break;
    case 3: play(m_stage_ps[3]); break;
    default: break;
    }
    particle_last_update = ui_time_ms() - 1;
}

void CUIModel::AnomalyUpdate()
{
    const u32 t = ui_time_ms() - m_stage_t0;
    if (m_stage == 2 && !m_blowout_fired && t >= m_blowout_at_ms)
    {
        m_blowout_fired = true;
        if (IRenderVisual* v = m_stage_ps[2])
        {
            auto* ps = v->dcast_ParticleCustom();
            ps->UpdateParent(m_model_xform, zero_vel, particle_xform);
            ps->Stop(false);
            ps->Play();
        }
    }
    if (t >= m_stage_ms[m_stage])
        AnomalyEnter((m_stage + 1) % 4);
}
