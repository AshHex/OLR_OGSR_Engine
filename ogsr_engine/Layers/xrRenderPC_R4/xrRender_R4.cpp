// xrRender_R2.cpp : Defines the entry point for the DLL application.
//
#include "stdafx.h"

#ifdef XRRENDER_STATIC

#include "../xrRender/dxRenderFactory.h"
#include "../xrRender/dxUIRender.h"
#include "../xrRender/dxDebugRender.h"

void AttachRender()
{
    ::Render = &RImplementation;
    ::RenderFactory = &RenderFactoryImpl;
    ::DU = &DUImpl;
    UIRender = &UIRenderImpl;
    DRender = &DebugRenderImpl;
    xrRender_initconsole();
}

#else

#include "../xrRender/xrRender_attach.h"

extern "C" {
bool _declspec(dllexport) SupportsDX11Rendering();
};

bool _declspec(dllexport) SupportsDX11Rendering()
{
    return xrRender_test_hw() ? true : false;
    /*
    D3DCAPS9					caps;
    CHW							_HW;
    _HW.CreateD3D				();
    _HW.pD3D->GetDeviceCaps		(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,&caps);
    _HW.DestroyD3D				();
    u16		ps_ver_major		= u16 ( u32(u32(caps.PixelShaderVersion)&u32(0xf << 8ul))>>8 );

    if (ps_ver_major<3)
        return false;
    else
        return true;
    */
}

#endif
