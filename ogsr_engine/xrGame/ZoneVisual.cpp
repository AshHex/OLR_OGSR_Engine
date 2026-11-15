#include "stdafx.h"
#include "CustomZone.h"
#include "..\Include/xrRender/KinematicsAnimated.h"
#include "../Include/xrRender/RenderVisual.h"
#include "ZoneVisual.h"
#include "xrServer_Objects_ALife_Monsters.h"

CVisualZone::CVisualZone() {}
CVisualZone::~CVisualZone() {}

BOOL CVisualZone::net_Spawn(CSE_Abstract* DC)
{
    BOOL ret = inherited::net_Spawn(DC);
    CSE_Abstract* e = (CSE_Abstract*)(DC);
    CSE_ALifeZoneVisual* Z = smart_cast<CSE_ALifeZoneVisual*>(e);
    IKinematicsAnimated* SA = smart_cast<IKinematicsAnimated*>(Visual());
    // У зон, заспавненных скриптом (alife():create), имена анимаций в пакете пустые - раньше это был вылет
    // "MODEL: can't find cycle". Берём анимации модели по умолчанию.
    m_attack_animation = SA->ID_Cycle_Safe(Z->attack_animation);
    m_idle_animation = SA->ID_Cycle_Safe(Z->startup_animation);
    if (!m_idle_animation.valid())
        m_idle_animation = SA->ID_Cycle_Safe("idle");
    if (!m_attack_animation.valid())
        m_attack_animation = m_idle_animation;
    if (m_idle_animation.valid())
        SA->PlayCycle(m_idle_animation);
    else
        Msg("! [%s] zone [%s]: no idle animation [%s] in visual", __FUNCTION__, cName().c_str(), Z->startup_animation.c_str());
    setVisible(TRUE);
    return ret;
}
void CVisualZone::net_Destroy() { inherited::net_Destroy(); }
void CVisualZone::AffectObjects()
{
    inherited::AffectObjects();
    //	smart_cast<IKinematicsAnimated*>(Visual())->PlayCycle(*m_attack_animation);
}
void CVisualZone::SwitchZoneState(EZoneState new_state)
{
    if (m_eZoneState == eZoneStateBlowout && new_state != eZoneStateBlowout && m_idle_animation.valid())
    {
        //	IKinematicsAnimated*	SA=smart_cast<IKinematicsAnimated*>(Visual());
        smart_cast<IKinematicsAnimated*>(Visual())->PlayCycle(m_idle_animation);
    }

    inherited::SwitchZoneState(new_state);
}
void CVisualZone::Load(LPCSTR section)
{
    inherited::Load(section);
    m_dwAttackAnimaionStart = pSettings->r_u32(section, "attack_animation_start");
    m_dwAttackAnimaionEnd = pSettings->r_u32(section, "attack_animation_end");
    VERIFY2(m_dwAttackAnimaionStart < m_dwAttackAnimaionEnd, "attack_animation_start must be less then attack_animation_end");
}

void CVisualZone::UpdateBlowout()
{
    inherited::UpdateBlowout();
    if (m_dwAttackAnimaionStart >= (u32)m_iPreviousStateTime && m_dwAttackAnimaionStart < (u32)m_iStateTime && m_attack_animation.valid())
        smart_cast<IKinematicsAnimated*>(Visual())->PlayCycle(m_attack_animation);

    if (m_dwAttackAnimaionEnd >= (u32)m_iPreviousStateTime && m_dwAttackAnimaionEnd < (u32)m_iStateTime && m_idle_animation.valid())
        smart_cast<IKinematicsAnimated*>(Visual())->PlayCycle(m_idle_animation);
}