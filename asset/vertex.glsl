#version 450

layout(location = 0) in vec3 input_position;
layout(location = 1) in vec3 input_color;

layout(location = 0) out vec3 output_color;

void main()
{
    gl_Position  = vec4(input_position, 1.0);
    output_color = input_color;
}
