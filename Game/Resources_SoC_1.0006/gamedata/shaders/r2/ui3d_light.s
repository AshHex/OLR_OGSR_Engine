-- 3D models in UI (xrRender\UIModels.cpp, R2): lights the tile's G-buffer with the fixed rig
function element_0(shader, t_base, t_second, t_detail)
	shader:begin("null", "ui3d_light")
		:fog(false)
		:zb(false, false)
	shader:sampler("s_ui3d_position"):texture("$user$ui3d_position"):clamp():f_point()
	shader:sampler("s_ui3d_normal"):texture("$user$ui3d_normal"):clamp():f_point()
	shader:sampler("s_ui3d_albedo"):texture("$user$ui3d_albedo"):clamp():f_point()
end
