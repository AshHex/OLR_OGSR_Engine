#include "stdafx.h"

#include "r__shader_audit.h"
#include "ResourceManager.h"
#include "dxRenderDeviceRender.h"
#include "PSLibrary.h"
#include "ParticleEffectDef.h"
#include "../../xr_3da/fmesh.h"
#include "../../xr_3da/xrLevel.h"
#include "../../xr_3da/IGame_Level.h"

#include <mutex>

namespace
{
struct missing_runtime_info
{
    xr_string textures;
    xr_string fallback;
    u32 count{};
};

std::mutex g_audit_lock;
xr_map<xr_string, missing_runtime_info> g_missing_shaders;
xr_map<xr_string, u32> g_missing_textures;

// Где встречается шейдер: тип источника + несколько примеров (модель, партикл, ...)
struct shader_usage
{
    u32 count{};
    xr_vector<xr_string> examples;
    xr_string textures;

    void add(const char* where, const char* tex)
    {
        ++count;
        if (examples.size() < 4)
            examples.emplace_back(where);
        if (textures.empty() && tex)
            textures = tex;
    }
};

using usage_map = xr_map<xr_string, shader_usage>;

xr_string lower(const char* s)
{
    xr_string r = s ? s : "";
    for (auto& c : r)
        c = (char)tolower((unsigned char)c);
    return r;
}

void add_usage(usage_map& m, const char* shader, const char* where, const char* tex)
{
    if (!shader || !shader[0])
        return;
    m[lower(shader)].add(where, tex);
}

void scan_ogf(IReader& R, const char* fname, usage_map& out, u32 depth = 0)
{
    if (depth > 8)
        return;

    if (R.find_chunk(OGF_TEXTURE))
    {
        string512 tex{}, sh{};
        R.r_stringZ(tex, sizeof(tex));
        R.r_stringZ(sh, sizeof(sh));
        add_usage(out, sh, fname, tex);
    }

    if (IReader* children = R.open_chunk(OGF_CHILDREN))
    {
        for (u32 id = 0;; ++id)
        {
            IReader* O = children->open_chunk(id);
            if (!O)
                break;
            scan_ogf(*O, fname, out, depth + 1);
            O->close();
        }
        children->close();
    }
}

void scan_meshes(usage_map& out, u32& files)
{
    FS_FileSet set;
    FS.file_list(set, "$game_meshes$", FS_ListFiles, "*.ogf");
    for (const auto& f : set)
    {
        IReader* R = FS.r_open("$game_meshes$", f.name.c_str());
        if (!R)
            continue;
        ++files;
        string_path where;
        xr_sprintf(where, "meshes\\%s", f.name.c_str());
        scan_ogf(*R, where, out);
        FS.r_close(R);
    }
}

void scan_particles(usage_map& out, u32& count)
{
    CPSLibrary& lib = RImplementation.PSLibrary;
    for (auto it = lib.FirstPED(); it != lib.LastPED(); ++it)
    {
        const PS::CPEDef* def = *it;
        if (!def)
            continue;
        ++count;
        string512 where;
        xr_sprintf(where, "particle %s", def->m_Name.c_str());
        add_usage(out, def->m_ShaderName.c_str(), where, def->m_TextureName.c_str());
    }
}

void scan_level(usage_map& out, u32& count)
{
    if (!g_pGameLevel)
        return;
    IReader* fs = FS.r_open("$level$", "level");
    if (!fs)
        return;
    if (IReader* chunk = fs->open_chunk(fsL_SHADERS))
    {
        const u32 n = chunk->r_u32();
        for (u32 i = 0; i < n; ++i)
        {
            string512 s{};
            chunk->r_stringZ(s, sizeof(s));
            if (!s[0])
                continue;
            LPSTR delim = strchr(s, '/');
            LPCSTR tex = "";
            if (delim)
            {
                *delim = 0;
                tex = delim + 1;
            }
            ++count;
            string512 where;
            xr_sprintf(where, "level %s", g_pGameLevel->name().c_str());
            add_usage(out, s, where, tex);
        }
        chunk->close();
    }
    FS.r_close(fs);
}

bool script_shader_exists(LPCSTR render_dir, const xr_string& shader)
{
    xr_string fn = shader;
    for (auto& c : fn)
        if (c == '\\')
            c = '_';
    string_path path;
    xr_sprintf(path, "%s%s.s", render_dir, fn.c_str());
    return !!FS.exist("$game_shaders$", path);
}

// Алиас из <render_dir>shader_aliases.ltx другого рендера (без проверки цели - она может быть C++ блендером того рендера)
LPCSTR dir_alias(LPCSTR render_dir, const xr_string& shader)
{
    static xr_map<xr_string, CInifile*> inis;
    auto it = inis.find(render_dir);
    if (it == inis.end())
    {
        string_path name, fn;
        xr_sprintf(name, "%sshader_aliases.ltx", render_dir);
        CInifile* ini = FS.exist(fn, "$game_shaders$", name) ? xr_new<CInifile>(fn, TRUE, TRUE, FALSE) : nullptr;
        it = inis.emplace(render_dir, ini).first;
    }
    if (!it->second || !it->second->line_exist("aliases", shader.c_str()))
        return nullptr;
    return it->second->r_string("aliases", shader.c_str());
}
} // namespace

void RAudit_NoteMissingShader(LPCSTR shader, LPCSTR textures, LPCSTR fallback)
{
    std::scoped_lock lock(g_audit_lock);
    auto& e = g_missing_shaders[lower(shader)];
    if (!e.count)
    {
        e.textures = textures ? textures : "";
        e.fallback = fallback ? fallback : "";
    }
    ++e.count;
}

