shader_type canvas_item;

// NV12 Texture Layout:
// +--------------------+
// |                    |
// |      Y Plane       |  <- Height = H
// |                    |
// +--------------------+
// |      UV Plane      |  <- Height = 0.5H
// +--------------------+
// Total Texture Height = 1.5H

void fragment() {
    // Texture width and total height (1.5H)
    vec2 tex_size = vec2(textureSize(TEXTURE, 0));
    float h = tex_size.y / 1.5;
    
    // Y component: UV.y goes from 0 to 2/3 of the total texture height
    vec2 y_uv = vec2(UV.x, UV.y * (h / tex_size.y));
    float y = texture(TEXTURE, y_uv).r;
    
    // UV component: UV.y goes from 2/3 to 1.0 of the total texture height
    // In NV12, U and V are interleaved: [U0, V0, U1, V1, ...]
    // We need to sample the same horizontal position but in the UV area
    float uv_y_offset = h / tex_size.y;
    vec2 uv_uv = vec2(UV.x, uv_y_offset + (UV.y * (0.5 * h / tex_size.y)));
    
    // Since we use L8/R8, we need to pick U or V based on pixel X position
    float pixel_x = UV.x * tex_size.x;
    float u, v;
    
    // A bit of a hack for interleaved data in a single-channel texture:
    // We sample two adjacent points to get U and V
    float texel_size_x = 1.0 / tex_size.x;
    if (mod(floor(pixel_x), 2.0) < 1.0) {
        u = texture(TEXTURE, uv_uv).r;
        v = texture(TEXTURE, uv_uv + vec2(texel_size_x, 0.0)).r;
    } else {
        u = texture(TEXTURE, uv_uv - vec2(texel_size_x, 0.0)).r;
        v = texture(TEXTURE, uv_uv).r;
    }

    // Adjust YUV values
    y = 1.1643 * (y - 0.0625);
    u = u - 0.5;
    v = v - 0.5;

    // Convert to RGB
    float r = y + 1.5958 * v;
    float g = y - 0.39173 * u - 0.8129 * v;
    float b = y + 2.017 * u;

    COLOR = vec4(r, g, b, 1.0);
}
