#pragma once

// CRC исходника шейдера для кеша шейдеров R1/R2/R3 (как в R4): исходник прогоняется через препроцессор с теми же
// define'ами и include'ами, что и при компиляции, поэтому правка самого .ps/.vs или любого подключаемого .h меняет CRC
// и кеш этого шейдера перекомпилируется. Профиль, точка входа и флаги компиляции тоже входят в CRC.
// Возвращает 0, если препроцессор не отработал (тогда кеш не используется).

#if defined(USE_DX10) || defined(USE_DX11)

inline u32 shader_source_crc(LPCVOID src, UINT src_len, const D3D_SHADER_MACRO* defines, ID3DInclude* includer, LPCSTR target, LPCSTR entry, UINT flags)
{
    ID3DBlob* pre{};
    ID3DBlob* err{};
    u32 crc = 0;
    if (SUCCEEDED(D3DPreprocess(src, src_len, "", defines, includer, &pre, &err)) && pre)
    {
        crc = crc32(pre->GetBufferPointer(), (u32)pre->GetBufferSize());
        string256 key;
        xr_sprintf(key, "%s|%s|%x", target ? target : "", entry ? entry : "", (u32)flags);
        crc ^= crc32(key, xr_strlen(key)) * 0x9E3779B1u;
        if (!crc)
            crc = 1;
    }
    _RELEASE(pre);
    _RELEASE(err);
    return crc;
}

#else

inline u32 shader_source_crc(LPCVOID src, UINT src_len, const D3DXMACRO* defines, LPD3DXINCLUDE includer, LPCSTR target, LPCSTR entry, DWORD flags)
{
    LPD3DXBUFFER pre{};
    LPD3DXBUFFER err{};
    u32 crc = 0;
    if (SUCCEEDED(D3DXPreprocessShader((LPCSTR)src, src_len, defines, includer, &pre, &err)) && pre)
    {
        crc = crc32(pre->GetBufferPointer(), (u32)pre->GetBufferSize());
        string256 key;
        xr_sprintf(key, "%s|%s|%x", target ? target : "", entry ? entry : "", (u32)flags);
        crc ^= crc32(key, xr_strlen(key)) * 0x9E3779B1u;
        if (!crc)
            crc = 1;
    }
    _RELEASE(pre);
    _RELEASE(err);
    return crc;
}

#endif
