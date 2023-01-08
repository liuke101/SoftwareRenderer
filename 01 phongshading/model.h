#ifndef __MODEL_H__
#define __MODEL_H__
#include <vector>
#include <string>
#include "geometry.h"
#include "tgaimage.h"

class Model {
private:
    std::vector<Vec3f> verts_;
    std::vector<std::vector<Vec3i> > faces_; //  Vec3i means vertex/uv/normal �� faces[��][vertex/uv/normal]
    std::vector<Vec3f> norms_;
    std::vector<Vec2f> uv_;
    TGAImage diffusemap_;   // ��������ͼ��Basecolor��
    TGAImage normalmap_;    // ������ͼ
    TGAImage specularmap_;  // �߹���ͼ
    void load_texture(std::string filename, const char *suffix, TGAImage &img);
public:
    Model(const char *filename);
    ~Model();
    int nverts();   // ���ض�����
    int nfaces();   // ��������
    Vec3f normal(int iface, int nthvert);   //���ط���
    Vec3f normal(Vec2f uv); //����
    Vec3f vert(int i);  // ���ض�������
    Vec3f vert(int iface, int nthvert); // ���ض�������
    Vec2f uv(int iface, int nthvert);   //����uv����
    TGAColor diffuse(Vec2f uv);
    float specular(Vec2f uv);
    std::vector<int> face(int idx);
};
#endif //__MODEL_H__

