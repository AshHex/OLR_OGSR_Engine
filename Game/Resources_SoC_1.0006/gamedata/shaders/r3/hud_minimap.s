-- HUD minimap with a mask (SOP zone_map.xml <mask>, xrGame\UIZoneMap.cpp): t_base - the level map, t_second - the mask picture
function normal(shader, t_base, t_second, t_detail)
	shader:begin("stub_notransform_t", "hud_minimap")
		:fog(false)
		:zb(false, false)
		:blend(true, blend.srcalpha, blend.invsrcalpha)
	shader:dx10texture("s_base", t_base)
	shader:dx10texture("s_mask", t_second)
	shader:dx10sampler("smp_rtlinear")
end
