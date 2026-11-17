-- 3D models in UI: a static drawing its tile of the atlas (premultiplied alpha), point sampled: the tile has the static's size on the screen
-- (a filtered sample at a fractional position blurred it by half a pixel)
function normal(shader, t_base, t_second, t_detail)
	shader:begin("stub_notransform_t", "hud_ui3d")
		:fog(false)
		:zb(false, false)
		:blend(true, blend.one, blend.invsrcalpha)
	shader:dx10texture("s_base", t_base)
	shader:dx10texture("s_mask", t_base .. "_mask") -- the page's outline mask (r4_ui_models.cpp)
	shader:dx10sampler("smp_nofilter")
end
