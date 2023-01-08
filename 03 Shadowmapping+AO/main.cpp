#include <vector>
#include <iostream>
#include <algorithm>
#include <complex>

#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model* model = NULL;
float* shadowbuffer = NULL; //shadowbufferҲ����Ȼ��壨zbuffer��,ֻ�����Ǵӹ�Դλ�ÿ���

const int width = 800;
const int height = 800;

//�������λ��
Vec3f LightDir(1, 0, 1);
//�����λ��
Vec3f ViewDir(1,0, 3);
//����λ��
Vec3f center(0, 0, 0);
// ����һ��up�������ڼ����������x����
Vec3f up(0, 1, 0);

// pass1����Ⱦ������ڹ�Դλ��ʱ��ͼ��
struct DepthShader : public IShader
{
    mat<3, 3, float> varying_NDC_Vertex; //NDC��Ļ���꣬��VSд�룬FS��ȡ

    mat<4, 4, float> M_Model = mat<4, 4, float>::identity();
    mat<4, 4, float> M_View = LookAt;
    mat<4, 4, float> M_Projection = Projection;
    mat<4, 4, float> M_MVP = M_Projection * M_View * M_Model;

    DepthShader() : varying_NDC_Vertex() {}

    virtual Vec4f vertex(int iface, int nthvert)
    {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Viewport* M_MVP * gl_Vertex;
        varying_NDC_Vertex.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));

        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor& color) {
        Vec3f p = varying_NDC_Vertex * bar;
        color = TGAColor(255, 255, 255) * (p.z / depth);
        return false;
    }
};

// pass2����Ⱦ������ڹ۲�λ��ʱ��ͼ��
struct Shader : public IShader
{
    
    mat<2, 3, float> varying_uv;    //uv���꣬��VSд�룬FS��ȡ
    mat<3, 3, float> varying_NDC_vertex;   //NDC�������꣬��VSд�룬FS��ȡ

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
    // ��framebuffer��Ļ����ת��Ϊshadowbuffer��Ļ����
    mat<4, 4, float> M_Fbscreen2Sbscreen;

    Shader(Matrix MVP, Matrix MVP_IT, Matrix Fbscreen2Sbscreen)
	: M_MVP(MVP), M_MVP_IT(MVP_IT), M_Fbscreen2Sbscreen(Fbscreen2Sbscreen), varying_uv(), varying_NDC_vertex()
    {}

    // ������ɫ��
    virtual Vec4f vertex(int iface, int nthvert)
    {
        // ��������źͶ�����Ŷ�ȡģ�Ͷ�Ӧ���㣬����չΪ4ά 
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));

        // �任
        gl_Vertex = Viewport * M_MVP * gl_Vertex;
        varying_NDC_vertex.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));

        return gl_Vertex;
    }

    // ƬԪ��ɫ��
    virtual bool fragment(Vec3f bar, TGAColor& color)
    {
        // framebuffer��Ļ����ת����shadowbuffer��Ļ����
        Vec4f sb_p = M_Fbscreen2Sbscreen * embed<4>(varying_NDC_vertex * bar);  //shadowbuffer�еĶ�Ӧ��
        sb_p = sb_p / sb_p[3];
        int idx = int(sb_p[0]) + int(sb_p[1]) * width; //shadowbuffer������

        // ͨ���Ƚ���shadowbuffer��Ļ�ռ��zֵ��shadowbufferֵȷ����ɫ�����zֵ����shadowbuffer��ôshadow = 1������=0.3����Ϊ��ɫȨ�أ�����������������
        //float shadow = 0.3 + 0.7 * (sb_p[2] > shadowbuffer[idx]); // ������ȳ�ͻ��z-fighting��
        //�򵥵ؽ�һ�� z-buffer �����һ���ƶ�һ�㣬����������������
        float shadow = 0.3 + 0.7 * (sb_p[2] + 43.3 > shadowbuffer[idx]);
       
        Vec2f uv = varying_uv * bar;
        // ����ͬ�ռ������գ�Ҳ�����������ռ䣬����ʹ����MVPת�����ڲü��ռ���й��ռ��㣩
        Vec3f n = proj<3>(M_MVP_IT * embed<4>(model->normal(uv))).normalize();
        Vec3f l = proj<3>(M_MVP * embed<4>(LightDir)).normalize();
        Vec3f r = (n * (n * l * 2.f) - l).normalize();   //����

        Vec3f v = proj<3>(M_MVP * embed<4>(ViewDir)).normalize();

        float specular = pow(std::max(0.0f, v * r), 16) *  0.5 * model->specular(uv);
        float diffuse = std::max(0.0f, n * l);
        
        //�������
        TGAColor basecolor = model->diffuse(uv);
        TGAColor emissive = model->emissive(uv);
        for(int i =0;i<3;i++)
        {
            // 20 ��ʾ�����������1.2 ��Ϊ�����������Ȩֵ��0.6 ��Ϊ�߹������Ȩֵ�����Ե���ϵ����
            //color[i] = model->ao(uv);
            color[i] = std::min<float>(20+ 5 * emissive[i] + basecolor[i] * shadow * (diffuse + 0.6 * specular) * 0.01 * model->ao(uv), 255);
        }

        return false;
    }
};



