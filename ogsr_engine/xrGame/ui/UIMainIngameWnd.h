// UIMainIngameWnd.h:  окошки-информация в игре
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "UIProgressBar.h"
#include "UIGameLog.h"

#include "../alife_space.h"

#include "UICarPanel.h"
#include "UIMotionIcon.h"
#include "../hudsound.h"
#include "../script_export_space.h"

struct GAME_NEWS_DATA;

class CUIPdaMsgListItem;
class CLAItem;
class CUIZoneMap;
class CUIArtefactPanel;
class CUIScrollView;
class CActor;
class CWeapon;
class CMissile;
class CInventoryItem;

class CUIMainIngameWnd : public CUIWindow
{
public:
    CUIMainIngameWnd();
    virtual ~CUIMainIngameWnd();

    virtual void Init();
    virtual void Draw();
    virtual void Update();

    bool OnKeyboardPress(int dik);

protected:
    //CUIStatic UIStaticDiskIO;
    CUIStatic UIStaticHealth;
    CUIStatic UIStaticArmor;
    CUIStatic UIStaticRadiation;
    CUIStatic UIStaticPsyHealth;
    CUIStatic UIStaticQuickHelp;
    CUIProgressBar UIHealthBar;
    CUIProgressBar UIArmorBar;
    CUIProgressBar UIRadiaitionBar;
    CUIProgressBar UIPsyHealth;
    CUIStatic UIStaticTime;
    CUIStatic UIStaticTimeString;
    CUICarPanel UICarPanel;
    CUIMotionIcon UIMotionIcon;
    CUIZoneMap* UIZoneMap;

    //иконка, показывающая количество активных PDA
    CUIStatic UIPdaOnline;

    //изображение оружия
    CUIStatic UIWeaponBack;
    CUIStatic UIWeaponSignAmmo;
    // optional (SOP HUD styles): name of the active item, ammo type and fire mode on the weapon panel
    CUIStatic* UIWeaponName{};
    CUIStatic* UIAmmoName{};
    CUIStatic* UIFireMode{};
    // SOP HUDs place static_firemode on top of static_ammo: the mode is then appended to the ammo count
    bool m_bFireModeInAmmo{};
    CUIStatic UIWeaponIcon;

    Frect UIWeaponIcon_rect;
    // static_wpn_icon with scale_x (SOP): the icon keeps its aspect inside UIWeaponIcon_rect scaled by m_WpnIconScale, left aligned and
    // centred vertically; the weapon's own icon unless use_ammo="1". Without it: the ammo icon in the old SoC size
    bool m_bWpnIconFit{}, m_bWpnIconUseAmmo{true};
    Fvector2 m_WpnIconScale{1.f, 1.f};
    // the 3D model of the icon (as the inventory cells: g_ui3d_cell_icons), over UIWeaponIcon's rect; UIWeaponIcon hidden while it shows
    CUIStatic UIWeaponIcon3D;
    u64 m_wpn_icon3d_key{};
    bool m_wpn_icon3d_failed{};
    bool UpdateWeaponIcon3D(class CInventoryItem* item, const shared_str& sect);
    void HideWeaponIcon();

public:
    CUIStatic* GetPDAOnline() { return &UIPdaOnline; };
    CUIZoneMap* GetUIZoneMap() { return UIZoneMap; }

protected:
    // 5 статиков для отображения иконок:
    // - сломанного оружия
    // - радиации
    // - ранения
    // - голода
    // - усталости
    CUIStatic UIWeaponJammedIcon;
    CUIStatic UIRadiaitionIcon;
    CUIStatic UIWoundIcon;
    CUIStatic UIStarvationIcon;
    CUIStatic UIPsyHealthIcon;
    CUIStatic UIStaminaIcon;
    CUIStatic UIInvincibleIcon;
    CUIStatic UIThirstIcon;
    //	CUIStatic			UISleepIcon;
    //	CUIStatic			UIArtefactIcon;

    CUIScrollView* m_UIIcons{};

public:
    CUIArtefactPanel* m_artefactPanel;

public:
    // Енумы соответсвующие предупреждающим иконкам
    enum EWarningIcons
    {
        ewiAll = 0,
        ewiWeaponJammed,
        ewiRadiation,
        ewiWound,
        ewiStarvation,
        ewiPsyHealth,
        ewiStamina,
        ewiInvincible,
        ewiThirst,
        //		ewiSleep,
        //		ewiArtefact,
    };

    // Задаем цвет соответствующей иконке
    void SetWarningIconColor(EWarningIcons icon, const u32 cl);
    void TurnOffWarningIcon(EWarningIcons icon);

    // Пороги изменения цвета индикаторов, загружаемые из system.ltx
    typedef xr_map<EWarningIcons, xr_vector<float>> Thresholds;
    typedef Thresholds::iterator Thresholds_it;
    Thresholds m_Thresholds;

    // Енум перечисления возможных мигающих иконок
    enum EFlashingIcons
    {
        efiPdaTask = 0,
        efiMail
    };

    void SetFlashIconState_(EFlashingIcons type, bool enable);

    void AnimateContacts(bool b_snd);
    HUD_SOUND m_contactSnd;

    void ReceiveNews(GAME_NEWS_DATA* news);

protected:
    void SetWarningIconColor(CUIStatic* s, const u32 cl);
    void InitFlashingIcons(CUIXml* node);
    void DestroyFlashingIcons();
    void UpdateFlashingIcons();
    void UpdateActiveItemInfo();

    void SetAmmoIcon(const shared_str& seсt_name, class CInventoryItem* item = nullptr);

    // first - иконка, second - анимация
    DEF_MAP(FlashingIcons, EFlashingIcons, CUIStatic*);
    FlashingIcons m_FlashingIcons;

    //для текущего активного актера и оружия
    CActor* m_pActor;
    CWeapon* m_pWeapon;
    CMissile* m_pGrenade;
    CInventoryItem* m_pItem;

    // Отображение подсказок при наведении прицела на объект
    void RenderQuickInfos();

public:
    CUICarPanel& CarPanel() { return UICarPanel; };
    CUIMotionIcon& MotionIcon() { return UIMotionIcon; }
    void OnConnected();
    void reset_ui();

protected:
    CInventoryItem* m_pPickUpItem;
    CUIStatic UIPickUpItemIcon;

    float m_iPickUpItemIconX{};
    float m_iPickUpItemIconY{};
    float m_iPickUpItemIconWidth{};
    float m_iPickUpItemIconHeight{};

    void UpdatePickUpItem();

public:
    void SetPickUpItem(CInventoryItem* PickUpItem);

    DECLARE_SCRIPT_REGISTER_FUNCTION
};

add_to_type_list(CUIMainIngameWnd)
#undef script_type_list
#define script_type_list save_type_list(CUIMainIngameWnd)
