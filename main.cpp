#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <set>
#include <map>
#include <utility>
#include <fstream>
void ApplyN3DLiteTitlebarTheme();
struct MeshObject {std::string name;std::vector<Vector3> vertices;std::vector<std::vector<int>> faces;Vector3 position{};int materialId=0;int smoothingGroup=0;float smoothingValues[33]={0};MeshObject(){for(int i=1;i<=32;i++)smoothingValues[i]=100.0f;}MeshObject(std::string n,std::vector<Vector3> v,std::vector<std::vector<int>> f):name(std::move(n)),vertices(std::move(v)),faces(std::move(f)){for(int i=1;i<=32;i++)smoothingValues[i]=100.0f;} };
struct EditorMaterial {std::string name;float baseColor[3]={0.53f,0.53f,0.53f};float roughness=0.5f;float metallic=0.0f;float specular=0.5f;};
std::vector<EditorMaterial> materials={{"Default"}};int activeMaterial=0;
Color meshMaterialColor(const MeshObject& o){
 const int id=std::clamp(o.materialId,0,(int)materials.size()-1);
 const auto& c=materials[id].baseColor;
 return {(unsigned char)(std::clamp(c[0],0.0f,1.0f)*255.0f),
         (unsigned char)(std::clamp(c[1],0.0f,1.0f)*255.0f),
         (unsigned char)(std::clamp(c[2],0.0f,1.0f)*255.0f),255};
}
// Lightweight viewport Blinn-Phong approximation: live roughness, metalness and specular.
// This is not a physically based BRDF; it provides responsive material previews.
Color shadeMaterialTriangle(const MeshObject& o,Vector3 a,Vector3 b,Vector3 c,Vector3 cameraPosition,Vector3 suppliedNormal={0,0,0}){
 const EditorMaterial& m=materials[std::clamp(o.materialId,0,(int)materials.size()-1)];
 Vector3 n=Vector3Length(suppliedNormal)>1e-7f?suppliedNormal:Vector3CrossProduct(Vector3Subtract(b,a),Vector3Subtract(c,a));
 if(Vector3Length(n)<1e-7f)return meshMaterialColor(o);
 n=Vector3Normalize(n);
 Vector3 center=Vector3Scale(Vector3Add(Vector3Add(a,b),c),1.0f/3.0f);
 Vector3 view=Vector3Subtract(cameraPosition,center);
 if(Vector3Length(view)<1e-6f)view={0,0,1};
 view=Vector3Normalize(view);
 if(Vector3DotProduct(n,view)<0)n=Vector3Negate(n);
 const Vector3 light=Vector3Normalize(Vector3{0.35f,-0.55f,0.75f});
 const float diffuse=std::max(0.0f,Vector3DotProduct(n,light));
 Vector3 halfVector=Vector3Normalize(Vector3Add(light,view));
 const float rough=std::clamp(m.roughness,0.02f,1.0f);
 const float shininess=2.0f+126.0f*powf(1.0f-rough,2.0f);
 const float highlight=powf(std::max(0.0f,Vector3DotProduct(n,halfVector)),shininess);
 const float metal=std::clamp(m.metallic,0.0f,1.0f);
 const float spec=std::clamp(m.specular,0.0f,1.0f);
 unsigned char channel[3];
 for(int k=0;k<3;k++){
  const float base=std::clamp(m.baseColor[k],0.0f,1.0f);
  const float diffusePart=base*(0.22f+0.78f*diffuse)*(1.0f-0.78f*metal);
  const float specTint=(1.0f-metal)*1.0f+metal*base;
  const float specPart=highlight*(0.06f+0.9f*spec)*specTint*(0.35f+0.65f*diffuse);
  channel[k]=(unsigned char)(255.0f*std::clamp(diffusePart+specPart,0.0f,1.0f));
 }
 return {channel[0],channel[1],channel[2],255};
}
static const char* smoothVS=R"GLSL(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
out vec3 worldNormal;
out vec3 worldPosition;
void main(){
 worldPosition=(matModel*vec4(vertexPosition,1.0)).xyz;
 worldNormal=mat3(matModel)*vertexNormal;
 gl_Position=mvp*vec4(vertexPosition,1.0);
}
)GLSL";
static const char* smoothFS=R"GLSL(#version 330
in vec3 worldNormal;
in vec3 worldPosition;
out vec4 finalColor;
uniform vec3 baseColor;
uniform vec3 cameraPosition;
uniform float roughness;
uniform float metallic;
uniform float specular;
void main(){
 vec3 n=normalize(worldNormal);
 vec3 v=normalize(cameraPosition-worldPosition);
 if(dot(n,v)<0.0)n=-n;
 vec3 light=normalize(vec3(0.35,-0.55,0.75));
 float ndl=max(dot(n,light),0.0);
 vec3 h=normalize(light+v);
 float shine=pow(max(dot(n,h),0.0),2.0+126.0*pow(1.0-clamp(roughness,0.02,1.0),2.0));
 float metal=clamp(metallic,0.0,1.0);
 vec3 diffuse=baseColor*(0.22+0.78*ndl)*(1.0-0.78*metal);
 vec3 specColor=mix(vec3(1.0),baseColor,metal);
 vec3 reflection=shine*(0.06+0.9*clamp(specular,0.0,1.0))*specColor*(0.35+0.65*ndl);
 finalColor=vec4(clamp(diffuse+reflection,0.0,1.0),1.0);
}
)GLSL";
struct Snapshot {std::vector<MeshObject> objects;int selected,face,sub;};
std::vector<MeshObject> objects;std::vector<Snapshot> undoStack,redoStack;int selected=-1,face=-1,mode=0,tool=1,sub=-1;Camera3D camera{};float gizmoSize=1.0f;
void checkpoint(){undoStack.push_back({objects,selected,face,sub});if(undoStack.size()>80)undoStack.erase(undoStack.begin());redoStack.clear();}
void undo(){if(undoStack.empty())return;redoStack.push_back({objects,selected,face,sub});auto s=undoStack.back();undoStack.pop_back();objects=s.objects;selected=s.selected;face=s.face;sub=s.sub;}
void redo(){if(redoStack.empty())return;undoStack.push_back({objects,selected,face,sub});auto s=redoStack.back();redoStack.pop_back();objects=s.objects;selected=s.selected;face=s.face;sub=s.sub;}
MeshObject box(){return {"Box",{{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}},{{0,3,2,1},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}}};}
MeshObject plane(){return {"Plane",{{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}},{{0,1,2,3}}};}
MeshObject sphere(){
 MeshObject o;o.name="Sphere";
 constexpr int N=24,R=12;
 // Single pole vertices avoid zero-area faces; rings have outward winding.
 const int top=0;
 o.vertices.push_back({0,0,1});
 for(int j=1;j<R;j++){
  const float p=PI*j/R;
  for(int i=0;i<N;i++){
   const float t=2.0f*PI*i/N;
   o.vertices.push_back({sinf(p)*cosf(t),sinf(p)*sinf(t),cosf(p)});
  }
 }
 const int bottom=(int)o.vertices.size();
 o.vertices.push_back({0,0,-1});
 auto ring=[&](int j,int i){return 1+(j-1)*N+(i+N)%N;};
 for(int i=0;i<N;i++)o.faces.push_back({top,ring(1,i),ring(1,i+1)});
 for(int j=1;j<R-1;j++)for(int i=0;i<N;i++)
  o.faces.push_back({ring(j,i),ring(j+1,i),ring(j+1,i+1),ring(j,i+1)});
 for(int i=0;i<N;i++)o.faces.push_back({ring(R-1,i),bottom,ring(R-1,i+1)});
 // Validate orientation against the sphere center rather than relying on winding guesses.
 for(auto& face:o.faces){
  Vector3 a=o.vertices[face[0]],b=o.vertices[face[1]],c=o.vertices[face[2]];
  Vector3 n=Vector3CrossProduct(Vector3Subtract(b,a),Vector3Subtract(c,a));
  Vector3 center=Vector3Scale(Vector3Add(Vector3Add(a,b),c),1.0f/3.0f);
  if(Vector3DotProduct(n,center)<0.0f)std::reverse(face.begin(),face.end());
 }
 return o;
}
Vector3 world(const MeshObject&o,int i){return Vector3Add(o.vertices[i],o.position);}
void extrude(){if(selected<0||face<0||mode!=3)return;checkpoint();auto&o=objects[selected];auto old=o.faces[face];if(old.size()<3)return;Vector3 a=o.vertices[old[0]],b=o.vertices[old[1]],c=o.vertices[old[2]];Vector3 n=Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b,a),Vector3Subtract(c,a)));std::vector<int> top;for(int i:old){top.push_back((int)o.vertices.size());o.vertices.push_back(Vector3Add(o.vertices[i],Vector3Scale(n,.5f)));}o.faces[face]=top;for(size_t i=0;i<old.size();i++)o.faces.push_back({old[i],old[(i+1)%old.size()],top[(i+1)%top.size()],top[i]});}
// Interactive polygon extrusion: create side walls at zero height, then move
// only the new cap vertices as the pointer moves. One undo checkpoint per drag.
struct PolygonExtrudeDrag {
 bool active=false;
 int object=-1;
 Vector2 start{};
 Vector3 normal{};
 Vector3 center{};
 std::vector<int> cap;
 std::vector<Vector3> base;
};
PolygonExtrudeDrag polygonExtrudeDrag;
void beginPolygonExtrude(Vector2 mouse){
 if(mode!=3||selected<0||selected>=(int)objects.size())return;
 auto& o=objects[selected];
 if(face<0||face>=(int)o.faces.size())return;
 const auto old=o.faces[face];
 if(old.size()<3)return;
 const Vector3 a=o.vertices[old[0]],b=o.vertices[old[1]],c=o.vertices[old[2]];
 Vector3 normal=Vector3CrossProduct(Vector3Subtract(b,a),Vector3Subtract(c,a));
 if(Vector3Length(normal)<0.00001f)return;
 normal=Vector3Normalize(normal);
 checkpoint();
 polygonExtrudeDrag={};
 auto& state=polygonExtrudeDrag;
 state.active=true;state.object=selected;state.start=mouse;state.normal=normal;
 for(int i:old){
  state.cap.push_back((int)o.vertices.size());
  state.base.push_back(o.vertices[i]);
  state.center=Vector3Add(state.center,o.vertices[i]);
  o.vertices.push_back(o.vertices[i]);
 }
 state.center=Vector3Scale(state.center,1.0f/(float)old.size());
 o.faces[face]=state.cap;
 for(size_t i=0;i<old.size();++i)
  o.faces.push_back({old[i],old[(i+1)%old.size()],state.cap[(i+1)%old.size()],state.cap[i]});
}
void updatePolygonExtrude(){
 auto& state=polygonExtrudeDrag;
 if(!state.active||state.object<0||state.object>=(int)objects.size())return;
 auto& o=objects[state.object];
 const Vector3 centerWorld=Vector3Add(state.center,o.position);
 const Vector2 p0=GetWorldToScreen(centerWorld,camera);
 const Vector2 p1=GetWorldToScreen(Vector3Add(centerWorld,state.normal),camera);
 const Vector2 screenAxis=Vector2Subtract(p1,p0);
 const float axisSquared=Vector2DotProduct(screenAxis,screenAxis);
 const Vector2 delta=Vector2Subtract(GetMousePosition(),state.start);
 const float axisLength=std::sqrt(Vector2DotProduct(screenAxis,screenAxis));
 const float cameraDistance=std::max(1.0f,Vector3Distance(camera.position,centerWorld));
 const float fovRadians=camera.fovy*DEG2RAD;
 const float worldPerPixel=(camera.projection==CAMERA_PERSPECTIVE)
  ?(2.0f*cameraDistance*tanf(fovRadians*0.5f)/std::max(1,GetScreenHeight()))
  :(camera.fovy/std::max(1,GetScreenHeight()));
 // Dragging along the projected OUTWARD face normal gives positive extrusion.
 // Use vertical motion only when the normal projects almost to a point.
 const float distance=(axisLength>5.0f)
  ?Vector2DotProduct(delta,screenAxis)/(axisLength*axisLength)
  :-delta.y*worldPerPixel;
 for(size_t i=0;i<state.cap.size();++i)
  o.vertices[state.cap[i]]=Vector3Add(state.base[i],Vector3Scale(state.normal,distance));
}
void endPolygonExtrude(){
 polygonExtrudeDrag.active=false;
 polygonExtrudeDrag.cap.clear();
 polygonExtrudeDrag.base.clear();
}
void inset(){if(selected<0||face<0||mode!=3)return;checkpoint();auto&o=objects[selected];auto old=o.faces[face];Vector3 c{};for(int i:old)c=Vector3Add(c,o.vertices[i]);c=Vector3Scale(c,1.0f/old.size());std::vector<int> inner;for(int i:old){inner.push_back((int)o.vertices.size());o.vertices.push_back(Vector3Lerp(c,o.vertices[i],.7f));}o.faces[face]=inner;for(size_t i=0;i<old.size();i++)o.faces.push_back({old[i],old[(i+1)%old.size()],inner[(i+1)%inner.size()],inner[i]});}

