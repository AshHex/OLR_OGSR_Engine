-- 3D models in UI (xrRender\UIModels.cpp, R2): supersampled tile -> atlas page
function element_0(shader, t_base, t_second, t_detail)
	shader:begin("null", "ui3d_resolve")
		:fog(false)
		:zb(false, false)
	shader:sampler("s_ui3d_lit"):texture("$user$ui3d_lit"):clamp():f_point()
	shader:sampler("s_ui3d_position"):texture("$user$ui3d_position"):clamp():f_point()
end
