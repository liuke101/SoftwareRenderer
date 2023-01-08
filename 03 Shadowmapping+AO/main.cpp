#include <vector>
#include <iostream>
#include <algorithm>
#include <complex>

#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model* model = NULL;
float* shadowbuffer = NULL; //shadowbuffer也是深度缓冲（zbuffer）,只不过是从光源位置看的

const int width = 800;
const int height = 800;

//定义光照位置
Vec3f LightDir(1, 0, 1);
//摄像机位置
Vec3f ViewDir(1,0, 3);
//焦点位置
Vec3f center(0, 0, 0);
// 定义一个up向量用于计算摄像机的x向量
Vec3f up(0, 1, 0);

// pass1：渲染摄像机在光源位置时的图像
struct DepthShader : public IShader
{
    mat<3, 3, float> varying_NDC_Vertex; //NDC屏幕坐标，由VS写入，FS读取

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

// pass2：渲染摄像机在观察位置时的图像
struct Shader : public IShader
{
    
    mat<2, 3, float> varying_uv;    //uv坐标，由VS写入，FS读取
    mat<3, 3, float> varying_NDC_vertex;   //NDC顶点坐标，由VS写入，FS读取

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
    // 将framebuffer屏幕坐标转换为shadowbuffer屏幕坐标
    mat<4, 4, float> M_Fbscreen2Sbscreen;

    Shader(Matrix MVP, Matrix MVP_IT, Matrix Fbscreen2Sbscreen)
	: M_MVP(MVP), M_MVP_IT(MVP_IT), M_Fbscreen2Sbscreen(Fbscreen2Sbscreen), varying_uv(), varying_NDC_vertex()
    {}

    // 顶点着色器
    virtual Vec4f vertex(int iface, int nthvert)
    {
        // 根据面序号和顶点序号读取模型对应顶点，并扩展为4维 
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));

        // 变换
        gl_Vertex = Viewport * M_MVP * gl_Vertex;
        varying_NDC_vertex.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));

        return gl_Vertex;
    }

    // 片元着色器
    virtual bool fragment(Vec3f bar, TGAColor& color)
    {
        // framebuffer屏幕坐标转换到shadowbuffer屏幕坐标
        Vec4f sb_p = M_Fbscreen2Sbscreen * embed<4>(varying_NDC_vertex * bar);  //shadowbuffer中的对应点
        sb_p = sb_p / sb_p[3];
        int idx = int(sb_p[0]) + int(sb_p[1]) * width; //shadowbuffer的索引

        // 通过比较在shadowbuffer屏幕空间的z值与shadowbuffer值确定颜色，如果z值大于shadowbuffer那么shadow = 1，否则=0.3，作为颜色权重，这样就区分了明暗
        //float shadow = 0.3 + 0.7 * (sb_p[2] > shadowbuffer[idx]); // 发生深度冲突（z-fighting）
        //简单地将一个 z-buffer 相对另一个移动一点，就足以消除走样。
        float shadow = 0.3 + 0.7 * (sb_p[2] + 43.3 > shadowbuffer[idx]);
       
        Vec2f uv = varying_uv * bar;
        // 在相同空间计算光照（也可以在其它空间，作者使用了MVP转换，在裁剪空间进行光照计算）
        Vec3f n = proj<3>(M_MVP_IT * embed<4>(model->normal(uv))).normalize();
        Vec3f l = proj<3>(M_MVP * embed<4>(LightDir)).normalize();
        Vec3f r = (n * (n * l * 2.f) - l).normalize();   //反射

        Vec3f v = proj<3>(M_MVP * embed<4>(ViewDir)).normalize();

        float specular = pow(std::max(0.0f, v * r), 16) *  0.5 * model->specular(uv);
        float diffuse = std::max(0.0f, n * l);
        
        //纹理采样
        TGAColor basecolor = model->diffuse(uv);
        TGAColor emissive = model->emissive(uv);
        for(int i =0;i<3;i++)
        {
            // 20 表示环境光分量，1.2 作为漫反射分量的权值，0.6 作为高光分量的权值，可以调节系数。
            //color[i] = model->ao(uv);
            color[i] = std::min<float>(20+ 5 * emissive[i] + basecolor[i] * shadow * (diffuse + 0.6 * specular) * 0.01 * model->ao(uv), 255);
        }

        return false;
    }
};



int main(int argc, char** argv)
{
    // 获取模型文件
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


    // 初始化变换矩阵
    viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
    LightDir.normalize();
   

    // 渲染shadowbuffer
    {
        TGAImage depth(width, height, TGAImage::RGB);
        lookat(LightDir, center, up); //把摄像机放到光源位置
        projection(0);  //这里为什么设置为0？

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

    // 保存shadowbuffer模型空间->屏幕空间的变换矩阵
    Matrix M_sb_Model2Screen = Viewport * Projection * LookAt;

    // 渲染framebuffer帧缓冲
    {
        // 初始化frame
        TGAImage frame(width, height, TGAImage::RGB);
        lookat(ViewDir, center, up); //把摄像机放到观察位置
        projection(-1.f / (ViewDir - center).norm());

        mat<4, 4, float> M_Model = mat<4, 4, float>::identity();
        mat<4, 4, float> M_View = LookAt;
        mat<4, 4, float> M_Projection = Projection;
        mat<4, 4, float> M_MVP = M_Projection * M_View * M_Model;
        mat<4, 4, float> M_MVP_IT = M_MVP.invert_transpose();
        mat<4, 4, float> M_Fbscreen2Sbscreen = M_sb_Model2Screen * (Viewport * M_MVP).invert(); // 将顶点屏幕坐标转换为shadowbuffer屏幕坐标(framebuffer屏幕空间->模型空间->shadowbuffer屏幕空间)

        // 实例化shader
        Shader shader(M_MVP, M_MVP_IT, M_Fbscreen2Sbscreen);

        Vec4f screen_coords[3];
        // 循环遍历所有三角形面
        for (int i = 0; i < model->nfaces(); i++)
        {
            // 遍历当前三角形的所有顶点，并对每个顶点调用顶点着色器
            for (int j = 0; j < 3; j++)
            {
                // 变换顶点坐标到屏幕坐标 ***其实此时并不是真正的屏幕坐标，因为没有除以最后一个分量
                screen_coords[j] = shader.vertex(i, j);
            }
    
            //遍历完3个顶点，一个三角形光栅化完成
            //绘制三角形，triangle内部通过片元着色器对三角形着色
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