bool validComponent(){return selected>=0&&selected<(int)objects.size();}
void compactVertices(MeshObject& o){
 std::vector<int> used(o.vertices.size(),0);
 for(const auto& f:o.faces)for(int v:f)if(v>=0&&v<(int)used.size())used[v]=1;
 std::vector<int> map(o.vertices.size(),-1);std::vector<Vector3> verts;
 for(int i=0;i<(int)o.vertices.size();i++)if(used[i]){map[i]=(int)verts.size();verts.push_back(o.vertices[i]);}
 for(auto& f:o.faces)for(int& v:f)v=map[v];
 o.vertices.swap(verts);
}
// Split polygon topology against a screen-defined plane, preserving shared edge intersections.
bool cutMeshByScreenLine(MeshObject& o,Vector2 start,Vector2 finish,bool allFaces,int selectedFace){
 if(Vector2Distance(start,finish)<6.0f)return false;
 Ray r0=GetScreenToWorldRay(start,camera),r1=GetScreenToWorldRay(finish,camera);
 Vector3 normal=Vector3CrossProduct(r0.direction,r1.direction);
 if(Vector3Length(normal)<1e-6f)return false;
 normal=Vector3Normalize(normal);
 const Vector3 planePoint=Vector3Subtract(r0.position,o.position);
 std::vector<std::vector<int>> result;
 std::map<std::pair<int,int>,int> intersections;
 bool changed=false;
 auto edgeIntersection=[&](int a,int b,float da,float db)->int{
  auto key=std::minmax(a,b);
  auto it=intersections.find({key.first,key.second});
  if(it!=intersections.end())return it->second;
  float t=da/(da-db);
  Vector3 p=Vector3Add(o.vertices[a],Vector3Scale(Vector3Subtract(o.vertices[b],o.vertices[a]),t));
  int id=(int)o.vertices.size();o.vertices.push_back(p);
  intersections[{key.first,key.second}]=id;
  return id;
 };
 const int count=(int)o.faces.size();
 for(int fi=0;fi<count;fi++){
  const auto polygon=o.faces[fi];
  if((!allFaces&&fi!=selectedFace)||polygon.size()<3){result.push_back(polygon);continue;}
  std::vector<int> positive,negative;
  bool hasPositive=false,hasNegative=false;
  for(int id:polygon){
   float d=Vector3DotProduct(normal,Vector3Subtract(o.vertices[id],planePoint));
   if(d>1e-5f)hasPositive=true;
   if(d< -1e-5f)hasNegative=true;
  }
  if(!hasPositive||!hasNegative){result.push_back(polygon);continue;}
  for(size_t j=0;j<polygon.size();j++){
   int a=polygon[j],b=polygon[(j+1)%polygon.size()];
   float da=Vector3DotProduct(normal,Vector3Subtract(o.vertices[a],planePoint));
   float db=Vector3DotProduct(normal,Vector3Subtract(o.vertices[b],planePoint));
   if(da>=-1e-5f)positive.push_back(a);
   if(da<=1e-5f)negative.push_back(a);
   if((da>1e-5f&&db< -1e-5f)||(da< -1e-5f&&db>1e-5f)){
    int id=edgeIntersection(a,b,da,db);
    positive.push_back(id);negative.push_back(id);
   }
  }
  auto clean=[](std::vector<int>& p){
   p.erase(std::unique(p.begin(),p.end()),p.end());
   if(p.size()>1&&p.front()==p.back())p.pop_back();
  };
  clean(positive);clean(negative);
  if(positive.size()>=3&&negative.size()>=3){
   result.push_back(positive);result.push_back(negative);changed=true;
  }else result.push_back(polygon);
 }
 if(!changed)return false;
 checkpoint();
 o.faces.swap(result);
 compactVertices(o);
 face=-1;sub=-1;
 return true;
}
void removeSelectedVertex(){
 if(mode!=1||!validComponent())return;
 auto& o=objects[selected];if(sub<0||sub>=(int)o.vertices.size())return;
 checkpoint();int v=sub;
 o.faces.erase(std::remove_if(o.faces.begin(),o.faces.end(),[&](const std::vector<int>& f){return std::find(f.begin(),f.end(),v)!=f.end();}),o.faces.end());
 compactVertices(o);sub=-1;face=-1;
}
// Weld only an existing polygon edge: merging non-adjacent corners can fold an n-gon.
bool verticesSharePolygonEdge(const MeshObject& o,int a,int b){
 for(const auto& f:o.faces)for(size_t i=0;i<f.size();i++)
  if((f[i]==a&&f[(i+1)%f.size()]==b)||(f[i]==b&&f[(i+1)%f.size()]==a))return true;
 return false;
}
void cleanWeldedFaces(MeshObject& o){
 for(auto& f:o.faces){
  std::vector<int> clean;clean.reserve(f.size());
  for(int v:f)if(clean.empty()||clean.back()!=v)clean.push_back(v);
  if(clean.size()>1&&clean.front()==clean.back())clean.pop_back();
  f.swap(clean);
 }
 o.faces.erase(std::remove_if(o.faces.begin(),o.faces.end(),[](const std::vector<int>& f){
  if(f.size()<3)return true;
  std::set<int> unique(f.begin(),f.end());
  return unique.size()!=f.size();
 }),o.faces.end());
}
void weldSelectedVertex(){
 if(mode!=1||!validComponent())return;
 auto& o=objects[selected];if(sub<0||sub>=(int)o.vertices.size()||o.vertices.size()<2)return;
 int other=-1;float nearest=1.0e20f;
 for(int i=0;i<(int)o.vertices.size();i++)if(i!=sub){float d=Vector3Distance(o.vertices[sub],o.vertices[i]);if(d<nearest){nearest=d;other=i;}}
 if(other<0)return;
 checkpoint();Vector3 midpoint=Vector3Scale(Vector3Add(o.vertices[sub],o.vertices[other]),.5f);
 o.vertices[other]=midpoint;
 for(auto& f:o.faces)for(int& v:f)if(v==sub)v=other;
 cleanWeldedFaces(o);
 compactVertices(o);sub=-1;face=-1;
}
void breakSelectedVertex(){
 if(mode!=1||!validComponent())return;auto& o=objects[selected];
 if(sub<0||sub>=(int)o.vertices.size())return;
 int uses=0;for(const auto& f:o.faces)if(std::find(f.begin(),f.end(),sub)!=f.end())uses++;
 if(uses<2)return;
 checkpoint();bool first=true;
 for(auto& f:o.faces)for(int& v:f)if(v==sub){if(first)first=false;else{o.vertices.push_back(o.vertices[sub]);v=(int)o.vertices.size()-1;}}
}
void splitSelectedEdge(){
 if(mode!=2||!validComponent())return;auto& o=objects[selected];
 std::set<std::pair<int,int>> all;
 for(const auto& f:o.faces)for(size_t j=0;j<f.size();j++){int a=f[j],b=f[(j+1)%f.size()];if(a>b)std::swap(a,b);all.insert({a,b});}
 if(sub<0||sub>=(int)all.size())return;
 auto it=all.begin();std::advance(it,sub);int a=it->first,b=it->second;
 checkpoint();int mid=(int)o.vertices.size();o.vertices.push_back(Vector3Scale(Vector3Add(o.vertices[a],o.vertices[b]),.5f));
 for(auto& f:o.faces)for(size_t j=0;j<f.size();j++){int x=f[j],y=f[(j+1)%f.size()];if((x==a&&y==b)||(x==b&&y==a)){f.insert(f.begin()+j+1,mid);break;}}
 sub=-1;face=-1;
}
void removeSelectedEdge(){
 if(mode!=2||!validComponent())return;auto& o=objects[selected];
 std::set<std::pair<int,int>> all;
 for(const auto& f:o.faces)for(size_t j=0;j<f.size();j++){int a=f[j],b=f[(j+1)%f.size()];if(a>b)std::swap(a,b);all.insert({a,b});}
 if(sub<0||sub>=(int)all.size())return;
 auto it=all.begin();std::advance(it,sub);int a=it->first,b=it->second;
 std::vector<int> adjacent;
 for(int i=0;i<(int)o.faces.size();i++){const auto& f=o.faces[i];for(size_t j=0;j<f.size();j++)if((f[j]==a&&f[(j+1)%f.size()]==b)||(f[j]==b&&f[(j+1)%f.size()]==a)){adjacent.push_back(i);break;}}
 if(adjacent.size()!=2)return;
 const auto first=o.faces[adjacent[0]],second=o.faces[adjacent[1]];
 std::vector<int> merged;
 auto append=[&](const std::vector<int>& f,int start,int end){int j=start;for(int n=0;n<(int)f.size();n++){merged.push_back(f[j]);if(f[j]==end)break;j=(j+1)%(int)f.size();}};
 auto findIndex=[](const std::vector<int>& f,int v){return (int)(std::find(f.begin(),f.end(),v)-f.begin());};
 append(first,(findIndex(first,b)+1)%(int)first.size(),b);
 append(second,(findIndex(second,a)+1)%(int)second.size(),a);
 std::set<int> unique(merged.begin(),merged.end());if(unique.size()!=merged.size()||merged.size()<3)return;
 checkpoint();o.faces.erase(o.faces.begin()+adjacent[1]);o.faces[adjacent[0]]=merged;sub=-1;face=-1;
}
void flipSelectedFace(){
 if(mode!=3||!validComponent())return;auto& o=objects[selected];if(face<0||face>=(int)o.faces.size())return;
 checkpoint();std::reverse(o.faces[face].begin(),o.faces[face].end());
}
void detachSelectedFace(){
 if(mode!=3||!validComponent())return;auto& o=objects[selected];if(face<0||face>=(int)o.faces.size())return;
 checkpoint();auto& f=o.faces[face];for(int& v:f){o.vertices.push_back(o.vertices[v]);v=(int)o.vertices.size()-1;}
}
void removeSelectedFace(){
 if(mode!=3||!validComponent())return;auto& o=objects[selected];if(face<0||face>=(int)o.faces.size())return;
 checkpoint();o.faces.erase(o.faces.begin()+face);face=-1;sub=-1;compactVertices(o);
}
void outlineSelectedFace(){
 if(mode!=3||!validComponent())return;auto& o=objects[selected];if(face<0||face>=(int)o.faces.size())return;
 auto old=o.faces[face];if(old.size()<3)return;Vector3 center{};
 for(int v:old)center=Vector3Add(center,o.vertices[v]);center=Vector3Scale(center,1.0f/old.size());
 checkpoint();std::vector<int> outer;
 for(int v:old){outer.push_back((int)o.vertices.size());o.vertices.push_back(Vector3Add(center,Vector3Scale(Vector3Subtract(o.vertices[v],center),1.2f)));}
 for(size_t i=0;i<old.size();i++)o.faces.push_back({old[i],old[(i+1)%old.size()],outer[(i+1)%outer.size()],outer[i]});
 o.faces[face]=outer;
}


std::vector<std::pair<int,int>> edges(const MeshObject& o);
std::vector<std::pair<int,int>> boundaryEdges(const MeshObject& o){
 std::map<std::pair<int,int>,int> count;
 for(const auto& f:o.faces)for(size_t i=0;i<f.size();i++){
  int a=f[i],b=f[(i+1)%f.size()];if(a>b)std::swap(a,b);count[{a,b}]++;
 }
 std::vector<std::pair<int,int>> result;
 for(const auto& item:count)if(item.second==1)result.push_back(item.first);
 return result;
}
bool selectedBoundaryEdge(int& a,int& b){
 if((mode!=2&&mode!=4)||!validComponent())return false;
 auto all=edges(objects[selected]);if(sub<0||sub>=(int)all.size())return false;
 a=all[sub].first;b=all[sub].second;
 auto boundary=boundaryEdges(objects[selected]);
 return std::find(boundary.begin(),boundary.end(),std::make_pair(a,b))!=boundary.end();
}
std::vector<int> selectedBoundaryLoop(){
 int a,b;if(!selectedBoundaryEdge(a,b))return {};
 auto boundary=boundaryEdges(objects[selected]);
 std::map<int,std::vector<int>> neighbors;
 for(auto [x,y]:boundary){neighbors[x].push_back(y);neighbors[y].push_back(x);}
 if(neighbors[a].size()!=2||neighbors[b].size()!=2)return {};
 std::vector<int> loop{a,b};int previous=a,current=b;
 for(size_t step=0;step<=boundary.size();step++){
  if(neighbors[current].size()!=2)return {};
  int next=neighbors[current][0]==previous?neighbors[current][1]:neighbors[current][0];
  if(next==a)return loop.size()>=3?loop:std::vector<int>{};
  if(std::find(loop.begin(),loop.end(),next)!=loop.end())return {};
  loop.push_back(next);previous=current;current=next;
 }
 return {};
}
void capBoundary(){
 auto loop=selectedBoundaryLoop();if(loop.empty())return;
 checkpoint();std::reverse(loop.begin(),loop.end());objects[selected].faces.push_back(loop);sub=-1;face=-1;
}
void extendBoundary(){
 auto loop=selectedBoundaryLoop();if(loop.empty())return;
 auto& o=objects[selected];Vector3 center{};
 for(int v:loop)center=Vector3Add(center,o.vertices[v]);
 center=Vector3Scale(center,1.0f/loop.size());
 checkpoint();std::vector<int> outer;
 for(int v:loop){
  Vector3 direction=Vector3Subtract(o.vertices[v],center);
  outer.push_back((int)o.vertices.size());
  o.vertices.push_back(Vector3Add(o.vertices[v],Vector3Scale(direction,.25f)));
 }
 for(size_t i=0;i<loop.size();i++)o.faces.push_back({loop[i],loop[(i+1)%loop.size()],outer[(i+1)%loop.size()],outer[i]});
 sub=-1;face=-1;
}
void extrudeSelectedVertex(){
 if(mode!=1||!validComponent())return;auto& o=objects[selected];
 if(sub<0||sub>=(int)o.vertices.size())return;
 checkpoint();Vector3 p=o.vertices[sub];o.vertices.push_back(Vector3Add(p,Vector3{0,0,.5f}));sub=(int)o.vertices.size()-1;
}
void extrudeSelectedEdge(){
 int a,b;if(!selectedBoundaryEdge(a,b))return;auto& o=objects[selected];
 Vector3 pa=o.vertices[a],pb=o.vertices[b];
 checkpoint();int c=(int)o.vertices.size();o.vertices.push_back(Vector3Add(pa,Vector3{0,0,.5f}));
 int d=(int)o.vertices.size();o.vertices.push_back(Vector3Add(pb,Vector3{0,0,.5f}));
 o.faces.push_back({a,b,d,c});sub=-1;face=-1;
}


