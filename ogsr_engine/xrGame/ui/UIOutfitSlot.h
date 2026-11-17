#pragma once
#include "UIDragDropListEx.h"

class CUIOutfitDragDropList : public CUIDragDropListEx
{
    typedef CUIDragDropListEx inherited;
    CUIStatic* m_background;
    shared_str m_default_outfit;
    void SetOutfit(CUICellItem* itm);

    // the actor's 3D model in place of the outfit picture (old_outfit_slot_style): the actor's visual (it changes with the outfit) standing idle,
    // unarmed; the mouse wheel over it turns it around the vertical. Made again when the
    // visual changes (m_actor_model_key); without a model (another renderer, ui3d_cell_icons 0) the picture
    CUIStatic* m_actor_model;
    xr_string m_actor_model_key;
    bool m_actor_model_allowed{true};
    float m_actor_model_fov{20.f};
    float m_actor_model_scale{1.1f};
    float m_actor_model_yaw{}; // the wheel's turn
    shared_str m_actor_model_legs{"norm_idle_0"}, m_actor_model_torso{"norm_torso_5_aim_1"};

    bool UpdateActorModel();

public:
    CUIOutfitDragDropList();
    virtual ~CUIOutfitDragDropList();

    virtual void SetItem(CUICellItem* itm); // auto
    virtual void SetItem(CUICellItem* itm, Fvector2 abs_pos); // start at cursor pos
    virtual void SetItem(CUICellItem* itm, Ivector2 cell_pos); // start at cell
    virtual CUICellItem* RemoveItem(CUICellItem* itm, bool force_root);
    virtual void Draw();
    virtual bool OnMouse(float x, float y, EUIMessages mouse_action);
    void SetDefaultOutfit(LPCSTR default_outfit);

    // dragdrop_outfit: model = 0/1, model_fov (degrees), model_scale, model_legs_anim, model_torso_anim
    void SetActorModelParams(bool allowed, float fov, float scale, LPCSTR legs, LPCSTR torso);
};
