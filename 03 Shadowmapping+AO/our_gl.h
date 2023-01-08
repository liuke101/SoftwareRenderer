#pragma once
#include "tgaimage.h"
#include "geometry.h"

extern Matrix LookAt;
extern Matrix Viewport;
extern Matrix Projection;
const float depth = 2000.f;

void viewport(int x, int y, int w, int h);
void projection(float coeff = 0.f); // coeff = -1/c
void lookat(Vec3f ViewDir, Vec3f center, Vec3f up);

struct IShader {
    virtual ~IShader();
    virtual Vec4f vertex(int iface, int nthvert) = 0;   //负责顶点坐标变换
    virtual bool fragment(Vec3f bar, TGAColor& color) = 0;  // 负责计算着色
};

void triangle(Vec4f* pts, IShader& shader, TGAImage& image, float *zbuffer);

