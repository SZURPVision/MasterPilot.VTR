shader_type canvas_item;

void fragment() {
    // 获取纹理的物理尺寸
    ivec2 tex_size = textureSize(TEXTURE, 0);
    // 计算 Y 平面的实际高度 (TotalHeight = 1.5H => H = TotalHeight / 1.5)
    int h = tex_size.y * 2 / 3;
    
    // 当前屏幕像素对应的整数坐标 (0 to W-1, 0 to H-1)
    ivec2 ipos = ivec2(UV * vec2(float(tex_size.x), float(h)));
    
    // 1. 采样 Y 分量: 直接对应物理像素坐标
    float y = texelFetch(TEXTURE, ipos, 0).r;
    
    // 2. 采样 UV 分量:
    // NV12 中，UV 平面紧跟在 Y 平面之后。
    // UV 的行索引 = h + (ipos.y / 2)
    // UV 的列索引 = (ipos.x / 2) * 2  --> 指向 U
    //            = (ipos.x / 2) * 2 + 1 --> 指向 V
    int uv_y = h + (ipos.y / 2);
    int uv_x_base = (ipos.x / 2) * 2;
    
    float u = texelFetch(TEXTURE, ivec2(uv_x_base, uv_y), 0).r;
    float v = texelFetch(TEXTURE, ivec2(uv_x_base + 1, uv_y), 0).r;

    // 标准 YUV (BT.601) 转 RGB 逻辑
    y = 1.1643 * (y - 0.0625);
    u = u - 0.5;
    v = v - 0.5;

    float r = y + 1.5958 * v;
    float g = y - 0.39173 * u - 0.8129 * v;
    float b = y + 2.017 * u;

    COLOR = vec4(r, g, b, 1.0);
}
