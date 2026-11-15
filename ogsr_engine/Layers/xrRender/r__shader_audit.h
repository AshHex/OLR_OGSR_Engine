#pragma once

// Диагностика рендера для бенчмарка (render_bench.script) и консольной команды r_shader_audit.
//
// Во время игры сюда сообщают о шейдерах, которых нет у текущего рендера (они рисуются запасным шейдером),
// и о ненайденных текстурах. Команда r_shader_audit дополнительно проходит по всем моделям (*.ogf),
// партиклам и шейдерам текущего уровня и пишет в $logs$\shader_audit_<render>.txt список того,
// чего не хватает текущему рендеру и каждой папке gamedata\shaders\r1|r2|r3.

void RAudit_NoteMissingShader(LPCSTR shader, LPCSTR textures, LPCSTR fallback);
void RAudit_NoteMissingTexture(LPCSTR texture);

// Пишет отчёт и возвращает число шейдеров, которых нет у текущего рендера
u32 RAudit_Run(bool b_scan_meshes);

// Алиас для шейдера, которого нет у текущего рендера: gamedata\shaders\<r1|r2|r3>\shader_aliases.ltx, секция [aliases]
// ("models\kamflight = models\selflight"). Возвращает nullptr, если алиаса нет.
LPCSTR RAudit_FindAlias(LPCSTR shader);
