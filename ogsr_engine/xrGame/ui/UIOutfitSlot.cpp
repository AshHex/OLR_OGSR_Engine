#include "stdafx.h"
#include "UIOutfitSlot.h"
#include "UIStatic.h"
#include "UICellItem.h"
#include "../CustomOutfit.h"
#include "../actor.h"
#include "UIInventoryUtilities.h"
#include "UIModel.h"
#include "../UICursor.h"
#include "../Weapon.h"
#include "../inventory.h"

extern BOOL g_ui3d_cell_icons;

CUIOutfitDragDropList::CUIOutfitDragDropList()
{
    m_background = xr_new<CUIStatic>();
    m_background->SetAutoDelete(true);
    m_actor_model = xr_new<CUIStatic>();
    m_actor_model->SetAutoDelete(true);
    if (Core.Features.test(xrCore::Feature::old_outfit_slot_style))
    {
        AttachChild(m_background);
        AttachChild(m_actor_model); // drawn by Draw, in place of the picture
    }
    else
        m_actor_model_allowed = false;
    m_default_outfit = "npc_icon_without_outfit";
}

CUIOutfitDragDropList::~CUIOutfitDragDropList()
{
    if (!m_actor_model->GetParent())
        xr_delete(m_actor_model);
}

#include "../level.h"
#include "../game_base_space.h"

void CUIOutfitDragDropList::SetOutfit(CUICellItem* itm)
{
    /*
    static Fvector2 fNoOutfit			= pSettings->r_fvector2(m_default_outfit, "full_scale_icon");
    Frect								r;
    r.x1								= fNoOutfit.x*ICON_GRID_WIDTH;
    r.y1								= fNoOutfit.y*ICON_GRID_HEIGHT;
    r.x2								= r.x1+CHAR_ICON_FULL_WIDTH*ICON_GRID_WIDTH;
    r.y2								= r.y1+CHAR_ICON_FULL_HEIGHT*ICON_GRID_HEIGHT;
    */
    m_background->Init(0, 0, GetWidth(), GetHeight());

    m_background->SetStretchTexture(true);

    {
        if (itm)
        {
            PIItem _iitem = (PIItem)itm->m_pData;
            CCustomOutfit* pOutfit = smart_cast<CCustomOutfit*>(_iitem);
            VERIFY(pOutfit);
            /*
            r.lt			= pOutfit->GetIconPos();
            r.x1			*= ICON_GRID_WIDTH;
            r.y1			*= ICON_GRID_HEIGHT;
            */
            m_background->InitTexture(pOutfit->GetFullIconName().c_str());
        }
        else
        {
            m_background->InitTexture("npc_icon_without_outfit");
        }
        /*
        r.x2			= r.x1+CHAR_ICON_FULL_WIDTH*ICON_GRID_WIDTH;
        r.y2			= r.y1+CHAR_ICON_FULL_HEIGHT*ICON_GRID_HEIGHT;

        m_background->SetShader				(InventoryUtilities::GetCharIconsShader());
        m_background->SetOriginalRect		(r);
        */
    }

    m_background->TextureAvailable(true);
    m_background->TextureOn();
}

void CUIOutfitDragDropList::SetDefaultOutfit(LPCSTR default_outfit) { m_default_outfit = default_outfit; }

void CUIOutfitDragDropList::SetItem(CUICellItem* itm)
{
    if (itm)
        inherited::SetItem(itm);
    SetOutfit(itm);
}

void CUIOutfitDragDropList::SetItem(CUICellItem* itm, Fvector2 abs_pos)
{
    if (itm)
        inherited::SetItem(itm, abs_pos);
    SetOutfit(itm);
}

void CUIOutfitDragDropList::SetItem(CUICellItem* itm, Ivector2 cell_pos)
{
    if (itm)
        inherited::SetItem(itm, cell_pos);
    SetOutfit(itm);
}

CUICellItem* CUIOutfitDragDropList::RemoveItem(CUICellItem* itm, bool force_root)
{
    VERIFY(!force_root);
    CUICellItem* ci = inherited::RemoveItem(itm, force_root);
    SetOutfit(NULL);
    return ci;
}

