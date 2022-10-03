#version 450
#extension GL_ARB_separate_shader_objects : enable

// Kernel dimensions for median filter
#define KERNEL_DIM 3

#define KERNEL_RADIUS KERNEL_DIM/2
#define KERNEL_SIZE KERNEL_DIM*KERNEL_DIM

layout(location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

//Median  filter
void main()
{
  ivec2 textureSz = textureSize(colorTex, 0);
  vec4 colorArray[KERNEL_SIZE];
  float lightnessArray[KERNEL_SIZE];

  for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; i++) {
    for (int j = -KERNEL_RADIUS; j <= KERNEL_RADIUS; j++) {
      vec4 selectedPixel =  textureLod(colorTex, surf.texCoord + vec2(i, j) / textureSz, 0);
      float minColorValue = min(min(selectedPixel.r, selectedPixel.g), selectedPixel.b);
      float maxColorValue = max(max(selectedPixel.r, selectedPixel.g), selectedPixel.b);

      // Color of the selected pixel
      colorArray[KERNEL_DIM * (i + KERNEL_RADIUS) + (j + KERNEL_RADIUS)] = selectedPixel;
      // Lightness of the pixel from HSL color model
      lightnessArray[KERNEL_DIM * (i + KERNEL_RADIUS) + (j + KERNEL_RADIUS)] = (minColorValue + maxColorValue) / 2.f;
    }
  }

  // Insertion sorting of neighborhood colors by their lightness
  int j;
  float selectedLightness;
  vec4 selectedColor;
  for (int i = 1; i < KERNEL_SIZE; i++) {
    selectedLightness = lightnessArray[i];
    selectedColor = colorArray[i];
    j = i - 1;

    while (j >= 0 && lightnessArray[j] > selectedLightness) {
        lightnessArray[j + 1] = lightnessArray[j];
        colorArray[j + 1] = colorArray[j];
        j = j - 1;
    }
    lightnessArray[j + 1] = selectedLightness;
    colorArray[j + 1] = selectedColor;
  }

  // CHoosing median from sorted array of colors
  color = colorArray[KERNEL_SIZE / 2];
}