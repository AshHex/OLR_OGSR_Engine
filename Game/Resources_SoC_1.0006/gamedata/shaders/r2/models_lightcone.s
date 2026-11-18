-- Световой конус (прожектор на Кордоне и т.п.): аддитивно (one,one как в r3), без освещения, края гаснут по углу к камере.
-- Без этого скрипта рендер берёт старый блендер MODEL из shaders.xr - освещённую модель с альфой, конус выходит чёрным.
function normal		(shader, t_base, t_second, t_detail)
  shader:begin  	("model_def_lplanes","lightcone")
      : fog    		(false)
      : zb     		(true,false)
      : blend   	(true,blend.one,blend.one)
      : aref    	(true,0)
      : sorting		(2, true)
  shader:sampler	("s_base")      :texture  (t_base)
end
