#include "View3D.h"

#include <math.h>

#include "raymath.h"
#include "rlgl.h"

namespace {

// Where the screen sits in the model (units = cm, from assets/3d/make_glb.py): the
// centre of the round window of the lid, the lid's normal (21 degrees up from the
// front, the front being -X) and the "up" of the screen along the lid.
const Vector3 SCREEN_C = {-1.709f, 3.113f, 0.0f};
const Vector3 SCREEN_N = {-0.934f, 0.357f, 0.0f};
const Vector3 SCREEN_UP = {0.357f, 0.934f, 0.0f};
const Vector3 SCREEN_RIGHT = {0.0f, 0.0f, 1.0f};  // as the viewer sees it
const float SCREEN_R = 2.6f;
const int DISC_SEG = 72;

Model shell;
bool loaded = false;
Shader lit;
Mesh disc;
Material discMat;
RenderTexture2D target = {};
int targetW = 0, targetH = 0;
Rectangle lastArea = {};
Camera3D cam = {};
float yaw = -0.55f, pitch = 0.22f, dist = 19.0f;  // yaw 0 = straight in front of the face

#if defined(PLATFORM_WEB)
const char *VS =
    "#version 100\n"
    "attribute vec3 vertexPosition; attribute vec2 vertexTexCoord; attribute vec3 vertexNormal;\n"
    "uniform mat4 mvp; uniform mat4 matModel; uniform mat4 matNormal;\n"
    "varying vec3 fragNormal; varying vec2 fragTexCoord; varying vec3 fragPos;\n"
    "void main() { fragTexCoord = vertexTexCoord; fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));\n"
    "  fragPos = vec3(matModel * vec4(vertexPosition, 1.0)); gl_Position = mvp * vec4(vertexPosition, 1.0); }\n";
const char *FS =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec3 fragNormal; varying vec2 fragTexCoord; varying vec3 fragPos;\n"
    "uniform sampler2D texture0; uniform vec4 colDiffuse; uniform vec3 viewPos;\n"
    "void main() { vec3 n = normalize(fragNormal);\n"
    "  vec3 l1 = normalize(vec3(-0.6, 0.8, 0.45)); vec3 l2 = normalize(vec3(0.7, 0.3, -0.6));\n"
    "  float d = 0.5 + 0.45 * max(dot(n, l1), 0.0) + 0.2 * max(dot(n, l2), 0.0);\n"
    "  vec3 v = normalize(viewPos - fragPos); vec3 h = normalize(l1 + v);\n"
    "  float s = pow(max(dot(n, h), 0.0), 40.0) * 0.22;\n"
    "  vec4 base = texture2D(texture0, fragTexCoord) * colDiffuse;\n"
    "  gl_FragColor = vec4(base.rgb * d + s, base.a); }\n";
#else
const char *VS =
    "#version 330\n"
    "in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal;\n"
    "uniform mat4 mvp; uniform mat4 matModel; uniform mat4 matNormal;\n"
    "out vec3 fragNormal; out vec2 fragTexCoord; out vec3 fragPos;\n"
    "void main() { fragTexCoord = vertexTexCoord; fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));\n"
    "  fragPos = vec3(matModel * vec4(vertexPosition, 1.0)); gl_Position = mvp * vec4(vertexPosition, 1.0); }\n";
const char *FS =
    "#version 330\n"
    "in vec3 fragNormal; in vec2 fragTexCoord; in vec3 fragPos;\n"
    "uniform sampler2D texture0; uniform vec4 colDiffuse; uniform vec3 viewPos;\n"
    "out vec4 finalColor;\n"
    "void main() { vec3 n = normalize(fragNormal);\n"
    "  vec3 l1 = normalize(vec3(-0.6, 0.8, 0.45)); vec3 l2 = normalize(vec3(0.7, 0.3, -0.6));\n"
    "  float d = 0.5 + 0.45 * max(dot(n, l1), 0.0) + 0.2 * max(dot(n, l2), 0.0);\n"
    "  vec3 v = normalize(viewPos - fragPos); vec3 h = normalize(l1 + v);\n"
    "  float s = pow(max(dot(n, h), 0.0), 40.0) * 0.22;\n"
    "  vec4 base = texture(texture0, fragTexCoord) * colDiffuse;\n"
    "  finalColor = vec4(base.rgb * d + s, base.a); }\n";
#endif

// A flat disc in the window of the lid, textured with the screen.
Mesh makeDisc() {
  Mesh m = {};
  m.triangleCount = DISC_SEG;
  m.vertexCount = DISC_SEG * 3;
  m.vertices = (float *)MemAlloc(m.vertexCount * 3 * sizeof(float));
  m.texcoords = (float *)MemAlloc(m.vertexCount * 2 * sizeof(float));
  m.normals = (float *)MemAlloc(m.vertexCount * 3 * sizeof(float));
  Vector3 c = Vector3Add(SCREEN_C, Vector3Scale(SCREEN_N, 0.01f));
  int k = 0;
  auto put = [&](float x, float y) {
    Vector3 p = Vector3Add(c, Vector3Add(Vector3Scale(SCREEN_RIGHT, x * SCREEN_R), Vector3Scale(SCREEN_UP, y * SCREEN_R)));
    m.vertices[k * 3] = p.x;
    m.vertices[k * 3 + 1] = p.y;
    m.vertices[k * 3 + 2] = p.z;
    m.texcoords[k * 2] = 0.5f + 0.5f * x;
    m.texcoords[k * 2 + 1] = 0.5f - 0.5f * y;
    m.normals[k * 3] = SCREEN_N.x;
    m.normals[k * 3 + 1] = SCREEN_N.y;
    m.normals[k * 3 + 2] = SCREEN_N.z;
    k++;
  };
  for (int i = 0; i < DISC_SEG; i++) {
    float a0 = i * 2.0f * PI / DISC_SEG, a1 = (i + 1) * 2.0f * PI / DISC_SEG;
    put(0.0f, 0.0f);
    put(cosf(a0), sinf(a0));  // counter-clockwise seen from the front
    put(cosf(a1), sinf(a1));
  }
  UploadMesh(&m, false);
  return m;
}

void updateCamera() {
  Vector3 tgt = {0.0f, 2.9f, 0.0f};
  cam.target = tgt;
  cam.position = {tgt.x - dist * cosf(pitch) * cosf(yaw), tgt.y + dist * sinf(pitch), tgt.z + dist * cosf(pitch) * sinf(yaw)};
  cam.up = {0.0f, 1.0f, 0.0f};
  cam.fovy = 32.0f;
  cam.projection = CAMERA_PERSPECTIVE;
}

}  // namespace

