#include "raylib.h"
#include "raymath.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <set>
#include <utility>
struct MeshObject {std::string name;std::vector<Vector3> vertices;std::vector<std::vector<int>> faces;Vector3 position{};};
struct Snapshot {std::vector<MeshObject> objects;int selected,face,sub;};
std::vector<MeshObject> objects;std::vector<Snapshot> undoStack,redoStack;int selected=-1,face=-1,mode=0,tool=1,sub=-1;Camera3D camera{};
void checkpoint(){undoStack.push_back({objects,selected,face,sub});if(undoStack.size()>80)undoStack.erase(undoStack.begin());redoStack.clear();}
void undo(){if(undoStack.empty())return;redoStack.push_back({objects,selected,face,sub});auto s=undoStack.back();undoStack.pop_back();objects=s.objects;selected=s.selected;face=s.face;sub=s.sub;}
void redo(){if(redoStack.empty())return;undoStack.push_back({objects,selected,face,sub});auto s=redoStack.back();redoStack.pop_back();objects=s.objects;selected=s.selected;face=s.face;sub=s.sub;}
MeshObject box(){return {"Box",{{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}},{{0,3,2,1},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}}};}
MeshObject plane(){return {"Plane",{{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}},{{0,1,2,3}}};}
MeshObject sphere(){MeshObject o;o.name="Sphere";constexpr int N=16,R=10;for(int j=0;j<=R;j++){float p=PI*j/R;for(int i=0;i<N;i++){float t=2*PI*i/N;o.vertices.push_back({sinf(p)*cosf(t),sinf(p)*sinf(t),cosf(p)});}}for(int j=0;j<R;j++)for(int i=0;i<N;i++)o.faces.push_back({j*N+i,j*N+(i+1)%N,(j+1)*N+(i+1)%N,(j+1)*N+i});return o;}
Vector3 world(const MeshObject&o,int i){return Vector3Add(o.vertices[i],o.position);}
void extrude(){if(selected<0||face<0||mode!=3)return;checkpoint();auto&o=objects[selected];auto old=o.faces[face];if(old.size()<3)return;Vector3 a=o.vertices[old[0]],b=o.vertices[old[1]],c=o.vertices[old[2]];Vector3 n=Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b,a),Vector3Subtract(c,a)));std::vector<int> top;for(int i:old){top.push_back((int)o.vertices.size());o.vertices.push_back(Vector3Add(o.vertices[i],Vector3Scale(n,.5f)));}o.faces[face]=top;for(size_t i=0;i<old.size();i++)o.faces.push_back({old[i],old[(i+1)%old.size()],top[(i+1)%top.size()],top[i]});}
void inset(){if(selected<0||face<0||mode!=3)return;checkpoint();auto&o=objects[selected];auto old=o.faces[face];Vector3 c{};for(int i:old)c=Vector3Add(c,o.vertices[i]);c=Vector3Scale(c,1.0f/old.size());std::vector<int> inner;for(int i:old){inner.push_back((int)o.vertices.size());o.vertices.push_back(Vector3Lerp(c,o.vertices[i],.7f));}o.faces[face]=inner;for(size_t i=0;i<old.size();i++)o.faces.push_back({old[i],old[(i+1)%old.size()],inner[(i+1)%inner.size()],inner[i]});}
void pick(Vector2 mouse){Ray ray=GetScreenToWorldRay(mouse,camera);float nearest=1e20f;int best=-1,bf=-1;for(int oi=0;oi<(int)objects.size();oi++){auto&o=objects[oi];for(int fi=0;fi<(int)o.faces.size();fi++){auto&f=o.faces[fi];for(size_t j=1;j+1<f.size();j++){auto hit=GetRayCollisionTriangle(ray,world(o,f[0]),world(o,f[j]),world(o,f[j+1]));if(!hit.hit)hit=GetRayCollisionTriangle(ray,world(o,f[0]),world(o,f[j+1]),world(o,f[j]));if(hit.hit&&hit.distance<nearest){nearest=hit.distance;best=oi;bf=fi;}}}}selected=best;face=bf;}

