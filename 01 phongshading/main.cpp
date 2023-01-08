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

//�������λ��
Vec3f LightDir(1, 1, 0);
//�����λ��
Vec3f ViewDir(1.5, 0, 3);
//����λ��
Vec3f center(0, 0, 0);
// ����һ��up�������ڼ����������x����
Vec3f up(0, 1, 0);


struct GouraudShader : public IShader
{ 
    // ������ɫ���Ὣ����д��varying_intensity
    // ƬԪ��ɫ����varying_intensity�ж�ȡ����
    Vec3f varying_intensity;
    mat<2, 3, float> varying_uv; // mat<�У���, ����>

    // M��ģ�Ϳռ�->����ռ�:����Ҫ�仯������ģ�Ϳռ�=����ռ�,Ϊ�˱�������趨Ϊ��λ����
    mat<4, 4, float> M_Model = mat<4, 4, float>::identity();
    // V������ռ�->�۲�ռ�
    mat<4, 4, float> M_View = LookAt;
    // P:�۲�ռ�->�ü��ռ�
    mat<4, 4, float> M_Projection = Projection;
    // MVP����
    mat<4, 4, float> MVP = M_Projection * M_View * M_Model;

    mat<4, 4, float> M_View_IT = M_View.invert_transpose(); //��ת��

    // ��������������(����ţ��������)
    virtual Vec4f vertex(int iface, int nthvert)
    {
        // ��������źͶ�����Ŷ�ȡģ�Ͷ�Ӧ���㣬����չΪ4ά 
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));

        // �任
        gl_Vertex = Viewport * MVP * gl_Vertex;

        // �õ�������ǿ��
        varying_intensity[nthvert] = std::max(0.0f, model->normal(iface, nthvert) * LightDir);

        return gl_Vertex;
    }
    // ���ݴ�����������꣬��ɫ���Լ�varying_intensity�������ǰ���ص���ɫ
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
    // ������ɫ���Ὣ����д��varying_uv
    // ƬԪ��ɫ����varying_uv�ж�ȡ����
    mat<2, 3, float> varying_uv; // mat<�У���, ����>

    // M��ģ�Ϳռ�->����ռ�:����Ҫ�仯������ģ�Ϳռ�=����ռ�,Ϊ�˱�������趨Ϊ��λ����
    mat<4, 4, float> M_Model = mat<4, 4, float>::identity();
    // V������ռ�->�۲�ռ�
    mat<4, 4, float> M_View = LookAt;
    // P:�۲�ռ�->�ü��ռ�
    mat<4, 4, float> M_Projection = Projection;
    // MVP����
    mat<4, 4, float> M_MVP = M_Projection * M_View * M_Model;
    // MVP�������ת�ã����ڱ任����
    mat<4, 4, float> M_MVP_IT = M_MVP.invert_transpose();
    // M�������ת�ã����ڱ任����
    mat<4, 4, float> M_Model_IT = M_Model.invert_transpose();


    // ��������������(����ţ��������)
    virtual Vec4f vertex(int iface, int nthvert)
    {
        // ��������źͶ�����Ŷ�ȡģ�Ͷ�Ӧ���㣬����չΪ4ά 
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));

        // �任
        gl_Vertex = Viewport * M_MVP * gl_Vertex;

        return gl_Vertex;
    }
    // ���ݴ�����������꣬��ɫ���Լ�varying_intensity�������ǰ���ص���ɫ
    virtual bool fragment(Vec3f bar, TGAColor& color)
    {
        Vec2f uv = varying_uv * bar;
        // ����ͬ�ռ������գ�Ҳ�����������ռ䣬����ʹ����MVPת�����ڲü��ռ���й��ռ��㣩
        Vec3f n = proj<3>(M_MVP_IT * embed<4>(model->normal(uv))).normalize();
        Vec3f l = proj<3>(M_MVP * embed<4>(LightDir)).normalize();
        Vec3f r = (n * (n * l * 2.f) - l).normalize();   //����

        float spec = pow(std::max(r.z, 0.0f), model->specular(uv));
        float diffuse = std::max(0.0f, n * l);
        TGAColor c = model->diffuse(uv);
        color = c;
        for(int i =0;i<3;i++)
        {
            // 5 ��ʾ�����������1 ��Ϊ�����������Ȩֵ��0.6 ��Ϊ�߹������Ȩֵ�����Ե���ϵ����
            color[i] = std::min<float>(5 + c[i] * (1 * diffuse + 0.6 * spec), 255);
        }
        return false;
    }
};

struct Shader :public IShader
{
    mat<2, 3, float> varying_uv;
    mat<4, 3, float> varying_tri;   // �����ζ������꣨�ü��ռ䣩��VSд��FS��ȡ
    mat<3, 3, float> varying_nrm;   //ÿ������ķ�����ƬԪ��ɫ���в�ֵ
    mat<3, 3, float> ndc_tri;       // �����ζ������꣨NDC��



    // M��ģ�Ϳռ�->����ռ�:����Ҫ�仯������ģ�Ϳռ�=����ռ�,Ϊ�˱�������趨Ϊ��λ����
    mat<4, 4, float> M_Model = mat<4, 4, float>::identity();
    // V������ռ�->�۲�ռ�
    mat<4, 4, float> M_View = LookAt;
    // P:�۲�ռ�->�ü��ռ�
    mat<4, 4, float> M_Projection = Projection;
    // MVP����
    mat<4, 4, float> M_MVP = M_Projection * M_View * M_Model;
    // MVP�������ת�ã����ڱ任����
    mat<4, 4, float> M_MVP_IT = M_MVP.invert_transpose();
    // M�������ת�ã����ڱ任����
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
    // ��ȡģ���ļ�
    if (2 == argc) {
     model = new Model(argv[1]);
    } else {
     model = new Model("obj/african_head/african_head.obj");
    }

    // ��ʼ���任����
    lookat(ViewDir, center, up);
    projection(-1.f / (ViewDir - center).norm());
    viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
    LightDir.normalize();

    // ��ʼ��image��zbuffer
    TGAImage image(width, height, TGAImage::RGB);
    TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);


    // ʵ�����������ɫ
    //GouraudShader shader;
    // ʵ����Phong��ɫ
    PhongShader shader;

    // ѭ������������������
    for (int i = 0; i < model->nfaces(); i++)
    {
        Vec4f screen_coords[3];
        // ������ǰ�����ε����ж��㣬����ÿ��������ö�����ɫ��
        for (int j = 0; j < 3; j++)
        {
            // �任�������굽��Ļ���� ***��ʵ��ʱ��������������Ļ���꣬��Ϊû�г������һ������
            // �������ǿ��
            screen_coords[j] = shader.vertex(i, j);
        }

        //������3�����㣬һ�������ι�դ�����
        //���������Σ�triangle�ڲ�ͨ��ƬԪ��ɫ������������ɫ
        triangle(screen_coords, shader, image, zbuffer);
    }

    image.flip_vertically();
    zbuffer.flip_vertically();
    image.write_tga_file("output.tga");
    zbuffer.write_tga_file("zbuffer.tga");

    delete model;
    return 0;
}
