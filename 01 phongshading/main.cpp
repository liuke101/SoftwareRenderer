#include <vector>
#include <iostream>
#include <algorithm>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model* model = NULL;

const int width = 800;
const int height = 800;

//定义光照位置
Vec3f LightDir(1, 1, 0);
//摄像机位置
Vec3f ViewDir(1.5, 0, 3);
//焦点位置
Vec3f center(0, 0, 0);
// 定义一个up向量用于计算摄像机的x向量
Vec3f up(0, 1, 0);


struct GouraudShader : public IShader
{ 
    // 顶点着色器会将数据写入varying_intensity
    // 片元着色器从varying_intensity中读取数据
    Vec3f varying_intensity;
    mat<2, 3, float> varying_uv; // mat<行，列, 类型>

    // M：模型空间->世界空间:不需要变化，这里模型空间=世界空间,为了便于理解设定为单位矩阵
    mat<4, 4, float> M_Model = mat<4, 4, float>::identity();
    // V：世界空间->观察空间
    mat<4, 4, float> M_View = LookAt;
    // P:观察空间->裁剪空间
    mat<4, 4, float> M_Projection = Projection;
    // MVP矩阵
    mat<4, 4, float> MVP = M_Projection * M_View * M_Model;

    mat<4, 4, float> M_View_IT = M_View.invert_transpose(); //逆转置

    // 接受两个变量，(面序号，顶点序号)
    virtual Vec4f vertex(int iface, int nthvert)
    {
        // 根据面序号和顶点序号读取模型对应顶点，并扩展为4维 
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));

        // 变换
        gl_Vertex = Viewport * MVP * gl_Vertex;

        // 得到漫反射强度
        varying_intensity[nthvert] = std::max(0.0f, model->normal(iface, nthvert) * LightDir);

        return gl_Vertex;
    }
    // 根据传入的重心坐标，颜色，以及varying_intensity计算出当前像素的颜色
    virtual bool fragment(Vec3f bar, TGAColor& color)
    {
        Vec2f uv = varying_uv * bar;
        TGAColor c = model->diffuse(uv);
        float intensity = varying_intensity * bar;
        color = c * intensity;
        return false;
    }
};

struct PhongShader : public IShader
{
    // 顶点着色器会将数据写入varying_uv
    // 片元着色器从varying_uv中读取数据
    mat<2, 3, float> varying_uv; // mat<行，列, 类型>

    // M：模型空间->世界空间:不需要变化，这里模型空间=世界空间,为了便于理解设定为单位矩阵
    mat<4, 4, float> M_Model = mat<4, 4, float>::identity();
    // V：世界空间->观察空间
    mat<4, 4, float> M_View = LookAt;
    // P:观察空间->裁剪空间
    mat<4, 4, float> M_Projection = Projection;
    // MVP矩阵
    mat<4, 4, float> M_MVP = M_Projection * M_View * M_Model;
    // MVP矩阵的逆转置，用于变换法线
    mat<4, 4, float> M_MVP_IT = M_MVP.invert_transpose();
    // M矩阵的逆转置，用于变换法线
    mat<4, 4, float> M_Model_IT = M_Model.invert_transpose();


    // 接受两个变量，(面序号，顶点序号)
    virtual Vec4f vertex(int iface, int nthvert)
    {
        // 根据面序号和顶点序号读取模型对应顶点，并扩展为4维 
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));

        // 变换
        gl_Vertex = Viewport * M_MVP * gl_Vertex;

        return gl_Vertex;
    }
    // 根据传入的重心坐标，颜色，以及varying_intensity计算出当前像素的颜色
    virtual bool fragment(Vec3f bar, TGAColor& color)
    {
        Vec2f uv = varying_uv * bar;
        // 在相同空间计算光照（也可以在其它空间，作者使用了MVP转换，在裁剪空间进行光照计算）
        Vec3f n = proj<3>(M_MVP_IT * embed<4>(model->normal(uv))).normalize();
        Vec3f l = proj<3>(M_MVP * embed<4>(LightDir)).normalize();
        Vec3f r = (n * (n * l * 2.f) - l).normalize();   //反射

        float spec = pow(std::max(r.z, 0.0f), model->specular(uv));
        float diffuse = std::max(0.0f, n * l);
        TGAColor c = model->diffuse(uv);
        color = c;
        for(int i =0;i<3;i++)
        {
            // 5 表示环境光分量，1 作为漫反射分量的权值，0.6 作为高光分量的权值，可以调节系数。
            color[i] = std::min<float>(5 + c[i] * (1 * diffuse + 0.6 * spec), 255);
        }
        return false;
    }
};

