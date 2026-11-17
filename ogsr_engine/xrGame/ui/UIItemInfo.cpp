#include "stdafx.h"

#include "uiiteminfo.h"
#include "uistatic.h"
#include "UIXmlInit.h"

#include "UIListWnd.h"
#include "UIProgressBar.h"
#include "UIScrollView.h"

#include "../string_table.h"
#include "../Inventory_Item.h"
#include "UIInventoryUtilities.h"
#include "../PhysicsShellHolder.h"
#include "UIWpnParams.h"
#include "ui_af_params.h"
#include "UICellItem.h"
#include "UIModel.h"
#include "../Weapon.h"
#include "../WeaponAmmo.h"
#include "../../Include/xrRender/Kinematics.h"
#include "../../xr_3da/vismask.h"

extern BOOL g_ui3d_cell_icons;

CUIItemInfo::CUIItemInfo()
{
    UIItemImageSize.set(0.0f, 0.0f);
    UICondProgresBar = NULL;
    UICondition = NULL;
    UICost = NULL;
    UIWeight = NULL;
    UIItemImage = NULL;
    UIDesc = NULL;
    UIWpnParams = NULL;
    UIArtefactParams = NULL;
    UIName = NULL;
    m_pInvItem = NULL;
    m_b_force_drawing = false;
}

CUIItemInfo::~CUIItemInfo()
{
    xr_delete(UIWpnParams);
    xr_delete(UIArtefactParams);
}

void CUIItemInfo::Init(LPCSTR xml_name)
{
    CUIXml uiXml;
    bool xml_result = uiXml.Init(CONFIG_PATH, UI_PATH, xml_name);
    R_ASSERT2(xml_result, "xml file not found");

    CUIXmlInit xml_init;

    if (uiXml.NavigateToNode("main_frame", 0))
    {
        Frect wnd_rect;
        wnd_rect.x1 = uiXml.ReadAttribFlt("main_frame", 0, "x", 0);
        wnd_rect.y1 = uiXml.ReadAttribFlt("main_frame", 0, "y", 0);

        wnd_rect.x2 = uiXml.ReadAttribFlt("main_frame", 0, "width", 0);
        wnd_rect.y2 = uiXml.ReadAttribFlt("main_frame", 0, "height", 0);

        inherited::Init(wnd_rect.x1, wnd_rect.y1, wnd_rect.x2, wnd_rect.y2);
    }

    if (uiXml.NavigateToNode("static_name", 0))
    {
        UIName = xr_new<CUIStatic>();
        AttachChild(UIName);
        UIName->SetAutoDelete(true);
        xml_init.InitStatic(uiXml, "static_name", 0, UIName);
    }
    if (uiXml.NavigateToNode("static_weight", 0))
    {
        UIWeight = xr_new<CUIStatic>();
        AttachChild(UIWeight);
        UIWeight->SetAutoDelete(true);
        xml_init.InitStatic(uiXml, "static_weight", 0, UIWeight);
    }

    if (uiXml.NavigateToNode("static_cost", 0))
    {
        UICost = xr_new<CUIStatic>();
        AttachChild(UICost);
        UICost->SetAutoDelete(true);
        xml_init.InitStatic(uiXml, "static_cost", 0, UICost);
    }

    if (uiXml.NavigateToNode("static_condition", 0))
    {
        UICondition = xr_new<CUIStatic>();
        AttachChild(UICondition);
        UICondition->SetAutoDelete(true);
        xml_init.InitStatic(uiXml, "static_condition", 0, UICondition);
    }

    if (uiXml.NavigateToNode("condition_progress", 0))
    {
        UICondProgresBar = xr_new<CUIProgressBar>();
        AttachChild(UICondProgresBar);
        UICondProgresBar->SetAutoDelete(true);
        xml_init.InitProgressBar(uiXml, "condition_progress", 0, UICondProgresBar);
    }

    if (uiXml.NavigateToNode("descr_list", 0))
    {
        UIWpnParams = xr_new<CUIWpnParams>();
        UIWpnParams->InitFromXml(uiXml);

        UIArtefactParams = xr_new<CUIArtefactParams>();
        UIArtefactParams->InitFromXml(uiXml);

        UIDesc = xr_new<CUIScrollView>();
        AttachChild(UIDesc);
        UIDesc->SetAutoDelete(true);
        xml_init.InitScrollView(uiXml, "descr_list", 0, UIDesc);

        m_desc_info.bShowDescrText = !!uiXml.ReadAttribInt("descr_list", 0, "only_text_info", 1);
        xml_init.InitFont(uiXml, "descr_list:font", 0, m_desc_info.uDescClr, m_desc_info.pDescFont);
    }

    if (uiXml.NavigateToNode("image_static", 0))
    {
        UIItemImage = xr_new<CUIStatic>();
        AttachChild(UIItemImage);
        UIItemImage->SetAutoDelete(true);
        xml_init.InitStatic(uiXml, "image_static", 0, UIItemImage);
        UIItemImage->TextureAvailable(true);

        UIItemImage->TextureOff();
        UIItemImage->ClipperOn();
        UIItemImageSize.set(UIItemImage->GetWidth(), UIItemImage->GetHeight());

        m_model_allowed = !!uiXml.ReadAttribInt("image_static", 0, "model", 1);
        m_model_spin = uiXml.ReadAttribFlt("image_static", 0, "model_rotation_speed", 40.f);
        m_model_scale = uiXml.ReadAttribFlt("image_static", 0, "model_scale", 1.25f);
        if (m_model_allowed)
        {
            // the whole image box (the 2D icon shrinks to its picture), never turned a quarter like a heading 2D icon
            UIItemModel = xr_new<CUIStatic>();
            AttachChild(UIItemModel);
            UIItemModel->SetAutoDelete(true);
            xml_init.InitStatic(uiXml, "image_static", 0, UIItemModel);
            UIItemModel->EnableHeading(false);
            UIItemModel->SetStretchTexture(true);
            UIItemModel->Show(false);
        }
    }

    xml_init.InitAutoStaticGroup(uiXml, "auto", 0, this);
}

