// Uniforms
uniform mat4 u_ModelViewProjectionMatrix;

uniform vec3 u_ViewUp;
uniform vec3 u_ViewLeft;
uniform vec3 u_ViewForward;

// Inputs
layout(points) in;

in vec4 vert_Color[];
in vec2 vert_TexCoord0[];

// Outputs
layout(triangle_strip) out;

out vec4 geom_Color;
out vec2 geom_TexCoord0;

void main()
{
	vec4 position = gl_in[0].gl_Position;
	position.z -= 80.0f;
	position.xyz += -u_ViewLeft * 0.6;
	gl_Position = u_ModelViewProjectionMatrix * position;

	geom_Color = vert_Color[0];
	geom_TexCoord0 = vec2 (1.0, 0.0);

	EmitVertex();

	position = gl_in[0].gl_Position;
	position.z -= 80.0f;
	position.xyz += u_ViewLeft * 0.6;
	gl_Position = u_ModelViewProjectionMatrix * position;

	geom_Color = vert_Color[0];
	geom_TexCoord0 = vec2 (0.0, 0.0);

	EmitVertex();

	gl_Position = u_ModelViewProjectionMatrix * gl_in[0].gl_Position;

	geom_Color = vert_Color[0];
	geom_TexCoord0 = vec2 (0.0, 1.0);

	EmitVertex();

	EndPrimitive();
}