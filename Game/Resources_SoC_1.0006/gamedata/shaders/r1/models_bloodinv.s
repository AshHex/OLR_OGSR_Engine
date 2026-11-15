-- Invisible bloodsucker (port of r3\models_bloodinv.s): writes neither color nor depth.
function normal		(shader, t_base, t_second, t_detail)
	shader:begin	("model_def_lqs","simple")
			: fog		(false)
			: zb		(false,false)
			: blend		(true,blend.zero,blend.one)
			: sorting	(2,false)
	shader:sampler	("s_base")      :texture	(t_base)
end
