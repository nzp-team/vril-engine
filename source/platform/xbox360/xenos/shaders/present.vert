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
layout(location=0) in vec3 a_position;
layout(location=2) in vec2 a_uv;
layout(location=0) out vec2 v_uv;
out gl_PerVertex { vec4 gl_Position; };
void main() {
    gl_Position=vec4(a_position.xy+vec2(-1.0/1280.0,1.0/720.0),a_position.z,1.0);
    v_uv=a_uv;
}