void turnSelectedEdge(){
 if(mode!=2||!validComponent())return;
 auto& o=objects[selected];auto all=edges(o);
 if(sub<0||sub>=(int)all.size())return;
 int a=all[sub].first,b=all[sub].second;
 std::vector<int> adjacent;
 for(int i=0;i<(int)o.faces.size();i++){
  const auto& f=o.faces[i];if(f.size()!=3)continue;
  if(std::find(f.begin(),f.end(),a)!=f.end()&&std::find(f.begin(),f.end(),b)!=f.end())adjacent.push_back(i);
 }
 if(adjacent.size()!=2)return;
 const auto& f=o.faces[adjacent[0]];const auto& g=o.faces[adjacent[1]];
 int c=-1,d=-1;for(int v:f)if(v!=a&&v!=b)c=v;for(int v:g)if(v!=a&&v!=b)d=v;
 if(c<0||d<0||c==d)return;
 // Reject an existing diagonal: a turn must not create a non-manifold duplicate.
 if(std::find(all.begin(),all.end(),std::make_pair(std::min(c,d),std::max(c,d)))!=all.end())return;
 // Preserve the original triangle winding along the shared edge.
 bool ab=false;for(int i=0;i<3;i++)if(f[i]==a&&f[(i+1)%3]==b)ab=true;
 checkpoint();
 if(ab){o.faces[adjacent[0]]={c,a,d};o.faces[adjacent[1]]={d,b,c};}
 else{o.faces[adjacent[0]]={c,b,d};o.faces[adjacent[1]]={d,a,c};}
 sub=-1;face=-1;
}
void bevelSelectedPolygon(){
 if(mode!=3||!validComponent())return;
 auto& o=objects[selected];if(face<0||face>=(int)o.faces.size())return;
 auto old=o.faces[face];if(old.size()<3)return;
 Vector3 center{},normal{};
 for(int v:old)center=Vector3Add(center,o.vertices[v]);
 center=Vector3Scale(center,1.0f/old.size());
 for(size_t i=0;i<old.size();i++){
  Vector3 a=Vector3Subtract(o.vertices[old[i]],center);
  Vector3 b=Vector3Subtract(o.vertices[old[(i+1)%old.size()]],center);
  normal=Vector3Add(normal,Vector3CrossProduct(a,b));
 }
 if(Vector3Length(normal)<1e-6f)return;
 normal=Vector3Normalize(normal);
 checkpoint();
 std::vector<int> top;
 for(int v:old){
  Vector3 inner=Vector3Lerp(center,o.vertices[v],.8f);
  top.push_back((int)o.vertices.size());
  o.vertices.push_back(Vector3Add(inner,Vector3Scale(normal,.2f)));
 }
 o.faces[face]=top;
 for(size_t i=0;i<old.size();i++)
  o.faces.push_back({old[i],old[(i+1)%old.size()],top[(i+1)%top.size()],top[i]});
}


void chamferSelectedVertex(){
 if(mode!=1||!validComponent())return;
 auto& o=objects[selected];if(sub<0||sub>=(int)o.vertices.size())return;
 int original=sub;std::set<int> neighbors;
 for(const auto& f:o.faces)for(size_t i=0;i<f.size();i++)if(f[i]==original){
  neighbors.insert(f[(i+1)%f.size()]);neighbors.insert(f[(i+f.size()-1)%f.size()]);
 }
 if(neighbors.size()<2)return;
 checkpoint();
 // Each incident face receives its own cut corners, avoiding dangling vertex references.
 std::vector<std::vector<int>> replacement;
 for(const auto& f:o.faces){
  auto it=std::find(f.begin(),f.end(),original);
  if(it==f.end()){replacement.push_back(f);continue;}
  int k=(int)(it-f.begin()),n=(int)f.size();
  int previous=f[(k+n-1)%n],next=f[(k+1)%n];
  int p=(int)o.vertices.size();
  o.vertices.push_back(Vector3Lerp(o.vertices[original],o.vertices[previous],.25f));
  int q=(int)o.vertices.size();
  o.vertices.push_back(Vector3Lerp(o.vertices[original],o.vertices[next],.25f));
  std::vector<int> cut;cut.reserve(n+1);
  for(int j=0;j<n;j++){if(j==k){cut.push_back(p);cut.push_back(q);}else cut.push_back(f[j]);}
  replacement.push_back(cut);
 }
 o.faces.swap(replacement);compactVertices(o);sub=-1;face=-1;
}
void connectSelectedFaceVertices(){
 if(mode!=3||!validComponent())return;
 auto& o=objects[selected];if(face<0||face>=(int)o.faces.size())return;
 const auto f=o.faces[face];if(f.size()<4)return;
 // Split a polygon by a non-adjacent diagonal between vertices 0 and 2.
 std::vector<int> first{f[0],f[1],f[2]};
 std::vector<int> second{f[0]};second.insert(second.end(),f.begin()+2,f.end());
 checkpoint();o.faces[face]=first;o.faces.push_back(second);
}
void selectBoundaryLoop(){
 // The current editor has single-edge selection; keep the edge active and expose
 // the detected loop length rather than falsely implying multi-selection.
}


bool targetWeldArmed=false;
int targetWeldSource=-1,targetWeldObject=-1;
void armTargetWeld(){
 if(mode!=1||!validComponent())return;
 const auto& o=objects[selected];
 if(sub<0||sub>=(int)o.vertices.size())return;
 targetWeldArmed=true;targetWeldSource=sub;targetWeldObject=selected;
}
void applyTargetWeld(int destination){
 if(!targetWeldArmed)return;
 if(mode!=1||selected!=targetWeldObject||!validComponent())return;
 auto& o=objects[selected];
 if(destination<0||destination>=(int)o.vertices.size()||targetWeldSource<0||targetWeldSource>=(int)o.vertices.size()||destination==targetWeldSource)return;
 checkpoint();const int source=targetWeldSource;
 for(auto& f:o.faces)for(int& v:f)if(v==source)v=destination;
 cleanWeldedFaces(o);
 // Preserve the destination vertex index after compacting unused vertices.
 std::set<int> surviving;
 for(const auto& f:o.faces)for(int v:f)surviving.insert(v);
 int nextSource=-1,index=0;
 for(int v:surviving){if(v==destination)nextSource=index;index++;}
 compactVertices(o);
 targetWeldSource=nextSource;
 if(nextSource<0)targetWeldArmed=false;
 sub=nextSource;face=-1;
}

void pick(Vector2 mouse){Ray ray=GetScreenToWorldRay(mouse,camera);float nearest=1e20f;int best=-1,bf=-1;for(int oi=0;oi<(int)objects.size();oi++){auto&o=objects[oi];for(int fi=0;fi<(int)o.faces.size();fi++){auto&f=o.faces[fi];for(size_t j=1;j+1<f.size();j++){auto hit=GetRayCollisionTriangle(ray,world(o,f[0]),world(o,f[j]),world(o,f[j+1]));if(!hit.hit)hit=GetRayCollisionTriangle(ray,world(o,f[0]),world(o,f[j+1]),world(o,f[j]));if(hit.hit&&hit.distance<nearest){nearest=hit.distance;best=oi;bf=fi;}}}}selected=best;face=bf;}

std::vector<std::pair<int,int>> edges(const MeshObject& o){
 std::set<std::pair<int,int>> e;
 for(const auto& f:o.faces)for(size_t j=0;j<f.size();j++){
  int a=f[j],b=f[(j+1)%f.size()];if(a>b)std::swap(a,b);e.insert({a,b});
 }
 return {e.begin(),e.end()};
}
std::set<int> rectangleSelected;
int rectangleObject=-1;
bool rectanglePending=false,rectangleDragging=false;
Vector2 rectangleStart{},rectangleEnd{};
bool rectangleContains(Vector2 p){
 return p.x>=std::min(rectangleStart.x,rectangleEnd.x)&&p.x<=std::max(rectangleStart.x,rectangleEnd.x)&&p.y>=std::min(rectangleStart.y,rectangleEnd.y)&&p.y<=std::max(rectangleStart.y,rectangleEnd.y);
}
void selectRectangle(){
 if(selected<0||selected>=(int)objects.size()||mode==0)return;
 auto& o=objects[selected];
 if(rectangleObject!=selected||(!IsKeyDown(KEY_LEFT_CONTROL)&&!IsKeyDown(KEY_RIGHT_CONTROL)))rectangleSelected.clear();
 rectangleObject=selected;
 if(mode==1){for(int i=0;i<(int)o.vertices.size();i++)if(rectangleContains(GetWorldToScreen(world(o,i),camera)))rectangleSelected.insert(i);}
 if(mode==2||mode==4){
  auto e=edges(o);
  for(int i=0;i<(int)e.size();i++){
   if(!rectangleContains(GetWorldToScreen(world(o,e[i].first),camera))||!rectangleContains(GetWorldToScreen(world(o,e[i].second),camera)))continue;
   if(mode==4){
    int count=0;
    for(const auto& f:o.faces)for(size_t j=0;j<f.size();j++){
     int a=f[j],b=f[(j+1)%f.size()];
     if(std::min(a,b)==e[i].first&&std::max(a,b)==e[i].second)count++;
    }
    if(count!=1)continue;
   }
   rectangleSelected.insert(i);
  }
 }
 if(mode==3){for(int i=0;i<(int)o.faces.size();i++){
  bool inside=!o.faces[i].empty();
  for(int v:o.faces[i])if(!rectangleContains(GetWorldToScreen(world(o,v),camera))){inside=false;break;}
  if(inside)rectangleSelected.insert(i);
 }}
 sub=-1;face=-1;
 if(rectangleSelected.size()==1){if(mode==3)face=*rectangleSelected.begin();else sub=*rectangleSelected.begin();}
}
std::vector<int> active(const MeshObject& o){
 if(selected>=0&&selected<(int)objects.size()&&&o==&objects[selected]&&rectangleObject==selected&&!rectangleSelected.empty()){
  std::set<int> vertices;
  if(mode==1)for(int v:rectangleSelected)if(v>=0&&v<(int)o.vertices.size())vertices.insert(v);
  if(mode==2||mode==4){auto e=edges(o);for(int i:rectangleSelected)if(i>=0&&i<(int)e.size()){vertices.insert(e[i].first);vertices.insert(e[i].second);}}
  if(mode==3)for(int i:rectangleSelected)if(i>=0&&i<(int)o.faces.size())for(int v:o.faces[i])vertices.insert(v);
  return {vertices.begin(),vertices.end()};
 }

 if(mode==1&&sub>=0&&sub<(int)o.vertices.size())return {sub};
 if(mode==2||mode==4){auto e=edges(o);if(sub>=0&&sub<(int)e.size())return {e[sub].first,e[sub].second};}
 if(mode==3&&face>=0&&face<(int)o.faces.size())return o.faces[face];
 return {};
}
Vector3 pivot(const MeshObject& o){auto a=active(o);if(a.empty())return o.position;
 Vector3 p{};for(int i:a)p=Vector3Add(p,world(o,i));return Vector3Scale(p,1.0f/a.size());}
float segmentDistance(Vector2 p,Vector2 a,Vector2 b){Vector2 d=Vector2Subtract(b,a),v=Vector2Subtract(p,a);
 float t=std::clamp(Vector2DotProduct(v,d)/std::max(1.0f,Vector2DotProduct(d,d)),0.0f,1.0f);
 return Vector2Distance(p,Vector2Add(a,Vector2Scale(d,t)));}
