#include "stdafx.h"
#include "UIStatic.h"
#include "UIModel.h"
#include "UI3dStatic.h"
#include "../ai_space.h"
#include "../script_engine.h"

void CUIStatic__set_color_a(CUIStatic* self, u8 alpha) { self->SetColor(subst_alpha(self->GetColor(), alpha)); }

// ui3d_tile_stats(): the counters of the 3D models in UI atlas (IRender_interface::ui_model_get_stats), for tests and profiling
luabind::object ui3d_tile_stats()
{
    ui_model_stats s;
    ::Render->ui_model_get_stats(s);
    luabind::object r = luabind::newtable(ai().script_engine().lua());
    r["renders"] = s.renders;
    r["shared_hits"] = s.shared_hits;
    r["shared_misses"] = s.shared_misses;
    r["cache_evicted"] = s.cache_evicted;
    r["tiles"] = s.tiles;
    r["cached"] = s.cached;
    r["pages"] = s.pages;
    return r;
}

// ui3d_supported():the renderer draws 3D models in UI (every renderer; R3 / R4 need the packed G-buffer); else statics with a model draw nothing
bool ui3d_supported() { return ::Render->ui_model_supported(); }

using namespace luabind;

#pragma optimize("s", on)
void CUIStatic::script_register(lua_State* L)
{
    module(L)[class_<CUIStatic, CUIWindow>("CUIStatic")
                  .def(constructor<>())

                  .def("SetText", (void(CUIStatic::*)(LPCSTR))(&CUIStatic::SetText))
                  .def("SetTextST", (void(CUIStatic::*)(LPCSTR))(&CUIStatic::SetTextST))
                  .def("GetText", &CUIStatic::GetText)

                  .def("SetTextX", &CUIStatic::SetTextX)
                  .def("SetTextY", &CUIStatic::SetTextY)
                  .def("GetTextX", &CUIStatic::GetTextX)
                  .def("GetTextY", &CUIStatic::GetTextY)

                  .def("SetColor", &CUIStatic::SetColor)
                  .def("GetColor", &CUIStatic::GetColor)
                  .def("SetColorA", &CUIStatic__set_color_a)
                  .def("SetTextColor", &CUIStatic::SetTextColor_script)
                  .def("Init", (void(CUIStatic::*)(float, float, float, float)) & CUIStatic::Init)
                  .def("Init", (void(CUIStatic::*)(LPCSTR, float, float, float, float)) & CUIStatic::Init)
                  .def("InitTexture", &CUIStatic::InitTexture)
                  .def("SetTextureOffset", &CUIStatic::SetTextureOffset)

                  .def("GetOriginalRect", &CUIStatic::GetOriginalRect)
                  .def("SetOriginalRect", (void(CUIStatic::*)(float, float, float, float)) & CUIStatic::SetOriginalRect)
                  .def("ResetOriginalRect", &CUIStatic::ResetOriginalRect)
                  .def("SetStretchTexture", &CUIStatic::SetStretchTexture)
                  .def("GetStretchTexture", &CUIStatic::GetStretchTexture)

                  .def("SetTextAlign", &CUIStatic::SetTextAlign_script)
                  .def("GetTextAlign", &CUIStatic::GetTextAlign_script)

                  .def("SetHeading", &CUIStatic::SetHeading)
                  .def("GetHeading", &CUIStatic::GetHeading)
                  .def("EnableHeading", &CUIStatic::EnableHeading)

                  .def("ClipperOn", &CUIStatic::ClipperOn)
                  .def("ClipperOff", (void(CUIStatic::*)(void)) & CUIStatic::ClipperOff)
                  .def("GetClipperState", &CUIStatic::GetClipperState)
                  .def("SetClipRect", &CUIStatic::SetClipRect)
                  .def("GetClipRect", &CUIStatic::GetClipperRect)

                  .def("SetTextComplexMode", &CUIStatic::SetTextComplexMode)
                  .def("AdjustWidthToText", &CUIStatic::AdjustWidthToText)
                  .def("AdjustHeightToText", &CUIStatic::AdjustHeightToText)
                  .def("SetVTextAlign", &CUIStatic::SetVTextAlignment)
                  .def("SetTextPos", &CUIStatic::SetTextPos)

                  // 3D model as the static's texture (UIModel.h); the model belongs to the static
                  .def("SetModelSection", &CUIStatic::SetModelSection)
                  .def("GetModel", &CUIStatic::GetModel)
                  .def("Model", &CUIStatic::Model)
                  .def("RemoveModel", &CUIStatic::RemoveModel),

              def("ui3d_tile_stats", &ui3d_tile_stats), def("ui3d_supported", &ui3d_supported),

              class_<CUIModel>("CUIModel")
                  .def("SetFromSection", (void(CUIModel::*)(LPCSTR)) & CUIModel::SetFromSection)
                  .def("SetFromSection", (void(CUIModel::*)(LPCSTR, LPCSTR)) & CUIModel::SetFromSection)
                  .def("SetVisual", (void(CUIModel::*)(LPCSTR, bool)) & CUIModel::SetVisual)
                  .def("HasVisual", &CUIModel::HasVisual)
                  .def("IsBoneVisible", &CUIModel::IsBoneVisible)
                  .def("SetAnimation", &CUIModel::SetAnimation)
                  .def("SetAnimated", &CUIModel::SetAnimated)
                  .def("IsAnimated", &CUIModel::IsAnimated)
                  .def("AddAttach", &CUIModel::AddAttach)
                  .def("AddWeaponAttach", (void(CUIModel::*)(LPCSTR, bool)) & CUIModel::AddWeaponAttach)
                  .def("AddWeaponAttach", (void(CUIModel::*)(LPCSTR, bool, LPCSTR, bool, bool)) & CUIModel::AddWeaponAttach)
                  .def("SetWeapon", &CUIModel::SetWeapon)
                  .def("SetWeaponItem", &CUIModel::SetWeaponItem)
                  .def("SetAnomaly", (void(CUIModel::*)(LPCSTR)) & CUIModel::SetAnomaly)
                  .def("SetAnomaly", (void(CUIModel::*)(LPCSTR, float)) & CUIModel::SetAnomaly)
                  .def("OrientToCell", (void(CUIModel::*)(float, float, float)) & CUIModel::OrientToCell)
                  .def("OrientToCell", (void(CUIModel::*)(float, float, float, int)) & CUIModel::OrientToCell)
                  .def("OrientIcon", (void(CUIModel::*)(float, float, float)) & CUIModel::OrientIcon)
                  .def("OrientIcon", (void(CUIModel::*)(float, float, float, bool)) & CUIModel::OrientIcon)
                  .def("TurnIcon", &CUIModel::TurnIcon)
                  .def("GetIconTurn", &CUIModel::GetIconTurn)
                  .def("SetIconTurn", &CUIModel::SetIconTurn)
                  .def("ResetIconTurn", &CUIModel::ResetIconTurn)
                  .def("SetOutline", (void(CUIModel::*)(u32, float)) & CUIModel::SetOutline)
                  .def("SetOutline", (void(CUIModel::*)(u32, float, float)) & CUIModel::SetOutline)
                  .def("HasOutline", &CUIModel::HasOutline)
                  .def("GetOutlineColor", &CUIModel::GetOutlineColor)
                  .def("GetOutlineWidth", &CUIModel::GetOutlineWidth)
                  .def("GetOutlineSoftness", &CUIModel::GetOutlineSoftness)
                  .def("PlayAnimation", &CUIModel::PlayAnimation)
                  .def("SetIdleAnimation", &CUIModel::SetIdleAnimation)
                  .def("PlayCycle", &CUIModel::PlayCycle)
                  .def("SetFrameLock", &CUIModel::SetFrameLock)
                  .def("HasAnimation", &CUIModel::HasAnimation)
                  .def("GetAnimationCount", &CUIModel::GetAnimationCount)
                  .def("GetAnimationName", &CUIModel::GetAnimationName)
                  .def("GetCurrentAnimation", &CUIModel::GetCurrentAnimation)
                  .def("IsAnimationPlaying", &CUIModel::IsAnimationPlaying)
                  .def("SetXYZ", &CUIModel::SetXYZ)
                  .def("GetXYZ", &CUIModel::GetXYZ)
                  .def("SetHPB", &CUIModel::SetHPB)
                  .def("GetHPB", &CUIModel::GetHPB)
                  .def("SetScale", &CUIModel::SetScale)
                  .def("GetScale", &CUIModel::GetScale)
                  .def("SetRotationPerFrame", &CUIModel::SetRotationPerFrame)
                  .def("GetRotationPerFrame", &CUIModel::GetRotationPerFrame)
                  .def("SetProjection", &CUIModel::SetProjection)
                  .def("IsOrtho", &CUIModel::IsOrtho)
                  .def("Invalidate", &CUIModel::Invalidate)
                  .def("SetParticleWarmup", &CUIModel::SetParticleWarmup)
                  .def("SetParticleReplay", &CUIModel::SetParticleReplay)
                  .def("SetZoomRange", &CUIModel::SetZoomRange),

              class_<CUI3dStatic, CUIStatic>("CUI3dStatic")
                  .def(constructor<>())
                  .def("SetFromSection", (void(CUI3dStatic::*)(LPCSTR)) & CUI3dStatic::SetFromSection)
                  .def("SetFromSection", (void(CUI3dStatic::*)(LPCSTR, LPCSTR)) & CUI3dStatic::SetFromSection)
                  .def("SetVisual", &CUI3dStatic::SetVisual)
                  .def("SetAnimation", &CUI3dStatic::SetAnimation)
                  .def("AddAttach", &CUI3dStatic::AddAttach)
                  .def("AddWeaponAttach", &CUI3dStatic::AddWeaponAttach)
                  .def("AllowMouseControl", &CUI3dStatic::AllowMouseControl)
                  .def("GetXYZ", &CUI3dStatic::GetXYZ)
                  .def("SetXYZ", &CUI3dStatic::SetXYZ)
                  .def("GetHPB", &CUI3dStatic::GetHPB)
                  .def("SetHPB", &CUI3dStatic::SetHPB)
                  .def("SetScale", &CUI3dStatic::SetScale)
                  .def("GetScale", &CUI3dStatic::GetScale)
                  .def("SetZOffset", &CUI3dStatic::SetZOffset)
                  .def("GetZOffset", &CUI3dStatic::GetZOffset)
                  .def("SetRotationPerFrame", &CUI3dStatic::SetRotationPerFrame)
                  .def("GetRotationPerFrame", &CUI3dStatic::GetRotationPerFrame)
                  .def("SetAnimated", &CUI3dStatic::SetAnimated)
                  .def("IsAnimated", &CUI3dStatic::IsAnimated)
                  .def("SetProjection", &CUI3dStatic::SetProjection)
                  .def("IsOrtho", &CUI3dStatic::IsOrtho)
                  .def("Invalidate", &CUI3dStatic::Invalidate)
                  .def("SetParticleWarmup", &CUI3dStatic::SetParticleWarmup)
                  .def("SetParticleReplay", &CUI3dStatic::SetParticleReplay)
                  .def("SetZoomRange", &CUI3dStatic::SetZoomRange)];
}
