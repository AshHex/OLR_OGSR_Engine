#ifndef ParticleCustom_included
#define ParticleCustom_included
#pragma once

class IParticleCustom
{
public:
    virtual ~IParticleCustom() { ; }

    virtual void OnDeviceCreate() = 0;
    virtual void OnDeviceDestroy() = 0;

    virtual void UpdateParent(const Fmatrix& m, const Fvector& velocity, BOOL bXFORM) = 0;
    virtual void OnFrame(u32 dt) = 0;

    virtual void Play() = 0;
    virtual void Stop(BOOL bDefferedStop = TRUE) = 0;
    virtual BOOL IsPlaying() = 0;

    virtual u32 ParticlesCount() = 0;

    // box (in the effect's own space, before the parent transform) of the particles that are visible: colour x alpha at least
    // min_visibility, grown by half their size; false when none are. 3D models in UI frame particle effects by it
    virtual bool GetVisibleBox(Fbox& box, float min_visibility) { return false; }

    // particles bounce off the level's static geometry (effects with collision): on by default. 3D models in UI turn it off: their effect
    // lives at the origin of its own small world, the level there is unrelated (and in the main menu there is none). A group passes it to
    // its effects, an effect to the child effects it starts
    virtual void SetWorldCollision(bool on) {}

    virtual float GetTimeLimit() = 0;
    virtual BOOL IsLooped() { return GetTimeLimit() < 0.f; }

    virtual const shared_str Name() = 0;
    virtual void SetHudMode(BOOL b) = 0;
    virtual BOOL GetHudMode() = 0;
};

#endif //	ParticleCustom_included