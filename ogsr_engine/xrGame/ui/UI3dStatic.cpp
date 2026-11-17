#include "stdafx.h"
#include "UI3dStatic.h"

#include "../UICursor.h"
#include "../ui_base.h"

#include "../../xr_3da/xr_input.h"

bool CUI3dStatic::OnMouse(float x, float y, EUIMessages mouse_action)
{
    const bool shiftKey = pInput->iGetAsyncKeyState(DIK_LSHIFT);

    if ((m_allowMouseControl || m_b3dStaticDebug) && shiftKey && CursorOverWindow())
    {
        switch (mouse_action)
        {
        case WINDOW_MOUSE_WHEEL_DOWN:
        case WINDOW_MOUSE_WHEEL_UP: {
            const float delta = mouse_action == WINDOW_MOUSE_WHEEL_DOWN ? 0.1f : -0.1f;
            if (m_b3dStaticDebug)
            {
                SetScale(GetScale() + delta);
                Msg("Scale = %f", GetScale());
            }
            else
                Model().UserZoom(mouse_action == WINDOW_MOUSE_WHEEL_DOWN ? 1.f : -1.f);
            return true;
        }
        }
    }

    if (mouse_action == WINDOW_LBUTTON_DOWN && (m_allowMouseControl || m_b3dStaticDebug) && CursorOverWindow() && pInput->iGetAsyncKeyState(MOUSE_1))
    {
        SetRotating(true);
        return true;
    }
    if (mouse_action == WINDOW_LBUTTON_UP && m_bRotateItemActive)
    {
        SetRotating(false);
        return true;
    }

    return inherited::OnMouse(x, y, mouse_action);
}

void CUI3dStatic::Update()
{
    if (m_bRotateItemActive && !pInput->iGetAsyncKeyState(MOUSE_1))
        SetRotating(false);

    const Fvector2 cursor_pos = GetUICursor()->GetCursorPosition();

    if (m_bRotateItemActive && (m_allowMouseControl || m_b3dStaticDebug) && !cursor_pos.similar(m_vMousePositionPrev, 0.01f))
    {
        const Fvector2 delta = Fvector2(cursor_pos).sub(m_vMousePositionPrev);
        Model().UserRotate(delta.x, delta.y, m_b3dStaticDebug);
    }

    m_vMousePositionPrev.set(cursor_pos);

    inherited::Update();
}
