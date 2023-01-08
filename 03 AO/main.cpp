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

TGAImage AO(1024, 1024, TGAImage::GRAYSCALE);
TGAImage  occl(1024, 1024, TGAImage::GRAYSCALE);

// 绘制each_view_zbuffer 
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
        color = TGAColor(255, 255, 255) * ((gl_FragCoord.z + 1.f) / 2.f);
        return false;
    }
};

// 绘制AO贴图
struct Shader : public IShader {
    mat<2, 3, float> varying_uv;
    mat<4, 3, float> varying_clip_coord;

    mat<4, 4, float> M_Model = mat<4, 4, float>::identity();
    mat<4, 4, float> M_View = LookAt;
    mat<4, 4, float> M_Projection = Projection;
    mat<4, 4, float> M_MVP = M_Projection * M_View * M_Model;

    virtual Vec4f vertex(int iface, int nthvert)
    {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = M_MVP * embed<4>(model->vert(iface, nthvert));
        varying_clip_coord.set_col(nthvert, gl_Vertex);
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f gl_FragCoord, Vec3f bar, TGAColor& color)
    {
        Vec2f uv = varying_uv * bar;

        // 如果不在阴影里，就在（u，v）位置画一个白点
        if (std::abs(each_view_zbuffer [int(gl_FragCoord.x + gl_FragCoord.y * width)] - gl_FragCoord.z < 1e-2))
        {
            occl.set(uv.x * 1024, uv.y * 1024, TGAColor(255));
        }
        color = TGAColor(255, 0, 0);
        return false;
    }
};

// 单位球面选取随机点
Vec3f rand_point_on_unit_sphere() {
    float u = (float)rand() / (float)RAND_MAX;
    float v = (float)rand() / (float)RAND_MAX;
    float theta = 2.f * M_PI * u;
    float phi = acos(2.f * v - 1.f);
    return Vec3f(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(phi));
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


    // 暴力AO：根据随机点生成观察位置
    const int nrenders = 1000; //循环次数
    for (int iter = 1; iter <= nrenders; iter++)
    {
        std::cerr << iter << " from " << nrenders << std::endl;
        
        ViewDir = rand_point_on_unit_sphere();
        ViewDir.y = std::abs(ViewDir.y); //只要上半球
        std::cout << "v " << ViewDir << std::endl;

        for (int i = width * height; i--; each_view_zbuffer [i] = zbuffer[i] = -std::numeric_limits<float>::max());

        TGAImage frame(width, height, TGAImage::RGB);
        lookat(ViewDir, center, up);
        viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
        projection(0);//-1.f/(ViewDir-center).norm());

        ZShader zshader;
        for (int i = 0; i < model->nfaces(); i++)
        {
            for (int j = 0; j < 3; j++)
            {
                zshader.vertex(i, j);
            }
            triangle(zshader.varying_clip_coord, zshader, frame, each_view_zbuffer );
        }

        frame.flip_vertically();
        frame.write_tga_file("each_view_zbuffer.tga");

        Shader shader;
        occl.clear();
        for (int i = 0; i < model->nfaces(); i++)
        {
            for (int j = 0; j < 3; j++)
            {
                shader.vertex(i, j);
            }
            triangle(shader.varying_clip_coord, shader, frame, zbuffer);
        }

        // 逐像素绘制白点图
        // 将这些occl白点图累加起来存入AO贴图，最后将叠加的颜色除以循环次数，得到最终的AO贴图。
        for (int i = 0; i < 1024; i++)
        {
            for (int j = 0; j < 1024; j++)
            {
                float tmp = AO.get(i, j)[0];
                AO.set(i, j, TGAColor((tmp * (iter - 1) + occl.get(i, j)[0]) / (float)iter + .5f));
            }
        }
    }
    AO.flip_vertically();
    AO.write_tga_file("occlusion.tga");
    occl.flip_vertically();
    occl.write_tga_file("occl.tga");

    delete[] zbuffer;
    delete model;
    delete[] each_view_zbuffer ;
    return 0;
}