void CUIItemInfo::Init(float x, float y, float width, float height, LPCSTR xml_name)
{
    inherited::Init(x, y, width, height);
    Init(xml_name);
}

void CUIItemInfo::InitItem(CInventoryItem* pInvItem)
{
    m_pInvItem = pInvItem;
    if (!m_pInvItem)
        return;

    string256 str;
    if (UIName)
    {
        UIName->SetText(pInvItem->Name());
    }
    if (UIWeight)
    {
        sprintf_s(str, "%3.2f kg", pInvItem->Weight());
        UIWeight->SetText(str);
    }
    if (UICost)
    {
        sprintf_s(str, "%d RU", pInvItem->Cost()); // will be owerwritten in multiplayer
        UICost->SetText(str);
    }

    if (UICondProgresBar)
    {
        float cond = pInvItem->GetConditionToShow();
        UICondProgresBar->Show(true);
        UICondProgresBar->SetProgressPos(cond * 100.0f + 1.0f - EPS);
    }

    if (UIDesc)
    {
        UIDesc->Clear();
        VERIFY(0 == UIDesc->GetSize());
        TryAddWpnInfo(pInvItem->object());
        TryAddArtefactInfo(pInvItem->object().cNameSect());
        TryAddCustomInfo(pInvItem->object());
        if (m_desc_info.bShowDescrText)
        {
            CUIStatic* pItem = xr_new<CUIStatic>();
            pItem->SetTextColor(m_desc_info.uDescClr);
            pItem->SetFont(m_desc_info.pDescFont);
            pItem->SetWidth(UIDesc->GetDesiredChildWidth());
            pItem->SetTextComplexMode(true);
            pItem->SetText(*pInvItem->ItemDescription());
            pItem->AdjustHeightToText();
            UIDesc->AddWindow(pItem, true);
        }
        UIDesc->ScrollToBegin();
    }
    if (UIItemImage)
    {
        // Загружаем картинку
        pInvItem->m_icon_params.set_shader(UIItemImage);

        Frect rect = pInvItem->m_icon_params.original_rect();
        UIItemImage->TextureOn();
        UIItemImage->ClipperOn();
        Frect v_r = {0.0f, 0.0f, rect.width(), rect.height()};
        v_r.x2 *= UI()->get_current_kx();

        UIItemImage->GetUIStaticItem().SetRect(v_r);
        UIItemImage->SetWidth(_min(v_r.width(), UIItemImageSize.x));
        UIItemImage->SetHeight(_min(v_r.height(), UIItemImageSize.y));
    }
}

void CUIItemInfo::TryAddWpnInfo(CPhysicsShellHolder& obj)
{
    if (UIWpnParams->Check(obj))
    {
        UIWpnParams->SetInfo(obj);
        UIDesc->AddWindow(UIWpnParams, false);
    }
}

void CUIItemInfo::TryAddArtefactInfo(const shared_str& af_section)
{
    if (UIArtefactParams->Check(af_section))
    {
        UIArtefactParams->SetInfo(af_section);
        UIDesc->AddWindow(UIArtefactParams, false);
    }
}

#include "script_game_object.h"

void CUIItemInfo::TryAddCustomInfo(CPhysicsShellHolder& obj)
{
    if (pSettings->line_exist("engine_callbacks", "ui_item_info_callback"))
    {
        const char* callback = pSettings->r_string("engine_callbacks", "ui_item_info_callback");
        if (luabind::functor<void> lua_function; ai().script_engine().functor(callback, lua_function))
        {
            lua_function(UIDesc, obj.lua_game_object());
        }
    }
}