std::vector<std::pair<int,int>> edges(const MeshObject& o){
 std::set<std::pair<int,int>> e;
 for(const auto& f:o.faces)for(size_t j=0;j<f.size();j++){
  int a=f[j],b=f[(j+1)%f.size()];if(a>b)std::swap(a,b);e.insert({a,b});
 }
 return {e.begin(),e.end()};
}
std::vector<int> active(const MeshObject& o){
 if(mode==1&&sub>=0&&sub<(int)o.vertices.size())return {sub};
 if(mode==2){auto e=edges(o);if(sub>=0&&sub<(int)e.size())return {e[sub].first,e[sub].second};}
 if(mode==3&&face>=0&&face<(int)o.faces.size())return o.faces[face];
 return {};
}
Vector3 pivot(const MeshObject& o){auto a=active(o);if(a.empty())return o.position;
 Vector3 p{};for(int i:a)p=Vector3Add(p,world(o,i));return Vector3Scale(p,1.0f/a.size());}
float segmentDistance(Vector2 p,Vector2 a,Vector2 b){Vector2 d=Vector2Subtract(b,a),v=Vector2Subtract(p,a);
 float t=std::clamp(Vector2DotProduct(v,d)/std::max(1.0f,Vector2DotProduct(d,d)),0.0f,1.0f);
 return Vector2Distance(p,Vector2Add(a,Vector2Scale(d,t)));}
int handleHit(Vector2 mouse){if(selected<0||selected>=(int)objects.size()||(mode!=0&&active(objects[selected]).empty()))return -1;Vector3 p=pivot(objects[selected]);
 Vector2 a=GetWorldToScreen(p,camera);int best=-1;float distance=12;
 for(int i=0;i<3;i++){Vector3 d{};(&d.x)[i]=1.35f;
  Vector2 b=GetWorldToScreen(Vector3Add(p,d),camera);
  float x=segmentDistance(mouse,a,b);if(x<distance){distance=x;best=i;}}
 return best;
}
void pickComponent(Vector2 mouse){
 if(mode==3){pick(mouse);sub=face;return;}
 // First find the object under the cursor, so component mode can switch objects.
 int prior=selected;pick(mouse);int hitObject=selected;
 if(hitObject<0){selected=-1;face=-1;sub=-1;return;}
 selected=hitObject;sub=-1;auto& o=objects[selected];
 float best=16.0f;
 if(mode==1){for(int i=0;i<(int)o.vertices.size();i++){
  float d=Vector2Distance(mouse,GetWorldToScreen(world(o,i),camera));
  if(d<best){best=d;sub=i;}
 }}
 else if(mode==2){auto e=edges(o);for(int i=0;i<(int)e.size();i++){
  float d=segmentDistance(mouse,GetWorldToScreen(world(o,e[i].first),camera),GetWorldToScreen(world(o,e[i].second),camera));
  if(d<best){best=d;sub=i;}
 }}
 // Permit selecting vertices and edges even on the visible silhouette.
 if(sub<0&&prior>=0&&prior<(int)objects.size()){
  auto& old=objects[prior];float distance=14;
  if(mode==1)for(int i=0;i<(int)old.vertices.size();i++){
   float d=Vector2Distance(mouse,GetWorldToScreen(world(old,i),camera));
   if(d<distance){distance=d;selected=prior;sub=i;}
  }
  if(mode==2){auto e=edges(old);for(int i=0;i<(int)e.size();i++){
   float d=segmentDistance(mouse,GetWorldToScreen(world(old,e[i].first),camera),GetWorldToScreen(world(old,e[i].second),camera));
   if(d<distance){distance=d;selected=prior;sub=i;}
  }}
 }
 face=-1;
}
struct DragState{bool active=false;int axis=-1;Vector2 start{};Vector3 startPos{},center{};std::vector<Vector3> startVertices;std::vector<int> affected;};
DragState drag;
void startDrag(int axis){if(selected<0)return;checkpoint();auto& o=objects[selected];
 drag={};drag.active=true;drag.axis=axis;drag.start=GetMousePosition();drag.startPos=o.position;
 drag.center=pivot(o);drag.startVertices=o.vertices;drag.affected=active(o);}
