-- ѕорт r3\models_selflight_det_3.s дл€ R1 (DX9, forward).
-- »спользуетс€ модел€ми ног от 1-го лица (actors\stalker_hero\stalker_hero_legs\*) и некоторыми HUD-модел€ми.
-- ѕовтор€ет C++ блендер models\model рендера R1 (CBlender_Model).

function normal		(shader, t_base, t_second, t_detail)
	shader:begin	("model_def_hq","model_def_hq")
			: fog		(true)
			: zb		(true,true)
	shader:sampler	("s_base")      :texture	(t_base)
	shader:sampler	("s_lmap")      :texture	("$user$projector")
			: clamp		()
			: f_linear	()
			: project	(true)
end

function l_point	(shader, t_base, t_second, t_detail)
	r1_lpoint	(shader, t_base, "model_def_point")
end

function l_spot		(shader, t_base, t_second, t_detail)
	r1_lspot	(shader, t_base, "model_def_spot")
end

-- проецируемые тени от моделей (SE_R1_LMODELS)
function l_special	(shader, t_base, t_second, t_detail)
	shader:begin	("model_def_shadow","model_shadow")
			: fog		(false)
			: zb		(false,false)
			: blend		(true,blend.zero,blend.srccolor)
end
