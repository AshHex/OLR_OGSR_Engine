#include "stdafx.h"
#include "dxUIRender.h"

#include "dxUIShader.h"

dxUIRender UIRenderImpl;

void dxUIRender::CreateUIGeom()
{
    hGeom_TL.create(FVF::F_TL, RCache.Vertex.Buffer(), 0);
    hGeom_LIT.create(FVF::F_LIT, RCache.Vertex.Buffer(), 0);
}

void dxUIRender::DestroyUIGeom()
{
    hGeom_TL = NULL;
    hGeom_LIT = NULL;
}

void dxUIRender::SetShader(IUIShader& shader)
{
    dxUIShader* pShader = (dxUIShader*)&shader;
    VERIFY(&pShader);
    VERIFY(pShader->hShader);
    RCache.set_Shader(pShader->hShader);

    // a 3D model tile's outline for this draw, every draw of hud\ui3d: on DX9 the pixel shader constants are shared registers, a tile rendered
    // between two UI draws (the first time the icons are shown) leaves its own values in them, and the binders run only on a table change
    if (R_constant* c = RCache.get_c("ui3d_outline")._get())
    {
        RCache.set_c(c, g_ui3d_outline);
        RCache.set_c("ui3d_outline_params", g_ui3d_outline_params);
        g_ui3d_outline_dirty = false;
    }

    // hud\minimap: where its mask lies on the screen and its rect in the mask texture (uv, by the size of the shader's texture of that name)
    if (R_constant* c = RCache.get_c("ui_mask_dst")._get())
    {
        Fvector4 src{0.f, 0.f, 1.f, 1.f};
        const SPass* pass = pShader->hShader->E[0] && !pShader->hShader->E[0]->passes.empty() ? pShader->hShader->E[0]->passes[0]._get() : nullptr;
        if (pass && pass->T && g_ui_mask_texture.size() && g_ui_mask_src.z > 0.f && g_ui_mask_src.w > 0.f) // empty rect: the whole texture
        {
            for (const auto& [stage, tex] : *pass->T)
            {
                if (!tex || _stricmp(tex->cName.c_str(), g_ui_mask_texture.c_str()))
                    continue;
                const float w = static_cast<float>(std::max(tex->get_Width(), 1u)), h = static_cast<float>(std::max(tex->get_Height(), 1u));
                src.set(g_ui_mask_src.x / w, g_ui_mask_src.y / h, g_ui_mask_src.z / w, g_ui_mask_src.w / h);
                break;
            }
        }
        RCache.set_c(c, g_ui_mask_dst);
        RCache.set_c("ui_mask_src", src);
    }
}

void dxUIRender::SetAlphaRef(int aref)
{
    // CHK_DX(HW.pDevice->SetRenderState(D3DRS_ALPHAREF,aref));
    RCache.set_AlphaRef(aref);
}

void dxUIRender::SetScissor(Irect* rect)
{
#if (RENDER == R_R3) || (RENDER == R_R4)
    RCache.set_Scissor(rect);
    StateManager.OverrideScissoring(rect ? true : false, TRUE);
#else //	(RENDER == R_R3) || (RENDER == R_R4)
    RCache.set_Scissor(rect);
#endif //	(RENDER == R_R3) || (RENDER == R_R4)
}

void dxUIRender::GetActiveTextureResolution(Fvector2& res)
{
    CTexture* T = RCache.get_ActiveTexture(0);
    res.set(float(T->get_Width()), float(T->get_Height()));
}

LPCSTR dxUIRender::UpdateShaderName(LPCSTR tex_name, LPCSTR sh_name)
{
    string_path buff;
    u32 v_dev = CAP_VERSION(HW.Caps.raster_major, HW.Caps.raster_minor);
    u32 v_need = CAP_VERSION(2, 0);
    // strstr(Core.Params,"-ps_movie") &&
    if ((v_dev >= v_need) && FS.exist(buff, "$game_textures$", tex_name, ".ogm"))
        return "hud\\movie";
    else
        return sh_name;
}

void dxUIRender::PushPoint(float x, float y, float z, u32 C, float u, float v)
{
    //.	VERIFY(m_PointType==pttLIT);
    switch (m_PointType)
    {
    case pttLIT:
        LIT_pv->set(x, y, z, C, u, v);
        ++LIT_pv;
        break;
    case pttTL:
        TL_pv->set(x, y, C, u, v);
        ++TL_pv;
        break;
    }
}

void dxUIRender::StartPrimitive(u32 iMaxVerts, ePrimitiveType primType, ePointType pointType)
{
    VERIFY(PrimitiveType == ptNone);
    VERIFY(m_PointType == pttNone);

    m_iMaxVerts = iMaxVerts;
    PrimitiveType = primType;
    m_PointType = pointType;

    switch (m_PointType)
    {
    case pttLIT:
        LIT_start_pv = (FVF::LIT*)RCache.Vertex.Lock(m_iMaxVerts, hGeom_LIT.stride(), vOffset);
        LIT_pv = LIT_start_pv;
        break;
    case pttTL:
        TL_start_pv = (FVF::TL*)RCache.Vertex.Lock(m_iMaxVerts, hGeom_TL.stride(), vOffset);
        TL_pv = TL_start_pv;
        break;
    }
}

void dxUIRender::FlushPrimitive()
{
    u32 primCount = 0;
    _D3DPRIMITIVETYPE d3dPrimType = D3DPT_FORCE_DWORD;
    std::ptrdiff_t p_cnt = 0;

    switch (m_PointType)
    {
    case pttLIT:
        p_cnt = LIT_pv - LIT_start_pv;
        VERIFY(u32(p_cnt) <= m_iMaxVerts);

        RCache.Vertex.Unlock(u32(p_cnt), hGeom_LIT.stride());
        RCache.set_Geometry(hGeom_LIT);
        break;
    case pttTL:
        p_cnt = TL_pv - TL_start_pv;
        VERIFY(u32(p_cnt) <= m_iMaxVerts);

        RCache.Vertex.Unlock(u32(p_cnt), hGeom_TL.stride());
        RCache.set_Geometry(hGeom_TL);
        break;
    default: NODEFAULT;
    }

    //	Update data for primitive type
    switch (PrimitiveType)
    {
    case ptTriStrip:
        primCount = (u32)(p_cnt - 2);
        d3dPrimType = D3DPT_TRIANGLESTRIP;
        break;
    case ptTriList:
        primCount = (u32)(p_cnt / 3);
        d3dPrimType = D3DPT_TRIANGLELIST;
        break;
    case ptLineStrip:
        primCount = (u32)(p_cnt - 1);
        d3dPrimType = D3DPT_LINESTRIP;
        break;
    case ptLineList:
        primCount = (u32)(p_cnt / 2);
        d3dPrimType = D3DPT_LINELIST;
        break;
    default: NODEFAULT;
    }

    if (primCount > 0)
        RCache.Render(d3dPrimType, vOffset, primCount);

    PrimitiveType = ptNone;
    m_PointType = pttNone;
}

void dxUIRender::CacheSetXformWorld(const Fmatrix& M) { RCache.set_xform_world(M); }

void dxUIRender::CacheSetCullMode(CullMode m) { RCache.set_CullMode(CULL_NONE + m); }
