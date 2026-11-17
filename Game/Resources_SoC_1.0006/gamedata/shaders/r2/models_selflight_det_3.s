-- Порт r3\models_selflight_det_3.s для R2 (DX9).
-- Используется моделями ног от 1-го лица (actors\stalker_hero\stalker_hero_legs\*) и некоторыми HUD-моделями.
-- В r3 это обычная (не emissive) отложенная модель со стенсилом 1, поэтому здесь - тоже обычная модель.

-- G-buffer
function normal		(shader, t_base, t_second, t_detail)
	shader:begin	("deffer_model_flat","deffer_base_flat")
			: fog		(false)
			: zb		(true,true)
	shader:sampler	("s_base")      :texture	(t_base)
end

-- l_point (карта теней) намеренно не задан: ноги от 1-го лица не должны отбрасывать
-- тень (её уже даёт модель игрока), как и в r3/r4.