u64 CUIItemInfo::ItemModelKey() const
{
    // the item and its section, a weapon's addons and its bone state (an addon or the magazine shown or hidden), as the cells' 3D icons
    struct
    {
        const void* item;
        u64 section, scope, bones[2];
        u32 flags;
    } s;
    std::memset(&s, 0, sizeof(s));
    s.item = m_pInvItem;
    const shared_str& section = m_pInvItem->object().cNameSect();
    s.section = std::hash<std::string_view>{}(std::string_view{section.c_str(), section.size()});
    if (auto* w = smart_cast<CWeapon*>(m_pInvItem))
    {
        const bool scope = w->ScopeAttachable() && w->IsScopeAttached();
        s.flags = (scope ? 1u : 0u) | (w->SilencerAttachable() && w->IsSilencerAttached() ? 2u : 0u) |
            (w->GrenadeLauncherAttachable() && w->IsGrenadeLauncherAttached() ? 4u : 0u);
        if (scope)
        {
            const shared_str& name = w->GetScopeName();
            s.scope = std::hash<std::string_view>{}(std::string_view{name.c_str(), name.size()});
        }
        if (IKinematics* k = smart_cast<IKinematics*>(w->Visual()))
        {
            const VisMask mask = k->LL_GetBonesVisible();
            s.bones[0] = mask._visimask.flags;
            s.bones[1] = mask._visimask_ex.flags;
        }
    }
    return std::hash<std::string_view>{}(std::string_view{reinterpret_cast<const char*>(&s), sizeof(s)});
}

// the item's model, turning around the vertical by itself; false: none (turned off, another renderer, no model file), the 2D icon
bool CUIItemInfo::UpdateItemModel()
{
    if (!UIItemModel || !m_pInvItem)
        return false;

    auto* wpn = smart_cast<CWeapon*>(m_pInvItem);
    const shared_str& section = m_pInvItem->object().cNameSect();
    const ui3d_cell_icons_settings& cfg = ui3d_cell_icons();
    const bool on = g_ui3d_cell_icons && cfg.enabled && (!wpn || cfg.weapons) && (cfg.ammo || !smart_cast<CWeaponAmmo*>(m_pInvItem)) && ::Render->ui_model_supported() &&
        READ_IF_EXISTS(pSettings, r_bool, section, "ui3d_icon", true);
    if (!on)
    {
        UIItemModel->RemoveModel();
        m_model_key = 0;
        UIItemModel->Show(false);
        return false;
    }

    const u64 key = ItemModelKey();
    if (key != m_model_key)
    {
        m_model_key = key;
        UIItemModel->RemoveModel();
        CUIModel& m = UIItemModel->Model();
        if (wpn)
        {
            // the world model with the attached addons and the object's bone state (addons, magazine), as the weapon cells show it
            const bool scope = wpn->ScopeAttachable() && wpn->IsScopeAttached();
            m.SetWeaponItem(section.c_str(), scope ? wpn->GetScopeName().c_str() : "", wpn->SilencerAttachable() && wpn->IsSilencerAttached(),
                            wpn->GrenadeLauncherAttachable() && wpn->IsGrenadeLauncherAttached());
            if (m.HasVisual())
                m.CopyWeaponBones(smart_cast<IKinematics*>(wpn->Visual()));
        }
        else
            m.SetFromSection(section.c_str());

        m_model_failed = !m.HasVisual();
        if (m_model_failed)
            UIItemModel->RemoveModel();
        else
        {
            // faced as its cell icon (oriented while still: the still model's box), then turning around the screen's vertical
            m.SetAnimated(false);
            m.SetRotationPerFrame(Fvector{});
            m.OrientIcon(static_cast<float>(m_pInvItem->GetGridWidth()), static_cast<float>(m_pInvItem->GetGridHeight()), cfg.tilt, false);
            m.SetRotationPerFrame(Fvector{0.f, deg2rad(m_model_spin), 0.f});

            // the window of the model: the 2D icon's size x model_scale (a small item stays small, as its icon), up to 85% of the image box
            // (the whole box reaches over the name and the condition bar)
            const Frect icon = m_pInvItem->m_icon_params.original_rect();
            const float w = icon.width() * UI()->get_current_kx() * m_model_scale, h = icon.height() * m_model_scale;
            UIItemModel->SetWidth(std::min(w, UIItemImageSize.x * 0.85f));
            UIItemModel->SetHeight(std::min(h, UIItemImageSize.y * 0.85f));
        }
    }

    UIItemModel->Show(!m_model_failed);
    return !m_model_failed;
}

void CUIItemInfo::Draw()
{
    if (!m_pInvItem && !m_b_force_drawing)
        return;

    if (m_pInvItem && UIItemImage)
        UIItemImage->Show(!UpdateItemModel());
    else if (UIItemModel)
        UIItemModel->Show(false);

    inherited::Draw();
}
