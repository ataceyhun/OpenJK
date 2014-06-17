// Uniforms
uniform mat4 u_ModelViewProjectionMatrix;

// Inputs
in vec3 attr_Position;
in vec4 attr_Color;
in vec2 attr_TexCoord0;

// Outputs
out vec4 vert_Color;
out vec2 vert_TexCoord0;

void main()
{
	gl_PointSize = 4.0;
	gl_Position = u_ModelViewProjectionMatrix * vec4 (attr_Position, 1.0);

	vert_Color = attr_Color;
	vert_TexCoord0 = attr_TexCoord0;
}