void RAudit_NoteMissingTexture(LPCSTR texture)
{
    std::scoped_lock lock(g_audit_lock);
    ++g_missing_textures[lower(texture)];
}

u32 RAudit_Run(bool b_scan_meshes)
{
    CResourceManager& RM = *dxRenderDeviceRender::Instance().Resources;
    const LPCSTR cur_dir = ::Render->getShaderPath();

    usage_map usage;
    u32 n_meshes{}, n_particles{}, n_level{};
    if (b_scan_meshes)
        scan_meshes(usage, n_meshes);
    scan_particles(usage, n_particles);
    scan_level(usage, n_level);

    {
        std::scoped_lock lock(g_audit_lock);
        for (const auto& [name, info] : g_missing_shaders)
            usage[name].add("runtime (created during game)", info.textures.c_str());
    }

    static constexpr LPCSTR dirs[] = {"r1\\", "r2\\", "r3\\"};

    string_path report_name, report_path;
    xr_string tag = cur_dir;
    tag.pop_back();
    xr_sprintf(report_name, "shader_audit_%s.txt", tag.c_str());
    FS.update_path(report_path, "$logs$", report_name);

    IWriter* W = FS.w_open(report_path);
    if (!W)
    {
        Msg("! [%s] can't open [%s]", __FUNCTION__, report_path);
        return 0;
    }

    string1024 line;
    auto wl = [&](LPCSTR fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(line, sizeof(line), fmt, args);
        va_end(args);
        W->w_string(line);
    };

    wl("Shader audit, active render shaders dir: %s", cur_dir);
    wl("Scanned: %u meshes, %u particle effects, %u level shaders%s", n_meshes, n_particles, n_level, b_scan_meshes ? "" : " (meshes skipped)");
    wl("A shader exists if there is gamedata\\shaders\\<dir>\\<name>.s or a C++ blender from shaders.xr supported by the render");
    wl("");

    u32 n_missing_cur{};
    xr_vector<const xr_string*> missing_other[std::size(dirs)];

    wl("=== MISSING for the active render (drawn with a fallback shader) ===");
    for (const auto& [name, u] : usage)
    {
        const bool has_lua = !!RM._lua_HasShader(name.c_str());
        const bool has_lib = !!RM._FindBlender(name.c_str());
        const LPCSTR alias = (!has_lua && !has_lib) ? RAudit_FindAlias(name.c_str()) : nullptr;
        const bool alias_ok = alias && (RM._lua_HasShader(alias) || RM._FindBlender(alias));
        if (alias_ok)
            wl("  (alias) [%s] -> [%s] from shader_aliases.ltx", name.c_str(), alias);
        else if (!has_lua && !has_lib)
        {
            ++n_missing_cur;
            if (alias)
                wl("  bad alias [%s] -> [%s]: target shader not found", name.c_str(), alias);
            wl("[%s] used %u times, textures [%s]", name.c_str(), u.count, u.textures.c_str());
            for (const auto& ex : u.examples)
                wl("      %s", ex.c_str());
        }

        // Есть ли шейдер в каждой папке рендера. Для активного рендера проверка точная, для остальных
        // считаем, что блендер из shaders.xr им тоже доступен (набор C++ блендеров у рендеров почти одинаков).
        for (u32 d = 0; d < std::size(dirs); ++d)
        {
            const bool ok = !xr_strcmp(dirs[d], cur_dir) ? (has_lua || has_lib || alias_ok) : (has_lib || script_shader_exists(dirs[d], name) || dir_alias(dirs[d], name));
            if (!ok)
                missing_other[d].push_back(&name);
        }
    }
    if (!n_missing_cur)
        wl("  none");
    wl("");

    for (u32 d = 0; d < std::size(dirs); ++d)
    {
        wl("=== No %s*.s (and no usable library blender) - %u shaders ===", dirs[d], (u32)missing_other[d].size());
        for (const xr_string* n : missing_other[d])
            wl("  %s", n->c_str());
        wl("");
    }

    {
        std::scoped_lock lock(g_audit_lock);
        wl("=== Textures not found during game: %u ===", (u32)g_missing_textures.size());
        for (const auto& [name, cnt] : g_missing_textures)
            wl("  %s (%u)", name.c_str(), cnt);
        wl("");
        wl("=== Shaders replaced by fallback during game: %u ===", (u32)g_missing_shaders.size());
        for (const auto& [name, info] : g_missing_shaders)
            wl("  %s -> %s (%u), textures [%s]", name.c_str(), info.fallback.c_str(), info.count, info.textures.c_str());
    }

    FS.w_close(W);

    Msg("* [%s] %u shaders checked, %u missing for [%s]. Report: %s", __FUNCTION__, (u32)usage.size(), n_missing_cur, cur_dir, report_path);
    return n_missing_cur;
}

LPCSTR RAudit_FindAlias(LPCSTR shader)
{
    static CInifile* aliases = nullptr;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        string_path fn, name;
        xr_sprintf(name, "%sshader_aliases.ltx", ::Render->getShaderPath());
        if (FS.exist(fn, "$game_shaders$", name))
            aliases = xr_new<CInifile>(fn, TRUE, TRUE, FALSE);
    }
    if (!aliases || !shader || !aliases->line_exist("aliases", shader))
        return nullptr;
    return aliases->r_string("aliases", shader);
}
