#pragma once

#include "UIStatic.h"
#include "UIDialogWnd.h"

class CUIDragItem;
class CUIDragDropListEx;
class CUICellItem;
class CUIProgressBar;
class CUIModel;

namespace cell_settings_db
{
struct db
{
    const u32 ClrAmmo{READ_IF_EXISTS(pSettings, r_color, "dragdrop", "color_ammo", color_argb(255, 0, 128, 0))};
    const u32 ClrEquipped{READ_IF_EXISTS(pSettings, r_color, "dragdrop", "color_equipped", color_argb(255, 255, 225, 0))};
    const u32 ClrUntradable{READ_IF_EXISTS(pSettings, r_color, "dragdrop", "color_untradable", color_argb(255, 124, 0, 0))};

    // the cell colours (ammo of the focused weapon, untradable, equipped) tint 3D icons too; off: their outline shows it
    const bool Colorize3DIcons{!!READ_IF_EXISTS(pSettings, r_bool, "dragdrop", "colorize_3d_icons", false)};
    // the grid under 3D icons is highlighted as under 2D ones; off: their outline shows it
    const bool SelectGridFor3DIcons{!!READ_IF_EXISTS(pSettings, r_bool, "dragdrop", "select_grid_for_3d_icons", false)};
};
const db& get();
} // namespace cell_settings_db

// [ui3d_cell_icons] of the configs (external.ltx), read once; without the section the defaults below. Every renderer draws them; one that
// cannot (IRender_interface::ui_model_supported false) keeps the 2D icons
//   enabled = true, enabled_for_weapons = true, enabled_for_ammo = true, tilt = 0 (degrees of the icon's turn for depth)
//   outline_default = r, g, b, a (0..255), width, softness (screen pixels, up to 20; softness 1: sharp line, more: glow); width 0: none
//   outline_focused / outline_selected / outline_untradable / outline_equipped: a highlighted cell's own outline, else the default one
struct ui3d_cell_icons_settings
{
    struct outline
    {
        u32 color{};
        float width{};
        float softness{1.f};
        bool set{}; // the line is in the config (or a default)
    };

    bool enabled{true};
    bool weapons{true}; // weapons too (their world model with the attached addons); false: their 2D icon with the addon icons
    bool ammo{true};
    float tilt{0.f}; // degrees of the icon's turn for depth (CUIModel::OrientIcon)
    outline def{color_argb(230, 175, 180, 190), 1.5f, 1.f, true}; // outline_default: every 3D icon
    outline outlines[4]{{color_argb(255, 0, 94, 128), 5.f, 1.f, true}, // by CUICellItem::HighlightState; a state without one: the default
                        {cell_settings_db::get().ClrAmmo, 10.f, 10.f, true},
                        {cell_settings_db::get().ClrUntradable, 10.f, 10.f, true},
                        {cell_settings_db::get().ClrEquipped, 10.f, 10.f, true}};

    static constexpr LPCSTR outline_names[4]{"outline_focused", "outline_selected", "outline_untradable", "outline_equipped"};
};

ui3d_cell_icons_settings& ui3d_cell_icons();

class ICustomDrawCell
{
public:
    virtual ~ICustomDrawCell(){};
    virtual void OnDraw(CUICellItem* cell) = 0;
};

class CUICellItem : public CUIStatic
{
private:
    typedef CUIStatic inherited;

protected:
    xr_vector<CUICellItem*> m_childs;

    CUIDragDropListEx* m_pParentList;
    Ivector2 m_grid_size;
    Fvector2 m_cell_size;
    ICustomDrawCell* m_custom_draw;
    int m_accelerator;
    virtual void UpdateItemText();

    CUIProgressBar* m_pConditionState;
    bool m_condition_auto_width;
    CUIStatic* m_text;
    void init();

