#include <vector>
#include <cstdlib>
#include <limits>
#include <iostream>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

#define M_PI 3.14159

Model* model = NULL;
float* each_view_zbuffer  = NULL; 

const int width = 800;
const int height = 800;

//定义光照位置
Vec3f LightDir(1, 0, 1);
//摄像机位置
Vec3f ViewDir(1, 0, 3);
//焦点位置
Vec3f center(0, 0, 0);
// 定义一个up向量用于计算摄像机的x向量
Vec3f up(0, 1, 0);

// 只用来获取zbuffer
struct ZShader : public IShader
{
    mat<4, 3, float> varying_clip_coord; //裁剪空间坐标，由VS写入，FS读取

    mat<4, 4, float> M_Model = mat<4, 4, float>::identity();
    mat<4, 4, float> M_View = LookAt;
    mat<4, 4, float> M_Projection = Projection;
    mat<4, 4, float> M_MVP = M_Projection * M_View * M_Model;

    virtual Vec4f vertex(int iface, int nthvert)
    {
        Vec4f gl_Vertex = M_MVP * embed<4>(model->vert(iface, nthvert));
        varying_clip_coord.set_col(nthvert, gl_Vertex);
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f gl_FragCoord, Vec3f bar, TGAColor& color)
    {
        color = TGAColor(0, 0, 0);
        return false;
    }
};

// 返回光线斜度的最大值
float max_elevation_angle(float* zbuffer, Vec2f p, Vec2f dir) {
    float maxangle = 0;
    for (float t = 0.; t < 1000.; t += 1.) {
        Vec2f cur = p + dir * t;
        if (cur.x >= width || cur.y >= height || cur.x < 0 || cur.y < 0)
        {
            return maxangle;
        }

        float distance = (p - cur).norm();

        if (distance < 1.f) continue;

        float elevation = zbuffer[int(cur.x) + int(cur.y) * width] - zbuffer[int(p.x) + int(p.y) * width];
        maxangle = std::max(maxangle, atanf(elevation / distance));
    }

    return maxangle;
}



int main(int argc, char** argv) {
    // 获取模型文件
    if (2 == argc) {
        model = new Model(argv[1]);
    }
    else {
        //model = new Model("obj/african_head/african_head.obj");
        model = new Model("obj/diablo3_pose/diablo3_pose.obj");

    }

    float* zbuffer = new float[width * height];
    each_view_zbuffer  = new float[width * height];
    for (int i = width * height; i--; zbuffer[i] = -std::numeric_limits<float>::max());

    TGAImage frame(width, height, TGAImage::RGB);
    lookat(ViewDir, center, up);
    viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
    projection(-1.f / (ViewDir - center).norm());


    ZShader zshader;
    for (int i = 0; i < model->nfaces(); i++)
    {
        for (int j = 0; j < 3; j++)
        {
            zshader.vertex(i, j);
        }
        triangle(zshader.varying_clip_coord, zshader, frame, zbuffer );
    }

    for (int x = 0; x < width; x++) 
    {
        for (int y = 0; y < height; y++) 
        {
            if (zbuffer[x + y * width] < -10000) continue;
            // 
            float total = 0;

            // 发射8条射线，计算八个方向各自的斜度
            for(float a =0; a<2*M_PI; a+=M_PI/4)
            {
	            total += M_PI / 2 - max_elevation_angle(zbuffer, Vec2f(x, y), Vec2f(cos(a), sin(a)));
            }

            total /= (M_PI / 2) * 8;
            total = pow(total, 100.f);
            frame.set(x, y, TGAColor(total * 255, total * 255, total * 255));
        }
    }

    frame.flip_vertically();
    frame.write_tga_file("framebuffer.tga");

    delete[] zbuffer;
    delete model;
    return 0;
}

