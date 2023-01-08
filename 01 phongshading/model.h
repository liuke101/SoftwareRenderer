#ifndef __MODEL_H__
#define __MODEL_H__
#include <vector>
#include <string>
#include "geometry.h"
#include "tgaimage.h"

class Model {
private:
    std::vector<Vec3f> verts_;
    std::vector<std::vector<Vec3i> > faces_; //  Vec3i means vertex/uv/normal 即 faces[面][vertex/uv/normal]
    std::vector<Vec3f> norms_;
    std::vector<Vec2f> uv_;
    TGAImage diffusemap_;   // 漫反射贴图（Basecolor）
    TGAImage normalmap_;    // 法线贴图
    TGAImage specularmap_;  // 高光贴图
    void load_texture(std::string filename, const char *suffix, TGAImage &img);
public:
    Model(const char *filename);
    ~Model();
    int nverts();   // 返回顶点数
    int nfaces();   // 返回面数
    Vec3f normal(int iface, int nthvert);   //返回法线
    Vec3f normal(Vec2f uv); //法线
    Vec3f vert(int i);  // 返回顶点坐标
    Vec3f vert(int iface, int nthvert); // 返回顶点坐标
    Vec2f uv(int iface, int nthvert);   //返回uv坐标
    TGAColor diffuse(Vec2f uv);
    float specular(Vec2f uv);
    std::vector<int> face(int idx);
};
#endif //__MODEL_H__

