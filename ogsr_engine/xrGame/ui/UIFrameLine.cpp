//=============================================================================
//  Filename:   UIFrameLine.cpp
//	Created by Roman E. Marchenko, vortex@gsc-game.kiev.ua
//	Copyright 2004. GSC Game World
//	---------------------------------------------------------------------------
//  Класс аналогичный UIFrameRect за исключением того, что он предназначен для
//	отображения затекстурированного узкого прямоуголника произвольной длинны или
//	ширины. В качестве исходных материалов необходимо 3 текстуры: правая(нижняя),
//	левая(верхняя) и центральная
//=============================================================================

#include "stdafx.h"
#include "UIFrameLine.h"
#include "../hudmanager.h"
#include "UITextureMaster.h"

//////////////////////////////////////////////////////////////////////////

CUIFrameLine::CUIFrameLine() : uFlags(0), iSize(0), bHorizontalOrientation(true) { iPos.set(0, 0); }

//////////////////////////////////////////////////////////////////////////

void CUIFrameLine::Init(LPCSTR base_name, float x, float y, float size, bool horizontal, DWORD align)
{
    SetPos(x, y);
    SetSize(size);
    SetAlign(align);
    SetOrientation(horizontal);

    InitTexture(base_name);
}

void CUIFrameLine::InitTexture(const char* texture)
{
    string256 buf;

    CUITextureMaster::InitTexture(strconcat(sizeof(buf), buf, texture, "_back"), &elements[flBack]);
    CUITextureMaster::InitTexture(strconcat(sizeof(buf), buf, texture, "_b"), &elements[flFirst]);
    CUITextureMaster::InitTexture(strconcat(sizeof(buf), buf, texture, "_e"), &elements[flSecond]);
}

//////////////////////////////////////////////////////////////////////////

void CUIFrameLine::SetColor(u32 cl)
{
    for (int i = 0; i < flMax; ++i)
        elements[i].SetColor(cl);
}

//////////////////////////////////////////////////////////////////////////

void CUIFrameLine::UpdateSize()
{
    VERIFY(g_bRendering);

    // keep_aspect: the widths of the textures in square units (the virtual 1024 width is stretched on a wide screen)
    for (int i = 0; i < flMax; ++i)
    {
        const Frect r = elements[i].GetOriginalRect();
        elements[i].SetRect(0, 0, r.width() * m_kx, r.height());
    }

    float f_width = elements[flFirst].GetRect().width();
    float f_height = elements[flFirst].GetRect().height();
    elements[flFirst].SetPos(iPos.x, iPos.y);

    // Right or bottom texture
    float s_width = elements[flSecond].GetRect().width();
    float s_height = elements[flSecond].GetRect().height();

    (bHorizontalOrientation) ? elements[flSecond].SetPos(iPos.x + iSize - s_width, iPos.y) : elements[flSecond].SetPos(iPos.x, iPos.y + iSize - s_height);

    // Dimentions of element textures must be the same
    if (bHorizontalOrientation)
        R_ASSERT(s_height == f_height);
    else
        R_ASSERT(f_width == s_width);

    // Now stretch back texture to remaining space
    float back_width, back_height;

    if (bHorizontalOrientation)
    {
        back_width = iSize - f_width - s_width;
        back_height = f_height;

        // Size of frameline must be equal or greater than sum of size of two side textures
        back_width = _max(back_width, 0.f);
    }
    else
    {
        back_width = f_width;
        back_height = iSize - f_height - s_height;

        // Size of frameline must be equal or greater than sum of size of two side textures
        back_height = _max(back_height, 0.f);
    }

    // Now resize back texture
    float rem;
    int tile;

    float b_width = elements[flBack].GetRect().width();
    float b_height = elements[flBack].GetRect().height();

    if (bHorizontalOrientation)
    {
        rem = fmod(back_width, b_width);
        tile = iFloor(back_width / b_width);
        elements[flBack].SetPos(iPos.x + f_width, iPos.y);
        elements[flBack].SetTile(tile, 1, rem, 0);
    }
    else
    {
        rem = fmod(back_height, b_height);
        tile = iFloor(back_height / b_height);
        elements[flBack].SetPos(iPos.x, iPos.y + f_height);
        elements[flBack].SetTile(1, tile, 0, rem);
    }

    uFlags |= flValidSize;
}

//////////////////////////////////////////////////////////////////////////

void CUIFrameLine::Render()
{
    // an empty frame line (OGSR needs the node, the SOP layout has not got the picture)
    if (iSize <= 0.f)
        return;

    // If size changed - update size
    if (!(uFlags & flValidSize))
        UpdateSize();
    // Now render all statics
    for (int i = 0; i < flMax; ++i)
    {
        elements[i].Render();
    }
}