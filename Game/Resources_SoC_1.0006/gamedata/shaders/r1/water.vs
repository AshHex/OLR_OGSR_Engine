// R1 water (port of r2\water.vs without soft water): base texture, two scrolling normal maps, sky reflection.
#include "common.h"
#include "shared\waterconfig.h"
#include "shared\watermove.h"

struct vf
{
	float4	hpos	: POSITION;
	float2	tbase	: TEXCOORD0;
	float2	tnorm0	: TEXCOORD1;
	float2	tnorm1	: TEXCOORD2;
	float3	v2point	: TEXCOORD3;
	float3	c0		: COLOR0;
	float	fog		: FOG;
};

#ifdef WATER_LMAP
// Lightmapped level geometry (water_lmap.vs, effects\water_lmap): no vertex colour, the level's lighting is in the lightmap,
// so the water is lit by the open sky (hemi + sun + ambient), like r1\lmap.vs before its lightmap.
struct v_water
{
	float4	P	: POSITION;
	float4	N	: NORMAL;
	float4	T	: TANGENT;
	float4	B	: BINORMAL;
	float2	uv	: TEXCOORD0;
};
#else
#define v_water v_vert
#endif

vf main (v_water v)
{
	vf		o;
	float4	P	= watermove(v.P);
	float3	N	= unpack_normal(v.N);

	o.v2point	= P.xyz - eye_position;
	o.tbase		= unpack_tc_base(v.uv, v.T.w, v.B.w);
	o.tnorm0	= watermove_tc(o.tbase*W_DISTORT_BASE_TILE_0, P.xz, W_DISTORT_AMP_0);
	o.tnorm1	= watermove_tc(o.tbase*W_DISTORT_BASE_TILE_1, P.xz, W_DISTORT_AMP_1);

#ifdef WATER_LMAP
	o.c0		= v_hemi(N) + v_sun(N) + L_ambient;
#else
	float3	L_rgb	= v.color.xyz;
	float3	L_hemi	= v_hemi(N)*v.N.w;
	float3	L_sun	= v_sun(N)*v.color.w;
	o.c0		= L_rgb + L_hemi + L_sun + L_ambient;
#endif

#ifdef RETRO_MODE
	o.hpos = snap_to_position(mul(m_VP, P));
#else
	o.hpos = mul(m_VP, P);
#endif
	o.fog		= calc_fogging(v.P);
	return o;
}