int handleHit(Vector2 mouse){if(selected<0||selected>=(int)objects.size()||(mode!=0&&active(objects[selected]).empty()))return -1;Vector3 p=pivot(objects[selected]);
 Vector2 a=GetWorldToScreen(p,camera);int best=-1;float distance=16;
 for(int i=0;i<3;i++){
  if(tool==2){
   Vector3 u=i==0?Vector3{0,1,0}:Vector3{1,0,0};
   Vector3 v=i==2?Vector3{0,1,0}:Vector3{0,0,1};
   // Z-up: blue Z rotation ring lies in the horizontal XY plane.
   float ringDistance=1.0e9f;
   for(int j=0;j<64;j++){
    float t0=6.2831853f*j/64.0f,t1=6.2831853f*(j+1)/64.0f;
    Vector3 q0=Vector3Add(p,Vector3Scale(Vector3Add(Vector3Scale(u,cosf(t0)),Vector3Scale(v,sinf(t0))),1.35f*gizmoSize));
    Vector3 q1=Vector3Add(p,Vector3Scale(Vector3Add(Vector3Scale(u,cosf(t1)),Vector3Scale(v,sinf(t1))),1.35f*gizmoSize));
    ringDistance=std::min(ringDistance,segmentDistance(mouse,GetWorldToScreen(q0,camera),GetWorldToScreen(q1,camera)));
   }
   if(ringDistance<distance){distance=ringDistance;best=i;}
  }else{
   Vector3 d{};(&d.x)[i]=1.35f*gizmoSize;
   Vector2 b=GetWorldToScreen(Vector3Add(p,d),camera);
   float x=segmentDistance(mouse,a,b);
   // End-cap picking allows selecting arrows and scale cubes outside the mesh silhouette.
   x=std::min(x,Vector2Distance(mouse,GetWorldToScreen(Vector3Add(p,Vector3Scale(d,1.6f/1.35f)),camera)));
   if(x<distance){distance=x;best=i;}
  }
 }
 return best;
}
void pickComponent(Vector2 mouse){
 if(selected<0||selected>=(int)objects.size())return;
 if(mode==3){Ray ray=GetScreenToWorldRay(mouse,camera);float nearest=1e20f;int hit=-1;const auto& o=objects[selected];for(int fi=0;fi<(int)o.faces.size();fi++){const auto& f=o.faces[fi];for(size_t j=1;j+1<f.size();j++){auto collision=GetRayCollisionTriangle(ray,world(o,f[0]),world(o,f[j]),world(o,f[j+1]));if(!collision.hit)collision=GetRayCollisionTriangle(ray,world(o,f[0]),world(o,f[j+1]),world(o,f[j]));if(collision.hit&&collision.distance<nearest){nearest=collision.distance;hit=fi;}}}face=hit;sub=hit;return;}
 // Sub-object selection is restricted to the actor already selected in Object mode.
 sub=-1;auto& o=objects[selected];
 float best=16.0f;
 if(mode==1){for(int i=0;i<(int)o.vertices.size();i++){
  float d=Vector2Distance(mouse,GetWorldToScreen(world(o,i),camera));
  if(d<best){best=d;sub=i;}
 }}
 else if(mode==2||mode==4){auto e=edges(o);for(int i=0;i<(int)e.size();i++){
  float d=segmentDistance(mouse,GetWorldToScreen(world(o,e[i].first),camera),GetWorldToScreen(world(o,e[i].second),camera));
  if(d<best){if(mode==4){auto bounds=boundaryEdges(o);if(std::find(bounds.begin(),bounds.end(),e[i])==bounds.end())continue;}best=d;sub=i;}
 }}
 face=-1;
}
struct DragState{bool active=false;int axis=-1;Vector2 start{};Vector3 startPos{},center{};std::vector<Vector3> startVertices;std::vector<int> affected;};
DragState drag;
void startDrag(int axis){if(selected<0)return;checkpoint();auto& o=objects[selected];
 drag={};drag.active=true;drag.axis=axis;drag.start=GetMousePosition();drag.startPos=o.position;
 drag.center=pivot(o);drag.startVertices=o.vertices;drag.affected=active(o);}
void applyDrag(){if(!drag.active||selected<0)return;auto& o=objects[selected];
 Vector3 axis{};(&axis.x)[drag.axis]=1.35f*gizmoSize;
 Vector2 a=GetWorldToScreen(drag.center,camera),b=GetWorldToScreen(Vector3Add(drag.center,axis),camera);
 Vector2 direction=Vector2Subtract(b,a),delta=Vector2Subtract(GetMousePosition(),drag.start);
 float amount=Vector2DotProduct(delta,direction)/std::max(1.0f,Vector2DotProduct(direction,direction))*1.35f*gizmoSize;
 if(mode==0){
  o.position=drag.startPos;
  o.vertices=drag.startVertices;
  if(tool==1){(&o.position.x)[drag.axis]+=amount;}
  else if(tool==3){float k=std::max(0.05f,1+amount);for(auto &v:o.vertices){float *a=&v.x; a[drag.axis]*=k;}}
  else if(tool==2){float c=cosf(amount),sn=sinf(amount);for(auto &v:o.vertices){Vector3 r=v;
    if(drag.axis==0){r.y=v.y*c-v.z*sn;r.z=v.y*sn+v.z*c;}
    if(drag.axis==1){r.x=v.x*c+v.z*sn;r.z=-v.x*sn+v.z*c;}
    if(drag.axis==2){r.x=v.x*c-v.y*sn;r.y=v.x*sn+v.y*c;}
    v=r;}}
  return;}
 o.vertices=drag.startVertices;
 Vector3 center=Vector3Subtract(drag.center,drag.startPos);
 for(int i:drag.affected){if(i<0||i>=(int)o.vertices.size())continue;
  Vector3 original=drag.startVertices[i];
  if(tool==1){(&o.vertices[i].x)[drag.axis]+=amount;}
  else if(tool==3){float k=std::max(0.05f,1+amount);(&o.vertices[i].x)[drag.axis]=(&center.x)[drag.axis]+((&original.x)[drag.axis]-(&center.x)[drag.axis])*k;}
  else if(tool==2){Vector3 p=Vector3Subtract(original,center),r=p;float c=cosf(amount),s=sinf(amount);
   if(drag.axis==0){r.y=p.y*c-p.z*s;r.z=p.y*s+p.z*c;}
   if(drag.axis==1){r.x=p.x*c+p.z*s;r.z=-p.x*s+p.z*c;}
   if(drag.axis==2){r.x=p.x*c-p.y*s;r.y=p.x*s+p.y*c;}
   o.vertices[i]=Vector3Add(center,r);
  }
 }
}

const char* connectionPrompt =
"N3DLITE_AI_REQUEST\n"
"protocol=1\n"
"action=connect\n"
"source=N3DLite\n"
"repository=https://github.com/3dmk/3dit\n"
"branch=main\n"
"workflow=GitHub\n"
"local_path=C:\\GPT\\CoreModel_GitHub\n"
"authority=candidate-only\n"
"\n"
"Connect to the N3DLite C++23/raylib editor project using its GitHub repository. "
"Read the current repository source before making changes. Treat all AI edits as "
"candidate changes until built and validated on the user's PC. Preserve the "
"previous known-good build. Implement requested changes in the GitHub source, "
"then tell me to double-click Update-and-Launch.cmd to pull, build, and launch. "
"Do not claim direct live access to my local editor or files.\n";

Font uiFont{};bool uiFontLoaded=false;
void UiText(const char* value,int x,int y,int size,Color color){
 if(uiFontLoaded)DrawTextEx(uiFont,value,{(float)x,(float)y},(float)size,1.0f,color);
 else DrawText(value,x,y,size,color);
}
// Compact transform icons drawn with ImGui primitives, no external image assets.
bool TransformIconButton(const char* id,int kind,bool active){
 ImGui::PushID(id);
 ImVec2 p=ImGui::GetCursorScreenPos();
 const ImVec2 size(37,34);
 bool clicked=ImGui::InvisibleButton("##icon",size);
 bool hovered=ImGui::IsItemHovered();
 ImDrawList* dl=ImGui::GetWindowDrawList();
 ImU32 bg=ImGui::GetColorU32(active?ImVec4(.23f,.42f,.62f,1.0f):hovered?ImVec4(.27f,.30f,.35f,1.0f):ImVec4(.15f,.18f,.22f,.95f));
 ImU32 fg=IM_COL32(229,234,241,255);
 dl->AddRectFilled(p,ImVec2(p.x+size.x,p.y+size.y),bg,5.0f);
 dl->AddRect(p,ImVec2(p.x+size.x,p.y+size.y),active?IM_COL32(114,183,255,255):IM_COL32(82,91,104,255),5.0f);
 ImVec2 c(p.x+18.5f,p.y+17.0f);
 if(kind==1){ // Move: four directional arrows
  dl->AddLine(ImVec2(c.x-10,c.y),ImVec2(c.x+10,c.y),fg,2);
  dl->AddLine(ImVec2(c.x,c.y-10),ImVec2(c.x,c.y+10),fg,2);
  dl->AddTriangleFilled(ImVec2(c.x,c.y-13),ImVec2(c.x-4,c.y-7),ImVec2(c.x+4,c.y-7),fg);
  dl->AddTriangleFilled(ImVec2(c.x,c.y+13),ImVec2(c.x-4,c.y+7),ImVec2(c.x+4,c.y+7),fg);
  dl->AddTriangleFilled(ImVec2(c.x-13,c.y),ImVec2(c.x-7,c.y-4),ImVec2(c.x-7,c.y+4),fg);
  dl->AddTriangleFilled(ImVec2(c.x+13,c.y),ImVec2(c.x+7,c.y-4),ImVec2(c.x+7,c.y+4),fg);
 }else if(kind==2){ // Rotate: circular arrow
  dl->PathArcTo(c,10.0f,-2.6f,2.2f,24);dl->PathStroke(fg,0,2.1f);
  dl->AddTriangleFilled(ImVec2(c.x-7,c.y+10),ImVec2(c.x-12,c.y+3),ImVec2(c.x-3,c.y+4),fg);
 }else{ // Scale: diagonal expansion
  dl->AddLine(ImVec2(c.x-8,c.y+8),ImVec2(c.x+8,c.y-8),fg,2.2f);
  dl->AddRect(ImVec2(c.x-12,c.y+5),ImVec2(c.x-5,c.y+12),fg,0,0,1.8f);
  dl->AddRectFilled(ImVec2(c.x+5,c.y-12),ImVec2(c.x+12,c.y-5),fg);
 }
 if(ImGui::IsItemHovered())ImGui::SetTooltip("%s (%s)",kind==1?"Move":kind==2?"Rotate":"Scale",kind==1?"W":kind==2?"E":"R");
 ImGui::PopID();
 return clicked;
}
int main(){
 // Persist the native editor window rectangle separately from ImGui docking.
 int savedX=0,savedY=0,savedW=1280,savedH=760;
 bool hasSavedWindow=false;
 {
  std::ifstream file("N3DLite-window.cfg");
  if(file>>savedX>>savedY>>savedW>>savedH){
   hasSavedWindow=savedW>=640&&savedH>=400&&savedW<=16384&&savedH<=16384;
  }
 }
 SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT);
 InitWindow(hasSavedWindow?savedW:1280,hasSavedWindow?savedH:760,"N3DLite Native v0.8 - C++23");
 ApplyN3DLiteTitlebarTheme();
 if(hasSavedWindow)SetWindowPosition(savedX,savedY);
 SetTargetFPS(60);
 Shader smoothShader=LoadShaderFromMemory(smoothVS,smoothFS);
 const bool smoothShaderReady=smoothShader.id!=0;
 const int locBase=GetShaderLocation(smoothShader,"baseColor");
 const int locRough=GetShaderLocation(smoothShader,"roughness");
 const int locMetal=GetShaderLocation(smoothShader,"metallic");
 const int locSpec=GetShaderLocation(smoothShader,"specular");
 const int locCamera=GetShaderLocation(smoothShader,"cameraPosition");
 rlImGuiSetup(true);
 ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
 ImGui::GetIO().IniFilename="N3DLite-layout.ini";
 ImGui::LoadIniSettingsFromDisk("N3DLite-layout.ini");
 auto saveWindowLayout=[](){
  if(IsWindowMinimized()||IsWindowFullscreen())return;
  const Vector2 pos=GetWindowPosition();
  const int width=GetScreenWidth(),height=GetScreenHeight();
  if(width<640||height<400)return;
  std::ofstream file("N3DLite-window.cfg",std::ios::trunc);
  if(file)file<<(int)pos.x<<" "<<(int)pos.y<<" "<<width<<" "<<height<<"\\n";
 };
 Vector2 lastWindowPos=GetWindowPosition();
 int lastWindowW=GetScreenWidth(),lastWindowH=GetScreenHeight();
 float layoutSaveElapsed=0.0f;
