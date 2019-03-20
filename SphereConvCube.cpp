/*
Implementation of a custom algroithm to convolve shapes with other shapes in iteration.
Built largely on Karl Yerkes method of rendering point clouds with shaders
*/

/**

Example: Point Cloud using Geometry Shader and Texture Sprite

Description: The goal of this example is to show how to make a "point cloud". It
is also an example of how to use a texture in the rendering of each point in a
mesh, a scheme known as "point sprites". We do not use the old OpenGL "fixed
pipeline"; Instead, we adapt shaders from the _OpenGL 4.0 Shading Language
Cookbook_ so this is also an example of using a "geometry shader". In addition,
we use GLSL structs to pass color data through the geometry shader.

Tags: point cloud, texture sprite, geometry shader, GLSL version 400

Author: Karl Yerkes, November 2018

*/

#include "al/core.hpp"
#include "stdio.h"
using namespace al;

#define N (80000)
#define CLOUD_WIDTH (80.0)

const char *vertex = R"(
#version 400

layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec4 vertexColor;

uniform mat4 al_ModelViewMatrix;
uniform mat4 al_ProjectionMatrix;

uniform float phase;
float amountToAdd = 1.0;
out Vertex {
  vec4 color;
} vertex;

float modulo;
vec3 bitToAdd;
float speed = 0.3;
void main() {
  bitToAdd = vertexPosition;
  bitToAdd = normalize(bitToAdd);
  bitToAdd *= phase * speed;
  vec3 temp = bitToAdd + vertexPosition;

  for(int i = 0; i < 3; i++){
    if (abs(temp[i]) > 1.0){
      bitToAdd -= vertexPosition;
    }
  }


    

  //vertexPosition += bitToAdd;
  //vertexPosition *= 1.01;
  gl_Position = al_ModelViewMatrix * vec4(vertexPosition, 1.0);
  vertex.color = vertexColor;
}
)";

const char *fragment = R"(
#version 400

in Fragment {
  vec4 color;
  vec2 textureCoordinate;
} fragment;

uniform sampler2D alphaTexture;

layout (location = 0) out vec4 fragmentColor;

void main() {
  // use the first 3 components of the color (xyz is rgb), but take the alpha value from the texture
  //
  fragmentColor = vec4(fragment.color.xyz, texture(alphaTexture, fragment.textureCoordinate));
}
)";

const char *geometry = R"(
#version 400

// take in a point and output a triangle strip with 4 vertices (aka a "quad")
//
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

uniform mat4 al_ProjectionMatrix;

// this uniform is *not* passed in automatically by AlloLib; do it manually
//
uniform float halfSize;

in Vertex {
  vec4 color;
} vertex[];

out Fragment {
  vec4 color;
  vec2 textureCoordinate;
} fragment;

void main() {
  mat4 m = al_ProjectionMatrix; // rename to make lines shorter
  vec4 v = gl_in[0].gl_Position; // al_ModelViewMatrix * gl_Position

  gl_Position = m * (v + vec4(-halfSize, -halfSize, 0.0, 0.0));
  fragment.textureCoordinate = vec2(0.0, 0.0);
  fragment.color = vertex[0].color;
  EmitVertex();

  gl_Position = m * (v + vec4(halfSize, -halfSize, 0.0, 0.0));
  fragment.textureCoordinate = vec2(1.0, 0.0);
  fragment.color = vertex[0].color;
  EmitVertex();

  gl_Position = m * (v + vec4(-halfSize, halfSize, 0.0, 0.0));
  fragment.textureCoordinate = vec2(0.0, 1.0);
  fragment.color = vertex[0].color;
  EmitVertex();

  gl_Position = m * (v + vec4(halfSize, halfSize, 0.0, 0.0));
  fragment.textureCoordinate = vec2(1.0, 1.0);
  fragment.color = vertex[0].color;
  EmitVertex();

  EndPrimitive();
}
)";

struct AlloApp : App
{
  ShaderProgram shader;
  Texture texture;
  Mesh pointMesh;

  void onCreate() override
  {
    // use a texture to control the alpha channel of each particle
    //
    texture.create2D(256, 256, Texture::R8, Texture::RED, Texture::SHORT);
    int Nx = texture.width();
    int Ny = texture.height();
    std::vector<short> alpha;
    alpha.resize(Nx * Ny);
    for (int j = 0; j < Ny; ++j)
    {
      float y = float(j) / (Ny - 1) * 2 - 1;
      for (int i = 0; i < Nx; ++i)
      {
        float x = float(i) / (Nx - 1) * 2 - 1;
        float m = exp(-13 * (x * x + y * y));
        m *= pow(2, 15) - 1; // scale by the largest positive short int
        alpha[j * Nx + i] = m;
      }
    }
    texture.submit(&alpha[0]);

    // compile and link the three shaders
    //
    shader.compile(vertex, fragment, geometry);

    // create a mesh of points scattered randomly with random colors
    //
    pointMesh.primitive(Mesh::POINTS);
    for (int i = 0; i < N; i++)
    {
      float horizontal = rnd::uniform(6.2831853);
      float vertical = rnd::uniform(6.2831853);
      float r = 0.01;
      pointMesh.vertex(
          Vec3f(r * cos(horizontal) * sin(vertical), r * cos(vertical), r * sin(horizontal) * sin(vertical)) *
          CLOUD_WIDTH);
      pointMesh.color(HSV(0.7, 1.0, 1.0));
    }
  }

  // animate the size of each sprite
  //
  float phase = 0;
  double ellapsedTime = 0;
  float halfSize = 0;
  void onAnimate(double dt) override
  {
    phase += dt;
    ellapsedTime += dt;
    if (phase > 3)
    {
      phase -= 3;
    }

    halfSize = 0.2 * phase / 3;
    phase = 1.0f;
    //std::cout << ellapsedTime << std::endl;
    float size = 10.0;
    for (int i = 0; i < N; i++)
    {
      Vec3f location;
      float increment = 0.01;
      Vec3f incVec = pointMesh.vertices()[i];
      incVec = incVec.normalize(increment);
      pointMesh.vertices()[i] += incVec;

      if (pointMesh.vertices()[i].x > size || pointMesh.vertices()[i].x < -size)
      {
        pointMesh.vertices()[i] *= 0.000001;
        pointMesh.colors()[i] = HSV(fmod(ellapsedTime * 0.05, 1.0), 1.0, 1.0);
      }

      if (pointMesh.vertices()[i].y > size || pointMesh.vertices()[i].y < -size)
      {
        pointMesh.vertices()[i] *= 0.000001;
        pointMesh.colors()[i] = HSV(fmod(ellapsedTime * 0.05, 1.0), 1.0, 1.0);
      }

      if (pointMesh.vertices()[i].z > size || pointMesh.vertices()[i].z < -size)
      {
        pointMesh.vertices()[i] *= 0.000001;
        pointMesh.colors()[i] = HSV(fmod(ellapsedTime * 0.05, 1.0), 1.0, 1.0);
      }

      //location = pointMesh.vertex.index(i);
      //std::cout << location.x << std::endl;
    }
  }

  void onDraw(Graphics &g) override
  {
    g.clear(0.0);

    g.depthTesting(false);
    g.blending(true);
    g.blendModeAdd();

    texture.bind();
    g.shader(shader);
    g.shader().uniform("halfSize", halfSize); // halfSize < 0.05 ? 0.05 : halfSize);
    g.shader().uniform("phase", float(ellapsedTime));
    g.draw(pointMesh);
    texture.unbind();
  }
};

int main() { AlloApp().start(); }
