-- 3D models in UI (r4_ui_models.cpp): supersampled tile -> atlas page
function element_0(shader, t_base, t_second, t_detail)
	shader:begin("ui3d_quad", "ui3d_resolve")
		:fog(false)
		:zb(false, false)
	shader:dx10texture("s_ui3d_lit", "$user$ui3d_lit")
	shader:dx10texture("s_ui3d_position", "$user$ui3d_position")
	shader:dx10sampler("smp_nofilter")
end
