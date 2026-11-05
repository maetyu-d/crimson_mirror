// Shared by the prisms and the surface grafts. Bass squashes the entire
// body, while mids wind the upper and lower folds in opposite directions.
uniform vec3 u_bodyScale;
uniform vec2 u_foldTwist; // maximum angle, inverse fold height

struct BodyFrame {
  vec3 centre;
  vec2 rotation;
  vec2 shear;
};

vec3 turnFold(vec3 v, vec2 rotation) {
  return vec3(rotation.x*v.x - rotation.y*v.z, v.y,
              rotation.y*v.x + rotation.x*v.z);
}

BodyFrame bodyFrame(vec3 p) {
  float height = clamp(p.y*u_foldTwist.y, -1., 1.);
  float angle = u_foldTwist.x*height*(1.5 - .5*height*height);
  vec2 rotation = vec2(cos(angle), sin(angle));
  vec3 centre = turnFold(p*u_bodyScale, rotation);
  float slope = 1.5*u_foldTwist.x*u_foldTwist.y*(1. - height*height);
  return BodyFrame(centre, rotation, vec2(-centre.z, centre.x)*slope);
}

vec3 bodyTangent(BodyFrame frame, vec3 tangent) {
  vec3 result = turnFold(tangent*u_bodyScale, frame.rotation);
  result.xz += frame.shear*tangent.y;
  return result;
}

vec3 bodyNormal(BodyFrame frame, vec3 normal) {
  vec3 result = turnFold(normal/u_bodyScale, frame.rotation);
  result.y -= dot(frame.shear, result.xz)/u_bodyScale.y;
  return result;
}
