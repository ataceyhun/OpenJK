// Inputs
layout(points) in;

in vec4 vert_Color[];
in vec2 vert_TexCoord0[];

// Outputs
layout(points) out;

out vec4 geom_Color;
out vec2 geom_TexCoord0;

void main()
{
	gl_Position = gl_in[0].gl_Position;
	gl_PointSize = gl_in[0].gl_PointSize;

	geom_Color = vert_Color[0];
	geom_TexCoord0 = vert_TexCoord0[0];

	EmitVertex();
	EndPrimitive();
}