ImGui::StyleColorsDark();
{
 // Unified neutral dark-gray N3DLite editor palette.
 ImGuiStyle& st=ImGui::GetStyle();
 st.WindowRounding=3.0f;st.ChildRounding=3.0f;st.FrameRounding=3.0f;
 st.PopupRounding=3.0f;st.GrabRounding=3.0f;st.TabRounding=3.0f;
 st.WindowBorderSize=1.0f;st.FrameBorderSize=0.0f;
 st.WindowPadding=ImVec2(10,9);st.FramePadding=ImVec2(8,5);
 st.ItemSpacing=ImVec2(8,6);st.ScrollbarSize=12.0f;
 auto& c=st.Colors;
 c[ImGuiCol_Text]=ImVec4(.88f,.88f,.88f,1);
 c[ImGuiCol_TextDisabled]=ImVec4(.53f,.53f,.53f,1);
 c[ImGuiCol_WindowBg]=ImVec4(.15f,.15f,.15f,1);
 c[ImGuiCol_ChildBg]=ImVec4(.16f,.16f,.16f,1);
 c[ImGuiCol_PopupBg]=ImVec4(.18f,.18f,.18f,.98f);
 c[ImGuiCol_Border]=ImVec4(.29f,.29f,.29f,.8f);
 c[ImGuiCol_FrameBg]=ImVec4(.23f,.23f,.23f,1);
 c[ImGuiCol_FrameBgHovered]=ImVec4(.31f,.31f,.31f,1);
 c[ImGuiCol_FrameBgActive]=ImVec4(.36f,.36f,.36f,1);
 c[ImGuiCol_TitleBg]=ImVec4(.12f,.12f,.12f,1);
 c[ImGuiCol_TitleBgActive]=ImVec4(.20f,.20f,.20f,1);
 c[ImGuiCol_MenuBarBg]=ImVec4(.17f,.17f,.17f,1);
 c[ImGuiCol_ScrollbarBg]=ImVec4(.14f,.14f,.14f,1);
 c[ImGuiCol_ScrollbarGrab]=ImVec4(.32f,.32f,.32f,1);
 c[ImGuiCol_ScrollbarGrabHovered]=ImVec4(.42f,.42f,.42f,1);
 c[ImGuiCol_ScrollbarGrabActive]=ImVec4(.51f,.51f,.51f,1);
 c[ImGuiCol_CheckMark]=ImVec4(.75f,.75f,.75f,1);
 c[ImGuiCol_SliderGrab]=ImVec4(.55f,.55f,.55f,1);
 c[ImGuiCol_SliderGrabActive]=ImVec4(.72f,.72f,.72f,1);
 c[ImGuiCol_Button]=ImVec4(.26f,.26f,.26f,1);
 c[ImGuiCol_ButtonHovered]=ImVec4(.36f,.36f,.36f,1);
 c[ImGuiCol_ButtonActive]=ImVec4(.44f,.44f,.44f,1);
 c[ImGuiCol_Header]=ImVec4(.29f,.29f,.29f,1);
 c[ImGuiCol_HeaderHovered]=ImVec4(.39f,.39f,.39f,1);
 c[ImGuiCol_HeaderActive]=ImVec4(.45f,.45f,.45f,1);
 c[ImGuiCol_Separator]=ImVec4(.31f,.31f,.31f,1);
 c[ImGuiCol_SeparatorHovered]=ImVec4(.48f,.48f,.48f,1);
 c[ImGuiCol_SeparatorActive]=ImVec4(.60f,.60f,.60f,1);
 c[ImGuiCol_ResizeGrip]=ImVec4(.35f,.35f,.35f,.55f);
 c[ImGuiCol_ResizeGripHovered]=ImVec4(.50f,.50f,.50f,.8f);
 c[ImGuiCol_ResizeGripActive]=ImVec4(.65f,.65f,.65f,1);
 c[ImGuiCol_Tab]=ImVec4(.19f,.19f,.19f,1);
 c[ImGuiCol_TabHovered]=ImVec4(.35f,.35f,.35f,1);
 c[ImGuiCol_TabSelected]=ImVec4(.28f,.28f,.28f,1);
 c[ImGuiCol_DockingPreview]=ImVec4(.57f,.57f,.57f,.55f);
 c[ImGuiCol_DockingEmptyBg]=ImVec4(.12f,.12f,.12f,1);
 c[ImGuiCol_TableHeaderBg]=ImVec4(.22f,.22f,.22f,1);
 c[ImGuiCol_TableBorderStrong]=ImVec4(.34f,.34f,.34f,1);
 c[ImGuiCol_TableBorderLight]=ImVec4(.26f,.26f,.26f,1);
 c[ImGuiCol_TableRowBg]=ImVec4(.17f,.17f,.17f,1);
 c[ImGuiCol_TableRowBgAlt]=ImVec4(.20f,.20f,.20f,1);
 c[ImGuiCol_NavHighlight]=ImVec4(.65f,.65f,.65f,.8f);
}
if(FileExists("Roboto.ttf")){uiFont=LoadFontEx("Roboto.ttf",32,nullptr,0);uiFontLoaded=uiFont.texture.id!=0;}
camera.position={7,-9,7};camera.target={0,0,0};camera.up={0,0,1};camera.fovy=45;camera.projection=CAMERA_PERSPECTIVE;objects.push_back(box());selected=0;
int connectFeedback=0;
bool crosshairCursorHidden=false;
bool openVertexContext=false;
bool openViewportContext=false;
bool backfaceCulling=false;
Vector2 rightPressPosition{};
bool rightPressInView=false;
bool cutMode=false,sliceMode=false,cutLineStarted=false;
Vector2 cutLineStart{},cutLineEnd{};
bool polygonExtrudePending=false;
int previousSubobjectMode=0;
// Cached visible UI rectangles from the completed ImGui frame. Raylib
// processes input before the next ImGui frame, so block viewport clicks
// over real tool windows rather than the pass-through dock host.
std::vector<Rectangle> uiInputRects;
Vector2 polygonExtrudePress{};
int polygonExtrudeFace=-1;

while(!WindowShouldClose()){
 // Save changes after the window stops moving/resizing; ImGui saves docking
 // positions independently to N3DLite-layout.ini.
 if(!IsWindowMinimized()&&!IsWindowFullscreen()){
  const Vector2 pos=GetWindowPosition();
  const int cw=GetScreenWidth(),ch=GetScreenHeight();
  if(pos.x!=lastWindowPos.x||pos.y!=lastWindowPos.y||cw!=lastWindowW||ch!=lastWindowH){
   lastWindowPos=pos;lastWindowW=cw;lastWindowH=ch;layoutSaveElapsed=0.0f;
  }else{
   layoutSaveElapsed+=GetFrameTime();
   if(layoutSaveElapsed>=1.0f){saveWindowLayout();layoutSaveElapsed=-1000000.0f;}
  }
 }
 if(connectFeedback>0)connectFeedback--;
 int w=GetScreenWidth(),h=GetScreenHeight();
 // Panels are Dear ImGui windows; the central 3D viewport stays raylib.
 // The viewport is raylib-rendered behind ImGui's transparent dockspace.
// WantCaptureMouse can remain true over the dock host and suppress all
// Shift+click interactions. Use the actual viewport rectangle instead.
 const Vector2 pointer=GetMousePosition();
 // Use the actual dockspace central viewport, not hard-coded side-panel
 // widths. A resized/docked panel must never limit extrusion to gizmo area.
 bool inView=pointer.x>=0.0f&&pointer.x<(float)w&&
             pointer.y>=44.0f&&pointer.y<(float)h;
 for(const Rectangle& rect:uiInputRects){
  if(CheckCollisionPointRec(pointer,rect)){inView=false;break;}
 }
 // Do not query ImGui dock nodes here: rlImGuiBegin() has not yet
 // started this frame. Input is validated by raycast for polygon actions.
 // Dockspace geometry belongs to the later UI/render phase.
 bool typing=ImGui::GetIO().WantTextInput;
 if(!typing){
  // Delete acts on the current selection level. Keep every edit undoable.
  if((IsKeyPressed(KEY_DELETE)||IsKeyPressed(KEY_BACKSPACE))&&
     selected>=0&&selected<(int)objects.size()){
   if(mode==0){
    checkpoint();
    objects.erase(objects.begin()+selected);
    selected=-1;
   }else{
    MeshObject& o=objects[selected];
    std::set<int> targets;
    if(rectangleObject==selected)targets=rectangleSelected;
    if(targets.empty()){
     const int index=mode==3?face:sub;
     if(index>=0)targets.insert(index);
    }
    std::set<int> facesToRemove;
    if(mode==3){
     for(int i:targets)if(i>=0&&i<(int)o.faces.size())facesToRemove.insert(i);
    }else if(mode==1){
     std::set<int> vertices;
     for(int i:targets)if(i>=0&&i<(int)o.vertices.size())vertices.insert(i);
     for(int i=0;i<(int)o.faces.size();i++)
      for(int v:o.faces[i])if(vertices.count(v)){facesToRemove.insert(i);break;}
    }else if(mode==2||mode==4){
     const auto all=edges(o);
     const auto boundary=boundaryEdges(o);
     std::set<std::pair<int,int>> edgeTargets;
     for(int i:targets)if(i>=0&&i<(int)all.size()){
      if(mode==2||std::find(boundary.begin(),boundary.end(),all[i])!=boundary.end())
       edgeTargets.insert(all[i]);
     }
     for(int i=0;i<(int)o.faces.size();i++){
      const auto& polygon=o.faces[i];
      for(size_t j=0;j<polygon.size();j++){
       int a=polygon[j],b=polygon[(j+1)%polygon.size()];
       if(a>b)std::swap(a,b);
       if(edgeTargets.count({a,b})){facesToRemove.insert(i);break;}
      }
     }
    }
    if(!facesToRemove.empty()){
     checkpoint();
     for(auto it=facesToRemove.rbegin();it!=facesToRemove.rend();++it)
      o.faces.erase(o.faces.begin()+*it);
     compactVertices(o);
    }
   }
   face=-1;sub=-1;
   rectangleSelected.clear();rectangleObject=-1;
   rectanglePending=false;rectangleDragging=false;
   polygonExtrudePending=false;
   targetWeldArmed=false;targetWeldSource=-1;targetWeldObject=-1;
   drag.active=false;
  }
  if(IsKeyDown(KEY_LEFT_CONTROL)&&IsKeyPressed(KEY_Z)){if(IsKeyDown(KEY_LEFT_SHIFT))redo();else undo();}
  if(IsKeyDown(KEY_LEFT_CONTROL)&&IsKeyPressed(KEY_Y))redo();
  if(IsKeyPressed(KEY_ONE)){mode=1;sub=-1;face=-1;}
  if(IsKeyPressed(KEY_TWO)){mode=2;sub=-1;face=-1;}
  if(IsKeyPressed(KEY_THREE)){mode=3;sub=-1;face=-1;}
  if(IsKeyPressed(KEY_FOUR)){mode=4;sub=-1;face=-1;}
  if(IsKeyPressed(KEY_ZERO)){mode=0;sub=-1;face=-1;}
  if(IsKeyPressed(KEY_W)&&!IsMouseButtonDown(MOUSE_BUTTON_RIGHT))tool=1;
  if(IsKeyPressed(KEY_E)&&!IsMouseButtonDown(MOUSE_BUTTON_RIGHT))tool=2;
  if(IsKeyPressed(KEY_R)&&!IsMouseButtonDown(MOUSE_BUTTON_RIGHT))tool=3;
  // +/- change only the visible gizmo size, never the object geometry.
  if(IsKeyPressed(KEY_EQUAL)||IsKeyPressed(KEY_KP_ADD))
   gizmoSize=std::min(4.0f,gizmoSize*1.15f);
  if(IsKeyPressed(KEY_MINUS)||IsKeyPressed(KEY_KP_SUBTRACT))
   gizmoSize=std::max(0.25f,gizmoSize/1.15f);
 }
 // Selection indices refer to different component tables in each mode.
 // Never carry vertex/edge/face selection sets across a mode change.
 if(mode!=previousSubobjectMode){
  rectangleSelected.clear();rectangleObject=-1;
  rectanglePending=false;rectangleDragging=false;
  polygonExtrudePending=false;
  sub=-1;face=-1;
  drag.active=false;
  previousSubobjectMode=mode;
 }
 if(inView&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
  Vector2 m=GetMousePosition();
  if((cutMode||sliceMode)&&mode==3&&selected>=0&&selected<(int)objects.size()){
   if(!cutLineStarted){cutLineStart=m;cutLineEnd=m;cutLineStarted=true;}
   else{
    cutLineEnd=m;
    if(cutMeshByScreenLine(objects[selected],cutLineStart,cutLineEnd,sliceMode,face)){
     rectangleSelected.clear();rectangleObject=-1;
    }
    cutLineStarted=false;
   }
  }else{
  const bool shift=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);
  // Arm extrusion on mouse-down; create geometry only after a real drag.
  // This also tolerates Shift being pressed just after the mouse button.
  if(mode==3&&validComponent()&&shift){
   // A Shift-drag can begin on a polygon OR on a gizmo handle
   // outside the actor silhouette, using the currently selected face.
   const int selectedFaceBeforeClick=face;
   const int gizmoAxis=handleHit(m);
   pickComponent(m);
   if(face<0&&gizmoAxis>=0&&selectedFaceBeforeClick>=0&&
      selectedFaceBeforeClick<(int)objects[selected].faces.size())
    face=selectedFaceBeforeClick;
   polygonExtrudePending=face>=0;
   polygonExtrudeFace=face;
   polygonExtrudePress=m;
   if(polygonExtrudePending){
    rectanglePending=false;rectangleDragging=false;
    rectangleSelected.clear();rectangleObject=-1;
   }
  }else{
   // Target Weld is a vertex-picking operation, independent of the active
   // Move/Rotate/Scale tool. Handle it before any gizmo hit testing.
   if(targetWeldArmed){
    if(mode==1&&selected==targetWeldObject&&validComponent()){
     pickComponent(m);
     if(sub>=0)applyTargetWeld(sub);
    }else{
     targetWeldArmed=false;
    }
   }else{
   // Visible transform handles must remain pickable over the mesh.
   // Prioritize the arrow/cube endpoint, then allow component picking
   // over the thin axis shaft. Rotation rings are direct gizmo targets.
   int axis=handleHit(m);
   if(axis>=0&&mode!=0&&tool!=2){
    Vector3 handleDirection{};(&handleDirection.x)[axis]=1.6f*gizmoSize;
    const Vector2 endpoint=GetWorldToScreen(Vector3Add(pivot(objects[selected]),handleDirection),camera);
    const bool onHandle=Vector2Distance(m,endpoint)<=20.0f;
    if(!onHandle){
     const int savedSub=sub,savedFace=face;
     pickComponent(m);
     const bool componentHit=(mode==3)?face>=0:sub>=0;
     sub=savedSub;face=savedFace;
     if(componentHit)axis=-1;
    }
   }
   if(axis>=0)startDrag(axis);
   else if(mode==0){pick(m);sub=-1;rectangleSelected.clear();}
   else if(selected>=0){rectanglePending=true;rectangleStart=m;rectangleEnd=m;}
   }
  }
 }
 }
