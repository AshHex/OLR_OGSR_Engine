-- Порт r3\models_selflight_det_3_aref.s для R2 (DX9): модель с альфа-тестом (ноги от 1-го лица, ремни, сетки и т.п.)

function normal		(shader, t_base, t_second, t_detail)
	shader:begin	("deffer_model_flat","deffer_base_aref_flat")
			: fog		(false)
			: zb		(true,true)
	shader:sampler	("s_base")      :texture	(t_base)
end

function l_point	(shader, t_base, t_second, t_detail)
	shader:begin	("shadow_direct_model_aref","shadow_direct_base_aref")
			: fog		(false)
			: zb		(true,true)
			: aref		(true,200)
	shader:sampler	("s_base")      :texture	(t_base)
end