void applyDrag(){if(!drag.active||selected<0)return;auto& o=objects[selected];
 Vector3 axis{};(&axis.x)[drag.axis]=1.35f;
 Vector2 a=GetWorldToScreen(drag.center,camera),b=GetWorldToScreen(Vector3Add(drag.center,axis),camera);
 Vector2 direction=Vector2Subtract(b,a),delta=Vector2Subtract(GetMousePosition(),drag.start);
 float amount=Vector2DotProduct(delta,direction)/std::max(1.0f,Vector2DotProduct(direction,direction))*1.35f;
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
"COREMODEL_AI_REQUEST\n"
"protocol=1\n"
"action=connect\n"
"source=CoreModel\n"
"repository=https://github.com/3dmk/3dit\n"
"branch=main\n"
"workflow=GitHub\n"
"local_path=C:\\GPT\\CoreModel_GitHub\n"
"authority=candidate-only\n"
"\n"
"Connect to the CoreModel C++23/raylib editor project using its GitHub repository. "
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
int main(){SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT);InitWindow(1280,760,"CoreModel Native v0.8 - C++23");SetTargetFPS(60);
rlImGuiSetup(true);
ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
ImGui::StyleColorsDark();
if(FileExists("Roboto.ttf")){uiFont=LoadFontEx("Roboto.ttf",32,nullptr,0);uiFontLoaded=uiFont.texture.id!=0;}
camera.position={7,-9,7};camera.target={0,0,0};camera.up={0,0,1};camera.fovy=45;camera.projection=CAMERA_PERSPECTIVE;objects.push_back(box());selected=0;
int connectFeedback=0;
bool showProperties=true,showAI=true;
Rectangle transformToolbarBounds={0,0,0,0};
while(!WindowShouldClose()){
 if(connectFeedback>0)connectFeedback--;
 int w=GetScreenWidth(),h=GetScreenHeight();
 // Panels are Dear ImGui windows; the central 3D viewport stays raylib.
 bool inView=!ImGui::GetIO().WantCaptureMouse && !CheckCollisionPointRec(GetMousePosition(),transformToolbarBounds);
 bool typing=ImGui::GetIO().WantTextInput;
 if(!typing){
  if(IsKeyDown(KEY_LEFT_CONTROL)&&IsKeyPressed(KEY_Z)){if(IsKeyDown(KEY_LEFT_SHIFT))redo();else undo();}
  if(IsKeyDown(KEY_LEFT_CONTROL)&&IsKeyPressed(KEY_Y))redo();
  if(IsKeyPressed(KEY_ONE)){mode=1;sub=-1;face=-1;}
  if(IsKeyPressed(KEY_TWO)){mode=2;sub=-1;face=-1;}
  if(IsKeyPressed(KEY_THREE)){mode=3;sub=-1;face=-1;}
  if(IsKeyPressed(KEY_ZERO)){mode=0;sub=-1;face=-1;}
  if(IsKeyPressed(KEY_W)&&!IsMouseButtonDown(MOUSE_BUTTON_RIGHT))tool=1;
  if(IsKeyPressed(KEY_E)&&!IsMouseButtonDown(MOUSE_BUTTON_RIGHT))tool=2;
  if(IsKeyPressed(KEY_R)&&!IsMouseButtonDown(MOUSE_BUTTON_RIGHT))tool=3;
 }
 if(inView&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
  Vector2 m=GetMousePosition();
  int axis=handleHit(m);
  if(axis>=0)startDrag(axis);
  else if(mode==0){pick(m);sub=-1;}
  else pickComponent(m);
 }
if(drag.active){if(IsMouseButtonDown(MOUSE_BUTTON_LEFT))applyDrag();else drag.active=false;}
if(inView&&IsMouseButtonDown(MOUSE_BUTTON_RIGHT)){
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
BeginDrawing();ClearBackground({18,21,26,255});BeginMode3D(camera);
// Z-up editor: XY ground grid (raylib DrawGrid is XZ and was vertical here).
for(int g=-20;g<=20;g++){
 Color c=(g==0)?Color{105,115,132,255}:((g%5==0)?Color{68,76,88,255}:Color{45,51,61,255});
 DrawLine3D({(float)g,-20,0},{(float)g,20,0},c);
 DrawLine3D({-20,(float)g,0},{20,(float)g,0},c);
}
DrawLine3D({0,0,0},{2,0,0},RED);DrawLine3D({0,0,0},{0,2,0},GREEN);
for(int oi=0;oi<(int)objects.size();oi++){auto&o=objects[oi];for(int fi=0;fi<(int)o.faces.size();fi++){auto&f=o.faces[fi];Color color=(oi==selected&&fi==face&&mode==3)?Color{215,45,50,255}:Color{135,135,135,255};for(size_t j=1;j+1<f.size();j++)DrawTriangle3D(world(o,f[0]),world(o,f[j]),world(o,f[j+1]),color);for(size_t j=0;j<f.size();j++)DrawLine3D(world(o,f[j]),world(o,f[(j+1)%f.size()]),(oi==selected&&fi==face&&mode==3)?Color{255,92,92,255}:Color{78,82,88,255});}if(oi==selected){Vector3 p=pivot(o);if(mode==0||!active(o).empty()){DrawLine3D(p,Vector3Add(p,{1.6f,0,0}),RED);DrawLine3D(p,Vector3Add(p,{0,1.6f,0}),GREEN);DrawLine3D(p,Vector3Add(p,{0,0,1.6f}),BLUE);
 if(mode==2){auto e=edges(o);for(int k=0;k<(int)e.size();k++)DrawLine3D(world(o,e[k].first),world(o,e[k].second),k==sub?Color{255,65,65,255}:Color{120,125,132,255});}
 DrawSphere(Vector3Add(p,{1.35f,0,0}),.09f,RED);DrawSphere(Vector3Add(p,{0,1.35f,0}),.09f,GREEN);DrawSphere(Vector3Add(p,{0,0,1.35f}),.09f,BLUE);} }}
EndMode3D();
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
  const bool chosen=i==sub;
  DrawCircleV(screen,chosen?5.0f:3.5f,chosen?Color{255,72,72,255}:Color{220,63,63,255});
  if(chosen)DrawCircleLines((int)screen.x,(int)screen.y,6.0f,Color{255,225,225,255});
 }
 EndScissorMode();
}
rlImGuiBegin();
// Dockable tool windows, with a transparent central node for the raylib scene.
ImGuiViewport* viewport=ImGui::GetMainViewport();
// Reserve the toolbar height before docking, so docked panels align without overlap.
const ImGuiID dockId=ImGui::GetID("CoreModelDockspace");
const ImVec2 dockPos(viewport->Pos.x,viewport->Pos.y+44.0f);
const ImVec2 dockSize(viewport->Size.x,ImMax(1.0f,viewport->Size.y-44.0f));
ImGui::SetNextWindowPos(dockPos,ImGuiCond_Always);
ImGui::SetNextWindowSize(dockSize,ImGuiCond_Always);
const ImGuiWindowFlags hostFlags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
 ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoBringToFrontOnFocus|
 ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_NoSavedSettings|
 ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoDocking;
ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
ImGui::Begin("##CoreModelDockHost",nullptr,hostFlags);
ImGui::PopStyleVar();
if(ImGui::DockBuilderGetNode(dockId)==nullptr){
 ImGui::DockBuilderRemoveNode(dockId);
 ImGui::DockBuilderAddNode(dockId,ImGuiDockNodeFlags_DockSpace|ImGuiDockNodeFlags_PassthruCentralNode);
 ImGui::DockBuilderSetNodeSize(dockId,dockSize);
 ImGuiID center=dockId;
 ImGuiID left=ImGui::DockBuilderSplitNode(center,ImGuiDir_Left,0.19f,nullptr,&center);
 ImGuiID right=ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,0.23f,nullptr,&center);
 ImGuiID bottomLeft=left;
 ImGuiID topLeft=ImGui::DockBuilderSplitNode(bottomLeft,ImGuiDir_Up,0.62f,nullptr,&bottomLeft);
 ImGui::DockBuilderDockWindow("Modeling Tools",topLeft);
 ImGui::DockBuilderDockWindow("Create Objects",bottomLeft);
 ImGui::DockBuilderDockWindow("Properties",right);
 ImGui::DockBuilderDockWindow("CoreModel AI",right);
 ImGui::DockBuilderFinish(dockId);
}
ImGui::DockSpace(dockId,ImVec2(0,0),ImGuiDockNodeFlags_PassthruCentralNode);
ImGui::End();
// Viewport overlay: icon buttons stay anchored to the central dock node.
ImGuiDockNode* central=ImGui::DockBuilderGetCentralNode(dockId);
ImVec2 toolPos=central?central->Pos:dockPos;
ImVec2 toolSize=central?central->Size:dockSize;
const float barWidth=139.0f; // Three transform gizmo modes
ImGui::SetNextWindowPos(ImVec2(toolPos.x+ImMax(4.0f,(toolSize.x-barWidth)*.5f),toolPos.y+9.0f),ImGuiCond_Always);
ImGui::SetNextWindowSize(ImVec2(barWidth,48),ImGuiCond_Always);
const ImGuiWindowFlags overlayFlags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
 ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoSavedSettings|
 ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoNav;
ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(8,7));
ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,7.0f);
if(ImGui::Begin("##ViewportTransformTools",nullptr,overlayFlags)){
 if(TransformIconButton("MoveIcon",1,tool==1))tool=1;
 ImGui::SameLine(0,4);
 if(TransformIconButton("RotateIcon",2,tool==2))tool=2;
 ImGui::SameLine(0,4);
 if(TransformIconButton("ScaleIcon",3,tool==3))tool=3;
}
transformToolbarBounds={(float)ImGui::GetWindowPos().x,(float)ImGui::GetWindowPos().y,
 (float)ImGui::GetWindowSize().x,(float)ImGui::GetWindowSize().y};