if((cutMode||sliceMode)&&cutLineStarted)cutLineEnd=GetMousePosition();
if(polygonExtrudePending){
 if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
  polygonExtrudePending=false;
 }else if(Vector2Distance(polygonExtrudePress,GetMousePosition())>=3.0f){
  polygonExtrudePending=false;
  face=polygonExtrudeFace;
  beginPolygonExtrude(polygonExtrudePress);
 }
}
if(polygonExtrudeDrag.active){
 if(IsMouseButtonDown(MOUSE_BUTTON_LEFT))updatePolygonExtrude();
 else endPolygonExtrude();
}
if(rectanglePending&&IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
 rectangleEnd=GetMousePosition();
 if(Vector2Distance(rectangleStart,rectangleEnd)>5.0f)rectangleDragging=true;
}
if(rectanglePending&&IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
 if(rectangleDragging)selectRectangle();
 else{
  const bool ctrl=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL);
  pickComponent(rectangleEnd);
  int hit=(mode==3)?face:sub;
  if(rectangleObject!=selected)rectangleSelected.clear();
  rectangleObject=selected;
  if(!ctrl)rectangleSelected.clear();
  if(hit>=0){
   if(ctrl&&rectangleSelected.count(hit))rectangleSelected.erase(hit);
   else rectangleSelected.insert(hit);
  }
  // Keep the clicked component active even when multi-selecting.
  sub=-1;face=-1;
  if(hit>=0&&rectangleSelected.count(hit)){
   if(mode==3)face=hit;
   else sub=hit;
  }else if(rectangleSelected.size()==1){
   if(mode==3)face=*rectangleSelected.begin();
   else sub=*rectangleSelected.begin();
  }
 }
 rectanglePending=false;rectangleDragging=false;
}
if(drag.active){if(IsMouseButtonDown(MOUSE_BUTTON_LEFT))applyDrag();else drag.active=false;}
if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)){
 rightPressPosition=GetMousePosition();
 rightPressInView=inView;
}
if(inView&&IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)){
 if(targetWeldArmed){targetWeldArmed=false;targetWeldSource=-1;targetWeldObject=-1;}
 else if(mode==1&&validComponent()){
  // Right-click a vertex to select it and expose its editing operations.
  pickComponent(GetMousePosition());
  if(sub>=0&&sub<(int)objects[selected].vertices.size()){
   rectangleSelected.clear();rectangleObject=selected;
   openVertexContext=true;
  }
 }
}
if(IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)){
 if(rightPressInView&&!openVertexContext&&Vector2Distance(GetMousePosition(),rightPressPosition)<5.0f)
  openViewportContext=true;
 rightPressInView=false;
}
if(inView&&IsMouseButtonDown(MOUSE_BUTTON_RIGHT)&&!openVertexContext){
 Vector2 d=GetMouseDelta();Vector3 offset=Vector3Subtract(camera.position,camera.target);
 float radius=std::max(.5f,Vector3Length(offset));
 float yaw=atan2f(offset.y,offset.x)-d.x*.004f;
 float elevation=std::clamp(asinf(std::clamp(offset.z/radius,-1.0f,1.0f))+d.y*.004f,-1.48f,1.48f);
 Vector3 forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
 Vector3 right=Vector3Normalize(Vector3CrossProduct(forward,camera.up));
 Vector3 flat=Vector3Normalize(Vector3{forward.x,forward.y,0});
 float speed=radius*.85f*GetFrameTime();Vector3 shift{};
 if(IsKeyDown(KEY_W))shift=Vector3Add(shift,Vector3Scale(flat,speed));
 if(IsKeyDown(KEY_S))shift=Vector3Subtract(shift,Vector3Scale(flat,speed));
 if(IsKeyDown(KEY_D))shift=Vector3Add(shift,Vector3Scale(right,speed));
 if(IsKeyDown(KEY_A))shift=Vector3Subtract(shift,Vector3Scale(right,speed));
 if(IsKeyDown(KEY_Q))shift.z-=speed;
 if(IsKeyDown(KEY_E))shift.z+=speed;
 camera.target=Vector3Add(camera.target,shift);
 camera.position=Vector3Add(camera.target,{radius*cosf(elevation)*cosf(yaw),radius*cosf(elevation)*sinf(yaw),radius*sinf(elevation)});
}
if(inView&&IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)){
 Vector2 d=GetMouseDelta();Vector3 forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
 Vector3 right=Vector3Normalize(Vector3CrossProduct(forward,camera.up));
 Vector3 up=Vector3Normalize(Vector3CrossProduct(right,forward));
 float k=Vector3Distance(camera.position,camera.target)*.0015f;
 Vector3 shift=Vector3Add(Vector3Scale(right,-d.x*k),Vector3Scale(up,d.y*k));
 camera.position=Vector3Add(camera.position,shift);camera.target=Vector3Add(camera.target,shift);
}
if(inView&&IsKeyPressed(KEY_F)&&selected>=0){camera.target=pivot(objects[selected]);camera.position=Vector3Add(camera.target,{5,-7,5});}
if(inView&&IsKeyPressed(KEY_KP_1)){camera.target={0,0,0};camera.position={0,-10,0.001f};}
if(inView&&IsKeyPressed(KEY_KP_3)){camera.target={0,0,0};camera.position={10,0,0.001f};}
if(inView&&IsKeyPressed(KEY_KP_7)){camera.target={0,0,0};camera.position={0,0,10};}
if(inView){float wheel=GetMouseWheelMove();if(wheel!=0){Vector3 dir=Vector3Normalize(Vector3Subtract(camera.target,camera.position));camera.position=Vector3Add(camera.position,Vector3Scale(dir,wheel*.5f));}}
// Object or face manipulation: hold X/Y/Z while using arrow keys; one history snapshot per key press.
int axis=IsKeyDown(KEY_X)?0:IsKeyDown(KEY_Y)?1:IsKeyDown(KEY_Z)?2:-1;float delta=(IsKeyPressed(KEY_UP)? .2f:0)+(IsKeyPressed(KEY_DOWN)?-.2f:0);if(selected>=0&&axis>=0&&delta!=0){checkpoint();auto&o=objects[selected];if(mode==3&&face>=0){for(int i:o.faces[face]){float *v=&o.vertices[i].x;v[axis]+=delta;}}else{float *v=&o.position.x;v[axis]+=delta;}}
BeginDrawing();ClearBackground({37,37,37,255});BeginMode3D(camera);
// Z-up editor: XY ground grid (raylib DrawGrid is XZ and was vertical here).
for(int g=-20;g<=20;g++){
 Color c=(g==0)?Color{100,100,100,255}:((g%5==0)?Color{72,72,72,255}:Color{53,53,53,255});
 DrawLine3D({(float)g,-20,0},{(float)g,20,0},c);
 DrawLine3D({-20,(float)g,0},{20,(float)g,0},c);
}
// Ground axes use the neutral grid palette; reserve RGB for transform gizmos.
// Two-pass mesh rendering: one shader activation per object, then wireframe.
// No shader switching per polygon; normal accumulation is linear in face count.
rlDrawRenderBatchActive();
if(backfaceCulling)rlEnableBackfaceCulling();
else rlDisableBackfaceCulling();
for(int oi=0;oi<(int)objects.size();oi++){
 const auto& o=objects[oi];
 const bool smoothEnabled=o.smoothingGroup>0&&o.smoothingValues[o.smoothingGroup]>0.0f;
 const bool pixel=smoothShaderReady&&smoothEnabled;
 std::vector<Vector3> smooth;
 if(smoothEnabled){
  smooth.assign(o.vertices.size(),Vector3{0,0,0});
  for(const auto& f:o.faces){
   if(f.size()<3)continue;
   Vector3 n{};
   for(size_t j=1;j+1<f.size();j++)
    n=Vector3Add(n,Vector3CrossProduct(Vector3Subtract(o.vertices[f[j]],o.vertices[f[0]]),Vector3Subtract(o.vertices[f[j+1]],o.vertices[f[0]])));
   for(int vi:f)if(vi>=0&&vi<(int)smooth.size())smooth[vi]=Vector3Add(smooth[vi],n);
  }
  for(auto& n:smooth)if(Vector3Length(n)>1e-7f)n=Vector3Normalize(n);
 }
 if(pixel){
  const auto& mat=materials[std::clamp(o.materialId,0,(int)materials.size()-1)];
  SetShaderValue(smoothShader,locBase,mat.baseColor,SHADER_UNIFORM_VEC3);
  SetShaderValue(smoothShader,locRough,&mat.roughness,SHADER_UNIFORM_FLOAT);
  SetShaderValue(smoothShader,locMetal,&mat.metallic,SHADER_UNIFORM_FLOAT);
  SetShaderValue(smoothShader,locSpec,&mat.specular,SHADER_UNIFORM_FLOAT);
  float cameraXYZ[3]={camera.position.x,camera.position.y,camera.position.z};
  SetShaderValue(smoothShader,locCamera,cameraXYZ,SHADER_UNIFORM_VEC3);
  BeginShaderMode(smoothShader);
 }
 for(int fi=0;fi<(int)o.faces.size();fi++){
  const auto& f=o.faces[fi];
  if(f.size()<3)continue;
  const bool highlighted=oi==selected&&mode==3&&(fi==face||(rectangleObject==oi&&rectangleSelected.count(fi)>0));
  if(highlighted)continue; // Draw selection after the material pass.
  const bool smoothTriangle=smoothEnabled;
  if(smoothTriangle)rlBegin(RL_TRIANGLES);
  for(size_t j=1;j+1<f.size();j++){
   const int ids[3]={f[0],f[j],f[j+1]};
   const Vector3 p[3]={world(o,ids[0]),world(o,ids[1]),world(o,ids[2])};
   if(!smoothTriangle){
    DrawTriangle3D(p[0],p[1],p[2],shadeMaterialTriangle(o,p[0],p[1],p[2],camera.position));
    continue;
   }
   Vector3 flat=Vector3CrossProduct(Vector3Subtract(p[1],p[0]),Vector3Subtract(p[2],p[0]));
   if(Vector3Length(flat)>1e-7f)flat=Vector3Normalize(flat);
   const float weight=std::clamp(o.smoothingValues[o.smoothingGroup]/100.0f,0.0f,1.0f);
   for(int k=0;k<3;k++){
    Vector3 blended=Vector3Add(Vector3Scale(flat,1.0f-weight),Vector3Scale(smooth[ids[k]],weight));
    if(pixel){rlColor4ub(255,255,255,255);rlNormal3f(blended.x,blended.y,blended.z);}
    else {
     Color c=shadeMaterialTriangle(o,p[k],p[(k+1)%3],p[(k+2)%3],camera.position,blended);
     rlColor4ub(c.r,c.g,c.b,c.a);
    }
    rlVertex3f(p[k].x,p[k].y,p[k].z);
   }
  }
  if(smoothTriangle)rlEnd();
 }
 if(pixel)EndShaderMode();
 // Selection and topology overlays are never processed by the material shader.
 for(int fi=0;fi<(int)o.faces.size();fi++){
  const auto& f=o.faces[fi];
  if(f.size()<3)continue;
  const bool highlighted=oi==selected&&mode==3&&(fi==face||(rectangleObject==oi&&rectangleSelected.count(fi)>0));
  if(highlighted)for(size_t j=1;j+1<f.size();j++)
   DrawTriangle3D(world(o,f[0]),world(o,f[j]),world(o,f[j+1]),Color{215,45,50,255});
  for(size_t j=0;j<f.size();j++)
   DrawLine3D(world(o,f[j]),world(o,f[(j+1)%f.size()]),highlighted?Color{255,92,92,255}:Color{78,82,88,255});
 }
}
rlDrawRenderBatchActive();
rlDisableBackfaceCulling();
// Draw the selected transform overlay only after all opaque object geometry.
if(selected>=0&&selected<(int)objects.size()){
 auto& o=objects[selected];
 rlDrawRenderBatchActive();
 rlDisableDepthTest();
 Vector3 p=pivot(o);if(mode==0||!active(o).empty()){if(tool==2){
  // Rotate: three independent axis-aligned circular rings.
  const Color colors[3]={RED,GREEN,BLUE};
  for(int axis=0;axis<3;axis++){
   Vector3 u=axis==0?Vector3{0,1,0}:Vector3{1,0,0};
   Vector3 v=axis==2?Vector3{0,1,0}:Vector3{0,0,1};
   // Z-up: X/Y rings are vertical; the Z ring is horizontal.
   for(int j=0;j<64;j++){
    float t0=6.2831853f*j/64.0f,t1=6.2831853f*(j+1)/64.0f;
    Vector3 q0=Vector3Add(p,Vector3Scale(Vector3Add(Vector3Scale(u,cosf(t0)),Vector3Scale(v,sinf(t0))),1.35f*gizmoSize));
    Vector3 q1=Vector3Add(p,Vector3Scale(Vector3Add(Vector3Scale(u,cosf(t1)),Vector3Scale(v,sinf(t1))),1.35f*gizmoSize));
    DrawLine3D(q0,q1,colors[axis]);
   }
  }
 }else{
  const Vector3 directions[3]={{1,0,0},{0,1,0},{0,0,1}};
  const Color colors[3]={RED,GREEN,BLUE};
  for(int axis=0;axis<3;axis++){
   Vector3 tip=Vector3Add(p,Vector3Scale(directions[axis],1.6f*gizmoSize));
   DrawLine3D(p,tip,colors[axis]);
   if(tool==1){
    // Move: arrowhead, not a scale cube.
    DrawCylinderEx(Vector3Add(p,Vector3Scale(directions[axis],1.32f*gizmoSize)),tip,.115f*gizmoSize,0.0f,10,colors[axis]);
   }else{
    // Scale: box handle at each axis endpoint.
    DrawCube(tip,.22f*gizmoSize,.22f*gizmoSize,.22f*gizmoSize,colors[axis]);
   }
  }
 }
 if(mode==2){auto e=edges(o);for(int k=0;k<(int)e.size();k++)DrawLine3D(world(o,e[k].first),world(o,e[k].second),(k==sub||(rectangleObject==selected&&rectangleSelected.count(k)>0))?Color{255,65,65,255}:Color{120,125,132,255});}
} }
rlDrawRenderBatchActive();
rlEnableDepthTest();
EndMode3D();
// Cut/slice guide line in screen coordinates.
if((cutMode||sliceMode)&&cutLineStarted){
 DrawLineEx(cutLineStart,cutLineEnd,2.0f,ORANGE);
 DrawCircleV(cutLineStart,4.0f,ORANGE);
}
// Target Weld direction guide: source vertex -> cursor, with destination snap feedback.
// Draw as a 2D overlay so it stays visible above the shaded mesh.
if(targetWeldArmed&&mode==1&&selected==targetWeldObject&&
   selected>=0&&selected<(int)objects.size()&&
   targetWeldSource>=0&&targetWeldSource<(int)objects[selected].vertices.size()){
 const auto& weldMesh=objects[selected];
 const Vector3 sourceWorld=world(weldMesh,targetWeldSource);
 const Vector3 viewForward=Vector3Subtract(camera.target,camera.position);
 if(Vector3DotProduct(Vector3Subtract(sourceWorld,camera.position),viewForward)>0.0f){
  Vector2 start=GetWorldToScreen(sourceWorld,camera);
  Vector2 end=GetMousePosition();
  int snap=-1;
  float snapDistance=16.0f;
  for(int i=0;i<(int)weldMesh.vertices.size();++i){
   if(i==targetWeldSource)continue;
   const Vector3 candidate=world(weldMesh,i);
   if(Vector3DotProduct(Vector3Subtract(candidate,camera.position),viewForward)<=0.0f)continue;
   const Vector2 screen=GetWorldToScreen(candidate,camera);
   const float distance=Vector2Distance(screen,end);
   if(distance<snapDistance){snap=i;snapDistance=distance;}
  }
  const bool canWeld=snap>=0&&verticesSharePolygonEdge(weldMesh,targetWeldSource,snap);
  if(snap>=0)end=GetWorldToScreen(world(weldMesh,snap),camera);
  const Color guideColor=canWeld?Color{90,235,150,255}:
                         snap>=0?Color{255,110,95,255}:Color{255,200,75,255};
  const Vector2 delta=Vector2Subtract(end,start);
  const float length=Vector2Length(delta);
  if(length>1.0f){
   DrawLineEx(start,end,2.5f,guideColor);
   const Vector2 dir=Vector2Scale(delta,1.0f/length);
   const Vector2 perp={-dir.y,dir.x};
   const Vector2 base=Vector2Subtract(end,Vector2Scale(dir,12.0f));
   DrawTriangle(end,Vector2Add(base,Vector2Scale(perp,5.0f)),
                Vector2Subtract(base,Vector2Scale(perp,5.0f)),guideColor);
  }
  DrawCircleV(start,6.0f,Color{255,220,110,255});
  DrawCircleLines((int)start.x,(int)start.y,8.0f,guideColor);
  if(snap>=0)DrawCircleLines((int)end.x,(int)end.y,9.0f,guideColor);
 }
}
// Screen-space vertex markers: fixed pixel radius regardless of camera zoom.
// Render in 2D after the 3D scene, clipped to the central viewport.
if(mode==1&&selected>=0&&selected<(int)objects.size()){
 const auto& o=objects[selected];
 BeginScissorMode(186,43,std::max(1,w-417),std::max(1,h-70));
 for(int i=0;i<(int)o.vertices.size();i++){
  Vector3 point=world(o,i);
  // Ignore vertices behind the camera.
  Vector3 forward=Vector3Subtract(camera.target,camera.position);
  if(Vector3DotProduct(Vector3Subtract(point,camera.position),forward)<=0.0f)continue;
  Vector2 screen=GetWorldToScreen(point,camera);
  if(screen.x<186||screen.x>w-231||screen.y<43||screen.y>h-27)continue;
  const bool chosen=i==sub||(rectangleObject==selected&&rectangleSelected.count(i)>0);
  DrawCircleV(screen,chosen?5.0f:3.5f,chosen?Color{255,72,72,255}:Color{55,145,255,255});
  if(chosen)DrawCircleLines((int)screen.x,(int)screen.y,6.0f,Color{255,225,225,255});
 }
 EndScissorMode();
}
if(rectanglePending&&rectangleDragging){
 float x=std::min(rectangleStart.x,rectangleEnd.x),y=std::min(rectangleStart.y,rectangleEnd.y);
 float rw=fabsf(rectangleEnd.x-rectangleStart.x),rh=fabsf(rectangleEnd.y-rectangleStart.y);
 DrawRectangle((int)x,(int)y,(int)rw,(int)rh,Color{70,145,235,42});
 DrawRectangleLinesEx({x,y,rw,rh},1.5f,Color{90,170,255,235});
}
// Show the precision crosshair only while hovering over a scene actor or transform gizmo.
bool cursorOnActor=false;
if(inView && !drag.active){
 Ray hoverRay=GetScreenToWorldRay(GetMousePosition(),camera);
 float closest=1.0e20f;
 for(const auto& actor:objects){
  for(const auto& polygon:actor.faces){
   if(polygon.size()<3)continue;
   for(size_t j=1;j+1<polygon.size();j++){
    Vector3 a=world(actor,polygon[0]);
    Vector3 b=world(actor,polygon[j]);
    Vector3 c=world(actor,polygon[j+1]);
    RayCollision hit=GetRayCollisionTriangle(hoverRay,a,b,c);
    if(!hit.hit)hit=GetRayCollisionTriangle(hoverRay,a,c,b);
    if(hit.hit && hit.distance<closest){closest=hit.distance;cursorOnActor=true;}
   }
  }
 }
}
// Screen-space cursor crosshair: always above scene geometry, but below UI panels.
bool showCrosshair=inView && !drag.active && (cursorOnActor || handleHit(GetMousePosition())>=0);
if(showCrosshair && !crosshairCursorHidden){HideCursor();crosshairCursorHidden=true;}
else if(!showCrosshair && crosshairCursorHidden){ShowCursor();crosshairCursorHidden=false;}
if(showCrosshair){
 Vector2 cursor=GetMousePosition();
 Color ink={230,236,245,230}; // Fixed crosshair color over actors and all gizmo axes.
 DrawCircleLines((int)cursor.x,(int)cursor.y,7.0f,Color{12,18,25,210});
 DrawLineEx({cursor.x-13,cursor.y},{cursor.x-4,cursor.y},1.5f,ink);
 DrawLineEx({cursor.x+4,cursor.y},{cursor.x+13,cursor.y},1.5f,ink);
 DrawLineEx({cursor.x,cursor.y-13},{cursor.x,cursor.y-4},1.5f,ink);
 DrawLineEx({cursor.x,cursor.y+4},{cursor.x,cursor.y+13},1.5f,ink);
 DrawCircleV(cursor,1.5f,ink);
}
rlImGuiBegin();
if(openViewportContext){
 ImGui::OpenPopup("Viewport Context Menu");
 openViewportContext=false;
}
if(ImGui::BeginPopup("Viewport Context Menu")){
 if(ImGui::MenuItem("Backface Culling",nullptr,backfaceCulling))
  backfaceCulling=!backfaceCulling;
 ImGui::EndPopup();
}
if(openVertexContext){
 ImGui::OpenPopup("Vertex Context Menu");
 openVertexContext=false;
}
if(ImGui::BeginPopup("Vertex Context Menu")){
 const bool canEdit=mode==1&&validComponent()&&sub>=0&&sub<(int)objects[selected].vertices.size();
 ImGui::BeginDisabled(!canEdit);
 if(ImGui::MenuItem("Target Weld"))armTargetWeld();
 if(ImGui::MenuItem("Chamfer"))chamferSelectedVertex();
 ImGui::Separator();
 if(ImGui::MenuItem("Backface Culling",nullptr,backfaceCulling))
  backfaceCulling=!backfaceCulling;
 ImGui::EndDisabled();
 ImGui::EndPopup();
}
// Dockable tool windows, with a transparent central node for the raylib scene.
ImGuiViewport* viewport=ImGui::GetMainViewport();
// Reserve the toolbar height before docking, so docked panels align without overlap.
const ImGuiID dockId=ImGui::GetID("N3DLiteDockspace");
const ImVec2 dockPos(viewport->Pos.x,viewport->Pos.y+44.0f);
const ImVec2 dockSize(viewport->Size.x,ImMax(1.0f,viewport->Size.y-44.0f));
ImGui::SetNextWindowPos(dockPos,ImGuiCond_Always);
ImGui::SetNextWindowSize(dockSize,ImGuiCond_Always);
const ImGuiWindowFlags hostFlags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
 ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoBringToFrontOnFocus|
 ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_NoSavedSettings|
 ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoDocking;
ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
ImGui::Begin("##N3DLiteDockHost",nullptr,hostFlags);
ImGui::PopStyleVar();
if(ImGui::DockBuilderGetNode(dockId)==nullptr){
 ImGui::DockBuilderRemoveNode(dockId);
 ImGui::DockBuilderAddNode(dockId,ImGuiDockNodeFlags_DockSpace|ImGuiDockNodeFlags_PassthruCentralNode);
 ImGui::DockBuilderSetNodeSize(dockId,dockSize);
 ImGuiID center=dockId;
 ImGuiID left=ImGui::DockBuilderSplitNode(center,ImGuiDir_Left,0.19f,nullptr,&center);
 ImGuiID bottomLeft=left;
 ImGuiID topLeft=ImGui::DockBuilderSplitNode(bottomLeft,ImGuiDir_Up,0.62f,nullptr,&bottomLeft);
 ImGui::DockBuilderDockWindow("Editable Polygon",topLeft);
 ImGui::DockBuilderDockWindow("Create Objects",bottomLeft);
 ImGui::DockBuilderDockWindow("Materials",bottomLeft);
 ImGui::DockBuilderDockWindow("Smoothing Groups",bottomLeft);
 ImGui::DockBuilderFinish(dockId);
}
ImGui::DockSpace(dockId,ImVec2(0,0),ImGuiDockNodeFlags_PassthruCentralNode);
ImGui::End();
const ImGuiWindowFlags toolbarFlags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
 ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking;
