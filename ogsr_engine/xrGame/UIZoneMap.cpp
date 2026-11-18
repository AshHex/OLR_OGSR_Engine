#include "stdafx.h"
#include "uizonemap.h"

#include "hudmanager.h"

#include "InfoPortion.h"
#include "Pda.h"

#include "Grenade.h"
#include "level.h"
#include "game_cl_base.h"

#include "actor.h"
#include "ai_space.h"
#include "game_graph.h"

#include "ui/UIMap.h"
#include "ui/UIXmlInit.h"
#include "ui/UITextureMaster.h"
//////////////////////////////////////////////////////////////////////////

CUIZoneMap::CUIZoneMap() {}

CUIZoneMap::~CUIZoneMap() {}

void CUIZoneMap::Init()
{
    CUIXml uiXml;
    bool xml_result = uiXml.Init(CONFIG_PATH, UI_PATH, "zone_map.xml");
    R_ASSERT3(xml_result, "xml file not found", "zone_map.xml");

    // load map background
    CUIXmlInit xml_init;
    xml_init.InitStatic(uiXml, "minimap:background", 0, &m_background);

    xml_init.InitStatic(uiXml, "minimap:background:dist_text", 0, &m_pointerDistanceText);
    m_background.AttachChild(&m_pointerDistanceText);

    xml_init.InitStatic(uiXml, "minimap:level_frame", 0, &m_clipFrame);

    m_map_color = uiXml.NavigateToNode("minimap:level_frame:map_color", 0) ? CUIXmlInit::GetColor(uiXml, "minimap:level_frame:map_color", 0, 0xff) : 0x7fffffff;

    m_mask_file = nullptr;
    LPCSTR mask = uiXml.Read("minimap:background:mask", 0, nullptr);
    if (mask && *mask)
    {
        if (!strchr(mask, '\\')) // an atlas id (textures_descr)
        {
            if (LPCSTR file = CUITextureMaster::GetTextureFileName(mask))
            {
                m_mask_file = file;
                m_mask_src = CUITextureMaster::GetTextureRect(mask);
            }
        }
        else
        {
            // a whole file: an empty rect, the render side takes the whole texture
            m_mask_file = mask;
            m_mask_src = {};
        }
    }

    xml_init.InitStatic(uiXml, "minimap:center", 0, &m_center);

    m_activeMap = xr_new<CUIMiniMap>();
    m_clipFrame.AttachChild(m_activeMap);
    m_activeMap->SetAutoDelete(true);

    m_activeMap->EnableHeading(true);
    xml_init.InitStatic(uiXml, "minimap:compass", 0, &m_compass);
    m_activeMap->SetWindowName("minimap");

    //	m_background.AttachChild(&m_compass);

    m_clipFrame.AttachChild(&m_center);
    m_center.SetWndPos(m_clipFrame.GetWidth() / 2, m_clipFrame.GetHeight() / 2);
    m_fScale = 1.f;
}

void CUIZoneMap::Render()
{
    if (m_mask_file.size())
    {
        // the mask lies over the background picture as CUIStatic::DrawTexture draws it (stretch="0": the texture's own size); in screen pixels
        Frect r;
        m_background.GetAbsoluteRect(r);
        if (!m_background.GetStretchTexture())
        {
            const Frect o = m_background.GetStaticItem()->GetOriginalRectScaled();
            if (o.width() > 0.f && o.height() > 0.f)
            {
                const Fvector2 off = m_background.GetTextureOffeset();
                r.x1 += off.x;
                r.y1 += off.y;
                r.x2 = r.x1 + o.width() * (m_background.GetKeepAspect() ? UI()->get_current_kx() : 1.f);
                r.y2 = r.y1 + o.height();
            }
        }
        Fvector2 lt, rb;
        UI()->ClientToScreenScaled(lt, r.x1, r.y1);
        UI()->ClientToScreenScaled(rb, r.x2, r.y2);
        ::Render->ui_mask(Frect{}.set(lt.x, lt.y, rb.x, rb.y), m_mask_file.c_str(), m_mask_src);
    }
    m_clipFrame.Draw();
    m_background.Draw();
    m_compass.Draw();
}

void CUIZoneMap::SetHeading(float angle)
{
    m_activeMap->SetHeading(angle);
    m_compass.SetHeading(angle);
};

void CUIZoneMap::UpdateRadar(Fvector pos)
{
    m_clipFrame.Update();
    m_background.Update();
    m_activeMap->SetActivePoint(pos);

    if (m_activeMap->GetPointerDistance() > 0.5f)
    {
        string64 str;
        sprintf_s(str, "%.1f m.", m_activeMap->GetPointerDistance());
        m_pointerDistanceText.SetText(str);
    }
    else
    {
        m_pointerDistanceText.SetText("");
    }
}

bool CUIZoneMap::ZoomIn() 
{
    m_fScale = m_fScale + m_fScale * 0.25f;
    clamp(m_fScale, 0.5f, 2.f);
    ApplyZoom();  
    
    return true; 
}

bool CUIZoneMap::ZoomOut() 
{ 
    m_fScale = m_fScale - m_fScale * 0.25f;
    clamp(m_fScale, 0.5f, 2.f);
    ApplyZoom();

    return true; 
}

void CUIZoneMap::SetupCurrentMap()
{
    CInifile* pLtx = pGameIni;

    if (!pLtx->section_exist(Level().name()))
        pLtx = Level().pLevel;

    // dsh: очередной костыль. Если не создавать новый CUIMiniMap, то после
    // перехода с локации, на которой нет текстуры миникарты, на локацию,
    // где эта текстура есть (например, из X-10 на Радар), миникарта
    // перестает показываться.
    m_clipFrame.DetachChild(m_activeMap);
    m_activeMap = xr_new<CUIMiniMap>();
    m_clipFrame.AttachChild(m_activeMap);
    m_activeMap->SetAutoDelete(true);
    m_activeMap->EnableHeading(true);
    m_activeMap->SetWindowName("minimap");

    m_activeMap->SetMaskTexture(m_mask_file);
    m_activeMap->Init(Level().name(), *pLtx, m_mask_file.size() ? "hud\\minimap" : "hud\\default");
    m_activeMap->SetColor(m_map_color);

    Frect r;
    m_clipFrame.GetAbsoluteRect(r);
    m_activeMap->SetClipRect(r);

    ApplyZoom();
}

void CUIZoneMap::ApplyZoom() 
{
    Fvector2 wnd_size;
    float zoom_factor = float(m_clipFrame.GetWndRect().width()) / 100.0f;
    wnd_size.x = m_activeMap->BoundRect().width() * zoom_factor * m_fScale;
    wnd_size.y = m_activeMap->BoundRect().height() * zoom_factor * m_fScale;
    m_activeMap->SetWndSize(wnd_size);
}
