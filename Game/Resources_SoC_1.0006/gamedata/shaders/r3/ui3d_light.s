-- 3D models in UI (r4_ui_models.cpp): lights the tile's G-buffer with the fixed rig
function element_0(shader, t_base, t_second, t_detail)
	shader:begin("ui3d_quad", "ui3d_light")
		:fog(false)
		:zb(false, false)
	shader:dx10texture("s_ui3d_position", "$user$ui3d_position")
	shader:dx10texture("s_ui3d_albedo", "$user$ui3d_albedo")
	shader:dx10sampler("smp_nofilter")
end
