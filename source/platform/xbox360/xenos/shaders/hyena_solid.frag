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
#ifdef FOG
layout(location=2) in vec4 v_eye;
#endif
layout(set=0,binding=16,std140) uniform State {
    mat4 mvp; mat4 modelview; vec4 uv; vec4 fog_color; vec4 params; vec4 flags;
} u;
layout(location=0) out vec4 outcol;
void main() {
    if(v_color.a<=u.params.z) discard;
    #ifdef FOG
    float visibility=clamp(u.flags.w-abs(v_eye.z)*u.flags.z,0.0,1.0);
    outcol=vec4(mix(u.fog_color.rgb,v_color.rgb,visibility),v_color.a);
    #else
    outcol=v_color;
    #endif
}