struct Shader :public IShader
{
    mat<2, 3, float> varying_uv;
    mat<4, 3, float> varying_tri;   // 三角形顶点坐标（裁剪空间）由VS写入FS读取
    mat<3, 3, float> varying_nrm;   //每个顶点的法线在片元着色器中插值
    mat<3, 3, float> ndc_tri;       // 三角形顶点坐标（NDC）



    // M：模型空间->世界空间:不需要变化，这里模型空间=世界空间,为了便于理解设定为单位矩阵
    mat<4, 4, float> M_Model = mat<4, 4, float>::identity();
    // V：世界空间->观察空间
    mat<4, 4, float> M_View = LookAt;
    // P:观察空间->裁剪空间
    mat<4, 4, float> M_Projection = Projection;
    // MVP矩阵
    mat<4, 4, float> M_MVP = M_Projection * M_View * M_Model;
    // MVP矩阵的逆转置，用于变换法线
    mat<4, 4, float> M_MVP_IT = M_MVP.invert_transpose();
    // M矩阵的逆转置，用于变换法线
    mat<4, 4, float> M_Model_IT = M_Model.invert_transpose();

    virtual Vec4f vertex(int iface, int nthvert)
    {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        varying_nrm.set_col(nthvert, proj<3>(M_MVP_IT * embed<4>(model->normal(iface, nthvert), 0.f)));

        Vec4f gl_Vertex = M_MVP * embed<4>(model->vert(iface, nthvert));
        varying_tri.set_col(nthvert, gl_Vertex);
        ndc_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor& color)
	{
        Vec3f bn = (varying_nrm * bar).normalize();
        Vec2f uv = varying_uv * bar;


        mat<3, 3, float> A;
        A[0] = ndc_tri.col(1) - ndc_tri.col(0);
        A[1] = ndc_tri.col(2) - ndc_tri.col(0);
        A[2] = bn;

        mat<3, 3, float> AI = A.invert();
        Vec3f i = AI * Vec3f(varying_uv[0][1] - varying_uv[0][0], varying_uv[0][2] - varying_uv[0][0], 0);
        Vec3f j = AI * Vec3f(varying_uv[1][1] - varying_uv[1][0], varying_uv[1][2] - varying_uv[1][0], 0);

        mat<3, 3, float> B;
        B.set_col(0, i.normalize());
        B.set_col(1, j.normalize());
        B.set_col(2, bn);

        Vec3f n = (B * model->normal(uv)).normalize();
        float diff = std::max(0.f, n * LightDir);
        color = model->diffuse(uv) * diff;
        return false;
    }
};

int main(int argc, char** argv)
{
    // 获取模型文件
    if (2 == argc) {
     model = new Model(argv[1]);
    } else {
     model = new Model("obj/african_head/african_head.obj");
    }

    // 初始化变换矩阵
    lookat(ViewDir, center, up);
    projection(-1.f / (ViewDir - center).norm());
    viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
    LightDir.normalize();

    // 初始化image和zbuffer
    TGAImage image(width, height, TGAImage::RGB);
    TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);


    // 实例化高洛德着色
    //GouraudShader shader;
    // 实例化Phong着色
    PhongShader shader;

    // 循环遍历所有三角形面
    for (int i = 0; i < model->nfaces(); i++)
    {
        Vec4f screen_coords[3];
        // 遍历当前三角形的所有顶点，并对每个顶点调用顶点着色器
        for (int j = 0; j < 3; j++)
        {
            // 变换顶点坐标到屏幕坐标 ***其实此时并不是真正的屏幕坐标，因为没有除以最后一个分量
            // 计算光照强度
            screen_coords[j] = shader.vertex(i, j);
        }

        //遍历完3个顶点，一个三角形光栅化完成
        //绘制三角形，triangle内部通过片元着色器对三角形着色
        triangle(screen_coords, shader, image, zbuffer);
    }

    image.flip_vertically();
    zbuffer.flip_vertically();
    image.write_tga_file("output.tga");
    zbuffer.write_tga_file("zbuffer.tga");

    delete model;
    return 0;
}
