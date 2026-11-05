// Staggered roots spread into surface-following prism lamellae.
uniform samplerBuffer u_anchorPositions;
uniform samplerBuffer u_anchorVelocities;
uniform samplerBuffer u_anchorMeta;
uniform vec3 u_bindingRoots[2];
uniform vec3 u_cameraRight;
uniform vec3 u_cameraUp;
uniform vec3 u_cameraFront;
uniform float u_bindingSize;
uniform float u_rootPixel;
uniform float u_energy;
const float CONTACT=.57;
vec3 safeNormal(vec3 p,vec3 fallback){return length(p)>1e-5?normalize(p):fallback;}
vec3 bezier(vec3 a,vec3 b,vec3 c,vec3 d,float t){
  float u=1.-t;return u*u*u*a+3.*u*u*t*b+3.*u*t*t*c+t*t*t*d;
}
vec3 skinPoint(int site,float t){
  float x=clamp(t,0.,1.)*float(BINDING_SAMPLES-1);
  int i=min(int(x),BINDING_SAMPLES-2);float u=x-float(i);
  int at=site*BINDING_SAMPLES+i;
  vec3 a=texelFetch(u_anchorPositions,at).xyz,b=texelFetch(u_anchorPositions,at+1).xyz;
  vec3 va=texelFetch(u_anchorVelocities,at).xyz,vb=texelFetch(u_anchorVelocities,at+1).xyz;
  float u2=u*u,u3=u2*u;
  return (2.*u3-3.*u2+1.)*a+(u3-2.*u2+u)*va+(-2.*u3+3.*u2)*b+(u3-u2)*vb;
}
vec3 skinNormal(int site,float t){
  float x=clamp(t,0.,1.)*float(BINDING_SAMPLES-1);
  int i=min(int(x),BINDING_SAMPLES-2),at=site*BINDING_SAMPLES+i;
  return safeNormal(mix(texelFetch(u_anchorMeta,at).xyz,texelFetch(u_anchorMeta,at+1).xyz,x-float(i)),u_cameraFront);
}
vec3 trunk(int root,float t,float strand){
  int site=root*3;
  vec3 skin=skinPoint(site,0.),n=skinNormal(site,0.);
  vec3 flow=safeNormal(texelFetch(u_anchorVelocities,site*BINDING_SAMPLES).xyz,u_cameraRight);
  vec3 start=u_bindingRoots[root]+u_cameraUp*(strand*u_rootPixel*5.)-u_cameraRight*(abs(strand)*u_rootPixel*4.);
  float reach=clamp(length(skin-start)*.18,1.,3.);
  vec3 p=bezier(start,start+u_cameraRight*reach,skin+n*1.8-flow*1.7,skin,t);
  float braid=sin(t*3.14159265)*(.12+.25*t);
  vec3 axis=safeNormal(mix(u_cameraUp,n,t),u_cameraUp);
  return p+axis*strand*braid*sin(t*8.+u_time*.13+float(root));
}
vec3 path(int root,int part,float t,float strand){
  if(part==0)return trunk(root,t,strand);
  const float forks[3]=float[3](.88,.43,.66);
  int site=root*3+part-1;
  float fork=forks[part-1];
  vec3 start=trunk(root,fork,0.);
  vec3 direction=safeNormal(trunk(root,min(.999,fork+.01),0.)-start,u_cameraRight);
  vec3 skin=skinPoint(site,0.);
  vec3 tangent=texelFetch(u_anchorVelocities,site*BINDING_SAMPLES).xyz*float(BINDING_SAMPLES-1);
  vec3 p;
  if(t<CONTACT){
    // The derivative matches the skin path at contact, removing a plug seam.
    vec3 a=start+direction*clamp(length(skin-start)*.32,.5,4.);
    vec3 b=skin-tangent*(CONTACT/(3.*(1.-CONTACT)));
    p=bezier(start,a,b,skin,t/CONTACT);
  }else p=skinPoint(site,(t-CONTACT)/(1.-CONTACT));
  float s=max(0.,(t-CONTACT)/(1.-CONTACT));
  vec3 n=skinNormal(site,s);
  vec3 flow=safeNormal(skinPoint(site,min(1.,s+.02))-skinPoint(site,max(0.,s-.02)),direction);
  vec3 side=safeNormal(cross(n,flow),u_cameraUp);
  float fan=smoothstep(.02,.55,t)*(1.-.45*smoothstep(.85,1.,t));
  p+=side*strand*fan*(.9+.18*sin(u_time*.17+float(site)));
  p+=n*(strand*strand)*fan*.12*sin(t*11.+float(site)+u_time*.13);
  return p;
}
void main(){
  int group=int(a_seed.x+.5),root=group/4,part=group-root*4;
  float t=a_seed.y,strand=a_seed.z;
  vec3 a=path(root,part,t,strand),b=path(root,part,min(1.,t+a_seed.w),strand);
  vec3 dir=safeNormal(b-a,u_cameraRight);
  float skinMix=part==0?smoothstep(.7,1.,t):smoothstep(CONTACT-.18,CONTACT+.10,t);
  float swell=exp(-pow((t-CONTACT)/.19,2.));
  float radius=part==0?mix(.055,.19,t)+sin(t*3.14159265)*.13
    :(.09+sin(t*3.14159265)*.22+swell*.30)*(1.-.985*smoothstep(.72,1.,t));
  float grain=1.+skinMix*.20*sin(t*173.+strand*17.+float(group)*3.);
  float pulse=pow(.5+.5*sin(t*10.-u_time*1.5+float(root)),5.);
  radius*=grain*u_bindingSize*(1.+u_reactivity*(u_energy*.1+u_audio.w*pulse*.28));
  int site=root*3+(part==0?0:part-1);
  vec3 n=skinNormal(site,max(0.,(t-CONTACT)/(1.-CONTACT)));
  vec3 up0=abs(dir.y)>.98?vec3(1.,0.,0.):vec3(0.,1.,0.);
  vec3 r=safeNormal(cross(dir,up0),u_cameraRight),u=cross(r,dir);
  vec3 desired=safeNormal(mix(u_cameraUp,n,skinMix),u);
  vec2 orient=vec2(dot(desired,u),dot(desired,r));
  float roll=dot(orient,orient)>1e-8?atan(orient.y,orient.x):0.;
  float route=texelFetch(u_anchorMeta,site*BINDING_SAMPLES).w;
  v_position=vec4((a+b)*.5,max(radius,.002));
  v_velocity=vec4(dir,max(length(b-a)*mix(1.28,1.5,skinMix),.015));
  v_meta=vec4(u_slot,route,roll,skinMix);
}