void CUIOutfitDragDropList::SetActorModelParams(bool allowed, float fov, float scale, LPCSTR legs, LPCSTR torso)
{
    m_actor_model_allowed = allowed && Core.Features.test(xrCore::Feature::old_outfit_slot_style);
    m_actor_model_fov = fov;
    m_actor_model_scale = scale;
    if (legs && legs[0])
        m_actor_model_legs = legs;
    if (torso && torso[0])
        m_actor_model_torso = torso;
    m_actor_model_key.clear();
}

bool CUIOutfitDragDropList::UpdateActorModel()
{
    CActor* actor = Actor();
    if (!m_actor_model_allowed || !g_ui3d_cell_icons || !::Render->ui_model_supported() || !actor || !actor->Visual())
    {
        m_actor_model->RemoveModel();
        m_actor_model_key.clear();
        return false;
    }

    // what the model shows: the visual (it changes with the outfit); no weapons, the actor stands unarmed whatever is in the hands
    const xr_string key = actor->cNameVisual().c_str();
    if (key == m_actor_model_key && m_actor_model->GetModel())
        return m_actor_model->GetModel()->HasVisual();
    m_actor_model_key = key;

    // a little larger than the slot: the model may grow past it (model_scale) without being cut
    const float mx = GetWidth() * 0.25f, my = GetHeight() * 0.06f;
    m_actor_model->Init(-mx, -my, GetWidth() + 2.f * mx, GetHeight() + 2.f * my);
    m_actor_model->SetStretchTexture(true);
    CUIModel& model = m_actor_model->Model();
    model.SetVisual(actor->cNameVisual().c_str(), false);
    if (!model.HasVisual())
        return false;
    model.SetProjection(false, m_actor_model_fov); // SetVisual resets the projection and the scale
    model.SetScale(m_actor_model_scale);
    model.SetHPB(Fvector{PI + m_actor_model_yaw, 0.f, 0.f}); // facing the camera, plus the wheel's turn
    // the frame of a standing figure (feet at the origin), not the visual's bounds: every outfit stands the same size at the same place
    Fbox frame;
    frame.set(-0.3f, 0.f, -0.3f, 0.3f, 1.9f, 0.3f);
    model.SetFixedFrame(frame);

    // standing unarmed as CActor does (STorsoWpn::Create for no weapon: norm_torso_5_aim_1, legs norm_idle_0); the first motion the
    // visual has
    const bool legs_ok = model.PlayCycle(m_actor_model_legs.c_str()) || model.PlayCycle("norm_idle_0") || model.PlayCycle("norm_idle_1");
    const bool torso_ok = model.PlayCycle(m_actor_model_torso.c_str()) || model.PlayCycle("norm_torso_5_aim_1") || model.PlayCycle("norm_torso_0_aim_1") ||
        model.PlayCycle("norm_torso_0_aim_0");
    if (!legs_ok && !torso_ok)
        Msg("~ [CUIOutfitDragDropList] no [%s] / [%s] in [%s]", m_actor_model_legs.c_str(), m_actor_model_torso.c_str(), actor->cNameVisual().c_str());
    return true;
}

bool CUIOutfitDragDropList::OnMouse(float x, float y, EUIMessages mouse_action)
{
    // the wheel over the actor's model turns it around the vertical (and only that)
    if ((mouse_action == WINDOW_MOUSE_WHEEL_UP || mouse_action == WINDOW_MOUSE_WHEEL_DOWN) && m_actor_model->GetModel() && m_actor_model->GetModel()->HasVisual() &&
        Core.Features.test(xrCore::Feature::old_outfit_slot_style))
    {
        Frect r;
        GetAbsoluteRect(r);
        Fvector2 cursor = GetUICursor()->GetCursorPosition();
        if (r.in(cursor))
        {
            m_actor_model_yaw += deg2rad(mouse_action == WINDOW_MOUSE_WHEEL_UP ? 15.f : -15.f);
            m_actor_model->Model().SetHPB(Fvector{PI + m_actor_model_yaw, 0.f, 0.f});
            return true;
        }
    }
    return inherited::OnMouse(x, y, mouse_action);
}

void CUIOutfitDragDropList::Draw()
{
    if (Core.Features.test(xrCore::Feature::old_outfit_slot_style))
    {
        if (UpdateActorModel())
            m_actor_model->Draw();
        else
            m_background->Draw();
    }
    else
        inherited::Draw();
}