ImGui::SetNextWindowPos(viewport->Pos,ImGuiCond_Always);
ImGui::SetNextWindowSize(ImVec2(viewport->Size.x,42.0f),ImGuiCond_Always);
ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(9,3));
if(ImGui::Begin("##N3DLiteTopToolbar",nullptr,toolbarFlags)){
 ImGui::AlignTextToFramePadding();
 ImGui::TextUnformatted("N3DLite");
 ImGui::SameLine(0,18);
 ImGui::TextDisabled("Transform");
 ImGui::SameLine(0,8);
 if(TransformIconButton("TopMove",1,tool==1))tool=1;
 ImGui::SameLine(0,4);
 if(TransformIconButton("TopRotate",2,tool==2))tool=2;
 ImGui::SameLine(0,4);
 if(TransformIconButton("TopScale",3,tool==3))tool=3;
 ImGui::SameLine(0,12);
 ImGui::TextDisabled("Gizmo: %s | X / Y / Z",tool==1?"Move":tool==2?"Rotate":tool==3?"Scale":"None");
 ImGui::SameLine(0,8);
 ImGui::TextDisabled("Size: %.0f%% (+/-)",gizmoSize*100.0f);
 ImGui::SameLine(0,12);
 if(selected>=0&&selected<(int)objects.size()){
  const Vector3 coords=pivot(objects[selected]);
  ImGui::Text("X: %.2f  Y: %.2f  Z: %.2f",coords.x,coords.y,coords.z);
 }else{
  ImGui::TextDisabled("X: --  Y: --  Z: --");
 }
 ImGui::SameLine(0,18);
 if(ImGui::Button("Copy AI Connect")){SetClipboardText(connectionPrompt);connectFeedback=180;}
 if(connectFeedback>0){ImGui::SameLine();ImGui::TextUnformatted("Copied");}
}
ImGui::End();
ImGui::PopStyleVar();

