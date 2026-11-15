#pragma once

// Общая точка входа для рендеров, собранных отдельными DLL (xrRender_R1..R4.dll).
//
// Движок сначала загружает все рендеры по очереди, только чтобы проверить поддержку железом
// (CEngineAPI::CreateRendererList), и сразу выгружает их. Поэтому в DllMain ничего не регистрируем:
// иначе консольные команды и глобальные указатели (::Render и т.п.) остались бы указывать в
// выгруженную DLL. Подключение делает движок явным вызовом AttachRender() у выбранного рендера.

#include "dxRenderFactory.h"
#include "dxUIRender.h"
#include "dxDebugRender.h"

#ifndef XRRENDER_STATIC

extern "C" __declspec(dllexport) void AttachRender()
{
    ::Render = &RImplementation;
    ::RenderFactory = &RenderFactoryImpl;
    ::DU = &DUImpl;
    UIRender = &UIRenderImpl;
    DRender = &DebugRenderImpl;
    xrRender_initconsole();
}

BOOL APIENTRY DllMain(HANDLE, DWORD, LPVOID) { return TRUE; }

#endif