ImGui::End();
ImGui::PopStyleVar(2);
const ImGuiWindowFlags toolbarFlags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
 ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking;
ImGui::SetNextWindowPos(viewport->Pos,ImGuiCond_Always);
ImGui::SetNextWindowSize(ImVec2(viewport->Size.x,42.0f),ImGuiCond_Always);
if(ImGui::Begin("##CoreModelTopToolbar",nullptr,toolbarFlags)){
 ImGui::TextUnformatted("CoreModel");
 ImGui::SameLine();ImGui::SeparatorText("Panels");
 ImGui::SameLine();
 if(ImGui::Button(showProperties?"Hide Properties":"Properties"))showProperties=!showProperties;
 ImGui::SameLine();
 if(ImGui::Button(showAI?"Hide CoreModel AI":"CoreModel AI"))showAI=!showAI;
 ImGui::SameLine();
 if(ImGui::Button("Copy AI Connect")){SetClipboardText(connectionPrompt);connectFeedback=180;}
 if(connectFeedback>0){ImGui::SameLine();ImGui::TextUnformatted("Copied");}
}
ImGui::End();


if(ImGui::Begin("Modeling Tools")){
 ImGui::TextUnformatted("Selection mode");
 const char* names[]={"Object","Vertex","Edge","Face"};
 for(int i=0;i<4;i++){
  if(i)ImGui::SameLine();
  if(ImGui::RadioButton(names[i],mode==i)){mode=i;sub=-1;face=-1;}
 }
 ImGui::Separator();
 if(ImGui::Button("Extrude Face",ImVec2(-1,0)))extrude();
 if(ImGui::Button("Inset Face",ImVec2(-1,0)))inset();
}
ImGui::End();


