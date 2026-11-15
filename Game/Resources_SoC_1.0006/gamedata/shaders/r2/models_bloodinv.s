-- Invisible bloodsucker (port of r3\models_bloodinv.s): forward pass that writes neither color nor depth.
function normal		(shader, t_base, t_second, t_detail)
	shader:begin	("dumb","dumb")
			: fog		(false)
			: zb		(false,false)
			: blend		(true,blend.zero,blend.one)
			: aref		(false,0)
			: sorting	(2,false)
	shader:sampler	("s_base")      :texture	(t_base)
end
