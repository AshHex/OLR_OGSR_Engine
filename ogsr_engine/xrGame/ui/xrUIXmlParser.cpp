#include "stdafx.h"
#include "xrUIXmlParser.h"

#ifdef XRGAME_EXPORTS
#include "../ui_base.h"
#endif

shared_str CUIXml::correct_file_name(LPCSTR path, LPCSTR fn)
{
#ifdef XRGAME_EXPORTS
    if (0 == xr_strcmp(path, "ui") || 0 == xr_strcmp(path, "UI"))
    {
        return UI()->get_xml_name(fn);
    }
    else
        return fn;
#else
    return fn;
#endif
}

/*
    UI anchors (idea from ixray-1.6-stcop, branch ui-anchor-system).

    A window node with an "anchor" (or "anchor_min") attribute is placed relative to its
    reference rect: the nearest xml ancestor that has width and height, otherwise the screen.
    Its coordinates and the coordinates of all its xml descendants are given in square units:
    768 units are the screen height and the screen width is 768 * aspect (1024 on 4:3), so the
    layout is not stretched on wide screens. keep_aspect="1" makes a subtree square without anchoring it.

    anchor     = top_left | top_center | top_right | center_left | center | center_right |
                 bottom_left | bottom_center | bottom_right | stretch_h | stretch_v | stretch_full
    anchor_min = "x,y", anchor_max = "x,y" (0..1 of the reference rect) instead of a preset
    offset     = "l,t,r,b" | "l,t" | "v", or offset_x / offset_y (offset_left, offset_top, offset_right,
                 offset_bottom); without them x / y are the offset of the left top corner from the anchor point
    width, height - size on the axes that are not stretched

    The values are resolved when the xml is read (the HUD and the game dialogs are reloaded when
    the screen ratio changes), so every class that reads x / width itself gets them too.
*/
#ifdef XRGAME_EXPORTS
namespace ui_anchors
{
constexpr float UI_W = UI_BASE_WIDTH, UI_H = UI_BASE_HEIGHT;

bool is_anchored(const TiXmlElement* e) { return e->Attribute("anchor") || e->Attribute("anchor_min"); }

bool in_aspect_tree(const TiXmlElement* e)
{
    for (const TiXmlNode* n = e; n; n = n->Parent())
    {
        const TiXmlElement* el = n->ToElement();
        if (!el)
            break;
        if (is_anchored(el))
            return true;
        if (const char* ka = el->Attribute("keep_aspect"))
            return atoi(ka) != 0;
    }
    return false;
}

// the tags that keep texture coordinates in x / width
// (texture, texture_e, texture_offset, base_texture, left_top_texture...; not windows like track_texture_lod / cap_r1_detail_textures)
bool is_texture_tag(const TiXmlElement* e)
{
    const char* v = e->Value();
    const size_t len = xr_strlen(v);
    constexpr size_t suffix = sizeof("_texture") - 1;
    return !strncmp(v, "texture", 7) || !strncmp(v, "_texture", 8) || (len >= suffix && !xr_strcmp(v + len - suffix, "_texture")) || !xr_strcmp(v, "mask") ||
        !xr_strcmp(v, "spots_mask");
}

bool is_horizontal(LPCSTR a)
{
    static constexpr LPCSTR names[] = {"x", "width", "cell_width", "cell_sp_x", "x_offset", "offset_x", "push_off_x", "shadow_offset_x", "left_ident", "right_ident", "min_width"};
    for (LPCSTR n : names)
        if (!xr_strcmp(a, n))
            return true;
    return false;
}

float attr_flt(const TiXmlElement* e, LPCSTR a, float def)
{
    const char* s = e->Attribute(a);
    return s ? (float)atof(s) : def;
}

bool parse_preset(LPCSTR preset, Fvector2& amin, Fvector2& amax)
{
    struct item
    {
        LPCSTR name;
        float x1, y1, x2, y2;
    };
    static constexpr item presets[] = {
        {"top_left", 0, 0, 0, 0},       {"top_center", .5f, 0, .5f, 0},   {"top_right", 1, 0, 1, 0},     {"center_left", 0, .5f, 0, .5f},
        {"center", .5f, .5f, .5f, .5f}, {"center_right", 1, .5f, 1, .5f}, {"bottom_left", 0, 1, 0, 1},   {"bottom_center", .5f, 1, .5f, 1},
        {"bottom_right", 1, 1, 1, 1},   {"stretch_h", 0, .5f, 1, .5f},    {"stretch_v", .5f, 0, .5f, 1}, {"stretch_full", 0, 0, 1, 1},
    };
    string64 buf;
    xr_strcpy(buf, preset);
    for (char* c = buf; *c; ++c)
        *c = (*c == '-') ? '_' : (char)tolower(*c);
    for (const item& it : presets)
        if (!xr_strcmp(buf, it.name))
        {
            amin.set(it.x1, it.y1);
            amax.set(it.x2, it.y2);
            return true;
        }
    return false;
}

float kx() { return UI()->get_current_kx(); }

Fvector2 resolved_size(CUIXml& xml, const TiXmlElement* e);

// size of the reference rect of e in ui units
Fvector2 reference_size(CUIXml& xml, const TiXmlElement* e)
{
    for (const TiXmlNode* n = e->Parent(); n; n = n->Parent())
    {
        const TiXmlElement* p = n->ToElement();
        if (!p)
            break;
        if (p->Attribute("width") && p->Attribute("height"))
            return resolved_size(xml, p);
    }
    return Fvector2().set(UI_W, UI_H);
}

// rect of an anchored node relative to its reference rect, ui units
Frect anchored_rect(CUIXml& xml, const TiXmlElement* e)
{
    const Fvector2 ref = reference_size(xml, e);
    const float k = kx();
    const float pw = ref.x / k, ph = ref.y; // square units

    Fvector2 amin{}, amax{};
    const char* preset = e->Attribute("anchor");
    if (!preset || !parse_preset(preset, amin, amax))
    {
        if (preset && preset[0])
            Msg("! Invalid UI anchor preset [%s] in [%s] node [%s]", preset, xml.m_xml_file_name, e->Value());
        if (const char* s = e->Attribute("anchor_min"))
            sscanf(s, "%f,%f", &amin.x, &amin.y);
        amax = amin;
        if (const char* s = e->Attribute("anchor_max"))
            sscanf(s, "%f,%f", &amax.x, &amax.y);
    }

    float l, t, r = 0, b = 0;
    if (const char* s = e->Attribute("offset"))
    {
        float v[4]{};
        const int n = sscanf(s, "%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3]);
        if (n == 1)
            l = t = r = b = v[0];
        else
            l = v[0], t = v[1], r = v[2], b = v[3];
    }
    else
    {
        l = attr_flt(e, "offset_left", attr_flt(e, "offset_x", attr_flt(e, "x", 0)));
        t = attr_flt(e, "offset_top", attr_flt(e, "offset_y", attr_flt(e, "y", 0)));
        r = attr_flt(e, "offset_right", 0);
        b = attr_flt(e, "offset_bottom", 0);
    }

    const float w = attr_flt(e, "width", 0), h = attr_flt(e, "height", 0);
    Frect res;
    res.x1 = amin.x * pw + l;
    res.x2 = (amin.x != amax.x) ? amax.x * pw - r : res.x1 + w;
    res.y1 = amin.y * ph + t;
    res.y2 = (amin.y != amax.y) ? amax.y * ph - b : res.y1 + h;

    res.x1 *= k;
    res.x2 *= k;
    return res;
}

Fvector2 resolved_size(CUIXml& xml, const TiXmlElement* e)
{
    if (is_anchored(e))
    {
        const Frect r = anchored_rect(xml, e);
        return Fvector2().set(r.width(), r.height());
    }
    Fvector2 s{attr_flt(e, "width", 0), attr_flt(e, "height", 0)};
    if (in_aspect_tree(e))
        s.x *= kx();
    return s;
}
} // namespace ui_anchors
#endif

bool CUIXml::IsKeepAspect(LPCSTR path, int index)
{
#ifdef XRGAME_EXPORTS
    XML_NODE* node = NavigateToNode(path, index);
    const TiXmlElement* e = node ? node->ToElement() : nullptr;
    return e && ui_anchors::in_aspect_tree(e);
#else
    return false;
#endif
}

bool CUIXml::TransformLayoutAttrib(XML_NODE* node, LPCSTR attrib, LPCSTR raw_value, float default_value, float& result)
{
#ifdef XRGAME_EXPORTS
    using namespace ui_anchors;
    const TiXmlElement* e = node->ToElement();
    if (!e || is_texture_tag(e) || !in_aspect_tree(e))
        return false;

    if (is_anchored(e))
    {
        const bool is_x = !xr_strcmp(attrib, "x"), is_y = !xr_strcmp(attrib, "y");
        const bool is_w = !xr_strcmp(attrib, "width"), is_h = !xr_strcmp(attrib, "height");
        if (is_x || is_y || is_w || is_h)
        {
            const Frect r = anchored_rect(*this, e);
            const char* al = e->Attribute("alignment");
            const bool centered = al && strchr(al, 'c'); // waCenter: the position is the centre of the window
            if (is_x)
                result = centered ? (r.x1 + r.x2) * 0.5f : r.x1;
            else if (is_y)
                result = centered ? (r.y1 + r.y2) * 0.5f : r.y1;
            else if (is_w)
                result = r.width();
            else
                result = r.height();
            return true;
        }
        if (!xr_strcmp(attrib, "offset_x") || !xr_strcmp(attrib, "offset_y"))
            return false; // anchor offsets, square units of the reference rect
    }

    if (raw_value && is_horizontal(attrib))
    {
        result = (float)atof(raw_value) * kx();
        return true;
    }
#endif
    return false;
}

//#define LOG_ALL_XMLS
#ifdef LOG_ALL_XMLS
int ListXmlCount = 0;
struct DBGList_
{
    int num;
    bool closed;
};
xr_vector<DBGList_> dbg_list_xmls;
void dump_list_xmls()
{
    Msg("------Total  xmls %d", dbg_list_xmls.size());
    xr_vector<DBGList_>::iterator _it = dbg_list_xmls.begin();
    for (; _it != dbg_list_xmls.end(); ++_it)
        if (!(*_it).closed)
            Msg("--leak detected ---- xml = %d", (*_it).num);
}
#else
void dump_list_xmls() {}
#endif

CUIXml::CUIXml()
{
#ifdef LOG_ALL_XMLS
    ListXmlCount++;
    m_dbg_id = ListXmlCount;
    dbg_list_xmls.push_back(DBGList_());
    dbg_list_xmls.back().num = m_dbg_id;
    dbg_list_xmls.back().closed = false;
#endif
}

CUIXml::~CUIXml()
{
#ifdef LOG_ALL_XMLS
    xr_vector<DBGList_>::iterator _it = dbg_list_xmls.begin();
    bool bOK = false;
    for (; _it != dbg_list_xmls.end(); ++_it)
    {
        if ((*_it).num == m_dbg_id && !(*_it).closed)
        {
            bOK = true;
            (*_it).closed = true;
            dbg_list_xmls.erase(_it);
            break;
        }
        if ((*_it).num == m_dbg_id && (*_it).closed)
        {
            Msg("--XML [%d] already deleted", m_dbg_id);
            bOK = true;
        }
    }
    if (!bOK)
        Msg("CUIXml::~CUIXml.[%d] cannot find xml in list", m_dbg_id);
#endif
}
