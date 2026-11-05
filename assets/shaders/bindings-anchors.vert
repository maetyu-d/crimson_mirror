// The simulation supplies the evolving boundary and curl field.
vec3 skinFlow(vec3 p) {
  vec3 n=containerNormal(p),v=current(p);
  return v-n*dot(v,n)*.78;
}
void main() {
  const int routes[6]=int[6](10,20,13,20,5,8);
  int site=gl_VertexID/BINDING_SAMPLES;
  int sampleIndex=gl_VertexID-site*BINDING_SAMPLES;
  float route=float(routes[site]);
  vec3 p=routeOrigin(route);
  // Bury each graft just inside the skin. Both feeds share one living site.
  for(int i=0;i<3;++i)p-=containerNormal(p)*(container(p)+.45)*.8;
  for(int i=0;i<BINDING_SAMPLES-1;++i){
    if(i>=sampleIndex)break;
    p+=skinFlow(p)*.14;
    p-=containerNormal(p)*(container(p)+.45)*.3;
  }
  BodyFrame frame=bodyFrame(p);
  v_position=vec4(frame.centre,1.);
  v_velocity=vec4(bodyTangent(frame,skinFlow(p)*.14),container(p));
  v_meta=vec4(normalize(bodyNormal(frame,containerNormal(p))),route);
}
