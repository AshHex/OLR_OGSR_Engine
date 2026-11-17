-- Water on lightmapped level geometry: pools that the level has as def_shaders\def_trans (lvl2_garbage), see [texture_aliases]
-- in shader_aliases.ltx. effects_water.s with the lightmapped vertex (water_lmap.vs) and no distortion pass (waterd_soft.vs
-- reads the vertex colour)
local tex_base = "water\\water_water"
local tex_nmap = "water\\water_normal"
local tex_env0 = "$user$sky0"
local tex_env1 = "$user$sky1"
local tex_leaves = "water\\water_foam"

function normal(shader, t_base, t_second, t_detail)
	shader:begin("water_lmap", "water_soft")
		:sorting(2, false)
		:blend(true, blend.srcalpha, blend.invsrcalpha)
		:zb(true, false)
		:distort(false)
		:fog(true)
	shader:sampler("s_base"):texture(tex_base)
	shader:sampler("s_nmap"):texture(tex_nmap)
	shader:sampler("s_env0"):texture(tex_env0):clamp()
	shader:sampler("s_env1"):texture(tex_env1):clamp()
	shader:sampler("s_position"):texture("$user$position"):f_none()
	shader:sampler("s_leaves"):texture(tex_leaves):wrap():f_anisotropic()
end