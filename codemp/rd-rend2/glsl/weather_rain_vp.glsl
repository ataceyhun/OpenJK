// Uniforms
uniform float u_Time;
uniform vec3 u_ViewOrigin;

// Inputs
in vec3 attr_Position;
in vec4 attr_Color;
in vec2 attr_TexCoord0;

// Outputs
out vec4 vert_Color;
out vec2 vert_TexCoord0;

void main()
{
	vec3 offset = vec3 (0.0);

	gl_Position = vec4 (attr_Position, 1.0);
	gl_Position.x += ((gl_InstanceID % 10) - 5) * 500.0;
	gl_Position.y += ((gl_InstanceID / 10) - 5) * 500.0;
	gl_Position.z -= u_Time * 800.0;
	gl_Position.z = mod(gl_Position.z, 2000.0) - 1000.0;

	gl_Position.xyz += u_ViewOrigin;

	vert_Color = attr_Color;
	vert_TexCoord0 = attr_TexCoord0;
}