// Non-interactive viewport information in the central docking area.
if(ImGuiDockNode* central=ImGui::DockBuilderGetCentralNode(dockId)){
 const ImVec2 origin(central->Pos.x+14.0f,central->Pos.y+14.0f);
 ImDrawList* overlay=ImGui::GetForegroundDrawList();
 char info[256];
 if(selected>=0&&selected<(int)objects.size()){
  const MeshObject& o=objects[selected];
  snprintf(info,sizeof(info),"%s  |  Vertices: %d  |  Faces: %d",
           o.name.c_str(),(int)o.vertices.size(),(int)o.faces.size());
 }else{
  snprintf(info,sizeof(info),"No object selected");
 }
 const ImVec2 textSize=ImGui::CalcTextSize(info);
 overlay->AddRectFilled(ImVec2(origin.x-7,origin.y-5),
                        ImVec2(origin.x+textSize.x+7,origin.y+textSize.y+5),
                        IM_COL32(28,28,28,205),4.0f);
 overlay->AddText(origin,IM_COL32(225,225,225,255),info);
}

if(ImGui::Begin("Editable Polygon")){
 ImGui::TextUnformatted("Selection mode");
 const char* names[]={"Object","Vertex","Edge","Border","Polygon"};
 const int subModes[]={0,1,2,4,3};
 for(int i=0;i<5;i++){if(i==3)ImGui::NewLine();else if(i)ImGui::SameLine();if(ImGui::RadioButton(names[i],mode==subModes[i])){if(subModes[i]==0||selected>=0){mode=subModes[i];sub=-1;face=-1;targetWeldArmed=false;rectangleSelected.clear();rectangleObject=-1;}}}
 if(selected<0)ImGui::TextDisabled("Select an actor in Object mode before editing sub-objects.");
 ImGui::Separator();
 auto operation=[&](const char* label,bool available,void(*fn)()){
  ImGui::BeginDisabled(!available);
  if(ImGui::Button(label,ImVec2(-1,0)))fn();
  ImGui::EndDisabled();
 };
 bool vertex=mode==1&&validComponent()&&sub>=0&&sub<(int)objects[selected].vertices.size();
 bool edge=mode==2&&validComponent()&&sub>=0;
 bool border=mode==4&&validComponent()&&sub>=0;
 bool polygon=mode==3&&validComponent()&&face>=0&&face<(int)objects[selected].faces.size();
 if(mode==1&&ImGui::CollapsingHeader("Vertex###VertexToolsHeader",ImGuiTreeNodeFlags_DefaultOpen)){
  ImGui::BeginDisabled(!vertex);if(ImGui::Button("Move Vertex (W)",ImVec2(-1,0)))tool=1;ImGui::EndDisabled();
  operation("Weld Nearest",vertex,weldSelectedVertex);
  ImGui::TextDisabled("Weld Nearest merges the closest vertex, including across faces.");
  operation("Remove Vertex",vertex,removeSelectedVertex);
  operation("Break Vertex",vertex,breakSelectedVertex);
  operation("Extrude Vertex (Z+)",vertex,extrudeSelectedVertex);
  operation("Chamfer Vertex",vertex,chamferSelectedVertex);
  operation("Target Weld (click destination)",vertex,armTargetWeld);
  ImGui::TextDisabled("Target Weld: select source, then click any destination vertex.");
  if(targetWeldArmed)ImGui::TextColored(ImVec4(1.0f,.8f,.3f,1.0f),"Target Weld active: click vertices; right-click to exit");
 }
 if(mode==2&&ImGui::CollapsingHeader("Edge###EdgeToolsHeader",ImGuiTreeNodeFlags_DefaultOpen)){
  operation("Split Edge",edge,splitSelectedEdge);
  operation("Remove Edge (merge faces)",edge,removeSelectedEdge);
  operation("Turn Edge (triangles)",edge,turnSelectedEdge);
  int boundaryA=0,boundaryB=0;bool boundary=edge&&selectedBoundaryEdge(boundaryA,boundaryB);
  operation("Extrude Boundary Edge (Z+)",boundary,extrudeSelectedEdge);
  ImGui::BeginDisabled();ImGui::Button("Connect Edges",ImVec2(-1,0));ImGui::Button("Chamfer Edge",ImVec2(-1,0));ImGui::Button("Bridge Edges",ImVec2(-1,0));ImGui::EndDisabled();
 }
 if(mode==4&&ImGui::CollapsingHeader("Border###BorderToolsHeader",ImGuiTreeNodeFlags_DefaultOpen)){
  ImGui::TextDisabled("Select a boundary edge to edit its loop");
  bool closedLoop=border&&!selectedBoundaryLoop().empty();
  operation("Cap Hole",closedLoop,capBoundary);
  operation("Extend Boundary",closedLoop,extendBoundary);
  ImGui::BeginDisabled();ImGui::Button("Select Open Edge Loop",ImVec2(-1,0));ImGui::Button("Bridge Borders",ImVec2(-1,0));ImGui::EndDisabled();
 }
 if(mode==3&&ImGui::CollapsingHeader("Polygon###PolygonToolsHeader",ImGuiTreeNodeFlags_DefaultOpen)){
  if(ImGui::Button(cutMode?"Free Cut: ON":"Free Cut",ImVec2(-1,0))){cutMode=!cutMode;sliceMode=false;cutLineStarted=false;}
  if(ImGui::Button(sliceMode?"Planar Slice: ON":"Planar Slice",ImVec2(-1,0))){sliceMode=!sliceMode;cutMode=false;cutLineStarted=false;}
  if(cutMode||sliceMode)ImGui::TextWrapped("Click two viewport points to define the cut line. Free Cut splits the selected polygon; Planar Slice splits all intersected polygons. Click button again to exit.");

  operation("Extrude Polygon",polygon,extrude);
  operation("Inset Polygon",polygon,inset);
  operation("Bevel Polygon",polygon,bevelSelectedPolygon);
  operation("Outline Polygon",polygon,outlineSelectedFace);
  operation("Flip Polygon",polygon,flipSelectedFace);
  operation("Detach Polygon",polygon,detachSelectedFace);
  operation("Remove Polygon",polygon,removeSelectedFace);
  operation("Connect Polygon Vertices (split)",polygon,connectSelectedFaceVertices);
  ImGui::BeginDisabled();ImGui::Button("Bridge Polygons",ImVec2(-1,0));ImGui::EndDisabled();
 }
 if(mode==0)ImGui::TextDisabled("Choose a sub-object mode to edit the mesh.");
 else ImGui::TextDisabled("Unavailable operations are disabled until implemented.");
}
ImGui::End();


if(ImGui::Begin("Create Objects")){
 if(ImGui::Button("Box",ImVec2(-1,0))){checkpoint();objects.push_back(box());selected=(int)objects.size()-1;face=-1;sub=-1;}
 if(ImGui::Button("Sphere",ImVec2(-1,0))){checkpoint();objects.push_back(sphere());selected=(int)objects.size()-1;face=-1;sub=-1;}
 if(ImGui::Button("Plane",ImVec2(-1,0))){checkpoint();objects.push_back(plane());selected=(int)objects.size()-1;face=-1;sub=-1;}
}
ImGui::End();
// Material library and object assignment; viewport base color is live.
if(ImGui::Begin("Smoothing Groups")){
 if(selected>=0&&selected<(int)objects.size()){
  MeshObject& o=objects[selected];
  ImGui::Text("Object: %s",o.name.c_str());
  ImGui::TextDisabled("Group 0 = Flat; groups 1-32 have independent smoothing values");
  // Compact drop-down replaces the 32-button grid.
  char selectedGroup[40];
  snprintf(selectedGroup,sizeof(selectedGroup),o.smoothingGroup==0?"0 - Flat":"%d - Smooth",o.smoothingGroup);
  if(ImGui::BeginCombo("Smoothing Group",selectedGroup)){
   for(int g=0;g<=32;g++){
    char label[40];
    snprintf(label,sizeof(label),g==0?"0 - Flat":"%d - Smooth",g);
    if(ImGui::Selectable(label,o.smoothingGroup==g)&&o.smoothingGroup!=g){
     checkpoint();o.smoothingGroup=g;
    }
    if(o.smoothingGroup==g)ImGui::SetItemDefaultFocus();
   }
   ImGui::EndCombo();
  }
  if(o.smoothingGroup>0){
   float value=o.smoothingValues[o.smoothingGroup];
   ImGui::SetNextItemWidth(-1);
   if(ImGui::SliderFloat("Smooth value (0-100)",&value,0.0f,100.0f,"%.0f")){
    if(value!=o.smoothingValues[o.smoothingGroup]){checkpoint();o.smoothingValues[o.smoothingGroup]=value;}
   }
   ImGui::TextDisabled("0 = hard edges; 100 = fully averaged normals");
  }
  if(ImGui::Button("Flat Shading",ImVec2(-1,0))&&o.smoothingGroup!=0){checkpoint();o.smoothingGroup=0;}
  if(ImGui::Button("Smooth Shading",ImVec2(-1,0))&&o.smoothingGroup==0){checkpoint();o.smoothingGroup=1;}
  ImGui::TextWrapped("Each group has its own 0-100 smoothing strength for this object. Groups are currently object-wide.");
 }else ImGui::TextDisabled("Select an object to edit smoothing");
}
ImGui::End();
if(ImGui::Begin("Materials")){
 if(ImGui::Button("New Material")){
  EditorMaterial m; m.name="Material "+std::to_string(materials.size());
  materials.push_back(m);activeMaterial=(int)materials.size()-1;
 }
 ImGui::SameLine();
 if(ImGui::Button("Duplicate")&&activeMaterial>=0&&activeMaterial<(int)materials.size()){
  EditorMaterial m=materials[activeMaterial];m.name+=" Copy";
  materials.push_back(m);activeMaterial=(int)materials.size()-1;
 }
 ImGui::Separator();
 if(ImGui::BeginListBox("##MaterialList",ImVec2(-1,110))){
  for(int i=0;i<(int)materials.size();i++)
   if(ImGui::Selectable(materials[i].name.c_str(),activeMaterial==i))activeMaterial=i;
  ImGui::EndListBox();
 }
 if(activeMaterial>=0&&activeMaterial<(int)materials.size()){
  EditorMaterial& m=materials[activeMaterial];
  char name[128];snprintf(name,sizeof(name),"%s",m.name.c_str());
  if(ImGui::InputText("Name",name,sizeof(name)))m.name=name;
  ImGui::ColorEdit3("Base Color",m.baseColor);
  ImGui::SliderFloat("Roughness",&m.roughness,0.0f,1.0f);
  ImGui::SliderFloat("Metallic",&m.metallic,0.0f,1.0f);
  ImGui::SliderFloat("Specular",&m.specular,0.0f,1.0f);
  ImGui::TextDisabled("Live viewport approximation (Blinn-Phong), not full PBR");
  if(selected>=0&&selected<(int)objects.size()){
   ImGui::Text("Selected: %s",objects[selected].name.c_str());
   if(ImGui::Button("Assign to Selected",ImVec2(-1,0))){
    checkpoint();objects[selected].materialId=activeMaterial;
   }
   ImGui::Text("Assigned: %s",materials[std::clamp(objects[selected].materialId,0,(int)materials.size()-1)].name.c_str());
  }else ImGui::TextDisabled("Select an object to assign material");
 }
}
ImGui::End();
// Record only real UI panels, not the pass-through dockspace. This
// prevents a click on selection-mode radio buttons from also picking
// the mesh or starting a transform underneath the panel.
uiInputRects.clear();
for(const char* name:{"Editable Polygon","Create Objects","Materials","Smoothing Groups","##N3DLiteTopToolbar","Vertex Context Menu"}){
 ImGuiWindow* win=ImGui::FindWindowByName(name);
 if(win&&win->WasActive&&!win->Hidden){
  const ImRect r=win->Rect();
  uiInputRects.push_back({r.Min.x,r.Min.y,r.GetWidth(),r.GetHeight()});
 }
}
rlImGuiEnd();
EndDrawing();
}
saveWindowLayout();
ImGui::SaveIniSettingsToDisk("N3DLite-layout.ini");
if(crosshairCursorHidden)ShowCursor();
rlImGuiShutdown();
if(uiFontLoaded)UnloadFont(uiFont);
CloseWindow();
}
