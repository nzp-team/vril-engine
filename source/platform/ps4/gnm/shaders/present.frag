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
layout(set=0,binding=0) uniform sampler2D u_texture;
layout(location=0) out vec4 outcol;
void main() {
    const vec2 pixel=vec2(1.0/1920.0,1.0/1080.0);
    const vec3 luma=vec3(0.299,0.587,0.114);
    vec3 nw=texture(u_texture,v_uv+vec2(-1,-1)*pixel).rgb;
    vec3 ne=texture(u_texture,v_uv+vec2(1,-1)*pixel).rgb;
    vec3 sw=texture(u_texture,v_uv+vec2(-1,1)*pixel).rgb;
    vec3 se=texture(u_texture,v_uv+vec2(1,1)*pixel).rgb;
    vec3 center=texture(u_texture,v_uv).rgb;
    float lnw=dot(nw,luma),lne=dot(ne,luma),lsw=dot(sw,luma),lse=dot(se,luma),lm=dot(center,luma);
    float lo=min(lm,min(min(lnw,lne),min(lsw,lse)));
    float hi=max(lm,max(max(lnw,lne),max(lsw,lse)));
    vec2 direction=vec2(-((lnw+lne)-(lsw+lse)),(lnw+lsw)-(lne+lse));
    float reduce=max((lnw+lne+lsw+lse)*0.03125,0.0078125);
    direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduce),vec2(-8),vec2(8))*pixel;
    vec3 a=0.5*(texture(u_texture,v_uv+direction*(-1.0/6.0)).rgb+texture(u_texture,v_uv+direction*(1.0/6.0)).rgb);
    vec3 b=a*0.5+0.25*(texture(u_texture,v_uv-direction*0.5).rgb+texture(u_texture,v_uv+direction*0.5).rgb);
    float lb=dot(b,luma);
    outcol=vec4(lb<lo||lb>hi?a:b,1);
}
