#pragma once

#include "UIStatic.h"
#include "UIModel.h"

// A static that shows a 3D model (CUIModel, drawn as the static's texture) with mouse control: drag rotates (+-30 degrees, free with
// g_3d_static_debug), shift + wheel zooms. The model API is forwarded for scripts (ported from NS_OGSR_Engine)
class CUI3dStatic : public CUIStatic
{
    typedef CUIStatic inherited;

public:
    CUI3dStatic() { Model(); }

    void Update() override;
    bool OnMouse(float x, float y, EUIMessages mouse_action) override;

    void SetScale(float s) { Model().SetScale(s); }
    float GetScale() { return Model().GetScale(); }
    void SetZOffset(float z) { Model().SetZOffset(z); }
    float GetZOffset() { return Model().GetZOffset(); }

    void SetFromSection(LPCSTR section) { Model().SetFromSection(section); }
    void SetFromSection(LPCSTR section, LPCSTR section_line) { Model().SetFromSection(section, section_line); }
    void SetVisual(LPCSTR path, bool pm) { Model().SetVisual(path, pm); }
    void SetAnimation(LPCSTR anim) { Model().SetAnimation(anim); }

    void AddAttach(LPCSTR visual, LPCSTR bone, Fvector pos, Fvector rot, Fvector scale) { Model().AddAttach(visual, bone, pos, rot, scale); }
    void AddWeaponAttach(LPCSTR section, bool strapped) { Model().AddWeaponAttach(section, strapped); }

    void SetXYZ(const Fvector& xyz) { Model().SetXYZ(xyz); }
    void SetHPB(const Fvector& hpb) { Model().SetHPB(hpb); }
    Fvector GetXYZ() { return Model().GetXYZ(); }
    Fvector GetHPB() { return Model().GetHPB(); }

    void SetRotationPerFrame(Fvector rot) { Model().SetRotationPerFrame(rot); }
    Fvector GetRotationPerFrame() { return Model().GetRotationPerFrame(); }

    void SetAnimated(bool animated) { Model().SetAnimated(animated); }
    bool IsAnimated() { return Model().IsAnimated(); }
    void SetProjection(bool ortho, float fov_deg) { Model().SetProjection(ortho, fov_deg); }
    bool IsOrtho() { return Model().IsOrtho(); }
    void Invalidate() { Model().Invalidate(); }
    void SetParticleWarmup(float seconds) { Model().SetParticleWarmup(seconds); }
    void SetParticleReplay(bool on) { Model().SetParticleReplay(on); }
    void SetZoomRange(float min_zoom, float max_zoom) { Model().SetZoomRange(min_zoom, max_zoom); }

    void AllowMouseControl(bool allow) { m_allowMouseControl = allow; }
    bool MouseControlAllowed() const { return m_allowMouseControl; }

private:
    bool m_allowMouseControl{};
    bool m_bRotateItemActive{};
    Fvector2 m_vMousePositionPrev{};

    void SetRotating(bool rotating)
    {
        m_bRotateItemActive = rotating;
        Model().SetUserRotating(rotating);
    }
};
