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

//�������λ��
Vec3f LightDir(1, 0, 1);
//�����λ��
Vec3f ViewDir(1, 0, 3);
//����λ��
Vec3f center(0, 0, 0);
// ����һ��up�������ڼ����������x����
Vec3f up(0, 1, 0);

TGAImage AO(1024, 1024, TGAImage::GRAYSCALE);
TGAImage  occl(1024, 1024, TGAImage::GRAYSCALE);

// ����each_view_zbuffer 
struct ZShader : public IShader
{
    mat<4, 3, float> varying_clip_coord; //�ü��ռ����꣬��VSд�룬FS��ȡ

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

// ����AO��ͼ
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

        // ���������Ӱ����ڣ�u��v��λ�û�һ���׵�
        if (std::abs(each_view_zbuffer [int(gl_FragCoord.x + gl_FragCoord.y * width)] - gl_FragCoord.z < 1e-2))
        {
            occl.set(uv.x * 1024, uv.y * 1024, TGAColor(255));
        }
        color = TGAColor(255, 0, 0);
        return false;
    }
};

// ��λ����ѡȡ�����
Vec3f rand_point_on_unit_sphere() {
    float u = (float)rand() / (float)RAND_MAX;
    float v = (float)rand() / (float)RAND_MAX;
    float theta = 2.f * M_PI * u;
    float phi = acos(2.f * v - 1.f);
    return Vec3f(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(phi));
}

int main(int argc, char** argv) {
    // ��ȡģ���ļ�
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


    // ����AO��������������ɹ۲�λ��
    const int nrenders = 1000; //ѭ������
    for (int iter = 1; iter <= nrenders; iter++)
    {
        std::cerr << iter << " from " << nrenders << std::endl;
        
        ViewDir = rand_point_on_unit_sphere();
        ViewDir.y = std::abs(ViewDir.y); //ֻҪ�ϰ���
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

        // �����ػ��ư׵�ͼ
        // ����Щoccl�׵�ͼ�ۼ���������AO��ͼ����󽫵��ӵ���ɫ����ѭ���������õ����յ�AO��ͼ��
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