namespace View3D {

bool load() {
  const char *paths[] = {"assets/vocat_shell.glb", "../assets/3d/vocat_shell.glb", "vocat_shell.glb"};
  for (const char *p : paths) {
    if (!FileExists(p)) continue;
    shell = LoadModel(p);
    if (shell.meshCount == 0) continue;
    loaded = true;
    break;
  }
  if (!loaded) return false;
  lit = LoadShaderFromMemory(VS, FS);
  lit.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(lit, "matModel");
  lit.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(lit, "matNormal");
  lit.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lit, "viewPos");
  for (int i = 0; i < shell.materialCount; i++) shell.materials[i].shader = lit;
  disc = makeDisc();
  discMat = LoadMaterialDefault();
  updateCamera();
  return true;
}

bool available() { return loaded; }

void unload() {
  if (!loaded) return;
  UnloadModel(shell);
  UnloadMesh(disc);
  UnloadShader(lit);
  if (targetW) UnloadRenderTexture(target);
  loaded = false;
}

void orbit(Vector2 d) {
  yaw -= d.x * 0.008f;  // the robot turns with the hand: drag right, its face goes right
  pitch = Clamp(pitch + d.y * 0.006f, -0.35f, 1.25f);
  updateCamera();
}

void zoom(float wheel) {
  dist = Clamp(dist * (1.0f - wheel * 0.1f), 7.0f, 30.0f);
  updateCamera();
}

