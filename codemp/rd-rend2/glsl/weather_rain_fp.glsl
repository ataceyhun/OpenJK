// Samplers
uniform sampler2D u_TextureMap;

// Inputs
in vec4 geom_Color;
in vec2 geom_TexCoord0;

// Outputs
out vec4 out_Glow;

void main()
{
	vec4 color = texture(u_TextureMap, geom_TexCoord0);

	gl_FragColor = color * geom_Color;
	out_Glow = vec4 (0.0, 0.0, 0.0, 1.0);
}