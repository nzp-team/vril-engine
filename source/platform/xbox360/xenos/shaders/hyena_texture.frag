/*
Copyright (C) 2026 NZ:P Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#version 450
layout(location=0) in vec2 v_uv;
layout(location=1) in vec4 v_color;
layout(set=0,binding=0) uniform sampler2D u_texture;
layout(set=0,binding=16,std140) uniform State {
    mat4 mvp; mat4 modelview; vec4 uv; vec4 fog_color; vec4 params; vec4 flags;
} u;
layout(location=0) out vec4 outcol;
void main() {
    vec4 texel=texture(u_texture,v_uv);
    vec4 color=mix(texel,texel*v_color,u.flags.x);
    #ifdef ALPHA_TEST
    if(color.a<=u.params.z) discard;
    #endif
    outcol=color;
}
