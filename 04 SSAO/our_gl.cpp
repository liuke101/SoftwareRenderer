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

// 重心坐标绘制三角形(裁剪空间坐标，zbuffer指针，tga指针，颜色)
void triangle(mat<4, 3, float>& clip_coord, IShader& shader, TGAImage& image, float *zbuffer)
{
    // 屏幕空间坐标 坐标用行表示（x,y,z,w）,共三行
    mat<3, 4, float> pts = (Viewport * clip_coord).transpose(); // 转置方便访问每个点
    mat<3, 2, float> pts2;  //只存x，y

	// 屏幕空间坐标转换成NDC坐标
    for (int i = 0; i < 3; i++)
    {
        pts2[i] = proj<2>(pts[i] / pts[i][3]);
    }

    // 初始化Bounding box
    Vec2f bboxmin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    Vec2f bboxmax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    Vec2f clamp(image.get_width() - 1, image.get_height() - 1);
    // bboxmin[0] bboxmax[0] 存box的x范围
    // bboxmin[1] bboxmax[1] 存box的y范围
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            bboxmin[j] = std::max(0.f, std::min(bboxmin[j], pts2[i][j]));
            bboxmax[j] = std::min(clamp[j], std::max(bboxmax[j], pts2[i][j]));
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
            //bc_screen为当前P对应的重心坐标
            Vec3f bc_screen = barycentric(pts2[0], pts2[1], pts2[2], P);
            Vec3f bc_clip = Vec3f(bc_screen.x / pts[0][3], bc_screen.y / pts[1][3], bc_screen.z / pts[2][3]);

            //插值计算P的zbuffer
            bc_clip = bc_clip / (bc_clip.x + bc_clip.y + bc_clip.z);
            float frag_depth = clip_coord[2] * bc_clip;

            // P的任一质心分量小于0或者zbuffer小于已有zbuffer，不渲染
            if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z<0 || zbuffer[P.x + P.y * image.get_width()]>frag_depth) continue;

            //调用片元着色器计算当前像素颜色
            bool discard = shader.fragment(Vec3f(P.x, P.y, frag_depth), bc_clip, color);
            if (!discard) {
                //zbuffer
                zbuffer[P.x + P.y * image.get_width()] = frag_depth;
                //为像素设置颜色
                image.set(P.x, P.y, color);
            }
        }
    }
}