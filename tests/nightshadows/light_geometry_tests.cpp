#include "../../source/ShadowLightGeometry.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace fusionfix::shadows;
static int checks = 0;
static void Check(bool success) { ++checks; if (!success) { std::printf("FAIL check %d\n", checks); std::exit(1); } }
int main()
{
    const Vec3 light{-970, 740, 14}, east{1, 0, 0};
    Check(LightVolumeContains({-965,740,14},light,east,0,5,0));
    Check(!LightVolumeContains({-964,740,14},light,east,0,5,0));
    Check(LightVolumeContains({-975,740,14},light,east,0,8,0));
    Check(LightVolumeContains({-965,740,14},light,east,2,10,0.8f));
    Check(!LightVolumeContains({-975,740,14},light,east,2,10,0.8f));
    Check(!LightVolumeContains({-970,745,14},light,east,2,10,0.8f));
    Check(LightVolumeContains({-965,741,14},light,east,2,10,0.8f));
    Check(!LightVolumeContains({-965,745,14},light,east,2,10,0.8f));
    Check(LightVolumeContains({-965,741,14},light,{7,0,0},2,10,0.8f));
    Check(!LightVolumeContains({-965,741,14},light,{-7,0,0},2,10,0.8f));
    Check(!LightVolumeContains({-965,740,14},light,east,2,3,0.8f));
    Check(!LightVolumeContains({-965,740,14},light,east,2,10,45));
    Check(!LightVolumeContains({-965,740,14},light,{},2,10,0.8f));
    for (int type : {-1,1,3,4,5,100}) Check(!LightVolumeContains(light,light,east,type,10,0.8f));
    const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
    for (float invalid : {nan,inf,-inf}) {
        Check(!LightVolumeContains({invalid,740,14},light,east,0,10,0));
        Check(!LightVolumeContains(light,{invalid,740,14},east,0,10,0));
        Check(!LightVolumeContains(light,light,{invalid,0,0},2,10,0.8f));
        Check(!LightVolumeContains(light,light,east,0,invalid,0));
        Check(!LightVolumeContains(light,light,east,2,10,invalid));
    }
    Check(!LightVolumeContains(light,light,east,0,0,0));
    Check(!LightVolumeContains(light,light,east,0,-2,0));
    Check(!LightVolumeContains(light,light,east,2,10,0));
    Check(!LightVolumeContains(light,light,east,2,10,-1));
    Check(LightVolumeContains(light,light,east,2,10,0.8f));
    std::printf("PASS %d light-volume checks; geometric relevance only, not visual proof.\n",checks);
}