void draw(Rectangle area, Texture2D lcd, float light, float voiceDeg, bool talking) {
  if (!loaded) return;
  int w = (int)area.width, h = (int)area.height;
  if (w < 16 || h < 16) return;
  if (w != targetW || h != targetH) {
    if (targetW) UnloadRenderTexture(target);
    target = LoadRenderTexture(w, h);
    targetW = w;
    targetH = h;
  }
  lastArea = area;
  float vp[3] = {cam.position.x, cam.position.y, cam.position.z};
  SetShaderValue(lit, lit.locs[SHADER_LOC_VECTOR_VIEW], vp, SHADER_UNIFORM_VEC3);

  BeginTextureMode(target);
  ClearBackground({0, 0, 0, 0});
  BeginMode3D(cam);
  // the desk: a soft disc, and the ring where the pretend speaker stands
  DrawCylinder({0.0f, -0.03f, 0.0f}, 6.5f, 6.5f, 0.02f, 64, {24, 28, 37, 255});
  DrawCircle3D({0.0f, 0.0f, 0.0f}, 5.2f, {1.0f, 0.0f, 0.0f}, 90.0f, {58, 66, 84, 255});
  float a = voiceDeg * DEG2RAD;
  Vector3 sp = {-cosf(a) * 5.2f, 0.3f, -sinf(a) * 5.2f};  // the robot faces -X, its right is -Z
  Color voice = {170, 120, 255, 255};
  DrawSphere(sp, talking ? 0.3f : 0.2f, talking ? voice : Fade(voice, 0.55f));
  if (talking) {
    float k = (float)fmod(GetTime() * 1.6, 1.0);
    DrawCircle3D({sp.x, 0.02f, sp.z}, 0.5f + 1.2f * k, {1.0f, 0.0f, 0.0f}, 90.0f, Fade(voice, 1.0f - k));
  }
  DrawModel(shell, {0, 0, 0}, 1.0f, WHITE);
  // the screen, lit by its own backlight
  discMat.maps[MATERIAL_MAP_DIFFUSE].texture = lcd;
  unsigned char k = (unsigned char)(255 * (0.12f + 0.88f * light));
  discMat.maps[MATERIAL_MAP_DIFFUSE].color = {k, k, k, 255};
  DrawMesh(disc, discMat, MatrixIdentity());
  EndMode3D();
  EndTextureMode();

  // a render texture is upside down
  DrawTexturePro(target.texture, {0, 0, (float)w, (float)-h}, area, {0, 0}, 0.0f, WHITE);
}

bool screenPixel(Vector2 mouse, int &px, int &py) {
  if (!loaded || targetW == 0) return false;
  Vector2 local = {mouse.x - lastArea.x, mouse.y - lastArea.y};
  if (local.x < 0 || local.y < 0 || local.x >= lastArea.width || local.y >= lastArea.height) return false;
  Ray ray = GetScreenToWorldRayEx(local, cam, targetW, targetH);
  float denom = Vector3DotProduct(ray.direction, SCREEN_N);
  if (denom >= -1e-4f) return false;  // looking at it from behind
  float t = Vector3DotProduct(Vector3Subtract(SCREEN_C, ray.position), SCREEN_N) / denom;
  if (t <= 0.0f) return false;
  Vector3 p = Vector3Subtract(Vector3Add(ray.position, Vector3Scale(ray.direction, t)), SCREEN_C);
  float x = Vector3DotProduct(p, SCREEN_RIGHT) / SCREEN_R, y = Vector3DotProduct(p, SCREEN_UP) / SCREEN_R;
  if (x * x + y * y > 1.0f) return false;
  px = (int)Clamp((0.5f + 0.5f * x) * 360.0f, 0.0f, 359.0f);
  py = (int)Clamp((0.5f - 0.5f * y) * 360.0f, 0.0f, 359.0f);
  return true;
}

}  // namespace View3D
