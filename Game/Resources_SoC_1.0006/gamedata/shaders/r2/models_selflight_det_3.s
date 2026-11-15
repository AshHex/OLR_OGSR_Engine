-- ѕорт r3\models_selflight_det_3.s дл€ R2 (DX9).
-- »спользуетс€ модел€ми ног от 1-го лица (actors\stalker_hero\stalker_hero_legs\*) и некоторыми HUD-модел€ми.
-- ¬ r3 это обычна€ (не emissive) отложенна€ модель со стенсилом 1, поэтому здесь - тоже обычна€ модель.

-- G-buffer
function normal		(shader, t_base, t_second, t_detail)
	shader:begin	("deffer_model_flat","deffer_base_flat")
			: fog		(false)
			: zb		(true,true)
	shader:sampler	("s_base")      :texture	(t_base)
end

-- карта теней
function l_point	(shader, t_base, t_second, t_detail)
	shader:begin	("shadow_direct_model","shadow_direct_base")
			: fog		(false)
			: zb		(true,true)
	shader:sampler	("s_base")      :texture	(t_base)
end
