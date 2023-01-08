#include <cmath>
#include <limits>
#include <cstdlib>
#include "our_gl.h"
#include <algorithm>

Matrix LookAt;
// 视口变换: 裁剪空间->屏幕空间
Matrix Viewport;
Matrix Projection;

IShader::~IShader() {}


// 视口变换: 裁剪空间->NDC->屏幕空间(未完成)
// 在裁剪空间先完成裁剪，然后对裁剪坐标执行透视除法后就能变换到标准化设备坐标（NDC），此时才是转换到了屏幕空间。

// 注意此时xyzw分量还没有执行透视除法(除以w分量)，所以此时还没有转换到屏幕空间
// 修正方法：我们在绘制三角形，判断边界以及计算重心的时候，都除以了最后一个分量。
void viewport(int x, int y, int w, int h)
{
    Viewport = Matrix::identity();
    //第4列表示平移信息
    Viewport[0][3] = x + w / 2.f;
    Viewport[1][3] = y + h / 2.f;
    Viewport[2][3] = depth / 2.f;

    //对角线表示缩放信息
    Viewport[0][0] = w / 2.f;
    Viewport[1][1] = h / 2.f;
    Viewport[2][2] = depth / 2.f;
}

//投影矩阵
void projection(float coeff) {
    Projection = Matrix::identity();
    Projection[3][2] = coeff;
}

// lookat矩阵
void lookat(Vec3f ViewDir, Vec3f center, Vec3f up)
{
    //计算出z，根据z和up算出x，再算出y
    Vec3f z = (ViewDir - center).normalize();
    Vec3f x = cross(up, z).normalize();
    Vec3f y = cross(z, x).normalize();

    LookAt = Matrix::identity();
    Matrix rotation = Matrix::identity();
    Matrix translation = Matrix::identity();

    //矩阵的第四列是用于平移的。因为观察位置从原点变为了center，所以需要将物体平移-center
    for (int i = 0; i < 3; i++)
    {
        translation[i][3] = -center[i];
    }

    for (int i = 0; i < 3; i++)
    {
        rotation[0][i] = x[i];
        rotation[1][i] = y[i];
        rotation[2][i] = z[i];
    }

    LookAt = rotation * translation;
}

//计算重心坐标
Vec3f barycentric(Vec2f A, Vec2f B, Vec2f C, Vec2f P)
{
    Vec3f s[2];
    //解读：
    //s[0]存储AC，AB，PA的x分量
    //s[1]存储AC，AB，PA的y分量
    //s[2]存储AC，AB，PA的z分量
    for (int i = 2; i--; ) {
        s[i][0] = C[i] - A[i];
        s[i][1] = B[i] - A[i];
        s[i][2] = A[i] - P[i];
    }
    //[u,v,1]和[AB,AC,PA]对应的x和y向量都垂直，所以叉乘
    Vec3f u = cross(s[0], s[1]);
    //三点共线时，会导致u[2]为0，此时返回(-1,1,1)
    if (std::abs(u[2]) > 1e-2)
        //若1-u-v，u，v全为大于0的数，表示点在三角形内部
        return Vec3f(1.f - (u.x + u.y) / u.z, u.y / u.z, u.x / u.z);
		//return Vec3f(1.0f , 1.0f, 1.0f);   //如果重心坐标返回一个常量，那么一个三角形内的所有点重心坐标都是相同的，所以会插值计算出相同的颜色，造成体素形态的着色
    return Vec3f(-1, 1, 1);
}

// 重心坐标绘制三角形(坐标数组，zbuffer指针，tga指针，颜色)
void triangle(Vec4f* pts, IShader& shader, TGAImage& image, float *zbuffer)
{
    // 初始化Bounding box
    Vec2f bboxmin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    Vec2f bboxmax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());

    // bboxmin[0] bboxmax[0] 存box的x范围
    // bboxmin[1] bboxmax[1] 存box的y范围
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            // 这里的[]都是重载运算符，[0]表示x分量，[1]表示y分量
            // 这里pts除以了最后一个分量，实现了透视中的缩放，所以作为边界框
            bboxmin[j] = std::min(bboxmin[j], pts[i][j] / pts[i][3]);
            bboxmax[j] = std::max(bboxmax[j], pts[i][j] / pts[i][3]);
        }
    }

    // 当前像素坐标P，颜色color
    Vec2i P;
    TGAColor color;
    // 遍历边框中的每一个点
    for (P.x = bboxmin.x; P.x <= bboxmax.x; P.x++)
    {
        for (P.y = bboxmin.y; P.y <= bboxmax.y; P.y++)
        {
            //c为当前P对应的重心坐标
            //这里pts除以了最后一个分量，实现了透视中的缩放，所以用于判断P是否在三角形内
            Vec3f c = barycentric(proj<2>(pts[0] / pts[0][3]), proj<2>(pts[1] / pts[1][3]), proj<2>(pts[2] / pts[2][3]), proj<2>(P));
            //插值计算P的zbuffer
            //pts[i]为三角形的三个顶点 
            //pts[i][2]为三角形的z信息(0~255)
            //pts[i][3]为三角形的w分量：投影系数(1-z/c)

            float z = pts[0][2] * c.x + pts[1][2] * c.y + pts[2][2] * c.z;
            float w = pts[0][3] * c.x + pts[1][3] * c.y + pts[2][3] * c.z;
            int frag_depth = z / w;

            // P的任一质心分量小于0或者zbuffer小于已有zbuffer，不渲染
            if (c.x < 0 || c.y < 0 || c.z < 0 || zbuffer[P.x + P.y * image.get_width()]>frag_depth) continue;

            // 透视矫正
            //求α，β，γ,只需要除以pts第四个分量即可
            Vec3f c_revised = { 0,0,0 };
            for(int i =0;i<3;i++)
            {
                c_revised[i] = c[i] / pts[i][3];
            }
            float Z_n = 1. / (c_revised[0] + c_revised[1] + c_revised[2]);
            for (int i = 0; i < 3; ++i)
            {
                //求正确透视下插值的系数
                c_revised[i] *= Z_n;
            }

            //调用片元着色器计算当前像素颜色
            bool discard = shader.fragment(c_revised, color);
            if (!discard) {
                //zbuffer
                zbuffer[P.x + P.y * image.get_width()] = frag_depth;
                //为像素设置颜色
                image.set(P.x, P.y, color);
            }
        }
    }
}