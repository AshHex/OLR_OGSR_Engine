-- 3D models in UI: a static drawing its tile of the atlas (premultiplied alpha), point sampled: the tile has the static's size on the screen
function normal(shader, t_base, t_second, t_detail)
	shader:begin("null", "hud_ui3d")
		:fog(false)
		:zb(false, false)
		:blend(true, blend.one, blend.invsrcalpha)
	shader:sampler("s_base"):texture(t_base):clamp():f_point()
	shader:sampler("s_mask"):texture(t_base .. "_mask"):clamp():f_point() -- the page's outline mask (xrRender\UIModels.cpp)
end
