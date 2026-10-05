#version 450
// HDR to display: exposure, ACES filmic curve (Narkowicz fit), a light vignette and dithering against banding.
// The swapchain is sRGB, the output stays linear.

layout(set = 0, binding = 0) uniform sampler2D hdr;
layout(push_constant) uniform Push { float exposure; } push;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

vec3 aces( vec3 x ) {
	return clamp( (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0 );
}

void main() {
	vec3 color = aces( texture( hdr, uv ).rgb * push.exposure );
	const vec2 centered = uv - 0.5;
	color *= 1.0 - dot( centered, centered ) * 0.35;
	const float noise = fract( sin( dot( gl_FragCoord.xy, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
	color += (noise - 0.5) / 255.0;
	outColor = vec4( color, 1.0 );
}
