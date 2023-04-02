#define pi 3.141592653589793

// Rotate a vec2
vec2 rotate(vec2 vec, float rot)
{
    float s = sin(rot), c = cos(rot);
    return vec2(vec.x*c-vec.y*s, vec.x*s+vec.y*c);
}

// Hash13 Hash without Sine: https://www.shadertoy.com/view/4djSRW
float hash(vec2 p, float t)
{
    vec3 p3 = vec3(p, t);
    p3  = fract(p3*0.1031);
    p3 += dot(p3, p3.zyx+31.32);
    return fract((p3.x+p3.y)*p3.z);
}

// manu210404's Improved Version
float noise(vec2 p, float t)
{
    vec4 b = vec4(floor(p), ceil(p));
    vec2 f = smoothstep(0.0, 1.0, fract(p));
    return mix(mix(hash(b.xy, t), hash(b.zy, t), f.x), mix(hash(b.xw, t), hash(b.zw, t), f.x), f.y);
}

// Number of FBM Octaves
#define num_octaves 16

// Fractal Brownian Motion Noise
float fbm(vec2 pos)
{
    float value = 0.0;
    float scale = 1.0;
    float atten = 0.5;
    float t = 0.0;
    for(int i = 0; i < num_octaves; i++)
    {
        t += atten;
        value += noise(pos*scale, float(i))*atten;
        scale *= 2.0;
        atten *= 0.5;
        pos = rotate(pos, 0.125*pi);
    }
    return value/t;
}

// https://www.shadertoy.com/view/wdsfDH
mat3 m = mat3(0.00, 1.60, 1.20, -1.60, 0.72, -0.96, -1.20, -0.96, 1.28);

// hash function              
float hash(float n)
{
    return fract(cos(n) * 114514.1919);
}

// 3d noise function
float noise(in vec3 x)
{
    vec3 p = floor(x);
    vec3 f = smoothstep(0.0, 1.0, fract(x));
        
    float n = p.x + p.y * 10.0 + p.z * 100.0;
    
    return mix(
        mix(mix(hash(n + 0.0), hash(n + 1.0), f.x),
            mix(hash(n + 10.0), hash(n + 11.0), f.x), f.y),
        mix(mix(hash(n + 100.0), hash(n + 101.0), f.x),
            mix(hash(n + 110.0), hash(n + 111.0), f.x), f.y), f.z);
}

// Fractional Brownian motion
float fbm(vec3 p)
{
    float f = 0.5000 * noise(p);
    p = m * p;
    f += 0.2500 * noise(p);
    p = m * p;
    f += 0.1666 * noise(p);
    p = m * p;
    f += 0.0834 * noise(p);
    return f;
}