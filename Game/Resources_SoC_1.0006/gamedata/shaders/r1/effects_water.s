-- R1 water. effects\water from shaders.xr is a plain S_SET blender and draws the surface of these levels black,
-- so R1 gets its own water: port of r2\effects_water.s without soft water (r1\water.vs, r1\water.ps).
local tex_base	= "water\\water_water"
local tex_nmap	= "water\\water_normal"
local tex_env0	= "$user$sky0"
local tex_env1	= "$user$sky1"

function normal		(shader, t_base, t_second, t_detail)
	shader:begin	("water","water")
			: sorting	(2, false)
			: blend		(true,blend.srcalpha,blend.invsrcalpha)
			: zb		(true,false)
			: fog		(true)
	shader:sampler	("s_base")	:texture	(tex_base)
	shader:sampler	("s_nmap")	:texture	(tex_nmap)
	shader:sampler	("s_env0")	:texture	(tex_env0)	: clamp()
	shader:sampler	("s_env1")	:texture	(tex_env1)	: clamp()
end
