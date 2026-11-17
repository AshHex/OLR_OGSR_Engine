#pragma once
#include "uiwindow.h"

class CInventoryItem;
class CUIStatic;
class CUIScrollView;
class CUIProgressBar;
class CUIWpnParams;
class CUIArtefactParams;
class CPhysicsShellHolder;

// extern const char * const 		fieldsCaptionColor;

class CUIItemInfo : public CUIWindow
{
private:
    typedef CUIWindow inherited;
    struct _desc_info
    {
        CGameFont* pDescFont;
        u32 uDescClr;
        bool bShowDescrText;
    } m_desc_info{};
    CInventoryItem* m_pInvItem;

public:
    CUIItemInfo();
    virtual ~CUIItemInfo();

    void Init(float x, float y, float width, float height, LPCSTR xml_name);
    void Init(LPCSTR xml_name);
    void InitItem(CInventoryItem* pInvItem);

    void TryAddWpnInfo(CPhysicsShellHolder& obj);
    void TryAddArtefactInfo(const shared_str& af_section);
    void TryAddCustomInfo(CPhysicsShellHolder& obj);

    virtual void Draw();
    bool m_b_force_drawing;
    CUIStatic* UIName;
    CUIStatic* UIWeight;
    CUIStatic* UICost;
    CUIStatic* UICondition;
    CUIScrollView* UIDesc;
    CUIProgressBar* UICondProgresBar;
    CUIWpnParams* UIWpnParams;
    CUIArtefactParams* UIArtefactParams;

    Fvector2 UIItemImageSize;
    CUIStatic* UIItemImage;

    // the item's 3D model in place of the 2D icon (image_static: model = 1, model_rotation_speed = degrees per second around the vertical, model_scale);
    // when the 3D icons are on (ui3d_cell_icons, [ui3d_cell_icons] enabled / enabled_for_weapons) and the renderer draws models
    CUIStatic* UIItemModel{};

private:
    float m_model_spin{40.f};
    float m_model_scale{1.25f}; // image_static: model_scale, the window of the model to the 2D icon's size
    bool m_model_allowed{true};
    u64 m_model_key{}; // what the model shows (the item, its addons and bones): made again when it changes
    bool m_model_failed{}; // no model for that key: the 2D icon

    bool UpdateItemModel();
    u64 ItemModelKey() const;
};