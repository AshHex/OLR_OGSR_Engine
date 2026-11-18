#pragma once

#include "ui/UIStatic.h"

class CActor;
class CUICustomMap;
//////////////////////////////////////////////////////////////////////////

class CUIZoneMap
{
    CUICustomMap* m_activeMap;
    float m_fScale;

    CUIStatic m_background;
    CUIStatic m_center;
    CUIStatic m_compass;
    CUIStatic m_clipFrame;
    CUIStatic m_pointerDistanceText;

    // SOP zone_map.xml: minimap:background:mask - the map shows through the red of this picture (atlas id or file), laid over the
    // background rect (hud\minimap, IRender_interface::ui_mask); minimap:level_frame:map_color - the map's colour
    shared_str m_mask_file;
    Frect m_mask_src{};
    u32 m_map_color{0x7fffffff};

public:
    CUIZoneMap();
    virtual ~CUIZoneMap();

    void SetHeading(float angle);
    void Init();

    void Render();
    void UpdateRadar(Fvector pos);

    void SetScale(float s) { m_fScale = s; }
    float GetScale() { return m_fScale; }

    bool ZoomIn();
    bool ZoomOut();

    void ApplyZoom();

    CUIStatic& Background() { return m_background; };
    CUIStatic& ClipFrame() { return m_clipFrame; }; // alpet: для экспорта в скрипты
    CUIStatic& Compass() { return m_compass; }; // alpet: для экспорта в скрипты

    void SetupCurrentMap();
};
