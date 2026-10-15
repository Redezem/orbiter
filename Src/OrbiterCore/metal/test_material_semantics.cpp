// CPU contract tests only; GPU evidence is material_regression.py.
#include "material_semantics.h"
#include <cassert>
#include <limits>

int main() {
    using namespace core_metal;
    unsigned previous = defaultMaterial;
    assert(resolveMaterial(inheritMaterial, previous, 3) == defaultMaterial);
    assert(resolveMaterial(0, previous, 3) == 0); // zero is first actual material, NOT none
    assert(resolveMaterial(inheritMaterial, previous, 3) == 0);
    assert(resolveMaterial(2, previous, 3) == 2);
    assert(resolveMaterial(inheritMaterial, previous, 3) == 2);
    assert(resolveMaterial(defaultMaterial, previous, 3) == defaultMaterial);
    assert(resolveMaterial(inheritMaterial, previous, 3) == defaultMaterial);
    bool rejected = false;
    try { resolveMaterial(3, previous, 3); } catch (const std::runtime_error &) { rejected=true; }
    assert(rejected);
    assert(validDiffuse(0, 1, .5f, .1f));
    assert(!validDiffuse(std::numeric_limits<float>::quiet_NaN(), 0, 0, 1));
    assert(!validDiffuse(0, -1, 0, 1));
    assert(!validDiffuse(1.1f, 0, 0, 1)); // bounded normalized diffuse pass
    const float gray[4]={.235f,.235f,.235f,.5f};
    const unsigned char valid[4]={30,30,30,255}, badRatio[4]={30,30,60,255};
    const unsigned char oldShader[4]={100,100,100,255}, badAlpha[4]={30,30,30,128};
    assert(diffusePixel(valid,gray));
    assert(!diffusePixel(badRatio,gray));
    assert(!diffusePixel(oldShader,gray));
    assert(!diffusePixel(badAlpha,gray));
}
