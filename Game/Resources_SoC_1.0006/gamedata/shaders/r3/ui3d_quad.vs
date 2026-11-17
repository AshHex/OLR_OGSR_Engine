#include "common.h"

// 3D models in UI (r4_ui_models.cpp): a quad in clip space over the viewport, uv 0..1 (top left 0, 0)
p_screen main(v_TL_positiont I)
{
    p_screen O;
    O.hpos = float4(I.P.xy, 0.f, 1.f);
    O.tc0 = I.Tex0;
    return O;
}
