-- Световой конус: base_lplanes складывал чёрный RGB текстуры (lights\lights_cone) - луч был невидим; lightcone.ps берёт форму из альфы.
function normal   (shader, t_base, t_second, t_detail)
  shader:begin    ("model_def_lplanes","lightcone")
      : fog       (false)
      : zb        (true,false)
      : blend     (true,blend.one,blend.one)
      : aref      (true,0)
      : sorting   (2, false)
	shader:dx10texture	("s_base", t_base)
	shader:dx10texture	("s_env", "sky\\sky_cube_cone")

	shader: dx10sampler ("smp_base")
	shader: dx10sampler ("smp_rtlinear")
end