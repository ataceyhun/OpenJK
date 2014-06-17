// Inputs
in vec4 geom_Color;
in vec2 geom_TexCoord0;

// Outputs
out vec4 out_Glow;

void main()
{
	gl_FragColor = vec4 (0.0, 1.0, 0.0, 1.0);
	out_Glow = vec4(0.0);
}