if(ImGui::Begin("Create Objects")){
 if(ImGui::Button("Box",ImVec2(-1,0))){checkpoint();objects.push_back(box());selected=(int)objects.size()-1;face=-1;sub=-1;}
 if(ImGui::Button("Sphere",ImVec2(-1,0))){checkpoint();objects.push_back(sphere());selected=(int)objects.size()-1;face=-1;sub=-1;}
 if(ImGui::Button("Plane",ImVec2(-1,0))){checkpoint();objects.push_back(plane());selected=(int)objects.size()-1;face=-1;sub=-1;}
}
ImGui::End();
if(showProperties){


if(ImGui::Begin("Properties")){
 if(selected>=0&&selected<(int)objects.size()){
  auto& o=objects[selected];
  ImGui::Text("Object: %s",o.name.c_str());
  ImGui::Text("Vertices: %d",(int)o.vertices.size());
  ImGui::Text("Faces: %d",(int)o.faces.size());
  ImGui::Separator();
  float pos[3]={o.position.x,o.position.y,o.position.z};
  if(ImGui::DragFloat3("Position",pos,0.05f)){
   checkpoint();
   o.position={pos[0],pos[1],pos[2]};
  }
  ImGui::Text("Selected vertex/edge: %d",sub);
  ImGui::Text("Selected face: %d",face);
 }else ImGui::TextUnformatted("No object selected.");
}
ImGui::End();
}
if(showAI){


if(ImGui::Begin("CoreModel AI")){
 if(ImGui::Button("Copy AI Connect")){SetClipboardText(connectionPrompt);connectFeedback=180;}
 if(connectFeedback>0)ImGui::TextUnformatted("Connection request copied.");
 ImGui::TextDisabled("GitHub workflow - local build required");
}
ImGui::End();
}
rlImGuiEnd();
EndDrawing();
}
rlImGuiShutdown();
if(uiFontLoaded)UnloadFont(uiFont);
CloseWindow();
}
