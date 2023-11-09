#ifndef NORMALMAPPING_HLSL
#define NORMALMAPPING_HLSL

// http://www.thetenthplanet.de/archives/1180
/*
    N and P need to be in the same space, the resulting matrix
    then transforms tangent space into that space
*/
float3x3 cotangentFrame( float3 N, float3 p, float2 uv )
{
    // get edge vectors of the pixel triangle
    float3 dp1 = ddx_fine( p );
    float3 dp2 = ddy_fine( p );
    float2 duv1 = ddx_fine( uv );
    float2 duv2 = ddy_fine( uv );
 
    // solve the linear system
    float3 dp2perp = cross( dp2, N );
    float3 dp1perp = cross( N, dp1 );
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
 
    // construct a scale-invariant frame 
    float invmax = rsqrt( max( dot(T,T), dot(B,B) ) );
    return transpose(float3x3( T * invmax, B * invmax, N ));
    //TODO: without transpose, initialize in correct order directly
}

// https://jcgt.org/published/0009/03/04/paper.pdf
//  Surface Gradient–Based Bump Mapping Framework

namespace SurfGrad
{
    static float3 sigmaX;
    static float3 sigmaY;
    static float3 baseNormal;
    static float3 dPdx;
    static float3 dPdy;
    static float flip_sign;

    static void initParams(const float3 relSurfPos, const float3 baseNormal)
    {
        #define SGP SurfGrad
        SGP::baseNormal = baseNormal;
        SGP::dPdx = ddx_fine(relSurfPos);
        SGP::dPdy = ddy_fine(relSurfPos);
        SGP::sigmaX = SGP::dPdx - dot(SGP::dPdx, SGP::baseNormal)*SGP::baseNormal;
        SGP::sigmaY = SGP::dPdy - dot(SGP::dPdy, SGP::baseNormal)*SGP::baseNormal;
        SGP::flip_sign = dot(SGP::dPdy, cross(SGP::baseNormal, SGP::dPdx)) < 0 ? -1 : 1;
        #undef SGP
    }

    static void genBasis(float2 texST, out float3 vT, out float3 vB)
    {
        float2 dSTdx = ddx_fine(texST);
        float2 dSTdy = ddy_fine(texST);
        float det = dot(dSTdx, float2(dSTdy.y, -dSTdy.x));
        float sign_det = det < 0.0 ? -1.0 : 1.0;
        // invC0 represents (dXds, dYds), but we don’t divide
        // by the determinant. Instead, we scale by the sign.
        float2 invC0 = sign_det*float2(dSTdy.y, -dSTdx.y);
        vT = SurfGrad::sigmaX*invC0.x + SurfGrad::sigmaY*invC0.y;
        if (abs(det) > 0.0) vT = normalize(vT);
        vB = (sign_det*SurfGrad::flip_sign)*cross(SurfGrad::baseNormal, vT);
    }
    
    // Input: vM is tangent space normal in [-1;1].
    // Output: convert vM to a derivative.
    static float2 DerivativeFromTangentNormal(float3 vM)
    {
        const float scale = 1.0/128.0;
        // Ensure vM delivers a positive third component using abs() and
        // constrain vM.z so the range of the derivative is [-128; 128].
        const float3 vMa = abs(vM);
        const float z_ma = max(vMa.z, scale*max(vMa.x, vMa.y));
        // Set to match positive vertical texture coordinate axis.
        const bool gFlipVertDeriv = false;
        const float s = gFlipVertDeriv ? -1.0 : 1.0;
        return -float2(vM.x, s*vM.y)/z_ma;
    }

    static float3 fromDerivativeAndBasis(float2 deriv, float3 vT, float3 vB)
    {
        return deriv.x*vT + deriv.y*vB;
    }

    // Input: vM is tangent space normal in [-1,1]
    static float3 fromTangentNormal(float3 vM, float3 vT, float3 vB)
    {
        float2 deriv = DerivativeFromTangentNormal(vM);
        return fromDerivativeAndBasis(deriv, vT, vB);
    }

    float3 resolveToNormal(float3 surfGrad)
    {
        return normalize(SurfGrad::baseNormal - surfGrad);
    }
};

#endif