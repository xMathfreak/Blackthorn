#version 330 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_ScreenTexture;

uniform vec2 u_TexelSize;

uniform bool u_Grayscale = false;
uniform bool u_Invert = false;
uniform float u_Brightness = 1.0;
uniform float u_Contrast = 1.0;
uniform float u_Saturation = 1.0;
uniform float u_GammaCorrect = 1.0;

uniform bool u_Vignette = false;
uniform float u_VignetteIntensity = 0.5;
uniform float u_VignetteRadius = 0.8;

uniform bool u_Blur = false;
uniform float u_BlurRadius = 3.0;

uniform bool u_Bloom = false;
uniform float u_BloomThreshold = 1.0;
uniform float u_BloomStrength = 0.5;

uniform bool u_Sepia = false;
uniform float u_HueShift = 0.0;

uniform float u_NoiseAmount = 0.0;
uniform float u_ChromaticAberration = 0.0;
uniform float u_Pixelation = 0.0;

vec3 applyBrightnessContrast(vec3 color) {
	color *= u_Brightness;
	color = (color - 0.5) * u_Contrast + 0.5;
	return color;
}

vec3 applySaturation(vec3 color) {
	float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
	return mix(vec3(luma), color, u_Saturation);
}

vec3 applyVignette(vec3 color, vec2 uv) {
	vec2 center = vec2(0.5, 0.5);
	float dist = distance(uv, center);
	float vignette = 1.0 - smoothstep(u_VignetteRadius, u_VignetteRadius + 0.1, dist);
	return mix(color, color * vignette, u_VignetteIntensity);
}

vec3 extractBloom(vec3 color) {
	return color * step(u_BloomThreshold, max(color.r, max(color.g, color.b)));
}

vec3 applySepia(vec3 color) {
	vec3 sepiaColor = vec3(0.393, 0.769, 0.189) * color.r +
					  vec3(0.349, 0.686, 0.168) * color.g +
					  vec3(0.272, 0.534, 0.131) * color.b;
	return sepiaColor;
}

vec3 applyHueShift(vec3 color, float hueShift) {
	float angle = hueShift * 6.283185307179586;
	mat3 hueRotation = mat3(
		cos(angle) + (1.0 - cos(angle)) / 3.0, (1.0 - cos(angle)) / 3.0 - sin(angle) / sqrt(3.0), (1.0 - cos(angle)) / 3.0 + sin(angle) / sqrt(3.0),
		(1.0 - cos(angle)) / 3.0 + sin(angle) / sqrt(3.0), cos(angle) + (1.0 - cos(angle)) / 3.0, (1.0 - cos(angle)) / 3.0 - sin(angle) / sqrt(3.0),
		(1.0 - cos(angle)) / 3.0 - sin(angle) / sqrt(3.0), (1.0 - cos(angle)) / 3.0 + sin(angle) / sqrt(3.0), cos(angle) + (1.0 - cos(angle)) / 3.0
	);
	return hueRotation * color;
}

vec3 addNoise(vec3 color, vec2 uv) {
	float noise = (fract(sin(dot(uv * vec2(12.9898, 78.233), vec2(12.9898, 78.233))) * 43758.5453) * 2.0 - 1.0);
	return color + vec3(noise * u_NoiseAmount);
}

vec3 applyChromaticAberration(vec3 color, vec2 uv) {
	float offset = u_ChromaticAberration * 0.1;
	vec3 redColor = texture(u_ScreenTexture, uv + vec2(offset, 0.0)).rgb;
	vec3 greenColor = texture(u_ScreenTexture, uv).rgb;
	vec3 blueColor = texture(u_ScreenTexture, uv - vec2(offset, 0.0)).rgb;
	return vec3(redColor.r, greenColor.g, blueColor.b);
}

vec3 applyPixelation(vec2 uv) {
	vec2 pixelUV = floor(uv / u_TexelSize) * u_TexelSize;
	return texture(u_ScreenTexture, pixelUV).rgb;
}

vec3 blur(vec2 uv) {
	vec3 result = vec3(0.0);

	result += texture(u_ScreenTexture, uv).rgb * 0.227027;

	result += texture(u_ScreenTexture, uv + u_TexelSize * vec2( 1, 0)).rgb * 0.194594;
	result += texture(u_ScreenTexture, uv + u_TexelSize * vec2(-1, 0)).rgb * 0.194594;

	result += texture(u_ScreenTexture, uv + u_TexelSize * vec2( 0, 1)).rgb * 0.194594;
	result += texture(u_ScreenTexture, uv + u_TexelSize * vec2( 0, -1)).rgb * 0.194594;

	result += texture(u_ScreenTexture, uv + u_TexelSize * vec2( 1, 1)).rgb * 0.121621;
	result += texture(u_ScreenTexture, uv + u_TexelSize * vec2(-1, -1)).rgb * 0.121621;

	result += texture(u_ScreenTexture, uv + u_TexelSize * vec2(-1, 1)).rgb * 0.121621;
	result += texture(u_ScreenTexture, uv + u_TexelSize * vec2( 1, -1)).rgb * 0.121621;

	return result;
}

void main() {
	vec2 texelSize = 1.0 / vec2(textureSize(u_ScreenTexture, 0));

	vec4 texColor = texture(u_ScreenTexture, v_TexCoord);
	vec3 color = texColor.rgb;

	if (u_Brightness != 1.0 || u_Contrast != 1.0)
		color = applyBrightnessContrast(color);

	if (u_Saturation != 1.0)
		color = applySaturation(color);

	if (u_Vignette)
		color = applyVignette(color, v_TexCoord);

	if (u_Sepia)
		color = applySepia(color);

	if (u_HueShift != 0.0)
		color = applyHueShift(color, u_HueShift);

	if (u_Grayscale)
		color = vec3(dot(color, vec3(0.2126, 0.7152, 0.0722)));

	if (u_Invert)
		color = vec3(1.0) - color;

	if (u_Blur || u_Bloom) {
		vec3 blurred = blur(v_TexCoord);

		if (u_Blur)
			color = blurred;

		if (u_Bloom)
			color += extractBloom(blurred) * u_BloomStrength;
	}

	if (u_NoiseAmount > 0.0)
		color = addNoise(color, v_TexCoord);

	if (u_ChromaticAberration > 0.0)
		color = applyChromaticAberration(color, v_TexCoord);

	if (u_Pixelation > 0.0)
		color = applyPixelation(v_TexCoord);

	if (u_GammaCorrect != 1.0)
		color = pow(max(color, vec3(0.0)), vec3(1.0 / u_GammaCorrect));

	FragColor = vec4(clamp(color, 0.0, 1.0), texColor.a);
}