int main(int argc, char** argv)
{
    // ��ȡģ���ļ�
    if (2 == argc) {
     model = new Model(argv[1]);
    } else {
     //model = new Model("obj/african_head/african_head.obj");
     model = new Model("obj/diablo3_pose/diablo3_pose.obj");
     
    }

    float* zbuffer = new float[width * height];
    shadowbuffer = new float[width * height];
    for(int i = width*height;--i; )
    {
        zbuffer[i] = shadowbuffer[i] = -std::numeric_limits<float>::max();
    }


    // ��ʼ���任����
    viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
    LightDir.normalize();
   

    // ��Ⱦshadowbuffer
    {
        TGAImage depth(width, height, TGAImage::RGB);
        lookat(LightDir, center, up); //��������ŵ���Դλ��
        projection(0);  //����Ϊʲô����Ϊ0��

        DepthShader depthshader;
        Vec4f screen_coords[3];
        for(int i=0; i<model->nfaces(); i++)
        {
	        for(int j =0;j<3;j++)
	        {
                screen_coords[j] = depthshader.vertex(i, j);
	        }
            triangle(screen_coords, depthshader, depth, shadowbuffer);
        }
        depth.flip_vertically();
        depth.write_tga_file("depth.tga");
    }

    // ����shadowbufferģ�Ϳռ�->��Ļ�ռ�ı任����
    Matrix M_sb_Model2Screen = Viewport * Projection * LookAt;

    // ��Ⱦframebuffer֡����
    {
        // ��ʼ��frame
        TGAImage frame(width, height, TGAImage::RGB);
        lookat(ViewDir, center, up); //��������ŵ��۲�λ��
        projection(-1.f / (ViewDir - center).norm());

        mat<4, 4, float> M_Model = mat<4, 4, float>::identity();
        mat<4, 4, float> M_View = LookAt;
        mat<4, 4, float> M_Projection = Projection;
        mat<4, 4, float> M_MVP = M_Projection * M_View * M_Model;
        mat<4, 4, float> M_MVP_IT = M_MVP.invert_transpose();
        mat<4, 4, float> M_Fbscreen2Sbscreen = M_sb_Model2Screen * (Viewport * M_MVP).invert(); // ��������Ļ����ת��Ϊshadowbuffer��Ļ����(framebuffer��Ļ�ռ�->ģ�Ϳռ�->shadowbuffer��Ļ�ռ�)

        // ʵ����shader
        Shader shader(M_MVP, M_MVP_IT, M_Fbscreen2Sbscreen);

        Vec4f screen_coords[3];
        // ѭ������������������
        for (int i = 0; i < model->nfaces(); i++)
        {
            // ������ǰ�����ε����ж��㣬����ÿ��������ö�����ɫ��
            for (int j = 0; j < 3; j++)
            {
                // �任�������굽��Ļ���� ***��ʵ��ʱ��������������Ļ���꣬��Ϊû�г������һ������
                screen_coords[j] = shader.vertex(i, j);
            }
    
            //������3�����㣬һ�������ι�դ�����
            //���������Σ�triangle�ڲ�ͨ��ƬԪ��ɫ������������ɫ
            triangle(screen_coords, shader, frame, zbuffer);
        }
    
        frame.flip_vertically();
        frame.write_tga_file("output.tga");
    }
    delete model;
    delete[] zbuffer;
    delete[] shadowbuffer;

    return 0;
}
