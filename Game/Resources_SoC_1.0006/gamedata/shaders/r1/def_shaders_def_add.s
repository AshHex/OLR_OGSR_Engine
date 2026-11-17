-- def_shaders\def_add of the level geometry (fire planes in barrels: fx\fx_fire1 and the like).
-- Their textures have an orange RGB background where alpha = 0, so the colour is added through the alpha
-- (srcalpha, one); one+one drew the whole plane as an orange rectangle.
function normal			(shader, t_base, t_second, t_detail)
	shader:begin		("base_lplanes","base_lplanes")
			: fog		(false)
			: zb 		(true,false)
			: blend		(true,blend.srcalpha,blend.one)
			: aref 		(false,0)
			: sorting	(2, true)
	shader:sampler		("s_base"):texture(t_base)
end
