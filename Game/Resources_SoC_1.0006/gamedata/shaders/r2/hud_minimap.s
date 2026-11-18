-- HUD minimap with a mask (SOP zone_map.xml <mask>, xrGame\UIZoneMap.cpp): t_base - the level map, t_second - the mask picture
function normal(shader, t_base, t_second, t_detail)
	shader:begin("null", "hud_minimap")
		:fog(false)
		:zb(false, false)
		:blend(true, blend.srcalpha, blend.invsrcalpha)
	shader:sampler("s_base"):texture(t_base):clamp():f_linear()
	shader:sampler("s_mask"):texture(t_second):clamp():f_linear()
end