    // 3D icon (ported from OGSR_Engine_private): the item's model in place of its 2D icon when [ui3d_cell_icons] enabled (the default
    // without the section), the console ui3d_cell_icons is on, the renderer draws models, the class allows it (Allows3DIcon) and the
    // section too (ui3d_icon, default true), and its model exists; highlighted cells get an outline ([ui3d_cell_icons] outline_* lines)
    shared_str m_icon3d_section; // the item section the state below is for
    bool m_icon3d_allowed{true};
    bool m_icon3d_failed{}; // the section's model is missing: the 2D icon
    bool m_icon3d_vertical{}; // the model was oriented for a vertical list
    u64 m_icon3d_state{}; // Icon3DState the model was made for
    u32 m_icon3d_gen{}; // g_ui3d_icons_gen the model was made for
    float m_icon3d_base_scale{1.f}; // the model's own scale (ui3d_icon_scale)
    float m_icon3d_cell_scale{1.f}; // the item's ui3d_cell_scale (cells only; times console ui3d_cell_icons_scale)
    void Update3DIcon();
    // back to the item's 2D icon (the model removed)
    void Restore2DIcon();
    // the section of the cell's item (empty: no item)
    shared_str Icon3DSection() const;
    // the cell's model: the section's (SetFromSection); a weapon its world model with the attached addons
    virtual void Setup3DModel(CUIModel& m);
    // what the model shows besides the section (a weapon's addons and magazine): a change makes the model again
    virtual u64 Icon3DState() { return 0; }
    // the highlight of the cell: 0 focused (under the cursor), 1 selected (ammo / addons of the focused weapon), 2 untradable, 3 equipped,
    // -1 none
    int HighlightState() const;

public:
    // the cell shows its item's 3D model (or will, once made)
    bool Is3DIcon() const;
    // the cell may show the item's 3D model
    virtual bool Allows3DIcon() const;

    CUICellItem();
    virtual ~CUICellItem();

    virtual bool OnKeyboard(int dik, EUIMessages keyboard_action);
    virtual bool OnMouse(float x, float y, EUIMessages mouse_action);
    virtual void Draw();
    virtual void Update();

    virtual void OnAfterChild(CUIDragDropListEx* parent_list){};

    u32 ChildsCount();
    void PushChild(CUICellItem*);
    CUICellItem* PopChild();
    CUICellItem* Child(u32 idx) { return m_childs[idx]; };
    bool HasChild(CUICellItem* item);
    virtual bool EqualTo(CUICellItem* itm);
    IC const Ivector2& GetGridSize() { return m_grid_size; }; // size in grid
    IC void SetAccelerator(int dik) { m_accelerator = dik; };
    IC int GetAccelerator() const { return m_accelerator; };

    virtual CUIDragItem* CreateDragItem();

    CUIDragDropListEx* OwnerList() { return m_pParentList; }
    static CUICellItem* m_mouse_selected_item;
    void SetOwnerList(CUIDragDropListEx* p);
    void SetCustomDraw(ICustomDrawCell* c);
    void* m_pData;
    int m_index;
    bool m_b_already_drawn;
    bool m_b_destroy_childs;
    void ColorizeItems(std::initializer_list<CUIDragDropListEx*>);
    void UpdateConditionProgressBar();
    bool m_selected;
    bool m_select_armament;
    bool m_select_equipped;
    bool m_select_untradable;
};

class CUIDragItem : public CUIWindow, public pureRender, public pureFrame
{
private:
    typedef CUIWindow inherited;
    CUIStatic m_static;
    CUICellItem* m_pParent;
    Fvector2 m_pos_offset;
    CUIDragDropListEx* m_back_list;

public:
    CUIDragItem(CUICellItem* parent);
    virtual void Init(const ui_shader& sh, const Frect& rect, const Frect& text_rect);
    virtual ~CUIDragItem();
    CUIStatic* wnd() { return &m_static; }
    virtual bool OnMouse(float x, float y, EUIMessages mouse_action);
    virtual void Draw();
    virtual void OnRender();
    virtual void OnFrame();
    CUICellItem* ParentItem() { return m_pParent; }
    void SetBackList(CUIDragDropListEx* l);
    CUIDragDropListEx* BackList() { return m_back_list; }
    Fvector2 GetPosition();
    virtual bool CapturesFocusToo() { return false; };
};
